"""Contract tests: six ICDs, command families and optional execution status.

All values are explicitly curated in the catalog; the tests require that the
catalog schema is valid but DO NOT establish runtime DDS/QoS conformance.
"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from verify_official_service_types import inspect
from generate_reviewed_service_api import generate

CATALOG = ROOT / "catalog" / "umaa6-six-icd-reviewed.json"


class AllSixICDContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.baseline = json.loads(CATALOG.read_text(encoding="utf-8"))

    def test_all_six_icd_literal_topics_and_release_gate(self):
        result = inspect(self.baseline)
        self.assertEqual(result["status"], "PASS", result["errors"])
        self.assertEqual(len(self.baseline["services"]), 53)
        self.assertEqual(sum(len(x["provider_topics"]) + len(x["consumer_topics"])
                             for x in self.baseline["services"]), 113)
        self.assertEqual(sorted({x["name"].split("::")[1] for x in self.baseline["services"]}),
                         ["EO", "MM", "MO", "SA", "SEM", "SO"])
        self.assertEqual(inspect(self.baseline, require_complete=True)["status"], "FAIL")
        self.assertFalse(self.baseline["complete"])

    def test_multi_command_and_execution_status_coverage(self):
        multiple = [s for s in self.baseline["services"] if len(s.get("command_groups", [])) > 1]
        self.assertEqual(len(multiple), 5)
        byname = {s["name"]: s for s in self.baseline["services"]}
        self.assertEqual(len(byname["UMAA::MM::MissionPlanMissionControl"]["command_groups"]), 3)
        self.assertEqual(sum(any(t["role"] == "execution_status" for t in s["provider_topics"])
                             for s in self.baseline["services"]), 5)
        self.assertTrue(byname["UMAA::MO::GlobalWaypointControl"]["command_groups"])

    def test_incomplete_or_duplicate_command_group_is_rejected(self):
        doc = copy.deepcopy(self.baseline)
        target = next(s for s in doc["services"] if s["name"] == "UMAA::MM::ConditionalControl")
        target["command_groups"].pop()
        self.assertEqual(inspect(doc)["status"], "FAIL")
        target["command_groups"] = copy.deepcopy(next(s for s in self.baseline["services"]
                           if s["name"] == target["name"])["command_groups"])
        target["command_groups"].append(copy.deepcopy(target["command_groups"][0]))
        self.assertEqual(inspect(doc)["status"], "FAIL")

    def test_real_official_type_names_preserved_by_multi_command_codegen(self):
        # Test fixtures only stand for already-generated official C headers.
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            for service in self.baseline["services"]:
                for topic in service["provider_topics"] + service["consumer_topics"]:
                    h = p / Path(*topic["type"].split("::")).with_suffix(".h")
                    h.parent.mkdir(parents=True, exist_ok=True)
                    h.write_text("/* placeholder; not an actual TypeSupport header */\n")
            out = p / "all_services.hpp"
            count, ntypes = generate(self.baseline, [p], out)
            self.assertEqual((count, ntypes), (53, 113))
            result = out.read_text(encoding="utf-8")
            self.assertIn("struct MissionPlanMissionControlApi", result)
            self.assertIn("using MissionPlanMissionAddCommand =", result)
            self.assertIn("using MissionPlanMissionClearCommand =", result)
            self.assertIn("using MissionPlanMissionDeleteCommand =", result)
            self.assertIn("virtual bool execute_MissionPlanMissionAdd(", result)
            self.assertIn("using ExecutionStatus = UMAA_MO_GlobalDriftControl_GlobalDriftExecutionStatusReportType", result)
            self.assertNotIn("RequestClient", result)


if __name__ == "__main__":
    unittest.main()
