import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from generate_reviewed_service_api import generate


class ReviewedServiceBindingTests(unittest.TestCase):
    def setUp(self):
        self.catalog = {
            "basis": {"version": "6.0", "sha256": "a" * 64},
            "complete": False,
            "services": [
                {"name": "UMAA::EO::BatteryStatus", "icd_reviewed": True,
                 "pattern": "request_reply_without_query_data", "clauses": ["5.2.1"],
                 "provider_topics": [{"name": "UMAA::EO::BatteryStatus::BatteryReportType",
                                      "type": "UMAA::EO::BatteryStatus::BatteryReportType", "role": "report"}],
                 "consumer_topics": []},
                {"name": "UMAA::EO::EngineControl", "icd_reviewed": True,
                 "pattern": "command_response", "clauses": ["5.1"],
                 "provider_topics": [
                     {"name": "UMAA::EO::EngineControl::EngineCommandStatusType",
                      "type": "UMAA::EO::EngineControl::EngineCommandStatusType", "role": "status"},
                     {"name": "UMAA::EO::EngineControl::EngineCommandAckReportType",
                      "type": "UMAA::EO::EngineControl::EngineCommandAckReportType", "role": "ack"}],
                 "consumer_topics": [{"name": "UMAA::EO::EngineControl::EngineCommandType",
                                      "type": "UMAA::EO::EngineControl::EngineCommandType", "role": "command"}]}
            ]
        }

    def test_preserves_full_official_message_types_and_no_rpc(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            for service in self.catalog["services"]:
                for topic in service["provider_topics"] + service["consumer_topics"]:
                    rel = pathlib.Path(*topic["type"].split("::")).with_suffix(".h")
                    path = root / rel
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("/* placeholder official generated header */\n")
            output = root / "generated.hpp"
            nservices, ntypes = generate(self.catalog, [root], output)
            self.assertEqual((nservices, ntypes), (2, 4))
            text = output.read_text()
            self.assertIn("struct BatteryStatusApi", text)
            self.assertIn("struct EngineControlApi", text)
            self.assertIn("using Command = UMAA_EO_EngineControl_EngineCommandType", text)
            self.assertIn("using Report = UMAA_EO_BatteryStatus_BatteryReportType", text)
            self.assertNotIn("RequestClient", text)
            self.assertNotIn("create_participant", text)
            with self.assertRaises(ValueError):
                generate(self.catalog, [root / "no-files"], output)

    def test_one_service_with_two_report_topics(self):
        service = {
            "name": "UMAA::EO::UVPlatformSpecs", "icd_reviewed": True,
            "pattern": "request_reply_without_query_data",
            "clauses": ["5.2.1", "6.1.14"],
            "provider_topics": [
                {"name": "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType",
                 "type": "UMAA::EO::UVPlatformSpecs::UVPlatformCapabilitiesReportType",
                 "role": "report"},
                {"name": "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType",
                 "type": "UMAA::EO::UVPlatformSpecs::UVPlatformSpecsReportType",
                 "role": "report"},
            ],
            "consumer_topics": []
        }
        self.catalog["services"].append(service)
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            for svc in self.catalog["services"]:
                for topic in svc["provider_topics"] + svc["consumer_topics"]:
                    path = root / pathlib.Path(*topic["type"].split("::")).with_suffix(".h")
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("/* official generated fixture */\n")
            out = root / "api.hpp"
            count, types = generate(self.catalog, [root], out)
            self.assertEqual((count, types), (3, 6))
            text = out.read_text()
            self.assertIn("struct UVPlatformSpecsApi", text)
            self.assertIn("read_UVPlatformCapabilitiesReportType", text)
            self.assertIn("read_UVPlatformSpecsReportType", text)
            self.assertNotIn("RequestClient", text)
            # Duplicate topic records are not a second DDS contract.
            service["provider_topics"].append(service["provider_topics"][0].copy())
            with self.assertRaises(ValueError):
                generate(self.catalog, [root], out)

    def test_incomplete_gate_is_not_silently_marked_complete(self):
        with tempfile.TemporaryDirectory() as td:
            self.catalog["complete"] = True
            with self.assertRaises(ValueError):
                generate(self.catalog, [pathlib.Path(td)], pathlib.Path(td) / "out.hpp")


    def test_foreign_topic_never_generates_official_cpp_api(self):
        topic = self.catalog["services"][0]["provider_topics"][0]
        topic["name"] = topic["type"] = "UMAA::EO::EngineStatus::EngineReportType"
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            output = root / "unsafe_api.hpp"
            with self.assertRaises(ValueError):
                generate(self.catalog, [root], output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
