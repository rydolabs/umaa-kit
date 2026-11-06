"""Minimum-input Studio/CLI System Planner: real reviewed Catalog + pinned roles.

Only a design preview; no guessed native DDS Topic names, keys, QoS or
manufactured runtime/conformance success.
"""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools import umaa_component_cli as component
from tools import umaa_service_contracts as contracts
from tools import umaa_system_model as common
from tools import umaa_system_planner as planner


ANCHOR = "UMAA::EO::AnchorControl"
DEMO = "Demo::EO::BatteryStatus"


class CompositionPlannerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        _, self.digest = contracts.catalog_names()
        self.model = {
            "kind": planner.KIND,
            "schemaVersion": "1",
            "system": {"id": "vehicle"},
            "catalogPin": {"kind": "reviewed-icd-catalog", "sha256": self.digest},
            "deployment": {"backend": "cyclone", "domainId": 7},
            "components": [
                self.member("ship_control", ANCHOR, "consumer"),
                self.member("anchor_actuator", ANCHOR, "provider"),
            ],
            "connections": [
                {"service": ANCHOR, "consumer": "ship_control",
                 "provider": "anchor_actuator"},
            ],
        }

    def member(self, name, service, role):
        directory = self.root / name
        component.new(name, directory)
        component.add(directory, service, role)
        path = directory / component.MANIFEST
        return {"id": name, "manifest": name + "/umaa.component.json",
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

    def fail(self, expected):
        with self.assertRaisesRegex(common.SystemModelError, expected):
            planner.plan(self.model, self.root)

    def test_automatic_anchor_endpoint_direction_without_user_qos(self):
        preview = planner.plan(self.model, self.root)
        self.assertEqual(preview["status"], "DESIGN_PREVIEW_NOT_DEPLOYABLE")
        self.assertEqual(preview["endpointCount"], 3)
        self.assertEqual(preview["componentCount"], 2)
        self.assertEqual(preview["connectionCount"], 1)
        self.assertTrue(preview["topologyReferencesValid"])
        by_role = {e["role"]: e for e in preview["endpoints"]}
        self.assertEqual(by_role["command"]["writerComponent"], "ship_control")
        self.assertEqual(by_role["command"]["readerComponent"], "anchor_actuator")
        for role in ("ack", "status"):
            self.assertEqual(by_role[role]["writerComponent"], "anchor_actuator")
            self.assertEqual(by_role[role]["readerComponent"], "ship_control")
        for endpoint in preview["endpoints"]:
            self.assertEqual(endpoint["backend"], "cyclone")
            self.assertEqual(endpoint["domainId"], 7)
            self.assertTrue(endpoint["topicId"].startswith("UMAA::EO::AnchorControl::"))
            self.assertEqual(endpoint["idlType"], endpoint["topicId"])
            self.assertEqual(endpoint["status"], "SOURCE_BINDING_REQUIRED")
            for k in ("ddsTopicName", "keyFields", "writerQosProfile",
                      "readerQosProfile"):
                self.assertIsNone(endpoint[k], k)
        codes = {b["code"] for b in preview["blockers"]}
        self.assertIn("TOPIC_NAME_KEY_AND_OPERATION_MAPPING_UNRESOLVED", codes)
        self.assertIn("MANDATORY_DDS_QOS_UNVERIFIED", codes)
        for flag in ("ddsDeploymentReady", "officialRuntimeClaim",
                     "officialConformanceClaim"):
            self.assertFalse(preview[flag])
        self.assertFalse(preview["ddSNamesKeysAndMandatoryQosResolved"])

    def test_backend_selection_changes_preview_only(self):
        self.model["deployment"] = {"backend": "fastdds", "domainId": 17}
        preview = planner.plan(self.model, self.root)
        self.assertTrue(all(e["backend"] == "fastdds" and e["domainId"] == 17
                            for e in preview["endpoints"]))
        self.assertFalse(preview["ddsDeploymentReady"])

    def test_empty_connection_is_an_explicit_draft_not_implicit_pair(self):
        self.model["connections"] = []
        preview = planner.plan(self.model, self.root)
        self.assertEqual(preview["endpointCount"], 0)
        self.assertIn("UNCONNECTED_CONSUMER_SERVICE",
                      {x["code"] for x in preview["blockers"]})
        self.assertFalse(preview["ddsDeploymentReady"])

    def test_role_reversal_unknown_and_self_link_fail(self):
        self.model["connections"][0]["provider"] = "ship_control"
        self.fail("distinct known Components")
        self.model["connections"][0]["provider"] = "anchor_actuator"
        self.model["connections"][0]["consumer"] = "anchor_actuator"
        self.fail("distinct known Components")
        self.model["connections"][0]["consumer"] = "ship_control"
        self.model["connections"][0]["service"] = "UMAA::EO::InventedService"
        self.fail("declared Consumer/Provider role")

    def test_missing_or_wrong_catalog_pin_rejected(self):
        self.model.pop("catalogPin")
        self.fail("missing or unknown fields")
        self.model["catalogPin"] = {"kind": "reviewed-icd-catalog", "sha256": "0" * 64}
        self.fail("stale reviewed Catalog")
        self.model["catalogPin"] = {"kind": "invented", "sha256": self.digest}
        self.fail("unknown Catalog pin")

    def test_mutated_component_and_duplicate_paths_rejected(self):
        self.model["components"][0]["sha256"] = "a" * 64
        self.fail("digest mismatch")
        self.model["components"][0]["sha256"] = hashlib.sha256(
            (self.root / "ship_control/umaa.component.json").read_bytes()).hexdigest()
        self.model["components"][0]["manifest"] = "anchor_actuator/umaa.component.json"
        self.fail("duplicate Component Manifest")

    def test_absolute_traversal_symlink_and_unknown_inputs_rejected(self):
        self.model["components"][0]["manifest"] = "../ship_control/umaa.component.json"
        self.fail("relative umaa.component.json")
        self.model["components"][0]["manifest"] = "/tmp/umaa.component.json"
        self.fail("unsafe Component Manifest path")
        self.model["components"][0]["manifest"] = "ship_control/umaa.component.json"
        link = self.root / "pointer"
        try:
            link.symlink_to(self.root / "ship_control", target_is_directory=True)
        except (OSError, NotImplementedError):
            pass
        else:
            self.model["components"][0]["manifest"] = "pointer/umaa.component.json"
            self.fail("symlinked Component Manifest")
            self.model["components"][0]["manifest"] = "ship_control/umaa.component.json"
        self.model["bogusDeploymentApproved"] = True
        self.fail("unknown fields")

    def test_reject_invalid_deployment_and_duplicate_links(self):
        self.model["deployment"]["backend"] = "rti"
        self.fail("unsupported standalone DDS backend")
        self.model["deployment"]["backend"] = "cyclone"
        self.model["deployment"]["domainId"] = True
        self.fail("invalid integer")
        self.model["deployment"]["domainId"] = 7
        self.model["connections"].append(copy.deepcopy(self.model["connections"][0]))
        self.fail("duplicate Service link")

    def test_reject_user_supplied_unverified_topic_qos_or_endpoint(self):
        self.model["connections"][0]["channels"] = [{
            "topicId": "UMAA::EO::AnchorControl::AnchorCommandType",
            "writerProfile": "my_qos"
        }]
        self.fail("unknown fields")
        self.model["connections"][0].pop("channels")
        self.model["deployment"]["qosProfiles"] = {"guessed": {}}
        self.fail("unknown fields")

    def test_source_digest_mismatch_or_duplicate_json_keys(self):
        path = self.root / "umaa.intent.json"
        path.write_text('{"kind":"umaa-system-composition-intent",'
                        '"kind":"umaa-system-composition-intent"}')
        with self.assertRaisesRegex(common.SystemModelError, "duplicate JSON field"):
            planner.plan_file(path)
        path.write_text(json.dumps(self.model), encoding="utf-8")
        self.assertEqual(planner.plan_file(path)["endpointCount"], 3)

    def test_deterministic_preview(self):
        one = planner.plan(self.model, self.root)
        two = planner.plan(copy.deepcopy(self.model), self.root)
        self.assertEqual(json.dumps(one, sort_keys=True), json.dumps(two, sort_keys=True))
        self.assertEqual([x["topicId"] for x in one["endpoints"]],
                         sorted(x["topicId"] for x in one["endpoints"]))

    def test_synthetic_fixture_does_not_fabricate_endpoints(self):
        other = self.root / "synthetic"
        other.mkdir()
        self.root = other
        self.model = {
            "kind": planner.KIND, "schemaVersion": "1",
            "system": {"id": "synthetic"},
            "deployment": {"backend": "cyclone", "domainId": 3},
            "components": [self.member("sensor", DEMO, "provider"),
                           self.member("client", DEMO, "consumer")],
            "connections": [{"service": DEMO, "provider": "sensor",
                             "consumer": "client"}],
        }
        preview = planner.plan(self.model, self.root)
        self.assertEqual(preview["endpointCount"], 0)
        self.assertIn("SYNTHETIC_DDS_TOPIC_BINDING_UNREVIEWED",
                      {x["code"] for x in preview["blockers"]})
        self.assertFalse(preview["ddsDeploymentReady"])

    def test_standalone_and_component_cli_consume_same_model(self):
        source = self.root / "umaa.intent.json"
        source.write_text(json.dumps(self.model), encoding="utf-8")
        commands = [
            [sys.executable, "-m", "tools.umaa_system_planner", "plan",
             "--input", str(source)],
            [sys.executable, "-m", "tools.umaa_component_cli", "system",
             "plan", "--input", str(source)],
        ]
        for argv in commands:
            with self.subTest(command=argv[2]):
                result = subprocess.run(argv, cwd=component.ROOT,
                                        text=True, capture_output=True, timeout=40)
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["endpointCount"], 3)
                self.assertFalse(report["ddsDeploymentReady"])


if __name__ == "__main__":
    unittest.main()
