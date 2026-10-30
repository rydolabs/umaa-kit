import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from generate_reviewed_runtime_registry import entries, generate


class ReviewedRegistryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((ROOT / 'catalog/umaa6-six-icd-reviewed.json').read_text())

    def test_exact_generated_output(self):
        expected = generate(self.catalog)
        self.assertEqual(expected, (ROOT / 'sdk/include/umaa/reviewed_service_registry.hpp').read_text())
        topics, services = entries(self.catalog)
        self.assertEqual((len(topics), services), (113, 53))

    def test_explicit_command_families_exhaustive(self):
        from generate_reviewed_runtime_registry import command_families
        families = command_families(self.catalog)
        self.assertEqual(len(families), 24)
        self.assertTrue(all(not x[5] for x in families))
        self.assertEqual(len({(x[0],x[2]) for x in families}), len(families))

    def test_command_group_must_be_complete_and_correct(self):
        from generate_reviewed_runtime_registry import command_families
        cat = copy.deepcopy(self.catalog)
        svc = next(s for s in cat["services"]
                   if s["name"] == "UMAA::MM::ConditionalControl")
        svc["command_groups"][0]["ack"] = svc["command_groups"][1]["ack"]
        with self.assertRaises(ValueError):
            command_families(cat)
        cat = copy.deepcopy(self.catalog)
        svc = next(s for s in cat["services"]
                   if s["name"] == "UMAA::MM::ConditionalControl")
        svc["command_groups"].pop()
        with self.assertRaises(ValueError):
            command_families(cat)

    def test_reject_unknown_or_wrong_role(self):
        cat = copy.deepcopy(self.catalog)
        cat['services'][0]['consumer_topics'][0]['role'] = 'unknown'
        with self.assertRaises(ValueError):
            entries(cat)

    def test_reject_duplicate_topic(self):
        cat = copy.deepcopy(self.catalog)
        cat['services'][0]['consumer_topics'].append(cat['services'][0]['consumer_topics'][0].copy())
        with self.assertRaises(ValueError):
            entries(cat)

    def test_reject_fabricated_qos(self):
        cat = copy.deepcopy(self.catalog)
        del cat['services'][0]['dds_qos_verified']
        with self.assertRaises(ValueError):
            entries(cat)

    def test_unproven_qos_flag_cannot_unlock_production_mode(self):
        cat = copy.deepcopy(self.catalog)
        cat['services'][0]['dds_qos_verified'] = True
        with self.assertRaisesRegex(ValueError, 'normative QoS'):
            entries(cat)

    def test_reject_unreviewed(self):
        cat = copy.deepcopy(self.catalog)
        cat['services'][0]['icd_reviewed'] = False
        with self.assertRaises(ValueError):
            entries(cat)


if __name__ == '__main__':
    unittest.main()
