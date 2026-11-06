"""Official BatteryStatus Consumer host-preview generator is source and role gated.

Unit tests mock only the external original-byte verifier; they DO NOT
assert that CI possessed/re-verified the original AUVSI ZIP/PDF bytes.
"""
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools import umaa_battery_preview_component_codegen as preview
from tools import umaa_component_cli as cli
from tools import umaa_eo_original_evidence as eo


class BatteryPreviewGenerationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.project = self.root / "component"
        cli.new("batterypreview", self.project)
        cli.add(self.project, eo.BATTERY, "consumer")
        self.zip = self.root / "original.zip"
        self.pdf = self.root / "original.pdf"
        # These are NOT official sources and must fail without the mocked
        # source verification boundary in a unit-only generator test.
        self.zip.write_bytes(b"not an original IDL archive")
        self.pdf.write_bytes(b"not an original EO ICD")

    def verified(self):
        record = eo.load_record(eo.BATTERY)
        return mock.patch.object(
            preview.evidence, "verify_originals", return_value={eo.BATTERY: record})

    def test_generation_produces_business_only_dll_and_contract(self):
        with self.verified():
            result = preview.generate(self.project, self.zip, self.pdf)
        self.assertEqual(result["kind"], "UMAA_BATTERY_HOST_PREVIEW_COMPONENT")
        self.assertTrue(result["buildableModule"])
        self.assertTrue(result["hostOnlyCodec"])
        self.assertFalse(result["nativeDdsRuntimeApproved"])
        self.assertFalse(result["mandatoryQosVerified"])
        header = (self.project / "generated/service_binding.hpp").read_text()
        self.assertIn("BatteryStatusHostPreview", header)
        self.assertIn("BatteryReportTypeCellsListElement", header)
        self.assertIn('"source"', header)
        self.assertIn('"listID", "elementID"', header)
        self.assertIn("nativeDdsQosVerified = false", header)
        self.assertIn("nativeDdsRuntimeApproved = false", header)
        self.assertIn("queryBattery", header)
        self.assertIn("reportBattery", header)
        self.assertNotIn("dds/dds.h", header)
        handler = self.project / "src/handlers/battery_status_consumer.hpp"
        self.assertIn("on_report(", handler.read_text())
        self.assertIn("on_dispose(", handler.read_text())
        self.assertIn("Result::Rejected", handler.read_text())
        self.assertNotIn("dds/dds.h", handler.read_text())
        entry = (self.project / "generated/component_entry.cpp").read_text()
        self.assertIn("c.consume<generated::BatteryStatus>", entry)
        self.assertIn("UMAA_EXPORT_COMPONENT_V2", entry)
        self.assertIn("UMAA::Core", (self.project / "CMakeLists.txt").read_text())
        self.assertNotIn("UMAA::Runtime", (self.project / "CMakeLists.txt").read_text())
        self.assertEqual(
            result["source"]["originalIdlArchiveSha256"],
            eo.load_record(eo.BATTERY)["sources"]["idlArchive"]["sha256"])
        # Regenerate generated bindings without overwriting user-owned Handler.
        handler.write_text(handler.read_text() + "\n// business logic retained\n")
        with self.verified():
            preview.generate(self.project, self.zip, self.pdf)
        self.assertIn("// business logic retained", handler.read_text())

    def test_without_actual_originals_cannot_write_even_one_generated_file(self):
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            preview.generate(self.project, self.zip, self.pdf)
        self.assertFalse((self.project / "CMakeLists.txt").exists())
        self.assertFalse((self.project / "generated").exists())
        with self.assertRaisesRegex(cli.ProjectError, "BOTH original"):
            preview.generate(self.project, None, self.pdf)

    def test_role_and_catalog_pin_must_match(self):
        manifest = self.project / cli.MANIFEST
        good = cli.read(self.project)
        for selection in (
            [{"id": eo.BATTERY, "role": "provider"}],
            [{"id": eo.BATTERY, "role": "consumer"},
             {"id": "Demo::EO::EngineControl", "role": "provider"}],
        ):
            with self.subTest(selection=selection):
                doc = {**good, "services": selection}
                manifest.write_text(json.dumps(doc))
                with self.verified(), self.assertRaisesRegex(
                        cli.ProjectError, "exactly one BatteryStatus consumer"):
                    preview.generate(self.project, self.zip, self.pdf)
                self.assertFalse((self.project / "CMakeLists.txt").exists())
        invalid = {**good, "catalogPin": {
            "kind": "reviewed-icd-catalog", "sha256": "0" * 64}}
        manifest.write_text(json.dumps(invalid))
        with self.verified(), self.assertRaisesRegex(
                cli.ProjectError, "reviewed catalog changed"):
            preview.generate(self.project, self.zip, self.pdf)
        self.assertFalse((self.project / "CMakeLists.txt").exists())

    def test_unreviewed_source_forgery_not_accepted(self):
        original = eo.load_record(eo.BATTERY)
        forged = json.loads(json.dumps(original))
        forged["collectionTopics"][0]["keyFields"] = ["source"]
        with mock.patch.object(preview.evidence, "verify_originals",
                               return_value={eo.BATTERY: forged}), self.assertRaisesRegex(
                                   cli.ProjectError, "verification changed"):
            preview.generate(self.project, self.zip, self.pdf)
        self.assertFalse((self.project / "CMakeLists.txt").exists())

    def test_user_owned_cmake_and_header_protected(self):
        handler = self.project / "src/handlers/battery_status_consumer.hpp"
        handler.parent.mkdir(parents=True)
        handler.write_text("// user code untouched\n")
        (self.project / "CMakeLists.txt").write_text("project(owned)\n")
        with self.verified(), self.assertRaisesRegex(cli.ProjectError, "overwrite"):
            preview.generate(self.project, self.zip, self.pdf)
        self.assertEqual(handler.read_text(), "// user code untouched\n")
        self.assertEqual((self.project / "CMakeLists.txt").read_text(),
                         "project(owned)\n")

    def test_cli_requires_explicit_preview_mode_and_exact_source_paths(self):
        # Current default official generation still cannot produce native DDS.
        with self.assertRaisesRegex(cli.ProjectError, "verified TypeSupport"):
            cli.generate(self.project)
        result = cli.main(["generate", "--project", str(self.project),
                           "--battery-host-preview"])
        self.assertEqual(result, 2)
        self.assertFalse((self.project / "generated").exists())


if __name__ == "__main__":
    unittest.main()
