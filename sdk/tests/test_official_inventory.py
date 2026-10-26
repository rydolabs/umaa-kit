import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from audit_official_idl_catalog import collect

SERVICE = "UMAA::EO::BatteryStatus"
REPORT = SERVICE + "::BatteryReportType"
CATALOG = {
    "complete": False,
    "basis": {"version": "6.0", "sha256": "a" * 64},
    "services": [
        {"name": SERVICE, "pattern": "request_reply_without_query_data",
         "icd_reviewed": True, "clauses": ["5.2.1"],
         "provider_topics": [{"name": REPORT, "type": REPORT, "role": "report"}],
         "consumer_topics": []}
    ]
}


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.root = Path(self.workspace.name)
        for rel in (
            "UMAA/Common/Measurement/Unit.idl",
            "UMAA/EO/BatteryStatus/BatteryReportType.idl",
            "UMAA/EO/EngineControl/EngineCommandType.idl",
            "UMAA/MO/HeadingControl/HeadingCommandType.idl",
        ):
            path = self.root / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("// official test fixture only\n", encoding="utf-8")

    def tearDown(self):
        self.workspace.cleanup()

    def test_all_files_accounted_without_service_guess(self):
        result = collect(self.root, CATALOG, expected_count=4)
        self.assertEqual(result["status"], "PASS")
        self.assertFalse(result["release_g6_complete"])
        self.assertEqual(result["input_idl_count"], 4)
        self.assertEqual(result["directory_candidate_count"], 3)
        self.assertEqual(result["reviewed_topic_type_count"], 1)
        self.assertEqual(result["unreviewed_directory_candidate_count"], 2)
        self.assertIn("UMAA::MO::HeadingControl", result["unreviewed_directory_candidates"])
        self.assertEqual(result["idl_files_not_explicit_reviewed_topic_types"], 3)
        self.assertEqual(len(result["idl_files"]), 4)
        self.assertEqual(len({f["sha256"] for f in result["idl_files"]}), 1)
        self.assertEqual(result["domain_file_counts"]["Common"], 1)

    def test_cardinality_drift_rejected(self):
        with self.assertRaisesRegex(ValueError, "cardinality drift"):
            collect(self.root, CATALOG, expected_count=5)

    def test_missing_explicit_topic_rejected(self):
        (self.root / "UMAA/EO/BatteryStatus/BatteryReportType.idl").unlink()
        with self.assertRaises(ValueError):
            collect(self.root, CATALOG, expected_count=3)

    def test_no_unrecognized_idl_domains(self):
        test = self.root / "UMAA/ZZ/Unknown/UnknownReportType.idl"
        test.parent.mkdir(parents=True)
        test.write_text("module UMAA {};")
        with self.assertRaisesRegex(ValueError, "unrecognized UMAA domain"):
            collect(self.root, CATALOG, expected_count=5)


if __name__ == "__main__":
    unittest.main()
