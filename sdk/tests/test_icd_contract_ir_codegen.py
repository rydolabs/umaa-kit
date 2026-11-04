"""IDL + authored ICD YAML -> Contract IR -> C++ Component v2 regression tests.

All positive input is generated synthetic Demo:: data. Official-source
BatteryStatus/EngineControl mappings are deliberately negative test cases.
"""
from __future__ import annotations
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.generate_icd_yaml import build_service, yaml_text
from tools.umaa_service_contracts import _read_catalog
from tools.umaa_contract_ir import ContractIRError, load_service, resolve
from tools.umaa_icd_component_codegen import generate
from tools.validate_icd_yaml import require_dependencies, ICDYamlError


DEMO_IDL = """// Synthetic fixture only; no official UMAA data or wire contract.
module Demo {
  module EO {
    struct BatteryReportType { @key long source_id; long level_pct; };
    struct EngineCommandType { @key string sessionID; long targetRpm; };
    struct EngineCommandAckReportType { @key string sessionID; long ackCode; };
    struct EngineCommandStatusType { @key string sessionID; long statusCode; };
  };
};
"""


class ContractIRCodegenTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            require_dependencies()
        except ICDYamlError as exc:
            raise unittest.SkipTest("install dedicated ICD YAML Python dependencies") from exc
        cls.catalog, _ = _read_catalog()

    def setUp(self):
        self.t = tempfile.TemporaryDirectory()
        self.addCleanup(self.t.cleanup)
        self.root = Path(self.t.name)
        self.idl = self.root / "demo.idl"
        self.idl.write_text(DEMO_IDL, encoding="utf-8")
        self.idl_sha = hashlib.sha256(self.idl.read_bytes()).hexdigest()

    def authored(self, short: str) -> Path:
        reviewed = next(s for s in self.catalog["services"]
                        if s["name"] == "UMAA::EO::" + short)
        doc = build_service(reviewed, self.catalog)
        doc["metadata"]["id"] = "Demo::EO::" + short
        doc["metadata"]["title"] = "Synthetic " + short
        doc["sources"]["idl"]["bundleSha256"] = self.idl_sha
        doc["spec"]["collections"] = []
        doc["spec"]["specializations"] = []
        doc["spec"]["unresolvedCollectionTypes"] = []
        for alias, topic in doc["spec"]["topics"].items():
            suffix = topic["idlType"].split("::")[-1]
            topic["idlType"] = "Demo::EO::" + suffix
            topic["catalogId"] = topic["idlType"]
            topic["ddsTopicName"] = "demo/" + suffix
            topic["qosProfileRef"] = "synthetic-reliable"
            topic["keyFields"] = ["source_id"] if short == "BatteryStatus" else ["sessionID"]
        if short == "BatteryStatus":
            alias = next(iter(doc["spec"]["topics"]))
            for operation, op in doc["spec"]["operations"].items():
                op["kind"] = "subscribe" if operation.startswith("query") else "publish"
                op["topicRef"] = alias
                op["sourceClause"] = doc["sources"]["icd"]["clauses"][0]
        else:
            for operation, op in doc["spec"]["operations"].items():
                if "Ack" in operation:
                    alias = "ack01"
                elif "Status" in operation:
                    alias = "status01"
                else:
                    alias = "command01"
                op["kind"] = ("cancel" if "cancel" in operation else
                              "subscribe" if "query" in operation else
                              "publish" if "report" in operation else "command")
                op["topicRef"] = alias
                op["sourceClause"] = doc["sources"]["icd"]["clauses"][0]
        path = self.root / (short + ".yaml")
        path.write_text(yaml_text(doc), encoding="utf-8")
        return path

    def manifest(self, services: list[tuple[str, str]]) -> Path:
        doc = {"schemaVersion": "1", "component": {"id": "demo_component",
                "artifact": "shared-library"},
               "services": [{"id": "Demo::EO::" + s, "role": role}
                            for s, role in services]}
        path = self.root / "umaa.component.json"
        path.write_text(json.dumps(doc), encoding="utf-8")
        return path

    def test_contract_ir_report_and_command_with_pinned_idl(self):
        for short in ("BatteryStatus", "EngineControl"):
            p = self.authored(short)
            ir = resolve(p, self.idl, fixture=True)
            self.assertTrue(ir["readiness"]["componentScaffold"], ir["readiness"]["blockers"])
            self.assertFalse(ir["readiness"]["nativeDdsBinding"])
            self.assertEqual(ir["inputs"]["idlSha256"], self.idl_sha)
            self.assertEqual(ir["scope"], "synthetic-fixture")
            self.assertEqual("Demo::EO::" + short, ir["serviceId"])

    def test_official_catalog_files_fail_closed_with_exact_blockers(self):
        for short in ("BatteryStatus", "EngineControl"):
            p = ROOT / "contracts/umaa6/services/EO" / (short + ".yaml")
            ir = resolve(p, self.idl)
            self.assertFalse(ir["readiness"]["componentScaffold"])
            blockers = " ".join(ir["readiness"]["blockers"])
            self.assertIn("source pin is absent", blockers)
            self.assertIn("DDS Topic name unresolved", blockers)
            self.assertIn("Operation", blockers)
            self.assertIn("official IDL binding not reviewed", blockers)

    def test_idl_key_schema_and_source_mutations_are_rejected(self):
        p = self.authored("EngineControl")
        self.assertTrue(resolve(p, self.idl, fixture=True)["readiness"]["componentScaffold"])
        bad = self.root / "bad.idl"
        bad.write_text(DEMO_IDL.replace("@key string sessionID", "string sessionID"))
        ir = resolve(p, bad, fixture=True)
        self.assertFalse(ir["readiness"]["componentScaffold"])
        self.assertTrue(any("SHA-256 does not match" in b for b in ir["readiness"]["blockers"]))
        self.assertTrue(any("Key fields differ" in b for b in ir["readiness"]["blockers"]))

        doc, _ = load_service(p)
        doc["spec"]["topics"]["command01"]["qosProfileRef"] = None
        p.write_text(yaml_text(doc), encoding="utf-8")
        self.assertFalse(resolve(p, self.idl, fixture=True)["readiness"]["componentScaffold"])
        doc["spec"]["topics"]["command01"]["qosProfileRef"] = "synthetic-reliable"
        doc["spec"]["operations"]["setEngine"]["topicRef"] = "missing_alias"
        p.write_text(yaml_text(doc), encoding="utf-8")
        with self.assertRaises(ICDYamlError):
            resolve(p, self.idl, fixture=True)

    def test_fixture_mode_cannot_override_official_service_status(self):
        p = ROOT / "contracts/umaa6/services/EO/BatteryStatus.yaml"
        ir = resolve(p, self.idl, fixture=True)
        self.assertFalse(ir["readiness"]["componentScaffold"])
        self.assertIn("fixture mode is restricted to Demo:: service IDs",
                      ir["readiness"]["blockers"])
        p = self.authored("BatteryStatus")
        self.assertFalse(resolve(p, self.idl)["readiness"]["componentScaffold"])

    def test_generator_fails_without_writes_for_unresolved_official(self):
        official = ROOT / "contracts/umaa6/services/EO/EngineControl.yaml"
        man = self.root / "manifest.json"
        man.write_text(json.dumps({"schemaVersion": "1",
           "component": {"id": "engine", "artifact": "shared-library"},
           "services": [{"id": "UMAA::EO::EngineControl", "role": "provider"}]}))
        out = self.root / "out"
        with self.assertRaisesRegex(ContractIRError, "source pin is absent"):
            generate(man, [official], self.idl, out)
        self.assertFalse(out.exists())

    def test_generator_providers_consumers_and_preserve_user_code(self):
        battery = self.authored("BatteryStatus")
        engine = self.authored("EngineControl")
        man = self.manifest([("BatteryStatus", "provider"),
                             ("EngineControl", "provider"),
                             ("BatteryStatus", "consumer"),
                             ("EngineControl", "consumer")])
        out = self.root / "generated-project"
        result = generate(man, [battery, engine], self.idl, out, fixture=True)
        self.assertEqual(2, result["providerCount"])
        self.assertEqual(2, result["consumerScaffolds"])
        self.assertFalse(result["nativeDdsBinding"])
        code = (out / "generated/component_entry.cpp").read_text()
        self.assertIn("UMAA_EXPORT_COMPONENT_V2", code)
        self.assertEqual(2, code.count("c.provide<"))
        trait = (out / "generated/service_contracts.hpp").read_text()
        self.assertIn("decode_command", trait)
        self.assertIn("static constexpr const char* idl_type =", trait)
        self.assertIn("Demo::EO::", trait)
        self.assertIn("encode_report", trait)
        sdk_traits=(out / "generated/sdk_v1_traits.hpp").read_text()
        self.assertIn("ServiceTraits<::generated::Demo_EO_EngineControl>", sdk_traits)
        self.assertIn("FlowKind::CommandResponse", sdk_traits)
        self.assertIn("FlowKind::QuerylessRequestReply", sdk_traits)
        self.assertIn("WireService::schema_id", sdk_traits)
        self.assertNotIn("dds/dds.h", sdk_traits)
        self.assertTrue((out / "tests/sdk_v1_traits_smoke.cpp").is_file())
        topics=(out / "generated/checked_topic_bindings.hpp").read_text()
        self.assertIn("ReviewedTopics",topics)
        self.assertIn("native_type_support_generated = false",topics)
        self.assertIn("static constexpr const char* command01_key_fields",topics)
        self.assertIn("synthetic-reliable",topics)
        self.assertIn("return {",topics)
        self.assertTrue((out / "tests/checked_topic_bindings_smoke.cpp").is_file())
        self.assertNotIn("dds/dds.h", trait)
        self.assertNotIn("fastdds/", trait)
        user = out / "src/handlers/Demo_EO_BatteryStatus.hpp"
        user.write_text(user.read_text() + "\n// retain business code\n")
        generate(man, [battery, engine], self.idl, out, fixture=True)
        self.assertIn("// retain business code", user.read_text())
        self.assertIn("NO DDS subscription", (out /
            "src/consumers/Demo_EO_BatteryStatus.hpp").read_text())

    def test_existing_component_cli_accepts_yaml_and_idl(self):
        p = self.authored("BatteryStatus")
        man = self.manifest([("BatteryStatus", "provider")])
        result = subprocess.run([sys.executable, "-m", "tools.umaa_component_cli",
                "generate", "--project", str(self.root), "--yaml", str(p),
                "--idl", str(self.idl), "--fixture"],
                cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertTrue((self.root / "generated/component_entry.cpp").is_file())
        self.assertEqual("synthetic-fixture",
                         json.loads((self.root / "generated/generation.json").read_text())["scope"])

    def test_no_silent_support_for_nonprimitive_idl(self):
        p = self.authored("BatteryStatus")
        self.idl.write_text(DEMO_IDL.replace("long level_pct", "double level_pct"))
        doc, _ = load_service(p)
        doc["sources"]["idl"]["bundleSha256"] = hashlib.sha256(self.idl.read_bytes()).hexdigest()
        p.write_text(yaml_text(doc))
        man = self.manifest([("BatteryStatus", "provider")])
        with self.assertRaisesRegex(ContractIRError, "supports IDL long/string only"):
            generate(man, [p], self.idl, self.root / "generated", fixture=True)

    def test_generated_component_compiles_and_runs_abi_and_codec_tests(self):
        if shutil.which("cmake") is None:
            self.fail("cmake required for Component integration smoke")
        battery = self.authored("BatteryStatus")
        engine = self.authored("EngineControl")
        man = self.manifest([("BatteryStatus", "provider"), ("EngineControl", "provider")])
        project = self.root / "module"
        generate(man, [battery, engine], self.idl, project, fixture=True)
        sdk_build, prefix, module_build = (self.root / x for x in ("sdk-build", "prefix", "module-build"))
        def run(args):
            completed = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
            if completed.returncode:
                self.fail("command failed: " + " ".join(map(str, args)) +
                          "\n" + completed.stdout[-4000:] + "\n" + completed.stderr[-4000:])
        run(["cmake", "-S", str(ROOT / "sdk"), "-B", str(sdk_build),
             "-DUMAA_BUILD_TESTS=OFF", "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_INSTALL_PREFIX=" + str(prefix)])
        run(["cmake", "--build", str(sdk_build), "--config", "Release", "--parallel", "2"])
        run(["cmake", "--install", str(sdk_build), "--config", "Release"])
        run(["cmake", "-S", str(project), "-B", str(module_build),
             "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(prefix)])
        run(["cmake", "--build", str(module_build), "--config", "Release", "--parallel", "2"])
        run(["ctest", "--test-dir", str(module_build), "-C", "Release", "--output-on-failure"])
        run(["ctest", "--test-dir", str(module_build), "-C", "Release",
             "--output-on-failure", "-R", "sdk_v1_traits_smoke"])
        run(["ctest", "--test-dir", str(module_build), "-C", "Release",
             "--output-on-failure", "-R", "checked_topic_bindings_smoke"])


if __name__ == "__main__":
    unittest.main()
