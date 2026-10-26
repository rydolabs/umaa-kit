import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from generate_reviewed_type_support import selected_types

CATALOG = {
    "complete": False,
    "basis": {"version": "6.0", "sha256": "a" * 64},
    "services": [{
        "name": "UMAA::EO::UVPlatformSpecs", "pattern": "request_reply_without_query_data",
        "icd_reviewed": True, "clauses": ["5.2.1", "6.1.14"],
        "provider_topics": [
            {"name": "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType",
             "type": "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType",
             "role": "report"},
            {"name": "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType",
             "type": "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType",
             "role": "report"}],
        "consumer_topics": []}]
}


class ReviewedTypeSupportTests(unittest.TestCase):
    def test_multiple_report_topics_are_distinct_dependencies(self):
        self.assertEqual(selected_types(CATALOG), [
            "UMAA/EO/UVPlatformSpecs/UVPlatformCapabilitiesReportType.idl",
            "UMAA/EO/UVPlatformSpecs/UVPlatformSpecsReportType.idl"])

    def test_unknown_exclusion_rejected(self):
        with self.assertRaises(ValueError):
            selected_types(CATALOG, ["UMAA::EO::Unknown"])

    def test_empty_after_exclusion_requires_fail_closed_runner(self):
        self.assertEqual(selected_types(CATALOG, ["UMAA::EO::UVPlatformSpecs"]), [])

    def test_duplicate_topic_not_silently_generated(self):
        CATALOG["services"][0]["provider_topics"].append(CATALOG["services"][0]["provider_topics"][0].copy())
        try:
            with self.assertRaises(ValueError):
                selected_types(CATALOG)
        finally:
            CATALOG["services"][0]["provider_topics"].pop()


if __name__ == "__main__":
    unittest.main()
