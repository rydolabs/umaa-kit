"""Component CLI: manifest validation, safe regeneration, installed SDK build smoke."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from tools import umaa_component_cli as cli
from tools import umaa_battery_preview_component_codegen as battery_preview
from tools import umaa_eo_original_evidence as eo_original


class ComponentCliTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.project = self.root / "engine"

    def create(self):
        cli.new("engine", self.project)
        cli.add(self.project, "Demo::EO::EngineControl", "provider")
        cli.add(self.project, "Demo::EO::BatteryStatus", "provider")

    def test_manifest_roundtrip_and_regeneration_without_user_overwrite(self):
        self.create()
        self.assertEqual(cli.read(self.project)["component"]["id"], "engine")
        cli.generate(self.project)
        header = self.project / "src/handlers/engine_control.hpp"
        original = header.read_text()
        header.write_text(original + "\n// user customization retained\n")
        before = (self.project / "generated/component_entry.cpp").read_text()
        cli.generate(self.project)
        self.assertIn("// user customization retained", header.read_text())
        self.assertEqual(before, (self.project / "generated/component_entry.cpp").read_text())
        self.assertIn("UMAA_EXPORT_COMPONENT_V2", before)
        self.assertIn("umaa::Result::Rejected", header.read_text())
        self.assertIn("throw std::runtime_error", (self.project / "src/handlers/battery_status.hpp").read_text())
        self.assertIn("UMAA::Core", (self.project / "CMakeLists.txt").read_text())

    def test_metadata_only_official_service_cannot_masquerade_as_runtime(self):
        cli.new("engine", self.project)
        cli.add(self.project, "UMAA::EO::EngineControl", "provider")
        with self.assertRaisesRegex(cli.ProjectError, "verified TypeSupport"):
            cli.generate(self.project)
        self.assertFalse((self.project / "CMakeLists.txt").exists())

    def test_synthetic_report_consumer_and_dual_role_scaffolds(self):
        cli.new("engine", self.project)
        cli.add(self.project, "Demo::EO::EngineControl", "provider")
        cli.add(self.project, "Demo::EO::BatteryStatus", "consumer")
        self.assertTrue(cli.check_project(self.project)["codegenReady"])
        cli.generate(self.project)
        entry = (self.project / "generated/component_entry.cpp").read_text()
        self.assertIn("c.provide<generated::EngineControl>", entry)
        self.assertIn("c.consume<generated::BatteryStatus>", entry)
        consumer = self.project / "src/handlers/battery_status_consumer.hpp"
        original = consumer.read_text()
        self.assertIn("ReportSink<generated::BatteryStatus>", original)
        self.assertIn("umaa::Result::Rejected", original)
        consumer.write_text(original + "// user-owned consumer customization\n")
        cli.generate(self.project)
        self.assertIn("// user-owned consumer customization", consumer.read_text())
        smoke = (self.project / "tests/module_smoke.cpp").read_text()
        self.assertIn("UMAA_COMPONENT_REPORT_ACCEPT", smoke)
        self.assertIn("UMAA_COMPONENT_REPORT_DISPOSE", smoke)
        self.assertIn("24u", smoke)

        both = self.root / "bidirectional"
        cli.new("bidirectional", both)
        cli.add(both, "Demo::EO::BatteryStatus", "provider")
        cli.add(both, "Demo::EO::BatteryStatus", "consumer")
        self.assertTrue(cli.check_project(both)["codegenReady"])
        cli.generate(both)
        self.assertEqual((both / "generated/service_contracts.hpp").
                         read_text().count("struct BatteryStatus {"), 1)
        self.assertIn("28u", (both / "tests/module_smoke.cpp").read_text())
        self.assertIn("c.provide<generated::BatteryStatus>",
                      (both / "generated/component_entry.cpp").read_text())
        self.assertIn("c.consume<generated::BatteryStatus>",
                      (both / "generated/component_entry.cpp").read_text())

    def test_unimplemented_synthetic_command_consumer_stays_blocked(self):
        cli.new("engine", self.project)
        cli.add(self.project, "Demo::EO::EngineControl", "consumer")
        self.assertFalse(cli.check_project(self.project)["codegenReady"])
        with self.assertRaisesRegex(cli.ProjectError, "Command consumer module bindings"):
            cli.generate(self.project)
        self.assertFalse((self.project / "CMakeLists.txt").exists())

    def test_duplicate_unknown_and_bad_role_are_rejected(self):
        cli.new("engine", self.project)
        cli.add(self.project, "Demo::EO::BatteryStatus", "provider")
        with self.assertRaises(cli.ProjectError):
            cli.add(self.project, "Demo::EO::BatteryStatus", "provider")
        with self.assertRaises(cli.ProjectError):
            cli.add(self.project, "UMAA::SA::NonexistentStatus", "provider")
        with self.assertRaises(cli.ProjectError):
            cli.add(self.project, "Demo::EO::EngineControl", "both")
        self.assertEqual(len(cli.read(self.project)["services"]), 1)

    def test_manifest_rejects_bad_keys_versions_paths_and_injection(self):
        self.create()
        path = self.project / cli.MANIFEST
        good = cli.read(self.project)
        cases = [
            {**good, "schemaVersion": "9"},
            {**good, "unexpected": 1},
            {**good, "component": {"id": "../outside", "artifact": "shared-library"}},
            {**good, "services": [{"id": "Demo::EO::EngineControl;evil", "role": "provider"}]},
            {**good, "services": [{"id": "Demo::EO::EngineControl", "role": "owner"}]},
        ]
        for bad in cases:
            with self.subTest(bad=bad):
                path.write_text(json.dumps(bad))
                with self.assertRaises(cli.ProjectError):
                    cli.generate(self.project)

    def test_refuse_overwrite_of_owned_cmake_file(self):
        self.create()
        (self.project / "CMakeLists.txt").write_text("project(user_custom)\n")
        with self.assertRaisesRegex(cli.ProjectError, "overwrite"):
            cli.generate(self.project)
        self.assertEqual((self.project / "CMakeLists.txt").read_text(),
                         "project(user_custom)\n")

    def test_cli_from_command_line(self):
        cmd = ["python", "-m", "tools.umaa_component_cli"]
        root = cli.ROOT
        result = subprocess.run(cmd + ["new", "component", "engine", "--output",
                                      str(self.project)], cwd=root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(cmd + ["service", "add", "Demo::EO::EngineControl",
                                      "--role", "provider", "--project", str(self.project)],
                                cwd=root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(cmd + ["generate", "--project", str(self.project)],
                                cwd=root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.project / "generated/component_entry.cpp").exists())


    def test_host_config_from_generated_component_is_user_owned_and_strict(self):
        self.create()
        cli.generate(self.project)
        module = self.root / "libengine.so"
        backend = self.root / "libtest-dds.so"
        module.write_bytes(b"mock executable module path (not loaded here)")
        backend.write_bytes(b"mock plugin path (not loaded here)")
        config = self.root / "host.yml"
        output = cli.generate_host_config(
            self.project, backend, module, config, 91, "vehicle-a", 25)
        self.assertEqual(output, config)
        text = config.read_text(encoding="utf-8")
        self.assertIn("  backend: ", text)
        self.assertIn("  domain_id: 91", text)
        self.assertIn("Demo::EO::BatteryStatus", text)
        self.assertIn("    topic: demo/BatteryStatus", text)
        self.assertIn("    interval_ms: 25", text)
        self.assertIn(module.resolve().as_posix(), text)
        before = config.read_bytes()
        cli.generate(self.project)
        self.assertEqual(config.read_bytes(), before)
        with self.assertRaisesRegex(cli.ProjectError, "no overwrite"):
            cli.generate_host_config(
                self.project, backend, module, config, 91, "vehicle-a", 25)
        for key, domain, interval in [
            ("", 91, 25), ("bad key", 91, 25), ("valid", -1, 25),
            ("valid", 231, 25), ("valid", 91, 0),
            ("valid", 91, 3600001),
        ]:
            with self.subTest(key=key, domain=domain, interval=interval):
                with self.assertRaises(cli.ProjectError):
                    cli.generate_host_config(
                        self.project, backend, module, self.root / "invalid.yml",
                        domain, key, interval)
        with self.assertRaisesRegex(cli.ProjectError, "ordinary file"):
            cli.generate_host_config(
                self.project, self.root / "missing", module,
                self.root / "bad.yml", 91, "vehicle-a", 25)
        other = self.root / "consumer-only"
        cli.new("consumeronly", other)
        cli.add(other, "Demo::EO::BatteryStatus", "consumer")
        cli.generate(other)
        with self.assertRaisesRegex(cli.ProjectError, "requires Demo Battery provider"):
            cli.generate_host_config(
                other, backend, module, self.root / "bad.yml", 91,
                "vehicle-a", 25)

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest"),
                         "CMake and CTest are needed for an actual native module smoke")
    def test_installed_sdk_generated_module_build_and_load(self):
        self.create()
        cli.generate(self.project)
        build = self.root / "sdk-build"
        prefix = self.root / "sdk-install"
        subprocess.run(
            ["cmake", "-S", str(cli.ROOT / "sdk"), "-B", str(build),
             "-DUMAA_BUILD_TESTS=OFF", "-DCMAKE_INSTALL_PREFIX=" + str(prefix)],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        subprocess.run(["cmake", "--build", str(build), "--config", "Release"],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        subprocess.run(["cmake", "--install", str(build), "--config", "Release"],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        cli.run_cmake(self.project, prefix)
        cli.run_test(self.project)

        # The same installed SDK must load real native C++ modules containing
        # both Provider and Consumer Service entries, then one Service with
        # *both* roles combined under a single ABI-v2 descriptor.
        mixed = self.root / "mixed"
        cli.new("mixed", mixed)
        cli.add(mixed, "Demo::EO::EngineControl", "provider")
        cli.add(mixed, "Demo::EO::BatteryStatus", "consumer")
        cli.generate(mixed)
        cli.run_cmake(mixed, prefix)
        cli.run_test(mixed)

        both = self.root / "both"
        cli.new("both", both)
        cli.add(both, "Demo::EO::BatteryStatus", "provider")
        cli.add(both, "Demo::EO::BatteryStatus", "consumer")
        cli.generate(both)
        cli.run_cmake(both, prefix)
        cli.run_test(both)


        # Explicit official Battery Consumer host-preview MODULE can be built
        # from source-pinned reviewed metadata and original-byte verification.
        # CI does NOT have the original source bytes: mock ONLY that upstream
        # boundary to verify native C++ generation, ABI and CTest behavior.
        preview = self.root / "battery-preview"
        cli.new("batterypreview", preview)
        cli.add(preview, eo_original.BATTERY, "consumer")
        original = eo_original.load_record(eo_original.BATTERY)
        with mock.patch.object(
                battery_preview.evidence, "verify_originals",
                return_value={eo_original.BATTERY: original}):
            battery_preview.generate(
                preview, self.root / "external.zip", self.root / "external.pdf")
        cli.run_cmake(preview, prefix)
        cli.run_test(preview)
        # User owns the business result. Even after switching from Rejected
        # to Completed, generated ABI smoke must validate protocol framing
        # without dictating the user's actual business policy.
        handler = preview / "src/handlers/battery_status_consumer.hpp"
        text = handler.read_text()
        self.assertEqual(text.count("::umaa::Result::Rejected;"), 2)
        handler.write_text(text.replace(
            "::umaa::Result::Rejected;", "::umaa::Result::Completed;"))
        cli.run_cmake(preview, prefix)
        cli.run_test(preview)



if __name__ == "__main__":
    unittest.main()
