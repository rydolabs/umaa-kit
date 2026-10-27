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

    def test_reject_unreviewed(self):
        cat = copy.deepcopy(self.catalog)
        cat['services'][0]['icd_reviewed'] = False
        with self.assertRaises(ValueError):
            entries(cat)


if __name__ == '__main__':
    unittest.main()
