"""Source evidence audit tests. All archive bodies are SYNTHETIC, never AUVSI IDL."""
from __future__ import annotations

from pathlib import Path
import json
import sys
import tempfile
import unittest
import warnings
import zipfile

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.generate_icd_yaml import build_service, yaml_text
from tools.umaa_service_contracts import _read_catalog
from tools.official_icd_evidence import SourceError, audit, inspected_zip, sha, source_type
from tools.validate_icd_yaml import require_dependencies, ICDYamlError, strict_load


def idl(namespace: str, declarations: list[tuple[str, list[str]]]) -> str:
    start = "".join("module " + part + " {\n" for part in namespace.split("::"))
    chunks = []
    for name, keys in declarations:
        fullname = namespace + "::" + name
        chunks.append(f'const string {name}Topic = "{fullname}";\n'
                      f'struct {name} {{\n'
                      + "".join(f"@key long {key};\n" for key in keys)
                      + "  long sample;\n};\n")
    return start + "\n".join(chunks) + "};\n" * len(namespace.split("::"))


class SourceAuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            require_dependencies()
        except ICDYamlError as e:
            raise unittest.SkipTest(str(e)) from e
        cls.catalog, _ = _read_catalog()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.zip_path = self.root / "synthetic.zip"
        self.blobs = {
            "LICENSE": b"synthetic test-only archive",
            "UMAA/EO/BatteryStatus/BatteryReportType.idl": idl(
                "UMAA::EO::BatteryStatus",
                [("BatteryReportType", ["source"]),
                 ("BatteryReportTypeCellsListElement", ["listID", "elementID"])]).encode(),
            "UMAA/EO/EngineControl/EngineCommandType.idl": idl(
                "UMAA::EO::EngineControl",
                [("EngineCommandType", ["source", "sessionID", "destination"])]).encode(),
            "UMAA/EO/EngineControl/EngineCommandAckReportType.idl": idl(
                "UMAA::EO::EngineControl",
                [("EngineCommandAckReportType", ["source", "sessionID"])]).encode(),
            "UMAA/EO/EngineControl/EngineCommandStatusType.idl": idl(
                "UMAA::EO::EngineControl",
                [("EngineCommandStatusType", ["source", "sessionID"])]).encode(),
        }
        self.rezip()

    def rezip(self):
        with zipfile.ZipFile(self.zip_path, "w", compression=zipfile.ZIP_DEFLATED) as z:
            for name, body in sorted(self.blobs.items()):
                z.writestr(name, body)

    def yaml(self, short: str) -> Path:
        reviewed = next(s for s in self.catalog["services"] if s["name"] == "UMAA::EO::" + short)
        doc = build_service(reviewed, self.catalog)
        dest = self.root / (short + ".yaml")
        dest.write_text(yaml_text(doc), encoding="utf-8")
        return dest

    def test_exact_source_and_embedded_collection_type(self):
        with inspected_zip(self.zip_path) as z:
            battery = source_type(z, "UMAA::EO::BatteryStatus::BatteryReportType")
            embedded = source_type(z, "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement")
            command = source_type(z, "UMAA::EO::EngineControl::EngineCommandType")
        self.assertEqual(["source"], battery["keyFields"])
        self.assertEqual(["listID", "elementID"], embedded["keyFields"])
        self.assertEqual(battery["idlPath"], embedded["idlPath"])
        self.assertEqual(["source", "sessionID", "destination"], command["keyFields"])
        self.assertEqual(sha(self.blobs[embedded["idlPath"]]), embedded["idlSha256"])

    def test_reviewed_catalog_contract_shapes_are_not_approved(self):
        result = audit(self.zip_path, [self.yaml("BatteryStatus"), self.yaml("EngineControl")])
        self.assertEqual("PASS", result["status"], result["issues"])
        self.assertEqual(2, result["servicesAudited"])
        self.assertEqual(4, result["topicsAudited"])
        self.assertTrue(result["archiveIntegrity"]["allIdlImportsPresent"])
        self.assertEqual(4, result["archiveIntegrity"]["idlFiles"])
        self.assertEqual(1, result["unresolvedCollectionTypesAudited"])
        self.assertEqual(["listID", "elementID"], result["services"][0]["unresolvedCollectionTypes"][0]["keyFields"])
        self.assertFalse(result["approvedDdsQos"])
        self.assertFalse(result["approvedOperationSemantics"])
        self.assertFalse(result["nativeDdsTypeSupportGenerated"])
        for service in result["services"]:
            for topic in service["topics"]:
                self.assertEqual({"ddsTopicName", "keyFields", "qosProfileRef"},
                                 set(topic["unresolvedInYaml"]))

    def test_conflicting_declared_topic_and_key_are_rejected(self):
        path = self.yaml("EngineControl")
        yaml, _ = require_dependencies()
        doc = strict_load(path, yaml)
        doc["spec"]["topics"]["command01"]["ddsTopicName"] = "contradictory"
        doc["spec"]["topics"]["command01"]["keyFields"] = ["wrong_key"]
        path.write_text(yaml_text(doc), encoding="utf-8")
        result = audit(self.zip_path, [path])
        self.assertEqual("CONFLICT", result["status"])
        self.assertEqual(2, len(result["issues"]))

    def test_missing_import_in_external_zip_is_rejected(self):
        source = "UMAA/EO/EngineControl/EngineCommandType.idl"
        self.blobs[source] = b'#include "UMAA/Common/MissingType.idl"\n' + self.blobs[source]
        self.rezip()
        with self.assertRaisesRegex(SourceError, "transitive IDL import"):
            audit(self.zip_path, [self.yaml("EngineControl")])

    def test_forged_topic_missing_type_and_namespace_fail_closed(self):
        path = "UMAA/EO/BatteryStatus/BatteryReportType.idl"
        self.blobs[path] = self.blobs[path].replace(
            b'UMAA::EO::BatteryStatus::BatteryReportType"', b'wrong-topic"')
        self.rezip()
        with inspected_zip(self.zip_path) as z:
            with self.assertRaisesRegex(SourceError, "Topic constant"):
                source_type(z, "UMAA::EO::BatteryStatus::BatteryReportType")
            with self.assertRaisesRegex(SourceError, "no unique owning source"):
                source_type(z, "UMAA::EO::BatteryStatus::MissingType")
        self.blobs[path] = self.blobs[path].replace(b"module BatteryStatus", b"module ForeignStatus")
        self.rezip()
        with inspected_zip(self.zip_path) as z:
            with self.assertRaisesRegex(SourceError, "namespace"):
                source_type(z, "UMAA::EO::BatteryStatus::BatteryReportType")

    def _candidate_inputs(self):
        from tools.umaa_contract_ir import load_service
        source = self.yaml("BatteryStatus")
        doc, _ = load_service(source)
        original = self.root / "synthetic-original-icd.pdf"
        original.write_bytes(b"synthetic exact-byte original, NOT a normative document")
        doc["sources"]["icd"]["reference"]["kind"] = "official"
        doc["sources"]["icd"]["originalSha256"] = sha(original.read_bytes())
        doc["sources"]["idl"]["bundleSha256"] = sha(self.zip_path.read_bytes())
        topic = doc["spec"]["topics"]["report01"]
        topic["ddsTopicName"] = "UMAA::EO::BatteryStatus::BatteryReportType"
        topic["keyFields"] = ["source"]
        source.write_text(yaml_text(doc), encoding="utf-8")
        return source, original, doc

    def test_official_report_source_handoff_is_candidate_only(self):
        from tools.official_report_source_handoff import inspect, main
        source, original, doc = self._candidate_inputs()
        digest = sha(self.zip_path.read_bytes())
        result = inspect(self.zip_path, source, original, digest)
        self.assertEqual("UNAPPROVED_OFFICIAL_REPORT_SOURCE_CANDIDATE", result["kind"])
        self.assertEqual(["source"], result["report"]["declaredKeyFields"])
        self.assertEqual("UMAA/EO/BatteryStatus/BatteryReportType.idl",
                         result["source"]["idlMemberPath"])
        self.assertEqual(sha(original.read_bytes()), result["source"]["originalIcdSha256"])
        self.assertTrue(result["sourceAudit"]["allIdlImportsPresent"])
        self.assertFalse(result["sourceAudit"]["normativeIcdSemanticsReviewed"])
        self.assertFalse(result["readyForOfficialNativeRuntime"])
        self.assertFalse(result["nativeDdsBinding"])
        self.assertFalse(result["componentRuntimeBound"])
        self.assertFalse(result["officialConformanceClaim"])
        self.assertIn("queryBattery", result["unresolvedOperations"])
        self.assertTrue(any("QoS" in s for s in result["blockers"]))
        self.assertNotIn("synthetic exact-byte original", str(result))
        output = self.root / "candidate.json"
        self.assertEqual(0, main(["--zip", str(self.zip_path),
                                  "--yaml", str(source),
                                  "--original-icd", str(original),
                                  "--expect-sha256", digest,
                                  "--out", str(output)]))
        self.assertEqual(result, __import__("json").loads(output.read_text()))
        self.assertEqual(2, main(["--zip", str(self.zip_path),
                                  "--yaml", str(source),
                                  "--original-icd", str(original),
                                  "--expect-sha256", digest,
                                  "--out", str(output)]))

    def test_official_report_candidate_fails_closed_on_forged_sources_and_keys(self):
        from tools.official_report_source_handoff import inspect
        source, original, baseline = self._candidate_inputs()
        correct_digest = sha(self.zip_path.read_bytes())
        for case in ("secondary", "icd-hash", "archive-hash", "topic", "key",
                     "missing-key", "role", "protocol"):
            with self.subTest(case=case):
                from copy import deepcopy
                doc = deepcopy(baseline)
                if case == "secondary":
                    doc["sources"]["icd"]["reference"]["kind"] = "secondary-transcription"
                elif case == "icd-hash":
                    doc["sources"]["icd"]["originalSha256"] = "0" * 64
                elif case == "archive-hash":
                    doc["sources"]["idl"]["bundleSha256"] = "0" * 64
                elif case == "topic":
                    doc["spec"]["topics"]["report01"]["ddsTopicName"] = "forged"
                elif case == "key":
                    doc["spec"]["topics"]["report01"]["keyFields"] = ["wrong"]
                elif case == "missing-key":
                    doc["spec"]["topics"]["report01"]["keyFields"] = None
                elif case == "role":
                    doc["spec"]["topics"]["report01"]["direction"] = "consumer-to-provider"
                elif case == "protocol":
                    doc["spec"]["protocols"][0]["bindings"]["reports"] = []
                source.write_text(yaml_text(doc), encoding="utf-8")
                with self.assertRaises((SourceError, ICDYamlError, ValueError)):
                    inspect(self.zip_path, source, original, correct_digest)
        source.write_text(yaml_text(baseline), encoding="utf-8")
        with self.assertRaisesRegex(SourceError, "archive SHA-256 mismatch"):
            inspect(self.zip_path, source, original, "f" * 64)
        original.write_bytes(b"mutated original")
        with self.assertRaisesRegex(SourceError, "original ICD SHA-256"):
            inspect(self.zip_path, source, original, correct_digest)

    def _synthetic_battery_evidence(self):
        import json
        from tools.official_battery_status_evidence import PIN_MANIFEST
        manifest = json.loads(PIN_MANIFEST.read_text(encoding="utf-8"))
        original = self.root / "synthetic-original.pdf"
        original.write_bytes(b"%PDF-1.4\nfake synthetic test bytes; not normative\n")
        example = self.root / "rti-synthetic-example.xml"
        example.write_text(
            '<dds><qos_library name="UMAAQoSLib">'
            '<qos_profile name="DefaultUMAAParticipant">'
            '<publisher_qos><presentation>'
            '<access_scope>TOPIC_PRESENTATION_QOS</access_scope>'
            '<ordered_access>true</ordered_access>'
            '</presentation></publisher_qos>'
            '<subscriber_qos><presentation>'
            '<access_scope>TOPIC_PRESENTATION_QOS</access_scope>'
            '<ordered_access>true</ordered_access>'
            '</presentation></subscriber_qos>'
            '</qos_profile><qos_profile name="ElementQoS"/>'
            '</qos_library></dds>', encoding="utf-8")
        manifest["sourcePins"]["originalEoIcdSha256"] = sha(original.read_bytes())
        manifest["sourcePins"]["officialIdlArchiveSha256"] = sha(self.zip_path.read_bytes())
        manifest["sourcePins"]["rtiExampleQosXmlSha256"] = sha(example.read_bytes())
        with inspected_zip(self.zip_path) as archive:
            for role, spec in manifest["topics"].items():
                actual = source_type(archive, spec["idlType"])
                for name in ("idlPath", "idlSha256", "idlType",
                             "ddsTopicName", "keyFields", "keyFieldTypes"):
                    spec[name] = actual[name]
        path = self.root / "battery-reference.json"
        path.write_text(json.dumps(manifest, sort_keys=True), encoding="utf-8")
        return path, original, example, manifest

    def test_battery_real_source_shapes_and_rti_example_are_evidence_only(self):
        from tools.official_battery_status_evidence import inspect, main
        path, original, example, pins = self._synthetic_battery_evidence()
        report = inspect(self.zip_path, original, manifest=path,
                         rti_reference_xml=example)
        self.assertEqual("SOURCE_CHECKED_UNAPPROVED_BATTERY_STATUS_EVIDENCE", report["kind"])
        self.assertEqual(["source"], report["topicDeclarations"]["report"]["keyFields"])
        self.assertEqual(["listID", "elementID"],
                         report["topicDeclarations"]["collectionElement"]["keyFields"])
        self.assertEqual(report["topicDeclarations"]["report"]["idlPath"],
                         report["topicDeclarations"]["collectionElement"]["idlPath"])
        self.assertTrue(report["sourceIntegrity"]["allIdlImportsPresent"])
        self.assertFalse(report["sourceIntegrity"]["originalIcdClauseTextMachineReviewed"])
        self.assertTrue(report["rtiExampleQosReference"]["referenceOnly"])
        self.assertFalse(report["rtiExampleQosReference"]["officialQosApproval"])
        self.assertEqual("collectionElement", report["collectionElementQosMinimum"]["appliesTo"])
        self.assertTrue(report["existingReviewedQosMinimum"]["reviewedScopedQosFileMatches"])
        self.assertFalse(report["existingReviewedQosMinimum"]["fullServiceQosApproved"])
        self.assertFalse(any(report["readiness"].values()))
        self.assertNotIn("fake synthetic test bytes", str(report))
        self.assertIsNone(inspect(self.zip_path, original, manifest=path)["rtiExampleQosReference"])
        output = self.root / "once.json"
        args = ["--idl-zip", str(self.zip_path), "--original-icd", str(original),
                "--manifest", str(path), "--rti-reference-xml", str(example),
                "--out", str(output)]
        self.assertEqual(0, main(args))
        self.assertEqual(report, json.loads(output.read_text()))
        self.assertEqual(2, main(args))

    def test_battery_real_source_evidence_fails_on_tampering_and_qos_scope(self):
        import copy
        import json
        from tools.official_battery_status_evidence import inspect
        path, original, example, good = self._synthetic_battery_evidence()
        cases = ("key", "topic", "qos-broadening", "qos-cross-topic",
                 "fake-runtime-approval", "fake-conformance")
        for case in cases:
            with self.subTest(case=case):
                bad = copy.deepcopy(good)
                if case == "key":
                    bad["topics"]["report"]["keyFields"] = ["forged"]
                elif case == "topic":
                    bad["topics"]["collectionElement"]["ddsTopicName"] = "forged"
                elif case == "qos-broadening":
                    bad["collectionElementQosMinimum"]["appliesTo"] = "report"
                elif case == "qos-cross-topic":
                    bad["collectionElementQosMinimum"]["crossTopicOrderingGuaranteed"] = True
                elif case == "fake-runtime-approval":
                    bad["qualification"]["nativeRuntimeApproved"] = True
                elif case == "fake-conformance":
                    bad["qualification"]["officialConformanceClaim"] = True
                path.write_text(json.dumps(bad), encoding="utf-8")
                with self.assertRaises(SourceError):
                    inspect(self.zip_path, original, manifest=path,
                            rti_reference_xml=example)
        path.write_text(json.dumps(good), encoding="utf-8")
        example.write_text(example.read_text().replace(
            "TOPIC_PRESENTATION_QOS", "GROUP_PRESENTATION_QOS"), encoding="utf-8")
        bad = copy.deepcopy(good)
        bad["sourcePins"]["rtiExampleQosXmlSha256"] = sha(example.read_bytes())
        path.write_text(json.dumps(bad), encoding="utf-8")
        with self.assertRaisesRegex(SourceError, "topic/ordered Presentation"):
            inspect(self.zip_path, original, manifest=path, rti_reference_xml=example)
        path.write_text(json.dumps(good), encoding="utf-8")
        with self.assertRaisesRegex(SourceError, "RTI example XML source digest"):
            inspect(self.zip_path, original, manifest=path, rti_reference_xml=example)
        reviewed_qos = self.root / "altered-qos.yaml"
        original_qos = (ROOT / "contracts/qos/umaa6/eo-large-collections.yaml").read_text()
        reviewed_qos.write_text(original_qos.replace('presentationScope: "topic"',
                                                       'presentationScope: "group"'))
        with self.assertRaisesRegex(SourceError, "scoped collection-element QoS minimum drift"):
            inspect(self.zip_path, original, manifest=path, scoped_qos=reviewed_qos)
        original.write_bytes(b"%PDF-1.4\naltered")
        with self.assertRaisesRegex(SourceError, "EO ICD PDF source digest"):
            inspect(self.zip_path, original, manifest=path)
        original.write_bytes(b"%PDF-1.4\nfake synthetic test bytes; not normative\n")
        self.blobs["UMAA/EO/BatteryStatus/BatteryReportType.idl"] += b"\n// altered"
        self.rezip()
        with self.assertRaisesRegex(SourceError, "official IDL ZIP digest"):
            inspect(self.zip_path, original, manifest=path)

    def test_source_checked_overlay_preserves_unreviewed_contract_and_blocks_overwrite(self):
        from tools.official_icd_overlay import produce
        from tools.umaa_contract_ir import load_service
        source = self.yaml("BatteryStatus")
        output = self.root / "authored"
        digest = sha(self.zip_path.read_bytes())
        baseline, _ = load_service(source)
        report = produce(self.zip_path, [source], output, digest)
        self.assertEqual(1, report["services"])
        self.assertEqual(1, report["unresolvedCollectionTypes"])
        result, _ = load_service(output / "services/EO/BatteryStatus.yaml")
        topic = next(iter(result["spec"]["topics"].values()))
        self.assertEqual("UMAA::EO::BatteryStatus::BatteryReportType", topic["ddsTopicName"])
        self.assertEqual(["source"], topic["keyFields"])
        self.assertIsNone(topic["qosProfileRef"])
        self.assertEqual(digest, result["sources"]["idl"]["bundleSha256"])
        self.assertEqual(baseline["verification"], result["verification"])
        self.assertEqual(baseline["spec"]["operations"], result["spec"]["operations"])
        self.assertEqual(baseline["spec"]["unresolvedCollectionTypes"],
                         result["spec"]["unresolvedCollectionTypes"])
        with self.assertRaisesRegex(SourceError, "nonexistent"):
            produce(self.zip_path, [source], output, digest)
        with self.assertRaisesRegex(SourceError, "SHA-256"):
            produce(self.zip_path, [source], self.root / "bad", "0" * 64)
        self.assertFalse((self.root / "bad").exists())

    def test_reject_zip_slip_duplicate_and_missing_license(self):
        cases = [
            {"LICENSE": b"x", "../escape.idl": b"bad"},
            {"LICENSE": b"x", "/UMAA/EO/bad.idl": b"bad"},
            {"UMAA/EO/BatteryStatus/BatteryReportType.idl": b"bad"},
        ]
        for blobs in cases:
            with self.subTest(blobs=blobs):
                self.blobs = blobs
                self.rezip()
                with self.assertRaises(SourceError):
                    inspected_zip(self.zip_path)
        # Windows ZipInfo normalizes backslashes when creating the archive.
        # Patch both central and local ZIP filename bytes for a genuine attack.
        self.blobs = {"LICENSE": b"x", "UMAA/EO/bad.idl": b"bad"}
        self.rezip()
        blob = self.zip_path.read_bytes()
        self.zip_path.write_bytes(blob.replace(b"UMAA/EO/bad.idl", b"UMAA\\EO\\bad.idl"))
        with self.assertRaisesRegex(SourceError, "unsafe"):
            inspected_zip(self.zip_path)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(self.zip_path, "w") as z:
                z.writestr("LICENSE", b"x")
                z.writestr("LICENSE", b"x")
        with self.assertRaisesRegex(SourceError, "duplicate"):
            inspected_zip(self.zip_path)


if __name__ == "__main__":
    unittest.main()
