"""Component CLI: manifest validation, safe regeneration, installed SDK build smoke."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools import umaa_component_cli as cli


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

    def test_consumer_role_requires_explicit_runtime_binding(self):
        cli.new("engine", self.project)
        cli.add(self.project, "Demo::EO::BatteryStatus", "consumer")
        with self.assertRaisesRegex(cli.ProjectError, "consumer module bindings"):
            cli.generate(self.project)

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


if __name__ == "__main__":
    unittest.main()
