"""DDS deployment admission must never turn reviewed identities into native DDS."""
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
from tools import umaa_dds_deployment as deployment


ANCHOR = "UMAA::EO::AnchorControl"


class DeploymentAdmissionTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        _, digest = contracts.catalog_names()
        self.model = {
            "kind": planner.KIND, "schemaVersion": "1",
            "system": {"id": "vehicle"},
            "catalogPin": {"kind": "reviewed-icd-catalog", "sha256": digest},
            "deployment": {"backend": "cyclone", "domainId": 7},
            "components": [self.member("command", "consumer"),
                           self.member("actuator", "provider")],
            "connections": [{"service": ANCHOR, "consumer": "command",
                             "provider": "actuator"}],
        }

    def member(self, name, role):
        directory = self.root / name
        component.new(name, directory)
        component.add(directory, ANCHOR, role)
        manifest = directory / component.MANIFEST
        return {"id": name, "manifest": name + "/umaa.component.json",
                "sha256": hashlib.sha256(manifest.read_bytes()).hexdigest()}

    def test_anchor_operation_and_topic_evidence_remains_unresolved(self):
        result = deployment.assess(self.model, self.root)
        self.assertEqual(result["kind"], deployment.KIND)
        self.assertEqual(result["status"], deployment.BLOCKED)
        self.assertEqual(result["logicalEndpointCount"], 3)
        self.assertEqual(result["componentCount"], 2)
        self.assertEqual(result["connectionCount"], 1)
        self.assertTrue(result["topologyReferencesValid"])
        self.assertEqual(result["nativeEndpointSpecs"], [])
        self.assertIsNone(result["nativeParticipantSpec"])
        self.assertFalse(result["canEmitNativeEndpoints"])
        self.assertFalse(result["ddsDeploymentReady"])
        self.assertFalse(result["officialRuntimeClaim"])
        self.assertFalse(result["officialConformanceClaim"])
        service = result["serviceChecks"][0]
        self.assertEqual(service["service"], ANCHOR)
        self.assertEqual({op["name"] for op in service["operations"]},
                         {"reportAnchorCommandAck", "reportAnchorCommandStatus",
                          "setAnchor", "queryAnchorCommandAck", "cancelAnchorCommand"})
        for op in service["operations"]:
            self.assertIsNone(op["topicRef"])
            self.assertIsNone(op["flow"])
        self.assertEqual(len(service["logicalTopics"]), 3)
        for item in service["logicalTopics"]:
            for field in ("ddsTopicName", "keyFields", "writerQosProfile",
                          "readerQosProfile"):
                self.assertIsNone(item[field])
        codes = {r["code"] for r in service["missingEvidence"]}
        self.assertIn("OPERATION_TOPIC_FLOW", codes)
        self.assertIn("ORIGINAL_TYPES_AND_INSTANCE_KEYS", codes)
        self.assertIn("MANDATORY_WRITER_READER_QOS", codes)
        self.assertIn("INDEPENDENT_VENDOR_WIRE_EVIDENCE", codes)
        self.assertFalse(any(service["catalogClaims"].values()))

    def test_fail_closed_on_explicit_user_qos_or_generated_endpoints(self):
        self.model["connections"][0]["channels"] = [{"topicId": "forged"}]
        with self.assertRaisesRegex(common.SystemModelError, "unknown fields"):
            deployment.assess(self.model, self.root)
        del self.model["connections"][0]["channels"]
        self.model["deployment"]["qosProfiles"] = {"selfApproved": {}}
        with self.assertRaisesRegex(common.SystemModelError, "unknown fields"):
            deployment.assess(self.model, self.root)

    def test_mutated_pinned_manifest_and_catalog_rejected(self):
        self.model["components"][0]["sha256"] = "f" * 64
        with self.assertRaisesRegex(common.SystemModelError, "digest mismatch"):
            deployment.assess(self.model, self.root)
        self.model["components"][0]["sha256"] = hashlib.sha256(
            (self.root / "command" / component.MANIFEST).read_bytes()).hexdigest()
        self.model["catalogPin"]["sha256"] = "f" * 64
        with self.assertRaisesRegex(common.SystemModelError, "stale reviewed Catalog"):
            deployment.assess(self.model, self.root)

    def test_no_connection_does_not_produce_default_dds_entities(self):
        self.model["connections"] = []
        result = deployment.assess(self.model, self.root)
        self.assertEqual(result["logicalEndpointCount"], 0)
        self.assertEqual(result["nativeEndpointSpecs"], [])
        self.assertIn("NO_EXPLICIT_SERVICE_CONNECTIONS",
                      {x["code"] for x in result["blockers"]})
        self.assertIn("UNCONNECTED_CONSUMER_SERVICE",
                      {x["code"] for x in result["blockers"]})

    def test_backend_switch_and_determinism_preserve_denial(self):
        left = deployment.assess(self.model, self.root)
        self.model["deployment"] = {"backend": "fastdds", "domainId": 9}
        right = deployment.assess(self.model, self.root)
        self.assertEqual(right["deploymentSelection"],
                         {"backend": "fastdds", "domainId": 9})
        self.assertFalse(right["canEmitNativeEndpoints"])
        self.assertNotEqual(left["source"]["intentCanonicalSha256"],
                            right["source"]["intentCanonicalSha256"])
        self.assertEqual(right, deployment.assess(copy.deepcopy(self.model), self.root))

    def test_reversed_bidirectional_service_links_do_not_cross_contaminate_topics(self):
        # Both Component Manifests declare both roles for this one Service.
        # A->B and B->A must not each include the other's three Topics.
        for index, (name, role) in enumerate(
                (("command", "provider"), ("actuator", "consumer"))):
            component.add(self.root / name, ANCHOR, role)
            path = self.root / name / component.MANIFEST
            self.model["components"][index]["sha256"] = hashlib.sha256(
                path.read_bytes()).hexdigest()
        self.model["connections"].append({
            "service": ANCHOR, "consumer": "actuator",
            "provider": "command"})
        report = deployment.assess(self.model, self.root)
        self.assertEqual(report["logicalEndpointCount"], 6)
        self.assertEqual(len(report["serviceChecks"]), 2)
        for check in report["serviceChecks"]:
            self.assertEqual(len(check["logicalTopics"]), 3)
            self.assertEqual(sum(t["writerComponent"] == check["consumer"]
                                 for t in check["logicalTopics"]), 1)
            self.assertEqual(sum(t["writerComponent"] == check["provider"]
                                 for t in check["logicalTopics"]), 2)

    def test_no_files_written_by_in_memory_admission(self):
        before = sorted(p.relative_to(self.root).as_posix()
                        for p in self.root.rglob("*"))
        deployment.assess(self.model, self.root)
        after = sorted(p.relative_to(self.root).as_posix()
                       for p in self.root.rglob("*"))
        self.assertEqual(before, after)

    def test_duplicate_input_json_keys_rejected(self):
        path = self.root / "umaa.intent.json"
        path.write_text('{"kind":"umaa-system-composition-intent",'
                        '"kind":"umaa-system-composition-intent"}')
        with self.assertRaisesRegex(common.SystemModelError, "duplicate JSON field"):
            deployment.assess_file(path)

    def test_both_cli_entrypoints_report_json_denial_and_exit_three(self):
        path = self.root / "umaa.intent.json"
        path.write_text(json.dumps(self.model), encoding="utf-8")
        commands = (
            [sys.executable, "-m", "tools.umaa_dds_deployment", "check",
             "--input", str(path)],
            [sys.executable, "-m", "tools.umaa_component_cli", "system",
             "deployment-check", "--input", str(path)],
        )
        for command in commands:
            with self.subTest(command=command):
                result = subprocess.run(command, cwd=component.ROOT,
                                        capture_output=True, text=True, timeout=40)
                self.assertEqual(result.returncode, 3, result.stderr)
                output = json.loads(result.stdout)
                self.assertEqual(output["status"], deployment.BLOCKED)
                self.assertFalse(output["canEmitNativeEndpoints"])


if __name__ == "__main__":
    unittest.main()
