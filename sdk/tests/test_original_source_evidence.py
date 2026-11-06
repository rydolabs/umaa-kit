"""Source-pinned literal topic/key/operation evidence is not DDS approval."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import umaa_original_evidence as originals
from tools import umaa_dds_deployment as deployment
from tools import umaa_system_planner as planner
from tools import umaa_service_contracts as contracts
from tools import umaa_component_cli as component
import hashlib


class OriginalEvidenceTest(unittest.TestCase):
    def test_original_metadata_matches_reviewed_catalog_exactly(self):
        record = originals.load_record()
        self.assertEqual(record["service"], originals.SERVICE)
        self.assertEqual(len(record["topics"]), 3)
        self.assertEqual(len(record["operations"]), 5)
        self.assertEqual(
            {x["action"] for x in record["operations"]},
            {"write", "subscribe", "dispose"})
        for topic in record["topics"]:
            self.assertEqual(topic["ddsTopicName"], topic["idlType"])

    def test_literal_idl_parser_not_a_name_guess(self):
        fixture = '''
            const string AnchorCommandTypeTopic = "wire::Anchor";
            struct AnchorCommandType {
                @key Common::Identifier source;
                int32 notKey;
                @key Common::Identifier destination;
            };
        '''
        topic, keys = originals._literal_layout(fixture, "AnchorCommandType")
        self.assertEqual(topic, "wire::Anchor")
        self.assertEqual(keys, ["source", "destination"])
        with self.assertRaisesRegex(originals.EvidenceError, "missing or ambiguous"):
            originals._literal_layout("struct AnchorCommandType {}", "AnchorCommandType")

    def test_corrupt_pdf_and_archive_do_not_approve_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            pdf = Path(tmp) / "eo.pdf"
            archive = Path(tmp) / "original.zip"
            pdf.write_bytes(b"fake pdf")
            archive.write_bytes(b"fake archive")
            for x, y in ((archive, pdf), (pdf, archive)):
                with self.assertRaisesRegex(originals.EvidenceError, "SHA-256 mismatch"):
                    originals.verify_originals(x, y)

    def test_manifest_key_or_operation_forgery_rejected(self):
        original = originals.RECORD
        with tempfile.TemporaryDirectory() as tmp:
            record = originals.load_record()
            for mutation in ("key", "operation", "source"):
                altered = copy.deepcopy(record)
                if mutation == "key":
                    altered["topics"][0]["keyFields"] = ["source"]
                elif mutation == "operation":
                    altered["operations"][3]["action"] = "write"
                else:
                    altered["sources"]["idlArchive"]["sha256"] = "0" * 64
                f = Path(tmp) / "evidence.json"
                import json
                f.write_text(json.dumps(altered))
                with mock.patch.object(originals, "RECORD", f):
                    with self.subTest(mutation=mutation):
                        with self.assertRaises(originals.EvidenceError):
                            originals.load_record()
        self.assertEqual(originals.RECORD, original)

    def test_admission_explicit_source_pair_only_and_still_blocked(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _, digest = contracts.catalog_names()
            def member(name, role):
                directory = root / name
                component.new(name, directory)
                component.add(directory, originals.SERVICE, role)
                m = directory / component.MANIFEST
                return {"id": name, "manifest": name + "/" + component.MANIFEST,
                        "sha256": hashlib.sha256(m.read_bytes()).hexdigest()}
            model = {
                "kind": planner.KIND, "schemaVersion": "1",
                "system": {"id": "ship"},
                "catalogPin": {"kind": "reviewed-icd-catalog", "sha256": digest},
                "deployment": {"backend": "cyclone", "domainId": 8},
                "components": [member("command", "consumer"), member("winch", "provider")],
                "connections": [{"service": originals.SERVICE,
                                 "consumer": "command", "provider": "winch"}],
            }
            pdf = root / "eo.pdf"
            pdf.write_bytes(b"not an original source")
            with self.assertRaisesRegex(ValueError, "must be supplied together"):
                deployment.assess(model, root, official_eo_icd=pdf)
            with self.assertRaisesRegex(originals.EvidenceError, "SHA-256 mismatch"):
                deployment.assess(model, root, official_eo_icd=pdf,
                                  official_idl_zip=pdf)
            # Mock only the independently verified source boundary. The mocked
            # path tests projection, NOT acceptance of a synthetic original.
            with mock.patch.object(originals, "verify_originals",
                                   return_value=originals.load_record()):
                report = deployment.assess(
                    model, root, official_eo_icd=pdf, official_idl_zip=pdf)
            self.assertEqual(report["sourceVerification"]["status"],
                             "PINNED_ORIGINAL_BYTES_VERIFIED_LIMITED_SCOPE")
            self.assertFalse(report["ddsDeploymentReady"])
            self.assertFalse(report["canEmitNativeEndpoints"])
            check = report["serviceChecks"][0]
            self.assertEqual(len(check["operations"]), 5)
            self.assertEqual(
                {x["name"]: x["flow"] for x in check["operations"]},
                {"setAnchor": "write", "reportAnchorCommandStatus": "write",
                 "reportAnchorCommandAck": "write",
                 "queryAnchorCommandAck": "subscribe",
                 "cancelAnchorCommand": "dispose"})
            self.assertEqual(
                {x["topicId"].split("::")[-1]: x["keyFields"]
                 for x in check["logicalTopics"]},
                {"AnchorCommandType": ["source", "sessionID", "destination"],
                 "AnchorCommandAckReportType": ["source", "sessionID"],
                 "AnchorCommandStatusType": ["source", "sessionID"]})
            self.assertTrue(all(x["writerQosProfile"] is None and
                                x["readerQosProfile"] is None
                                for x in check["logicalTopics"]))
            self.assertEqual(
                {x["code"] for x in check["missingEvidence"]},
                {"MANDATORY_WRITER_READER_QOS", "PROTOCOL_LIFECYCLE",
                 "INDEPENDENT_VENDOR_WIRE_EVIDENCE"})


if __name__ == "__main__":
    unittest.main()
