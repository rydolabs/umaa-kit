"""Backend-neutral IDL compiler orchestration and fail-closed regression tests."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase
from unittest.mock import patch
import hashlib
import json
import sys
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.native_dds_codegen import native_generate
from tools.native_dds_backends import discover, load


class NativeCompilerOrchestrationTests(TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.root = self.folder / "idl"
        self.root.mkdir()
        (self.root / "Common.idl").write_text(
            "module Demo { struct Value { long n; }; };")
        (self.root / "Device.idl").write_text(
            '#include "Common.idl"\nmodule Demo { struct Device { sequence<Value> values; }; };')
        self.out = self.folder / "generated"
        self.commands = []

    def mock_compiler(self, args, **kwargs):
        self.assertTrue(kwargs.get("check"))
        self.assertGreater(kwargs.get("timeout", 0), 0)
        self.commands.append(args)
        if args[0] == "idlc":
            dest = Path(args[args.index("-o") + 1])
            stem = Path(args[-1]).stem
            dest.mkdir(parents=True, exist_ok=True)
            (dest / (stem + ".c")).write_text("/* mock */")
            (dest / (stem + ".h")).write_text("/* mock */")
        else:
            self.assertEqual("fastddsgen", args[0])
            dest = Path(args[args.index("-d") + 1])
            stem = Path(args[-1]).stem
            dest.mkdir(parents=True, exist_ok=True)
            (dest / (stem + "PubSubTypes.cxx")).write_text("/* mock */")
            (dest / (stem + ".h")).write_text("/* mock */")

    def mock_tools(self):
        return (patch("shutil.which", side_effect=lambda x: x),
                patch("subprocess.run", side_effect=self.mock_compiler))

    def test_compilers_and_backend_independent_cmake(self):
        a, b = self.mock_tools()
        digest = hashlib.sha256((self.root / "Device.idl").read_bytes()).hexdigest()
        with a, b:
            result = native_generate(self.root, "Device.idl", self.out,
                                     vendor="both", source_sha256=digest)
        self.assertEqual(["cyclone", "fastdds"], sorted(result["backends"]))
        self.assertEqual(4, len(self.commands))
        self.assertEqual(2, result["transitiveIdls"])
        self.assertEqual("UMAA-native-IDL-generator-v2", result["format"])
        self.assertFalse(result["nativeDdsBinding"])
        self.assertFalse(result["officialConformanceClaim"])
        self.assertEqual(len(result["nativeTargets"]), 2)
        self.assertTrue((self.out / "native-generation.json").is_file())
        cmake = (self.out / "native-targets.cmake").read_text()
        self.assertIn("CycloneDDS::ddsc", cmake)
        self.assertIn("fastrtps fastcdr", cmake)
        self.assertIn("UMAA_NATIVE_TARGETS", cmake)
        self.assertIn("DevicePubSubTypes.cxx", str(result["nativeSources"]["fastdds"]))

    def test_single_backend_does_not_need_other_compiler(self):
        a, b = self.mock_tools()
        with a, b:
            result = native_generate(self.root, "Device.idl", self.out,
                                     backends=["cyclone"])
        self.assertEqual(["cyclone"], result["backends"])
        self.assertEqual(["idlc", "idlc"], [cmd[0] for cmd in self.commands])

    def test_new_plugin_requires_no_orchestrator_or_cmake_edit(self):
        def generate(root, target, closure, dest, *, executable=None):
            (dest / "user_types.cxx").write_text("int example = 0;")
            return {
                "sources": ["user_types.cxx"], "includeDirs": ["."],
                "language": "CXX", "cmakePackages": [],
                "cmakeLibraries": [], "cmakeFeatures": ["cxx_std_17"],
                "cmakeProperties": {},
            }
        fake = SimpleNamespace(available=lambda executable=None: True,
                               generate=generate)
        with patch("tools.native_dds_codegen.discover", return_value=("custom",)), \
             patch("tools.native_dds_codegen.load", return_value=fake):
            result = native_generate(self.root, "Device.idl", self.out, vendor="all")
        self.assertEqual(["custom"], result["backends"])
        cmake = (self.out / "native-targets.cmake").read_text()
        self.assertIn("add_library(umaa_native_custom_types", cmake)
        self.assertNotIn("CycloneDDS", cmake)
        self.assertEqual(["umaa_native_custom_types"], result["nativeTargets"])

    def test_external_entry_point_discovery_requires_no_code_modification(self):
        backend = SimpleNamespace(generate=lambda *a, **kw: {})
        entry = SimpleNamespace(name="thirdparty", load=lambda: backend)
        points = SimpleNamespace(select=lambda group: [entry] if
                                 group == "umaa.native_dds_backends" else [])
        with patch("tools.native_dds_backends.metadata.entry_points",
                   return_value=points):
            self.assertIn("thirdparty", discover())
            self.assertIs(load("thirdparty"), backend)

    def test_native_report_runtime_plugin_contract_no_vendor_core_changes(self):
        binding = {
            "typeName": "Demo::EO::BatteryReportType",
            "idlSource": "DemoBatteryReport.idl",
            "keyField": "source_id",
            "valueField": "level_pct",
            "topicName": "demo/NativeBatteryReport",
            "schemaId": "demo:native-battery-report-v2",
            "qosProfile": "synthetic-reliable",
            "serviceId": "Demo::EO::BatteryStatus",
        }
        for name in ("cyclone", "fastdds"):
            info = load(name).runtime_adapter(binding)
            self.assertEqual("CXX", info["language"])
            self.assertEqual(".cpp", info["extension"])
            self.assertIn("DemoBatteryReport.h", info["source"])
            self.assertIn("demo:native-battery-report-v2", info["source"])
            self.assertIn("level_pct", info["source"])
            self.assertIn("UMAA_DDS_EVENT_DISPOSE", info["source"])
            self.assertIn("umaa_get_dds_backend_v1", info["source"])
            self.assertNotIn('"DemoBattery.h"', info["source"])
        with self.assertRaisesRegex(ValueError, "unsafe"):
            load("cyclone").runtime_adapter({**binding, "valueField": "foo;evil()"})

    def test_pin_mismatch_prevents_compiler_and_output(self):
        with patch("subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                native_generate(self.root, "Device.idl", self.out,
                                source_sha256="0" * 64)
            run.assert_not_called()
        self.assertFalse(self.out.exists())

    def test_path_traversal_and_symlink_rejected(self):
        with self.assertRaisesRegex(ValueError, "relative"):
            native_generate(self.root, "../secret.idl", self.out)
        (self.root / "Linked.idl").symlink_to(self.root / "Device.idl")
        with self.assertRaisesRegex(ValueError, "symlink"):
            native_generate(self.root, "Linked.idl", self.out)

    def test_missing_tool_fails_closed_before_generation(self):
        with patch("shutil.which", return_value=None):
            with self.assertRaises(FileNotFoundError):
                native_generate(self.root, "Device.idl", self.out)
        self.assertFalse(self.out.exists())

    def test_unknown_plugin_rejected(self):
        with self.assertRaisesRegex(ValueError, "unregistered"):
            load("nonexistent")
        with self.assertRaisesRegex(ValueError, "unsafe"):
            load("../evil")
        self.assertIn("cyclone", discover())
