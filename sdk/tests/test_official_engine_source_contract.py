"""Synthetic-only source/Key/protocol evidence tests for legacy EngineControl.

No original AUVSI IDL/PDF or generated official TypeSupport is stored here.
The Engine ignition-only user module must never enable official SDK Runtime.
"""
from __future__ import annotations

from pathlib import Path
import json
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.generate_icd_yaml import build_service, yaml_text
from tools.generate_official_engine_plugin import (
    main as generate_main, verify_engine_source_contract,
)
from tools.official_icd_evidence import SourceError, sha
from tools.umaa_contract_ir import load_service
from tools.umaa_service_contracts import _read_catalog
from tools.validate_icd_yaml import ICDYamlError, require_dependencies


def synthetic_type(name: str, keys: list[str]) -> bytes:
    prefix = "UMAA::EO::EngineControl"
    body = "".join(f"@key long {key};\n" for key in keys)
    parts = prefix.split("::")
    return ("".join(f"module {part} {{\n" for part in parts)
            + f'const string {name}Topic = "{prefix}::{name}";\n'
            + f"struct {name} {{\n{body}long sample;\n}};\n"
            + "};\n" * len(parts)).encode("utf-8")


class EngineSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            require_dependencies()
        except ICDYamlError as exc:
            raise unittest.SkipTest("pinned YAML validation dependencies unavailable") from exc
        cls.catalog, _ = _read_catalog()

    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.archive = self.root / "synthetic-engine.zip"
        self.data = {
            "LICENSE": b"SYNTHETIC TEST DATA ONLY",
            "UMAA/EO/EngineControl/EngineCommandType.idl":
                synthetic_type("EngineCommandType",
                               ["source", "sessionID", "destination"]),
            "UMAA/EO/EngineControl/EngineCommandAckReportType.idl":
                synthetic_type("EngineCommandAckReportType",
                               ["source", "sessionID"]),
            "UMAA/EO/EngineControl/EngineCommandStatusType.idl":
                synthetic_type("EngineCommandStatusType",
                               ["source", "sessionID"]),
        }
        self.pack()
        service = next(s for s in self.catalog["services"]
                       if s["name"] == "UMAA::EO::EngineControl")
        self.doc = build_service(service, self.catalog)
        self.doc["sources"]["idl"]["bundleSha256"] = self.digest()
        for alias, keys in (
            ("command01", ["source", "sessionID", "destination"]),
            ("ack01", ["source", "sessionID"]),
            ("status01", ["source", "sessionID"]),
        ):
            t = self.doc["spec"]["topics"][alias]
            t["ddsTopicName"] = t["idlType"]
            t["keyFields"] = keys
        self.yaml = self.root / "engine.yaml"
        self.save_yaml()

    def pack(self):
        with zipfile.ZipFile(self.archive, "w", zipfile.ZIP_DEFLATED) as archive:
            for name, body in sorted(self.data.items()):
                archive.writestr(name, body)

    def digest(self):
        return sha(self.archive.read_bytes())

    def save_yaml(self):
        self.yaml.write_text(yaml_text(self.doc), encoding="utf-8")

    def test_exact_three_source_topics_are_candidate_only(self):
        result = verify_engine_source_contract(self.yaml, self.archive, self.digest())
        self.assertEqual("ENGINECONTROL_IGNITION_SUBSET_ONLY", result["scope"])
        self.assertEqual(3, result["verifiedNativeTopicTypes"])
        self.assertEqual(sha(self.yaml.read_bytes()), result["candidateYamlSha256"])
        self.assertFalse(result["reviewedPerServiceQos"])
        self.assertFalse(result["completeProtocolGenerated"])
        self.assertFalse(result["officialNativeRuntimeAuthorized"])
        self.assertFalse(result["formalUmaaConformance"])

    def test_rejects_forged_source_key_role_protocol_and_self_approval(self):
        for label in ("wrong-sha", "wrong-key", "wrong-topic",
                      "wrong-role", "wrong-protocol", "claim-qos", "claim-behavior",
                      "mutated-original-key", "wrong-service"):
            with self.subTest(label=label):
                self.doc, _ = load_service(self.yaml)
                self.doc["sources"]["idl"]["bundleSha256"] = self.digest()
                if label == "wrong-key":
                    self.doc["spec"]["topics"]["command01"]["keyFields"] = ["source"]
                elif label == "wrong-topic":
                    self.doc["spec"]["topics"]["ack01"]["ddsTopicName"] = "Invented"
                elif label == "wrong-role":
                    self.doc["spec"]["topics"]["status01"]["direction"] = "consumer-to-provider"
                elif label == "wrong-protocol":
                    self.doc["spec"]["protocols"][0]["bindings"]["ack"] = "status01"
                elif label == "claim-qos":
                    self.doc["verification"]["ddsQosVerified"] = True
                elif label == "claim-behavior":
                    self.doc["verification"]["ddsBehaviorVerified"] = True
                elif label == "wrong-service":
                    self.doc["metadata"]["id"] = "UMAA::EO::OtherControl"
                elif label == "mutated-original-key":
                    source = "UMAA/EO/EngineControl/EngineCommandType.idl"
                    original = self.data[source]
                    self.data[source] = original.replace(
                        b"@key long destination", b"long destination")
                    self.pack()
                self.save_yaml()
                with self.assertRaises((SourceError, ValueError)):
                    verify_engine_source_contract(
                        self.yaml, self.archive,
                        ("0" * 64 if label == "wrong-sha" else self.digest()))
                if label == "mutated-original-key":
                    self.data[source] = original
                    self.pack()
                self.doc = build_service(
                    next(s for s in self.catalog["services"]
                         if s["name"] == "UMAA::EO::EngineControl"), self.catalog)
                self.doc["sources"]["idl"]["bundleSha256"] = self.digest()
                for alias, keys in (
                    ("command01", ["source", "sessionID", "destination"]),
                    ("ack01", ["source", "sessionID"]),
                    ("status01", ["source", "sessionID"]),
                ):
                    topic = self.doc["spec"]["topics"][alias]
                    topic["ddsTopicName"] = topic["idlType"]
                    topic["keyFields"] = keys
                self.save_yaml()

    def make_synthetic_generated_headers(self):
        types = self.root / "fixture-generated"
        cmd = types / "UMAA/EO/EngineControl/EngineCommandType.h"
        enum = types / "UMAA/Common/MaritimeEnumeration/MaritimeEnumerationSets.h"
        cmd.parent.mkdir(parents=True)
        enum.parent.mkdir(parents=True)
        cmd.write_text("""
typedef struct UMAA_EO_EngineControl_EngineCommandType {
  UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType state;
  UMAA_Common_Measurement_NumericGUID sessionID;
  struct UMAA_Common_IdentifierType source;
  struct UMAA_Common_IdentifierType destination;
  struct UMAA_Common_Measurement_DateTime timeStamp;
} UMAA_EO_EngineControl_EngineCommandType;
""", encoding="utf-8")
        enum.write_text("""
typedef enum UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType {
  UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_OFF = 0,
  UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_RUN = 1
} UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType;
""", encoding="utf-8")
        return types

    def test_cli_writes_limited_evidence_and_rejects_bad_source_before_output(self):
        types = self.make_synthetic_generated_headers()
        output = self.root / "generated"
        argv = ["--type-root", str(types), "--output", str(output),
                "--yaml", str(self.yaml), "--zip", str(self.archive),
                "--expect-sha256", self.digest()]
        self.assertEqual(0, generate_main(argv))
        evidence = json.loads(
            (output / "engine_control_source_evidence.json").read_text())
        self.assertFalse(evidence["officialNativeRuntimeAuthorized"])
        self.assertEqual(3, evidence["verifiedNativeTopicTypes"])
        self.assertTrue((output / "engine_control_handler.hpp").is_file())
        self.assertTrue((output / "engine_control_module.cpp").is_file())

        denied = self.root / "denied-output"
        bad = argv.copy()
        bad[bad.index("--output") + 1] = str(denied)
        bad[bad.index("--expect-sha256") + 1] = "f" * 64
        self.assertEqual(2, generate_main(bad))
        self.assertFalse(denied.exists())
        self.assertEqual(2, generate_main([
            "--type-root", str(types), "--output", str(denied),
            "--yaml", str(self.yaml)]))
        self.assertFalse(denied.exists())


if __name__ == "__main__":
    unittest.main()
