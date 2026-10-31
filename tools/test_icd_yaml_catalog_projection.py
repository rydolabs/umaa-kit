"""Validate complete *reviewed-catalog projection* into YAML, NOT normative ICD content.

Requires PyYAML and jsonschema. Reject duplicate YAML keys, extra files,
loss of original reviewed records, Topic/Operation drift and unsupported
official-runtime claims. Never infer Key/QoS/behavior from a type name.
"""
from __future__ import annotations
import json
from pathlib import Path
import unittest

import yaml
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/umaa6-six-icd-reviewed.json"
SCHEMA = ROOT / "schemas/umaa-icd-v1alpha1.schema.json"
SERVICES = ROOT / "contracts/services"
ICDS = ROOT / "contracts/icds"
DOMAINS = {"EO", "MM", "MO", "SA", "SEM", "SO"}


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def unique_mapping(loader, node):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=True)
        if key in result:
            raise ValueError("duplicate YAML key: " + str(key))
        result[key] = loader.construct_object(value_node, deep=True)
    return result


UniqueKeyLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,
                                unique_mapping)


def load_yaml(path):
    return yaml.load(path.read_text(encoding="utf-8"), Loader=UniqueKeyLoader)


class AllReviewedIcdYamlTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
        cls.validator = Draft202012Validator(json.loads(SCHEMA.read_text(encoding="utf-8")))
        cls.validator.check_schema(cls.validator.schema)

    def test_all_services_and_six_document_indexes(self):
        records = {r["name"]: r for r in self.catalog["services"]}
        self.assertEqual(53, len(records))
        self.assertEqual(113, sum(len(r["provider_topics"]) +
                                  len(r["consumer_topics"]) for r in records.values()))
        expected = {SERVICES / r["name"].split("::")[1] /
                    (r["name"].split("::")[2] + ".yaml") for r in records.values()}
        self.assertEqual(expected, set(SERVICES.rglob("*.yaml")))
        self.assertEqual({ICDS / (d + ".yaml") for d in DOMAINS},
                         set(ICDS.glob("*.yaml")))
        for path in sorted(expected):
            with self.subTest(path=path.relative_to(ROOT)):
                doc = load_yaml(path)
                self.validator.validate(doc)
                original = doc["extensions"]["x-catalog"]["originalRecord"]
                self.assertEqual(records[doc["metadata"]["id"]], original)
                self.assertFalse(doc["extensions"]["x-catalog"]["officialIdlVerified"])
                self.assertFalse(doc["extensions"]["x-catalog"]["ddsQosVerified"])
                self.assertFalse(doc["extensions"]["x-catalog"]["ddsBehaviorVerified"])
                self.assertEqual(original["clauses"], doc["sources"]["icd"]["clauses"])
                self.assertIsNone(doc["sources"]["icd"]["originalSha256"])
                self.assertIsNone(doc["sources"]["idl"]["bundleSha256"])
                self.assertFalse(doc["spec"]["requirements"])
                self.assertFalse(doc["spec"]["collections"])
                self.assertFalse(doc["spec"]["specializations"])
                self.assertEqual(set(original["operations"]), set(doc["spec"]["operations"]))
                self.assertTrue(all(op["kind"] == "unresolved" and
                                    op["topicRef"] is None
                                    for op in doc["spec"]["operations"].values()))
                types = {t["catalogId"]: t["idlType"] for t in doc["spec"]["topics"].values()}
                self.assertEqual({t["name"]: t["type"] for t in
                                  original["provider_topics"] + original["consumer_topics"]},
                                 types)
                self.assertTrue(all(t["ddsTopicName"] is None and
                                    t["keyFields"] is None and
                                    t["qosProfileRef"] is None
                                    for t in doc["spec"]["topics"].values()))
                for proto in doc["spec"]["protocols"]:
                    self.assertEqual("without-query-data", proto.get("mode")) if (
                        original["pattern"] == "request_reply_without_query_data"
                    ) else self.assertEqual("command-response", proto["kind"])
                    topic_refs = set(doc["spec"]["topics"])
                    for refs in proto["bindings"].values():
                        self.assertTrue(set(refs if isinstance(refs, list) else [refs]) <= topic_refs)
        for domain in sorted(DOMAINS):
            doc = load_yaml(ICDS / (domain + ".yaml"))
            self.validator.validate(doc)
            self.assertEqual({r["name"] for r in records.values()
                              if r["name"].split("::")[1] == domain},
                             set(doc["spec"]["services"]))
            self.assertFalse(doc["extensions"]["x-catalog"]["originalDocumentTextIncluded"])
            self.assertIsNone(doc["sources"]["document"]["originalSha256"])

    def test_schema_rejects_unrecognized_and_invented_claims(self):
        sample = load_yaml(SERVICES / "SA/GlobalPoseStatus.yaml")
        import copy
        bad = copy.deepcopy(sample)
        bad["spec"]["topics"]["topic01"]["unexpected"] = True
        self.assertFalse(self.validator.is_valid(bad))
        bad = copy.deepcopy(sample)
        bad["spec"]["operations"]["reportGlobalPose"]["kind"] = "invented"
        self.assertFalse(self.validator.is_valid(bad))
        bad = copy.deepcopy(sample)
        bad["extensions"]["vendor"] = {"guess": True}
        self.assertFalse(self.validator.is_valid(bad))

    def test_yaml_duplicate_key_rejected(self):
        with self.assertRaises(ValueError):
            yaml.load("kind: Service\nkind: ICD\n", Loader=UniqueKeyLoader)


if __name__ == "__main__":
    unittest.main()
