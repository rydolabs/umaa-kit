"""Guard the ROS 2 RMW-style split: SDK ABI / vendor plugins / wire proof.

This checks source-tree packaging invariants, not actual QoS or middleware
semantics. Native CTest and independent-process Cyclone/Fast DDS CI remain
mandatory. All active vendor plugins must be declared and ABI-compatible.
"""
from pathlib import Path
import copy
import json
import unittest

ROOT = Path(__file__).resolve().parents[2]
DDS = ROOT / "dds"
VENDORS = DDS / "vendors"
ENTRY = "umaa_get_dds_backend_v1"
ABI = "sdk/include/umaa/internal/dds_backend.h"
FIXTURES = {
    "report": ("adapter.cpp", "DemoBattery.idl", "Family::Report"),
    "command": ("command_adapter.cpp", "DemoCommandEvent.idl", "Family::Command"),
    "collection": ("collection_adapter.cpp", "DemoCollectionBatch.idl", "Family::Collection"),
}


def validate_manifest(vendor: str, data: object):
    if not isinstance(data, dict):
        raise ValueError("vendor manifest must be an object")
    required = {"schemaVersion", "vendor", "role", "status", "abi",
                "capabilities", "normativeUmaaWireVerified", "families"}
    allowed = required | {"blockers"}
    if set(data) - allowed or not required <= set(data):
        raise ValueError("missing or unknown vendor manifest keys")
    if data["schemaVersion"] != "1" or data["vendor"] != vendor or data["role"] != "runtime-plugin":
        raise ValueError("wrong vendor schema/name/role")
    if data["abi"] != {"version": 1, "entry": ENTRY, "header": ABI}:
        raise ValueError("backend ABI v1 must be implemented exactly")
    if data["normativeUmaaWireVerified"] is not False:
        raise ValueError("official wire validation cannot be inferred from synthetic tests")
    if not isinstance(data["capabilities"], list) or not isinstance(data["families"], list):
        raise ValueError("invalid backend capabilities/families")
    if data["status"] == "blocked":
        if data["families"] or data["capabilities"] or not data.get("blockers"):
            raise ValueError("blocked vendor may not claim working bindings")
        return
    if data["status"] != "synthetic-development":
        raise ValueError("unreviewed backend readiness/validation class")
    if data["capabilities"] != ["reliable", "instance-lifecycle"]:
        raise ValueError("unsupported native capability claim")
    by_name = {}
    for family in data["families"]:
        if not isinstance(family, dict) or set(family) != {"name", "source", "fixture"}:
            raise ValueError("invalid binding family metadata")
        name = family["name"]
        if name in by_name or name not in FIXTURES:
            raise ValueError("missing/duplicate/unknown family")
        source, fixture, _ = FIXTURES[name]
        if family["source"] != source or family["fixture"] != "dds/fixtures/idl/" + fixture:
            raise ValueError("backend TypeSupport fixture/entry mismatch")
        by_name[name] = family
    if set(by_name) != set(FIXTURES):
        raise ValueError("all synthetic fixture families must be declared")
    if "blockers" in data:
        raise ValueError("active backend cannot masquerade as blocked")


class DdsBackendLayoutTest(unittest.TestCase):
    def test_plugin_manifest_contract_and_vendor_coverage(self):
        plugins = [v for v in VENDORS.iterdir()
                   if v.is_dir() and v.name != "common"]
        self.assertEqual({"cyclone", "fastdds", "rti"},
                         {v.name for v in plugins})
        for directory in plugins:
            with self.subTest(vendor=directory.name):
                manifest = json.loads((directory / "backend.json").read_text(encoding="utf-8"))
                validate_manifest(directory.name, manifest)
                if manifest["status"] == "blocked":
                    self.assertFalse(list(directory.glob("*.cpp")))
                    self.assertFalse((directory / "CMakeLists.txt").exists())
                    continue
                cmake = (directory / "CMakeLists.txt").read_text(encoding="utf-8")
                self.assertNotIn("../../cyclone/idl/", cmake)
                self.assertIn("../../fixtures/idl/", cmake)
                for family in manifest["families"]:
                    src = (directory / family["source"]).read_text(encoding="utf-8")
                    self.assertIn('#include "umaa/internal/dds_backend.h"', src)
                    self.assertIn("demo_type_binding_rules.hpp", src)
                    self.assertIn(ENTRY, src)
                    self.assertIn('extern "C"', src)
                    self.assertIn("UMAA_DDS_ABI_VERSION", src)
                    self.assertIn("UmaaDdsBackendV1", src)
                    self.assertIn("UMAA_DDS_EXPORT", src)
                    self.assertIn("vendor::demo::capabilities", src)
                    self.assertIn("vendor::demo::accepts", src)
                    self.assertIn(FIXTURES[family["name"]][2], src)
                    self.assertIn(family["source"], cmake)
                    self.assertTrue((ROOT / family["fixture"]).is_file())

    def test_verification_harness_not_runtime_and_fixtures_vendor_neutral(self):
        harness = DDS / "cyclone"
        self.assertTrue((harness / "CMakeLists.txt").is_file())
        self.assertFalse((harness / "idl").exists())
        self.assertIn("NOT the SDK plugin",
                      (harness / "README.md").read_text(encoding="utf-8"))
        self.assertIn("../fixtures/idl/",
                      (harness / "CMakeLists.txt").read_text(encoding="utf-8"))
        self.assertFalse(any(ENTRY in p.read_text(encoding="utf-8")
                             for p in harness.rglob("*.cpp")))
        self.assertEqual({f.name for f in (DDS / "fixtures/idl").glob("*.idl")},
                         {fixture for _, fixture, _ in FIXTURES.values()})

    def test_shared_allowlist_and_exact_capabilities(self):
        source = (VENDORS / "common/demo_type_binding_rules.hpp").read_text(encoding="utf-8")
        for topic, schema in (
            ("demo/BatteryStatus", "demo:battery-v1"),
            ("demo/AsyncEngineCommand", "demo:async-command-v1"),
            ("demo/AsyncEngineAck", "demo:async-ack-v1"),
            ("demo/AsyncEngineStatus", "demo:async-status-v1"),
            ("demo/CollectionBatch", "demo:collection-batch-v1"),
        ):
            self.assertIn(f'{{"{topic}", "{schema}"}}', source)
        self.assertIn("UMAA_DDS_CAP_RELIABLE | UMAA_DDS_CAP_INSTANCE_LIFECYCLE", source)
        self.assertNotIn("UMAA_DDS_CAP_COHERENT_GROUP |", source)

    def test_fail_closed_manifest_negative_cases(self):
        good = json.loads((VENDORS / "cyclone/backend.json").read_text(encoding="utf-8"))
        cases = []
        wrong = copy.deepcopy(good); wrong["vendor"] = "fastdds"; cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["abi"]["version"] = 2; cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["normativeUmaaWireVerified"] = True; cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["capabilities"].append("coherent-group"); cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["families"][0]["fixture"] = "dds/cyclone/idl/DemoBattery.idl"; cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["families"].pop(); cases.append(wrong)
        wrong = copy.deepcopy(good); wrong["extra"] = "unapproved"; cases.append(wrong)
        for case in cases:
            with self.subTest(case=case):
                with self.assertRaises(ValueError):
                    validate_manifest("cyclone", case)


if __name__ == "__main__":
    unittest.main()
