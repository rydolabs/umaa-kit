"""Reviewed EO BatteryStatus/EngineControl source assertions and strict denial."""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import umaa_component_cli as component
from tools import umaa_dds_deployment as deployment
from tools import umaa_eo_original_evidence as eo
from tools import umaa_original_evidence as anchor
from tools import umaa_service_contracts as contracts
from tools import umaa_system_planner as planner


class EOOriginalEvidenceTest(unittest.TestCase):
    def test_reviewed_records_are_exact_catalog_matches(self):
        battery = eo.load_record(eo.BATTERY)
        engine = eo.load_record(eo.ENGINE)
        self.assertEqual(battery["service"], eo.BATTERY)
        self.assertEqual(engine["service"], eo.ENGINE)
        self.assertEqual(len(battery["topics"]), 1)
        self.assertEqual(len(battery["collectionTopics"]), 1)
        self.assertEqual(len(engine["topics"]), 3)
        self.assertEqual(engine["collectionTopics"], [])
        self.assertEqual(len(battery["operations"]), 2)
        self.assertEqual(len(engine["operations"]), 5)
        self.assertEqual(battery["topics"][0]["keyFields"], ["source"])
        self.assertEqual(battery["collectionTopics"][0]["keyFields"],
                         ["listID", "elementID"])
        rules = battery["sourceConstraints"]
        self.assertEqual(rules[0]["accessScope"], "topic")
        self.assertTrue(rules[0]["orderedAccess"])
        self.assertEqual(
            {x["name"]: x["action"] for x in battery["operations"]},
            {"reportBattery": "write", "queryBattery": "subscribe"})
        self.assertEqual(
            {x["name"]: x["action"] for x in engine["operations"]},
            {"setEngine": "write", "queryEngineCommandAck": "subscribe",
             "cancelEngineCommand": "dispose",
             "reportEngineCommandStatus": "write",
             "reportEngineCommandAck": "write"})

    def test_record_mutations_rejected_fail_closed(self):
        for service in (eo.BATTERY, eo.ENGINE):
            with self.subTest(service=service), tempfile.TemporaryDirectory() as tmp:
                for change in ("topic_name", "key", "operation", "source",
                               "collection", "inject"):
                    altered = copy.deepcopy(eo.load_record(service))
                    if change == "topic_name":
                        altered["topics"][0]["ddsTopicName"] = "invented"
                    elif change == "key":
                        altered["topics"][0]["keyFields"] = ["fake"]
                    elif change == "operation":
                        altered["operations"][0]["action"] = "write" if altered[
                            "operations"][0]["action"] != "write" else "dispose"
                    elif change == "source":
                        altered["sources"]["icd"]["sha256"] = "0" * 64
                    elif change == "collection":
                        altered["collectionTopics"].append({"topicId": "fake"})
                    else:
                        altered["canEmitNativeEndpoints"] = True
                    p = Path(tmp) / "forged.json"
                    p.write_text(json.dumps(altered))
                    with mock.patch.dict(eo.RECORDS, {service: p}):
                        with self.subTest(change=change):
                            with self.assertRaises(anchor.EvidenceError):
                                eo.load_record(service)

    def test_literal_extraction_uses_distinct_structs_in_same_idl(self):
        idl = '''
            const string BatteryReportTypeTopic = "UMAA::EO::BatteryStatus::BatteryReportType";
            struct BatteryReportType {
              @key UMAA::Common::IdentifierType source;
              int32 value;
            };
            const string BatteryReportTypeCellsListElementTopic =
                "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement";
            struct BatteryReportTypeCellsListElement {
              @key UMAA::Common::Measurement::NumericGUID listID;
              @key UMAA::Common::Measurement::NumericGUID elementID;
            };
        '''
        name, keys = anchor._literal_layout(idl, "BatteryReportType")
        self.assertEqual(name, "UMAA::EO::BatteryStatus::BatteryReportType")
        self.assertEqual(keys, ["source"])
        name, keys = anchor._literal_layout(
            idl, "BatteryReportTypeCellsListElement")
        self.assertEqual(name,
                         "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement")
        self.assertEqual(keys, ["listID", "elementID"])

    def test_bad_originals_rejected_even_when_only_battery_selected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "fake.pdf"
            z = Path(tmp) / "fake.zip"
            p.write_bytes(b"fake original PDF")
            z.write_bytes(b"fake original ZIP")
            with self.assertRaisesRegex(anchor.EvidenceError, "SHA-256 mismatch"):
                eo.verify_originals(z, p, {eo.BATTERY})

    def test_real_originals_only_when_provided_by_test_environment(self):
        # CI has no redistributable AUVSI inputs; opt-in validates actual
        # source bytes, IDL member digests and each literal Topic/Key.
        pdf = os.environ.get("UMAA_OFFICIAL_EO_ICD")
        z = os.environ.get("UMAA_OFFICIAL_IDL_ZIP")
        if not pdf or not z:
            self.skipTest("original AUVSI PDF/ZIP not provided")
        records = eo.verify_originals(Path(z), Path(pdf),
                                      {eo.BATTERY, eo.ENGINE})
        self.assertEqual(set(records), {eo.BATTERY, eo.ENGINE})

    def _system(self, root: Path, services: tuple[str, ...]):
        _, digest = contracts.catalog_names()
        requested = []
        for name, role in (("provider", "provider"),
                           ("consumer", "consumer")):
            target = root / name
            component.new(name, target)
            for service in services:
                component.add(target, service, role)
            path = target / component.MANIFEST
            requested.append({
                "id": name, "manifest": name + "/" + component.MANIFEST,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        return {
            "kind": planner.KIND, "schemaVersion": "1",
            "system": {"id": "vehicle"},
            "catalogPin": {"kind": "reviewed-icd-catalog", "sha256": digest},
            "deployment": {"backend": "fastdds", "domainId": 20},
            "components": requested,
            "connections": [{"service": name, "provider": "provider",
                             "consumer": "consumer"} for name in services],
        }

    def test_selected_source_reports_battery_collection_without_endpoint(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            model = self._system(root, (eo.BATTERY, eo.ENGINE))
            bad = root / "bad"
            bad.write_bytes(b"not an official original")
            with mock.patch.object(eo, "verify_originals", return_value={
                eo.BATTERY: eo.load_record(eo.BATTERY),
                eo.ENGINE: eo.load_record(eo.ENGINE),
            }):
                report = deployment.assess(
                    model, root, official_eo_icd=bad, official_idl_zip=bad)
            self.assertEqual(set(report["sourceVerification"]["services"]),
                             {eo.BATTERY, eo.ENGINE})
            self.assertEqual(report["logicalEndpointCount"], 4)
            self.assertEqual(report["nativeEndpointSpecs"], [])
            self.assertIsNone(report["nativeParticipantSpec"])
            self.assertFalse(report["canEmitNativeEndpoints"])
            self.assertFalse(report["ddsDeploymentReady"])
            mapping = {x["service"]: x for x in report["serviceChecks"]}
            battery = mapping[eo.BATTERY]
            self.assertEqual(len(battery["logicalTopics"]), 1)
            self.assertEqual(battery["logicalTopics"][0]["keyFields"], ["source"])
            self.assertEqual(len(battery["sourceCollectionTopics"]), 1)
            self.assertEqual(battery["sourceCollectionTopics"][0]["keyFields"],
                             ["listID", "elementID"])
            self.assertEqual(
                battery["partialSourceQosRules"][0]["accessScope"], "topic")
            self.assertTrue(battery["partialSourceQosRules"][0]["orderedAccess"])
            self.assertIsNone(battery["logicalTopics"][0]["writerQosProfile"])
            self.assertIsNone(battery["logicalTopics"][0]["readerQosProfile"])
            self.assertIn("COLLECTION_ENDPOINT_OWNERSHIP",
                          {x["code"] for x in battery["missingEvidence"]})
            self.assertIn("COLLECTION_TOPIC_COVERAGE_INCOMPLETE",
                          {x["code"] for x in report["blockers"]})
            self.assertEqual(
                {x["name"]: x["flow"] for x in battery["operations"]},
                {"queryBattery": "subscribe", "reportBattery": "write"})
            self.assertEqual(len(mapping[eo.ENGINE]["logicalTopics"]), 3)
            self.assertEqual(mapping[eo.ENGINE]["sourceCollectionTopics"], [])
            self.assertEqual(
                {x["name"]: x["flow"] for x in mapping[eo.ENGINE]["operations"]}[
                    "cancelEngineCommand"], "dispose")

    def test_only_selected_and_verified_sources_can_fill_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            model = self._system(root, (eo.BATTERY, eo.ENGINE))
            default = deployment.assess(model, root)
            self.assertEqual(default["sourceVerification"]["status"],
                             "ORIGINAL_BYTES_NOT_PROVIDED")
            self.assertTrue(all(
                t["keyFields"] is None
                for c in default["serviceChecks"] for t in c["logicalTopics"]))
            fake = root / "fake"
            fake.write_bytes(b"fake")
            # A source verifier is mocked for one service *only*.
            # Unselected/unverified services must remain unresolved.
            with mock.patch.object(eo, "verify_originals",
                                   return_value={eo.BATTERY: eo.load_record(eo.BATTERY)}):
                report = deployment.assess(
                    model, root, official_eo_icd=fake, official_idl_zip=fake)
            mapping = {x["service"]: x for x in report["serviceChecks"]}
            self.assertIsNone(mapping[eo.ENGINE]["logicalTopics"][0]["keyFields"])
            self.assertIsNotNone(mapping[eo.BATTERY]["logicalTopics"][0]["keyFields"])


if __name__ == "__main__":
    unittest.main()
