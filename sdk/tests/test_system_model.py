"""System Model v1: pinned real Component Manifests and reviewed AnchorControl.

Strictly validates design topology; never claims official native DDS Runtime,
normative QoS, source-verified Operation bindings or conformance.
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
from tools import umaa_system_model as system


ANCHOR = "UMAA::EO::AnchorControl"
BATTERY = "Demo::EO::BatteryStatus"
GOOD_QOS = {
    "reliability": "reliable",
    "durability": "volatile",
    "history": "keep_last",
    "depth": 16,
    "presentation": "instance",
    "orderedAccess": False,
    "coherentAccess": False,
    "partition": "",
}


class SystemModelTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        _, self.catalog_sha = contracts.catalog_names()
        self.model = self.make_model(ANCHOR)

    def manifest(self, name, service, role, domain=7):
        folder = self.root / name
        component.new(name, folder)
        component.add(folder, service, role)
        file = folder / component.MANIFEST
        return {
            "id": name, "manifest": name + "/umaa.component.json",
            "sha256": hashlib.sha256(file.read_bytes()).hexdigest(),
            "domainId": domain,
        }

    def make_model(self, service):
        official = service.startswith("UMAA::")
        provider = self.manifest("anchor_provider", service, "provider")
        consumer = self.manifest("anchor_consumer", service, "consumer")
        topics = []
        if official:
            catalog, _ = contracts._read_catalog()
            record = next(item for item in catalog["services"] if item["name"] == service)
            topics = [x["name"] for x in record["provider_topics"] + record["consumer_topics"]]
        return {
            "schemaVersion": "1",
            "system": {"id": "anchor_system"},
            **({"catalogPin": {"kind": "reviewed-icd-catalog", "sha256": self.catalog_sha}}
               if official else {}),
            "components": [provider, consumer],
            "dds": {"domains": [{
                "id": 7, "backend": "cyclone",
                "qosProfiles": {"nominal": copy.deepcopy(GOOD_QOS)},
            }]},
            "connections": [{
                "service": service, "provider": "anchor_provider",
                "consumer": "anchor_consumer", "domainId": 7,
                "channels": [{"topicId": x, "writerProfile": "nominal",
                              "readerProfile": "nominal"} for x in topics],
            }],
        }

    def bad(self, message):
        with self.assertRaisesRegex(system.SystemModelError, message):
            system.validate(self.model, self.root)

    def test_original_anchor_three_topics_and_directions(self):
        report = system.validate(self.model, self.root)
        self.assertEqual(report["status"], "VALID_SYSTEM_DESIGN_NOT_DDS_DEPLOYMENT")
        self.assertEqual(report["components"], 2)
        self.assertEqual(report["connections"], 1)
        self.assertEqual(report["catalogTopicsMapped"], 3)
        directions = {ch["catalogRole"]: (ch["writerComponent"], ch["readerComponent"])
                      for ch in report["channels"]}
        self.assertEqual(directions["command"], ("anchor_consumer", "anchor_provider"))
        for kind in ("ack", "status"):
            self.assertEqual(directions[kind], ("anchor_provider", "anchor_consumer"))
        self.assertFalse(report["officialQosVerified"])
        self.assertFalse(report["officialSourceAndOperationMappingComplete"])
        self.assertFalse(report["ddsDeploymentReady"])
        self.assertFalse(report["officialRuntimeClaim"])
        self.assertFalse(report["officialConformanceClaim"])
        self.assertFalse(report["componentCodegenReady"])

    def test_synthetic_report_model_is_design_not_deployment(self):
        self.temp.cleanup()
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.model = self.make_model(BATTERY)
        report = system.validate(self.model, self.root)
        self.assertTrue(report["componentCodegenReady"])
        self.assertEqual(report["catalogTopicsMapped"], 0)
        self.assertFalse(report["ddsDeploymentReady"])

    def test_missing_or_extra_catalog_channels_rejected(self):
        self.model["connections"][0]["channels"].pop()
        self.bad("every catalog Topic")
        self.model = self.make_model(ANCHOR) if False else self.model
        self.model["connections"][0]["channels"].append({
            "topicId": "UMAA::EO::AnchorControl::InventedType",
            "writerProfile": "nominal", "readerProfile": "nominal"})
        self.bad("unknown or duplicate Topic")

    def test_duplicate_topic_rejected(self):
        ch = self.model["connections"][0]["channels"]
        ch[1]["topicId"] = ch[0]["topicId"]
        self.bad("unknown or duplicate Topic")

    def test_roles_reversed_or_unknown_service_rejected(self):
        self.model["connections"][0]["consumer"] = "anchor_provider"
        self.bad("matching Provider/Consumer")
        self.model["connections"][0]["consumer"] = "anchor_consumer"
        self.model["connections"][0]["service"] = "UMAA::EO::NotPublished"
        self.bad("matching Provider/Consumer")

    def test_cross_domain_and_unknown_backend_rejected(self):
        self.model["components"][1]["domainId"] = 9
        self.bad("unknown DDS domain")
        self.model["components"][1]["domainId"] = 7
        self.model["connections"][0]["domainId"] = 8
        self.bad("cross-domain")
        self.model["connections"][0]["domainId"] = 7
        self.model["dds"]["domains"][0]["backend"] = "rti"
        self.bad("unsupported DDS backend")

    def test_qos_writer_reader_mismatch_rejected(self):
        domain = self.model["dds"]["domains"][0]
        profile = domain["qosProfiles"]["nominal"]
        weaker = copy.deepcopy(profile)
        weaker["reliability"] = "best_effort"
        domain["qosProfiles"]["weak"] = weaker
        self.model["connections"][0]["channels"][0]["writerProfile"] = "weak"
        self.bad("RELIABLE")
        domain["qosProfiles"]["weak"]["reliability"] = "reliable"
        domain["qosProfiles"]["weak"]["partition"] = "different"
        self.bad("partition")
        self.model["connections"][0]["channels"][0]["writerProfile"] = "missing"
        self.bad("unknown explicit DDS QoS")

    def test_unreviewed_qos_cannot_claim_official_verification(self):
        self.model["dds"]["domains"][0]["qosProfiles"]["nominal"]["unreviewedFlag"] = True
        self.bad("unknown fields")
        self.model["dds"]["domains"][0]["qosProfiles"]["nominal"].pop("unreviewedFlag")
        self.model["dds"]["domains"][0]["qosProfiles"]["nominal"]["orderedAccess"] = True
        self.bad("requires TOPIC or GROUP")

    def test_stale_catalog_and_manifest_sha_rejected(self):
        self.model["catalogPin"]["sha256"] = "0" * 64
        self.bad("stale reviewed ICD")
        self.model["catalogPin"]["sha256"] = self.catalog_sha
        self.model["components"][0]["sha256"] = "0" * 64
        self.bad("digest mismatch")

    def test_unsafe_manifest_path_and_symlink_rejected(self):
        self.model["components"][0]["manifest"] = "../anchor_provider/umaa.component.json"
        self.bad("relative umaa.component.json")
        self.model["components"][0]["manifest"] = "/tmp/umaa.component.json"
        self.bad("unsafe Component Manifest path")
        self.model["components"][0]["manifest"] = "anchor_consumer/umaa.component.json"
        self.bad("same Component Manifest referenced twice")

    def test_duplicate_component_connection_and_unknown_fields_rejected(self):
        self.model["components"][1]["id"] = "anchor_provider"
        self.bad("duplicate Component")
        self.model["components"][1]["id"] = "anchor_consumer"
        self.model["connections"].append(copy.deepcopy(self.model["connections"][0]))
        self.bad("duplicate Service connection")
        self.model["connections"].pop()
        self.model["inventedComplianceClaim"] = True
        self.bad("unknown fields")

    def test_duplicate_json_keys_rejected_by_file_loader(self):
        path = self.root / "umaa.system.json"
        path.write_text('{"schemaVersion":"1","schemaVersion":"1"}', encoding="utf-8")
        with self.assertRaisesRegex(system.SystemModelError, "duplicate JSON field"):
            system.validate_file(path)

    def test_cli_both_entry_points(self):
        path = self.root / "umaa.system.json"
        path.write_text(json.dumps(self.model), encoding="utf-8")
        for cli in (
            [sys.executable, "-m", "tools.umaa_system_model", "validate", "--input", str(path)],
            [sys.executable, "-m", "tools.umaa_component_cli", "system", "validate",
             "--input", str(path)],
        ):
            with self.subTest(cli=cli[2]):
                result = subprocess.run(cli, cwd=component.ROOT, text=True,
                                        capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertFalse(json.loads(result.stdout)["ddsDeploymentReady"])


if __name__ == "__main__":
    unittest.main()
