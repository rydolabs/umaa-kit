"""ServiceICD v1alpha1: test schema, normative safety and Docker-style extension."""
from __future__ import annotations

import copy
from pathlib import Path
import tempfile
import unittest

import yaml

from contracts.validate import ContractError, CONTRACTS, validate_file

EXAMPLE = CONTRACTS / "services/SA/GlobalPoseStatus.service.yaml"


class ServiceIcdYamlTest(unittest.TestCase):
    def check_text(self, source: str):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "service.yaml"
            path.write_text(source, encoding="utf-8")
            return validate_file(path)

    def check_object(self, source: dict):
        return self.check_text(yaml.safe_dump(source, sort_keys=False))

    def test_actual_icd_content_retained(self):
        doc = validate_file(EXAMPLE)
        self.assertEqual(doc["metadata"]["namespace"], "UMAA::SA::GlobalPoseStatus")
        self.assertEqual(set(doc["spec"]["operations"]), {"queryGlobalPose", "reportGlobalPose"})
        self.assertEqual(doc["spec"]["documentation"]["operationsTable"], 18)
        self.assertEqual(doc["spec"]["documentation"]["messageTables"][0]["number"], 19)
        self.assertEqual(len(doc["spec"]["topics"]["pose"]["attributes"]), 11)
        self.assertIn("UMAA::UMAAStatus", doc["spec"]["topics"]["pose"]["inherits"])
        self.assertEqual(doc["spec"]["topics"]["pose"]["idlPath"],
                         "UMAA/SA/GlobalPoseStatus/GlobalPoseReportType.idl")
        self.assertFalse(doc["verification"]["nativeDdsWire"])
        self.assertIsNone(doc["spec"]["topics"]["pose"]["ddsTopicName"])
        self.assertIsNone(doc["provenance"]["icd"]["officialSha256"])

    def test_no_fabricated_query_topic_and_correct_flow_reference(self):
        doc = validate_file(EXAMPLE)
        f = doc["spec"]["flows"]["report"]
        self.assertFalse(f["queryUsesDdsTopic"])
        self.assertEqual(doc["spec"]["operations"]["queryGlobalPose"]["transport"], "subscribe")
        self.assertEqual(doc["spec"]["operations"]["reportGlobalPose"]["transport"], "publish")
        for change in ("queryUsesDdsTopic", "topicRef", "queryOperation"):
            wrong = copy.deepcopy(doc)
            flow = wrong["spec"]["flows"]["report"]
            if change == "queryUsesDdsTopic":
                flow["queryUsesDdsTopic"] = True
            else:
                flow[change] = "NoSuchThing"
            with self.subTest(change=change), self.assertRaises(ContractError):
                self.check_object(wrong)

    def test_unknown_fields_are_not_silently_ignored_but_x_are_allowed(self):
        doc = validate_file(EXAMPLE)
        valid = copy.deepcopy(doc)
        valid["x-example.vendor"] = {"theme": "dark", "future": {"foo": 1}}
        valid["spec"]["topics"]["pose"]["x-org.custom"] = {"uiGroup": "geo"}
        self.check_object(valid)
        for obj in (doc, doc["spec"], doc["spec"]["topics"]["pose"],
                    doc["spec"]["operations"]["reportGlobalPose"]):
            wrong = copy.deepcopy(doc)
            if obj is doc: dst = wrong
            elif obj is doc["spec"]: dst = wrong["spec"]
            elif obj is doc["spec"]["topics"]["pose"]: dst = wrong["spec"]["topics"]["pose"]
            else: dst = wrong["spec"]["operations"]["reportGlobalPose"]
            dst["wireReady"] = True
            with self.assertRaises(ContractError):
                self.check_object(wrong)

    def test_wrong_role_unknown_topic_and_false_qos(self):
        doc = validate_file(EXAMPLE)
        wrong = copy.deepcopy(doc)
        wrong["spec"]["operations"]["queryGlobalPose"]["actor"] = "provider"
        with self.assertRaises(ContractError): self.check_object(wrong)
        wrong = copy.deepcopy(doc)
        wrong["spec"]["operations"]["reportGlobalPose"]["topicRef"] = "forged"
        with self.assertRaises(ContractError): self.check_object(wrong)
        wrong = copy.deepcopy(doc)
        wrong["spec"]["topics"]["pose"]["keyFields"] = ["source"]
        wrong["spec"]["topics"]["pose"]["qosProfileRef"] = "unverified-qos"
        # Form-only validation still cannot certify these fields: source
        # evidence must be checked independently before native codegen.
        accepted = self.check_object(wrong)
        self.assertFalse(accepted["verification"]["keysAndQos"])

    def test_yaml_duplicate_unsafe_merge_and_schema_constraints(self):
        original = EXAMPLE.read_text(encoding="utf-8")
        with self.assertRaises(ContractError):
            self.check_text(original + "\nkind: ServiceICD\n")
        with self.assertRaises(ContractError):
            self.check_text("a: &p {x: 1}\nb: *p\n")
        with self.assertRaises(ContractError):
            self.check_text("apiVersion: kumaa.dev/v1alpha1\nkind: Evil\n")
        with self.assertRaises(ContractError):
            self.check_text(original.replace("stage: mapping-reviewed", "stage: wire-ready"))
        with self.assertRaises(ContractError):
            self.check_text(original.replace("officialSha256: null", "officialSha256: deadbeef"))

    def test_imports_are_pinned_local_and_path_bounded(self):
        original = validate_file(EXAMPLE)
        bad = copy.deepcopy(original)
        bad["imports"] = [{"path": "../secret.yaml", "sha256": "0" * 64,
                           "kind": "ProtocolProfile"}]
        with self.assertRaises(ContractError):
            self.check_object(bad)
        bad["imports"][0]["path"] = "profiles/report.yaml"
        with self.assertRaises(ContractError):
            self.check_object(bad)


if __name__ == "__main__":
    unittest.main()
