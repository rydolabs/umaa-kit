import json
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from verifier.cli import main

from verifier.contracts import ROOT
from verifier.profiles import PROFILES, command_for
from verifier.scope import assess_scope


class ScopeCoverageTest(unittest.TestCase):
    def test_explicitly_blocked_normative_status(self):
        value = assess_scope()
        self.assertEqual(value['gateStatus'], 'BLOCKED')
        self.assertEqual(value['profileCoverage']['approvedNormativeTestContracts'], 0)
        self.assertEqual(value['profileCoverage']['developmentOnlyProfiles'], 6)
        self.assertFalse(value['officialConformanceClaim'])
        self.assertTrue(value['blockers']['requirementsUnmapped'])

    def test_release_gate_cannot_pass_on_unreviewed_scope(self):
        output = StringIO()
        with redirect_stdout(output):
            rc = main(['scope-report', '--require-ready'])
        self.assertEqual(rc, 2)
        self.assertEqual(json.loads(output.getvalue())['gateStatus'], 'BLOCKED')

    def test_unknown_scope_schema_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'catalog').mkdir()
            (root / 'catalog/umaa6-conformance-scope.json').write_text(json.dumps({
                'schema_version': 'unrecognized'}))
            with self.assertRaisesRegex(ValueError, 'schema'):
                assess_scope(root)


class FixtureScopeTest(unittest.TestCase):
    def test_single_run_cannot_load_external_unreviewed_fixture(self):
        with tempfile.TemporaryDirectory() as td:
            external = Path(td) / 'fake.json'
            external.write_text('{}')
            with self.assertRaisesRegex(ValueError, 'repository simulator/scenarios'):
                command_for(PROFILES['stdio-fixture'], ROOT, external, None,
                            external, None, None, external, 'python')


if __name__ == '__main__':
    unittest.main()
