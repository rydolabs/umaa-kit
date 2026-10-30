import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from verify_official_service_types import inspect, type_idl_path


class OfficialCatalogProofTests(unittest.TestCase):
    def setUp(self):
        self.doc = {
            "basis": {"version": "6.0", "sha256": "a" * 64},
            "complete": False,
            "services": [{
                "name": "UMAA::EO::BatteryStatus",
                "pattern": "request_reply_without_query_data",
                "icd_reviewed": True,
                "clauses": ["5.2.1", "6.1.5"],
                "provider_topics": [{"name": "UMAA::EO::BatteryStatus::BatteryReportType",
                                     "type": "UMAA::EO::BatteryStatus::BatteryReportType",
                                     "role": "report"}],
                "consumer_topics": [],
                "collection_topics": []
            }]
        }

    def test_type_validation_blocks_path_traversal(self):
        with self.assertRaises(ValueError):
            type_idl_path("UMAA::EO::../../tmp")
        with self.assertRaises(ValueError):
            type_idl_path("UMAA::EO::Battery-Report")
        self.assertEqual(type_idl_path("UMAA::EO::BatteryStatus::BatteryReportType").as_posix(),
                         "UMAA/EO/BatteryStatus/BatteryReportType.idl")

    def test_official_idl_existence_and_content_hash(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            rel = type_idl_path("UMAA::EO::BatteryStatus::BatteryReportType")
            (root / rel).parent.mkdir(parents=True)
            (root / rel).write_text("module UMAA {};", encoding="utf-8")
            result = inspect(self.doc, root)
            self.assertEqual(result["status"], "PASS")
            self.assertFalse(result["complete"])
            self.assertEqual(result["verified_topic_types"], 1)
            self.assertEqual(len(result["verified"][0]["idl_sha256"]), 64)
            (root / rel).unlink()
            self.assertEqual(inspect(self.doc, root)["status"], "FAIL")

    def test_rejects_false_compliance_and_unsupported_rpc(self):
        self.assertEqual(inspect(self.doc, require_complete=True)["status"], "FAIL")
        self.doc["services"][0]["pattern"] = "request_reply_with_query_data"
        self.assertEqual(inspect(self.doc)["status"], "FAIL")
        self.doc["services"][0]["pattern"] = "request_reply_without_query_data"
        self.doc["services"][0]["consumer_topics"] = [{
            "name": "UMAA::EO::BatteryStatus::FakeRequest",
            "type": "UMAA::EO::BatteryStatus::FakeRequest", "role": "command"
        }]
        self.assertEqual(inspect(self.doc)["status"], "FAIL")

    def test_rejects_duplicate_service_or_topic_role(self):
        self.doc["services"].append(self.doc["services"][0].copy())
        self.assertEqual(inspect(self.doc)["status"], "FAIL")


    def test_rejects_foreign_service_topic_even_with_valid_official_type(self):
        topic = self.doc["services"][0]["provider_topics"][0]
        original = topic["name"], topic["type"]
        try:
            topic["name"] = topic["type"] = "UMAA::EO::EngineStatus::EngineReportType"
            result = inspect(self.doc)
            self.assertEqual(result["status"], "FAIL")
            self.assertTrue(any("foreign" in e for e in result["errors"]))
        finally:
            topic["name"], topic["type"] = original

    def test_rejects_foreign_type_with_local_topic(self):
        topic = self.doc["services"][0]["provider_topics"][0]
        original = topic["type"]
        try:
            topic["type"] = "UMAA::EO::EngineStatus::EngineReportType"
            self.assertEqual(inspect(self.doc)["status"], "FAIL")
        finally:
            topic["type"] = original

    def test_rejects_reversed_direction_or_foreign_command(self):
        topic = self.doc["services"][0]["provider_topics"].pop()
        try:
            self.doc["services"][0]["consumer_topics"].append(topic)
            self.assertEqual(inspect(self.doc)["status"], "FAIL")
        finally:
            self.doc["services"][0]["consumer_topics"].pop()
            self.doc["services"][0]["provider_topics"].append(topic)

    def test_malformed_json_shapes_fail_without_unhandled_exception(self):
        topic = self.doc["services"][0]["provider_topics"][0]
        original = topic["name"]
        try:
            for invalid in ([], {}, None, True):
                topic["name"] = invalid
                self.assertEqual(inspect(self.doc)["status"], "FAIL")
        finally:
            topic["name"] = original
        original = self.doc["services"][0]["collection_topics"] if "collection_topics" in self.doc["services"][0] else []
        try:
            self.doc["services"][0]["collection_topics"] = None
            self.assertEqual(inspect(self.doc)["status"], "FAIL")
        finally:
            self.doc["services"][0]["collection_topics"] = original
        original = self.doc["services"][0]["provider_topics"]
        try:
            self.doc["services"][0]["provider_topics"] = "not-a-list"
            self.assertEqual(inspect(self.doc)["status"], "FAIL")
        finally:
            self.doc["services"][0]["provider_topics"] = original

    def test_rejects_foreign_collection_type(self):
        self.doc["services"][0]["collection_topics"] = [
            "UMAA::EO::EngineStatus::EngineReportTypeCellsListElement"]
        result = inspect(self.doc)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("foreign" in e for e in result["errors"]))


if __name__ == "__main__":
    unittest.main()
