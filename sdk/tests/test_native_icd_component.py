"""Actual ICD YAML -> backend registry -> native DDS compiler -> CMake test.

The old scalar IDL parser cannot parse this input; the native component path
uses real compiler plugins for include, nested module, sequence and union.
"""
from __future__ import annotations
from pathlib import Path
import hashlib
import json
import os
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
from tools.native_component_codegen import generate, _binding_map
from tools.umaa_contract_ir import ContractIRError
from tools.validate_icd_yaml import ICDYamlError, require_dependencies

SOURCE = ROOT / "tests/fixtures/idl/native/DemoBatteryReport.idl"
BINDING = "Demo::EO::BatteryReportType=DemoBatteryReport.idl"


class NativeICDGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            require_dependencies()
        except ICDYamlError as exc:
            if os.environ.get("UMAA_NATIVE_REQUIRED"):
                raise
            raise unittest.SkipTest("PyYAML/jsonschema are not installed") from exc
        cls.catalog, _ = _read_catalog()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.idl_root = ROOT / "tests/fixtures/idl/native"
        self.yaml = self.root / "battery.yaml"
        reviewed = next(s for s in self.catalog["services"]
                        if s["name"] == "UMAA::EO::BatteryStatus")
        doc = build_service(reviewed, self.catalog)
        doc["metadata"]["id"] = "Demo::EO::BatteryStatus"
        doc["metadata"]["title"] = "Native synthetic BatteryStatus"
        doc["sources"]["idl"]["bundleSha256"] = hashlib.sha256(
            SOURCE.read_bytes()).hexdigest()
        doc["spec"]["collections"] = []
        doc["spec"]["specializations"] = []
        doc["spec"]["unresolvedCollectionTypes"] = []
        aliases = list(doc["spec"]["topics"])
        for alias, topic in doc["spec"]["topics"].items():
            topic["idlType"] = "Demo::EO::BatteryReportType"
            topic["catalogId"] = topic["idlType"]
            topic["ddsTopicName"] = "demo/NativeBatteryReport"
            topic["qosProfileRef"] = "synthetic-reliable"
            topic["keyFields"] = ["source_id"]
        for operation, spec in doc["spec"]["operations"].items():
            spec["kind"] = "subscribe" if operation.startswith("query") else "publish"
            spec["topicRef"] = aliases[0]
            spec["sourceClause"] = doc["sources"]["icd"]["clauses"][0]
        self.yaml.write_text(yaml_text(doc), encoding="utf-8")
        self.project = self.root / "project"
        self.project.mkdir()
        self.manifest = self.project / "umaa.component.json"
        self.manifest.write_text(json.dumps({
            "schemaVersion": "1",
            "component": {"id": "native_battery", "artifact": "shared-library"},
            "services": [{"id": "Demo::EO::BatteryStatus", "role": "provider"}],
        }), encoding="utf-8")

    def test_missing_explicit_type_binding_fails_before_output(self):
        out = self.root / "unwritten"
        with self.assertRaisesRegex(ContractIRError, "explicit native IDL"):
            generate(self.manifest, [self.yaml], self.idl_root, {}, out, fixture=True)
        self.assertFalse(out.exists())

    def test_unverified_official_contract_fails_closed(self):
        # A source digest and a successful compiler cannot establish formal
        # ICD authorization, source-derived mandatory QoS or runtime codec.
        from tools.umaa_contract_ir import load_service
        doc, _ = load_service(self.yaml)
        doc["metadata"]["id"] = "UMAA::EO::BatteryStatus"
        official = self.root / "fake_official.yaml"
        official.write_text(yaml_text(doc), encoding="utf-8")
        with self.assertRaises(ContractIRError):
            generate(self.manifest, [official], self.idl_root,
                     _binding_map([BINDING]), self.root / "official")
        self.assertFalse((self.root / "official").exists())

    def test_native_generated_idl_complex_fields_compile_and_load(self):
        tools = [shutil.which("cmake"), shutil.which("idlc"),
                 shutil.which("fastddsgen")]
        if not all(tools):
            if os.environ.get("UMAA_NATIVE_REQUIRED"):
                self.fail("real DDS toolchains missing in required integration environment")
            self.skipTest("native compiler integration test needs vendor toolchains")
        # This is the actual user-facing CLI, not a direct invocation of a
        # vendor-specific compiler.
        command = [sys.executable, "-m", "tools.umaa_component_cli",
                   "generate", "--project", str(self.project),
                   "--yaml", str(self.yaml), "--idl-root", str(self.idl_root),
                   "--idl-binding", BINDING, "--fixture"]
        result = subprocess.run(command, cwd=ROOT, capture_output=True,
                                text=True, timeout=100)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        meta = json.loads((self.project / "generated/native-generation.json").read_text())
        self.assertTrue(meta["nativeTypeSupportGenerated"])
        self.assertFalse(meta["nativeDdsBinding"])
        self.assertFalse(meta["componentRuntimeBound"])
        self.assertEqual(["cyclone", "fastdds"], sorted(meta["nativeCompilerBackends"]))
        self.assertIn("runtime_binding_verified = false",
                      (self.project / "generated/native_service_contracts.hpp").read_text())
        self.assertFalse((self.project / "generated/component_entry.cpp").exists())
        build = self.root / "build"
        commands = [
            ["cmake", "-S", str(self.project), "-B", str(build),
             "-DCMAKE_BUILD_TYPE=Release"],
            ["cmake", "--build", str(build), "--parallel", "2"],
            ["ctest", "--test-dir", str(build), "--output-on-failure", "-V"],
        ]
        for argv in commands:
            proc = subprocess.run(argv, text=True, capture_output=True,
                                  timeout=150)
            self.assertEqual(0, proc.returncode,
                             " ".join(argv) + "\n" + proc.stdout[-4000:] +
                             "\n" + proc.stderr[-4000:])
        # Re-generation must preserve user-authored files and produce a
        # deterministic backend-neutral result.
        native = self.project / "generated/native_service_contracts.hpp"
        before = native.read_bytes()
        repeat = subprocess.run(command, cwd=ROOT, text=True, capture_output=True,
                                timeout=100)
        self.assertEqual(0, repeat.returncode, repeat.stderr)
        self.assertEqual(before, native.read_bytes())


if __name__ == "__main__":
    unittest.main()
