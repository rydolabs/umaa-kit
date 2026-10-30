"""Reviewed source provenance, contract projection, UI/CLI manifest invariants."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools import umaa_component_cli as cli
from tools import umaa_service_contracts as contracts


class ReviewedContractTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_exact_catalog_projection_is_deterministic_and_not_conformance(self):
        snapshot = contracts.create_snapshot()
        self.assertEqual(snapshot, contracts.create_snapshot())
        verdict = contracts.validate_snapshot(snapshot)
        self.assertEqual(verdict["services"], 53)
        self.assertEqual(verdict["topics"], 113)
        self.assertFalse(verdict["normativeQosClaim"])
        self.assertFalse(verdict["officialServiceRuntimeClaim"])
        self.assertEqual(snapshot["source"]["scope"],
                         "partial-reviewed-mapping-not-normative-runtime")
        self.assertEqual(snapshot["source"]["catalogSha256"],
                         hashlib.sha256(contracts.CATALOG.read_bytes()).hexdigest())
        report = next(s for s in snapshot["services"]
                      if s["id"] == "UMAA::SA::GlobalPoseStatus")
        self.assertEqual(report["pattern"], "request_reply_without_query_data")
        self.assertEqual(len(report["topics"]), 1)
        self.assertEqual(report["topics"][0]["role"], "report")
        self.assertEqual(report["topics"][0]["direction"], "provider")
        self.assertNotIn("queryGlobalPose", report["operations"])
        self.assertFalse(report["evidence"]["ddsQosVerified"])
        # The reviewed catalog really omits per-service source text for two
        # EO records; do not forge an authoritative clause or provenance link.
        missing = [s["id"] for s in snapshot["services"]
                   if s["source"]["reference"] is None]
        self.assertEqual(missing, ["UMAA::EO::BatteryStatus", "UMAA::EO::EngineControl"])

    def test_roundtrip_and_regeneration_with_user_owned_file_guard(self):
        path = self.root / "contracts.json"
        contracts.write_snapshot(path, contracts.create_snapshot())
        before = path.read_bytes()
        self.assertEqual(contracts.read_snapshot(path), contracts.create_snapshot())
        contracts.write_snapshot(path, contracts.create_snapshot())
        self.assertEqual(before, path.read_bytes())
        other = self.root / "user.json"
        other.write_text('{"owner": "person"}')
        with self.assertRaisesRegex(contracts.ContractError, "non-generated"):
            contracts.write_snapshot(other, contracts.create_snapshot())
        self.assertEqual(other.read_text(), '{"owner": "person"}')

    def test_reject_forged_source_digest_unreviewed_qos_and_changed_role(self):
        good = contracts.create_snapshot()
        cases = []
        corrupted = copy.deepcopy(good)
        corrupted["source"]["catalogSha256"] = "0" * 64
        cases.append(corrupted)
        corrupted = copy.deepcopy(good)
        corrupted["services"][0]["evidence"]["ddsQosVerified"] = True
        cases.append(corrupted)
        corrupted = copy.deepcopy(good)
        corrupted["services"][0]["topics"][0]["direction"] = "consumer"
        cases.append(corrupted)
        corrupted = copy.deepcopy(good)
        corrupted["services"][0]["topics"].append({
            "id": "UMAA::EO::Fake::Unknown", "role": "report",
            "type": "UMAA::EO::Fake::Unknown", "direction": "provider"})
        cases.append(corrupted)
        corrupted = copy.deepcopy(good)
        corrupted["services"][0]["operations"].append("inventedOperation")
        cases.append(corrupted)
        corrupted = copy.deepcopy(good)
        corrupted["extraApproved"] = True
        cases.append(corrupted)
        for case in cases:
            with self.subTest(change=case["services"][0].get("id")):
                with self.assertRaises(contracts.ContractError):
                    contracts.validate_snapshot(case)

    def test_reject_stale_catalog_and_changed_untrusted_source(self):
        snapshot = contracts.create_snapshot()
        altered = self.root / "altered.json"
        altered.write_bytes(contracts.CATALOG.read_bytes().replace(
            b"0.5.0-six-icd-literal-contract-map",
            b"0.5.1-six-icd-literal-contract-map"))
        with self.assertRaisesRegex(contracts.ContractError, "mismatch"):
            contracts.validate_snapshot(snapshot, altered)
        with self.assertRaises(contracts.ContractError):
            contracts.read_snapshot(self.root / "missing.json")

    def test_official_add_pins_reviewed_source_and_refuses_drift(self):
        proj = self.root / "project"
        cli.new("sensor", proj)
        cli.add(proj, "UMAA::SA::GlobalPoseStatus", "consumer")
        doc = cli.read(proj)
        self.assertEqual(doc["catalogPin"]["kind"], "reviewed-icd-catalog")
        self.assertEqual(doc["catalogPin"]["sha256"],
                         contracts.create_snapshot()["source"]["catalogSha256"])
        status = cli.check_project(proj)
        self.assertTrue(status["catalogPinned"])
        self.assertFalse(status["codegenReady"])
        self.assertFalse(status["normativeQosClaim"])
        # Same Service can be supplied and consumed by a logical Component.
        cli.add(proj, "UMAA::SA::GlobalPoseStatus", "provider")
        self.assertEqual(len(cli.read(proj)["services"]), 2)
        doc = cli.read(proj)
        doc["catalogPin"]["sha256"] = "f" * 64
        cli.save(proj, doc)
        with self.assertRaisesRegex(cli.ProjectError, "changed"):
            cli.check_project(proj)
        with self.assertRaisesRegex(cli.ProjectError, "changed"):
            cli.generate(proj)

    def test_manual_unknown_and_unpinned_official_assembly_are_rejected(self):
        proj = self.root / "project"
        cli.new("sensor", proj)
        doc = cli.read(proj)
        doc["services"].append({"id": "UMAA::SA::InventedService", "role": "provider"})
        cli.save(proj, doc)
        with self.assertRaises(cli.ProjectError):
            cli.check_project(proj)
        doc["services"] = [{"id": "UMAA::SA::GlobalPoseStatus", "role": "provider"}]
        cli.save(proj, doc)
        with self.assertRaisesRegex(cli.ProjectError, "unpinned"):
            cli.check_project(proj)

    def test_existing_synthetic_manifest_stays_buildable_and_unpinned(self):
        proj = self.root / "project"
        cli.new("engine", proj)
        cli.add(proj, "Demo::EO::EngineControl", "provider")
        status = cli.check_project(proj)
        self.assertTrue(status["codegenReady"])
        self.assertNotIn("catalogPin", cli.read(proj))
        cli.generate(proj)
        self.assertTrue((proj / "CMakeLists.txt").exists())

    def test_cli_contract_export_verify_and_validate(self):
        cmd = [sys.executable, "-m", "tools.umaa_component_cli"]
        contract_file = self.root / "contracts.json"
        def invoke(*args):
            return subprocess.run(cmd + list(args), cwd=contracts.ROOT,
                                  capture_output=True, text=True)
        ex = invoke("contract", "export", "--output", str(contract_file))
        self.assertEqual(ex.returncode, 0, ex.stderr)
        self.assertEqual(json.loads(ex.stdout)["services"], 53)
        self.assertEqual(invoke("contract", "verify", "--input", str(contract_file)).returncode, 0)
        exported = json.loads(contract_file.read_text())
        exported["services"][0]["evidence"]["ddsQosVerified"] = True
        contract_file.write_text(json.dumps(exported))
        self.assertEqual(invoke("contract", "verify", "--input", str(contract_file)).returncode, 2)

        proj = self.root / "project"
        cli.new("engine", proj)
        cli.add(proj, "Demo::EO::BatteryStatus", "provider")
        result = invoke("validate", "--project", str(proj))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["codegenReady"])


if __name__ == "__main__":
    unittest.main()
