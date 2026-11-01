"""Source evidence audit tests. All archive bodies are SYNTHETIC, never AUVSI IDL."""
from __future__ import annotations

from pathlib import Path
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

    def test_source_checked_native_registry_is_pinned_and_build_glue_only(self):
        from tools.official_icd_overlay import produce
        from tools.generate_official_native_type_registry import generate
        sources=[self.yaml("BatteryStatus"),self.yaml("EngineControl")]
        digest=sha(self.zip_path.read_bytes())
        candidate=self.root/"candidate"
        produce(self.zip_path,sources,candidate,digest)
        docs=sorted(candidate.glob("services/*/*.yaml"))
        original_root=self.root/"original"
        for rel,body in self.blobs.items():
            if not rel.endswith(".idl"): continue
            path=original_root/rel
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(body)
        out=self.root/"native"
        report=generate(self.zip_path,docs,out,digest,original_root)
        self.assertEqual((2,4,5),(report["sourceEvidenceServices"],
                         report["sourceEvidenceTopics"],report["nativeDescriptorCount"]))
        self.assertEqual(4,len(report["nativeIdlFiles"]))
        self.assertFalse(report["qosApproved"])
        self.assertFalse(report["payloadCodecGenerated"])
        text=(out/"umaa_native_type_registry.c").read_text(encoding="utf-8")
        self.assertIn("&UMAA_EO_BatteryStatus_BatteryReportType_desc",text)
        self.assertIn("&UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc",text)
        self.assertIn("&UMAA_EO_EngineControl_EngineCommandType_desc",text)
        self.assertNotIn("synthetic test-only archive",text)
        with self.assertRaisesRegex(SourceError,"already exist"):
            generate(self.zip_path,docs,out,digest,original_root)
        with self.assertRaisesRegex(SourceError,"does not match"):
            generate(self.zip_path,docs,self.root/"wrong","0"*64,original_root)
        with self.assertRaisesRegex(SourceError,"unreviewed or unpinned"):
            generate(self.zip_path,sources,self.root/"raw",digest,original_root)
        self.assertFalse((self.root/"raw").exists())

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
