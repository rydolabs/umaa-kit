import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from extract_icd_topic_ledger import extract, audit, DOMAINS


def fixture(code):
    return (
        "**Version 6.0**\n"
        "**6.1.1 UVPlatformSpecs**\n"
        "**6.1.1.1 reportCapabilities**\n"
        "**Namespace:** UMAA::" + code + "::UVPlatformSpecs\n"
        "**Topic:** UVPlatformCapabilitiesReportType\n"
        "**Data Type:** UVPlatformCapabilitiesReportType\n"
        "**6.1.1.2 reportSpecs**\n"
        "**Namespace:** UMAA::" + code + "::UVPlatformSpecs\n"
        "**Topic:** UVPlatformSpecsReportType\n"
        "**Data Type:** UVPlatformSpecsReportType\n"
    ) if code not in ("SEM", "SO") else (
        "**Version 6.0**\n"
        "**5.1.1 SensorStatus**\n"
        "**5.1.1.1 reportSensor**\n"
        "**Namespace:** UMAA::" + code + "::SensorStatus\n"
        "**Topic:** SensorReportType\n"
        "**Data Type:** SensorReportType\n"
    )


class LiteralIcdExtractionTests(unittest.TestCase):
    def test_multiple_topic_operations_preserve_provenance(self):
        records = extract("EO", fixture("EO"))
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0]["operation_clause"], "6.1.1.1")
        self.assertEqual(records[1]["topic"], "UVPlatformSpecsReportType")
        self.assertEqual(records[0]["review_status"], "EXTRACTED_UNREVIEWED")

    def test_other_document_section_numbering(self):
        result = extract("SEM", fixture("SEM"))
        self.assertEqual(result[0]["operation_clause"], "5.1.1.1")

    def test_rejects_invalid_prose_mappings(self):
        with self.assertRaisesRegex(ValueError, "mismatched"):
            extract("EO", fixture("EO").replace(
                "UMAA::EO::UVPlatformSpecs", "UMAA::EO::Unrelated"))
        with self.assertRaisesRegex(ValueError, "mismatched"):
            extract("EO", fixture("EO").replace(
                "**Data Type:** UVPlatformSpecsReportType",
                "**Data Type:** A_DifferentType"))
        with self.assertRaises(ValueError):
            extract("CO", fixture("EO"))

    def test_six_document_idl_and_reviewed_catalog_join(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            docs = root / "markdown"
            orig = root / "idl"
            docs.mkdir()
            for code in DOMAINS:
                (docs / ("UMAA-SPEC-" + code + "ICD.md")).write_text(fixture(code))
                for entry in extract(code, fixture(code)):
                    path = orig / entry["idl_path"]
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("// test fixture")
            catalog = {"services": [{
                "name": "UMAA::EO::UVPlatformSpecs",
                "provider_topics": [{"name": "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType"}],
                "consumer_topics": []}]}
            result = audit(docs, orig, catalog)
            self.assertEqual(result["status"], "PASS")
            self.assertEqual(result["icd_service_sections"], 6)
            self.assertEqual(result["icd_operation_entries"], 10)
            self.assertEqual(result["reviewed_catalog_topics_cross_checked"], 1)
            with self.assertRaisesRegex(ValueError, "Topic mapping incomplete"):
                audit(docs, orig, catalog, complete_domains=("EO",))
            catalog["services"][0]["provider_topics"].append({
                "name": "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType"})
            audited = audit(docs, orig, catalog, complete_domains=("EO",))
            self.assertEqual(audited["domain_topic_mapping_checks"]["EO"]["topic_mappings"], 2)
            self.assertFalse(audited["domain_topic_mapping_checks"]["EO"]["protocol_qos_verified"])
            (orig / "UMAA/EO/UVPlatformSpecs/UVPlatformSpecsReportType.idl").unlink()
            with self.assertRaisesRegex(ValueError, "absent from official IDL"):
                audit(docs, orig, catalog)


if __name__ == "__main__":
    unittest.main()
