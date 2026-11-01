"""Regression: all reviewed UMAA 6.0 ICD mappings are represented exactly in YAML."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.generate_icd_yaml import build_service, expected_files, yaml_text, write_files
from tools.umaa_service_contracts import _read_catalog
from tools.validate_icd_yaml import ICDYamlError, strict_load, validate, require_dependencies, validate_document_elements, validate_topic_references


class ICDYAMLCoverage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog, cls.digest = _read_catalog()
        cls.files = expected_files(cls.catalog)
        try:
            cls.yaml, cls.jsonschema = require_dependencies()
        except ICDYamlError as exc:
            # General SDK jobs do not install optional YAML parser dependencies.
            # The dedicated ICD workflow installs both and MUST execute all tests.
            raise unittest.SkipTest(str(exc)) from exc
        cls.root = ROOT / "contracts/umaa6"

    def test_all_six_icds_services_topics_and_operations(self):
        self.assertEqual(59, len(self.files))
        self.assertEqual(6, sum(p.startswith("icd/") for p in self.files))
        self.assertEqual(53, sum(p.startswith("services/") for p in self.files))
        self.assertEqual(113, sum(len(s["provider_topics"]) + len(s["consumer_topics"])
                                  for s in self.catalog["services"]))
        self.assertEqual(131, sum(len(s["operations"]) for s in self.catalog["services"]))
        self.assertEqual({"EO", "MM", "MO", "SA", "SEM", "SO"},
                         {p.split("/")[1] for p in self.files if p.startswith("services/")})

    def test_exact_generated_bytes_and_full_schema_validation(self):
        write_files(self.root, self.files, check=True)
        summary = validate(self.root)
        self.assertEqual(6, summary["icds"])
        self.assertEqual(53, summary["services"])
        self.assertEqual(113, summary["topics"])
        self.assertEqual(131, summary["operations"])
        self.assertEqual("REVIEWED_MAPPING_ONLY", summary["status"])

    def test_rejects_dangling_explicit_topic_references(self):
        baseline = self.yaml.safe_load(
            self.files["services/SA/GlobalPoseStatus.yaml"])
        validate_topic_references(baseline)
        bad = copy.deepcopy(baseline)
        bad["spec"]["operations"]["reportGlobalPose"]["topicRef"] = "missing"
        with self.assertRaises(ICDYamlError):
            validate_topic_references(bad)
        bad = copy.deepcopy(baseline)
        bad["spec"]["protocols"][0]["bindings"]["reports"] = ["missing"]
        with self.assertRaises(ICDYamlError):
            validate_topic_references(bad)

    def test_yaml_rejects_explicit_merge_mapping(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "merge.yaml"
            path.write_text("a:\\n  <<: {b: 1}\\n".replace("\\\\n", "\\n"),
                            encoding="utf-8")
            with self.assertRaises(ICDYamlError):
                strict_load(path, self.yaml)

    def test_official_document_element_schema_and_source_consistency(self):
        # Synthetic in-memory fixture only; no official text/hash is asserted.
        sample = self.yaml.safe_load(self.files["services/SA/GlobalPoseStatus.yaml"])
        original_sha = "a" * 64
        sample["sources"]["icd"]["originalSha256"] = original_sha
        verbatim = "Synthetic test-only requirement, not original UMAA text."
        sample["documentation"]["elements"] = [{
            "id": "test-element-1", "kind": "requirement",
            "source": {"kind": "official-icd",
                       "documentId": "UMAA-SPEC-SAICD", "version": "6.0",
                       "originalSha256": original_sha,
                       "clause": "6.1.6", "locator": "synthetic fixture paragraph 1"},
            "verbatim": verbatim,
            "verbatimSha256": hashlib.sha256(verbatim.encode("utf-8")).hexdigest(),
            "requirement": {"modality": "SHALL", "normative": True},
        }]
        schema = self.jsonschema.Draft202012Validator(
            json.loads((ROOT / "schemas/umaa-icd-v1alpha1.schema.json").read_text()))
        schema.validate(sample)
        validate_document_elements(sample)

        wrong = copy.deepcopy(sample)
        wrong["documentation"]["elements"][0]["source"]["originalSha256"] = "b" * 64
        with self.assertRaises(ICDYamlError):
            validate_document_elements(wrong)
        wrong = copy.deepcopy(sample)
        wrong["documentation"]["elements"][0]["verbatim"] += " modified"
        with self.assertRaises(ICDYamlError):
            validate_document_elements(wrong)
        wrong = copy.deepcopy(sample)
        wrong["sources"]["icd"]["originalSha256"] = None
        with self.assertRaises(ICDYamlError):
            validate_document_elements(wrong)
        wrong = copy.deepcopy(sample)
        wrong["documentation"]["elements"].append(copy.deepcopy(
            wrong["documentation"]["elements"][0]))
        with self.assertRaises(ICDYamlError):
            validate_document_elements(wrong)
        wrong = copy.deepcopy(sample)
        wrong["documentation"]["elements"][0].pop("verbatimSha256")
        self.assertFalse(schema.is_valid(wrong))
        wrong = copy.deepcopy(sample)
        wrong["documentation"]["elements"][0]["requirement"]["unknown"] = True
        self.assertFalse(schema.is_valid(wrong))

    def test_icd_document_sections_accept_source_anchored_elements(self):
        record = self.yaml.safe_load(self.files["icd/SA.yaml"])
        original_sha = "c" * 64
        record["sources"]["document"]["originalSha256"] = original_sha
        verbatim = "Synthetic table fixture, not an official ICD transcription."
        record["spec"]["sections"][0]["elements"] = [{
            "id": "test-table-1", "kind": "table",
            "source": {"kind": "official-icd", "documentId": "UMAA-SPEC-SAICD",
                       "version": "6.0", "originalSha256": original_sha,
                       "clause": "6.1.1", "locator": "test page / table"},
            "verbatim": verbatim,
            "verbatimSha256": hashlib.sha256(verbatim.encode("utf-8")).hexdigest(),
            "table": {"title": "Synthetic table", "columns": ["A"], "rows": [["B"]]},
        }]
        schema = self.jsonschema.Draft202012Validator(
            json.loads((ROOT / "schemas/umaa-icd-v1alpha1.schema.json").read_text()))
        schema.validate(record)
        validate_document_elements(record)
        wrong = copy.deepcopy(record)
        wrong["spec"]["sections"][1]["elements"] = copy.deepcopy(
            wrong["spec"]["sections"][0]["elements"])
        with self.assertRaises(ICDYamlError):
            validate_document_elements(wrong)

    def test_legacy_parallel_projection_does_not_reappear(self):
        # All catalog-derived YAML belongs to the deterministic canonical root.
        for old_path in ("contracts/services", "contracts/icds"):
            legacy = ROOT / old_path
            self.assertFalse(legacy.exists() and list(legacy.rglob("*.yaml")),
                             "duplicate YAML projection: " + old_path)

    def test_multiple_command_groups_are_lossless(self):
        groups = [s for s in self.catalog["services"]
                  if len(s.get("command_groups", [])) > 1]
        self.assertEqual(5, len(groups))
        for s in groups:
            doc = self.yaml.safe_load(self.files["services/" +
                                s["name"].split("::")[1] + "/" + s["name"].split("::")[2] + ".yaml"])
            generated = doc["spec"]["protocols"]
            self.assertEqual([g["name"] for g in s["command_groups"]],
                             [g["id"] for g in generated])
            by_alias = {alias: t["idlType"] for alias, t in doc["spec"]["topics"].items()}
            for raw, made in zip(s["command_groups"], generated):
                self.assertEqual(raw["command"], by_alias[made["bindings"]["command"]])
                self.assertEqual(raw["ack"], by_alias[made["bindings"]["ack"]])
                self.assertEqual(raw["status"], by_alias[made["bindings"]["status"]])

    def test_all_queryless_services_are_subscriptions_not_rpc(self):
        report = [s for s in self.catalog["services"]
                  if s["pattern"] == "request_reply_without_query_data"]
        self.assertEqual(35, len(report))
        for s in report:
            record = build_service(s, self.catalog)
            self.assertEqual("request-reply", record["spec"]["protocols"][0]["kind"])
            self.assertEqual("without-query-data", record["spec"]["protocols"][0]["mode"])
            self.assertEqual(set(s["operations"]), set(record["spec"]["operations"]))
            self.assertEqual([], record["spec"]["collections"])

    def test_unresolved_collection_types_preserved_without_invented_structure(self):
        s = next(x for x in self.catalog["services"]
                 if x["name"] == "UMAA::EO::BatteryStatus")
        record = build_service(s, self.catalog)
        self.assertEqual(s["collection_topics"], record["spec"]["unresolvedCollectionTypes"])
        self.assertEqual([], record["spec"]["collections"])
        self.assertEqual([], record["spec"]["specializations"])

    def test_yaml_rejects_duplicate_keys_and_aliases(self):
        with tempfile.TemporaryDirectory() as d:
            file = Path(d) / "invalid.yaml"
            file.write_text('apiVersion: "umaa.dev/v1alpha1"\napiVersion: "evil"\n')
            with self.assertRaises(ICDYamlError):
                strict_load(file, self.yaml)
            file.write_text('value: &anchor true\nother: *anchor\n')
            with self.assertRaises(ICDYamlError):
                strict_load(file, self.yaml)

    def test_editing_catalog_projection_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            target = Path(d) / "converted"
            # Use actual generated bytes (not the generator) as baseline.
            shutil.copytree(self.root, target)
            path = target / "services/SA/GlobalPoseStatus.yaml"
            source = path.read_text()
            for mutated in (
                source.replace("ddsTopicName: null", 'ddsTopicName: "invented"', 1),
                source.replace("ddsQosVerified: false", "ddsQosVerified: true", 1),
                source.replace("without-query-data", "with-query-data", 1),
                source.replace("GlobalPoseReportType", "WrongReportType", 1),
            ):
                path.write_text(mutated)
                with self.assertRaises(ICDYamlError):
                    validate(target)
            path.write_text(source)
            self.assertEqual(53, validate(target)["services"])

    def test_no_forged_source_hash_or_approval_flag(self):
        for s in self.catalog["services"]:
            doc = build_service(s, self.catalog)
            self.assertIsNone(doc["sources"]["icd"]["originalSha256"])
            self.assertIsNone(doc["sources"]["idl"]["bundleSha256"])
            self.assertFalse(doc["verification"]["ddsBehaviorVerified"])
            self.assertFalse(doc["verification"]["ddsQosVerified"])
            for t in doc["spec"]["topics"].values():
                self.assertIsNone(t["ddsTopicName"])
                self.assertIsNone(t["keyFields"])
                self.assertIsNone(t["qosProfileRef"])

    def test_generated_yaml_deterministic(self):
        s = self.catalog["services"][0]
        doc = build_service(s, self.catalog)
        a = yaml_text(doc)
        b = yaml_text(copy.deepcopy(doc))
        self.assertEqual(a, b)
        self.assertEqual(doc, self.yaml.safe_load(a))


if __name__ == "__main__":
    unittest.main()
