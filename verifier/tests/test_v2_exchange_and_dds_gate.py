"""V2: explicit DDS execution authorization and verifiable Studio exchange."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from verifier.contracts import ROOT, sha256
from verifier.evidence import verify_run, verify_suite
from verifier.exchange import export_results
from verifier.runner import run
from verifier.suite import run_suite

SCENARIO = ROOT / 'simulator/scenarios/normal.json'


def fake_sut(path):
    events = json.loads(SCENARIO.read_text())['expected_events']
    path.write_text('#!' + sys.executable + '\nimport sys\nsys.stdin.read()\nprint(' + repr('\n'.join(events)) + ')\n')
    path.chmod(0o755)
    return path


class DdsNetworkGateTests(unittest.TestCase):
    def test_dds_without_approval_is_blocked_before_process_creation(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            oracle = fake_sut(root / 'oracle')
            with patch('verifier.runner.subprocess.Popen') as popen:
                result = run('dds-anchor', sut, root / 'evidence', oracle=oracle)
            popen.assert_not_called()
            self.assertEqual(result['result'], 'BLOCKED')
            self.assertIn('approval required', result['message'])
            self.assertFalse(result['ddsNetworkApprovalDeclared'])
            self.assertEqual(verify_run(root / 'evidence'), [])

    def test_dds_suite_without_approval_never_runs_external_binaries(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            oracle = fake_sut(root / 'oracle')
            suite = root / 'dds-suite.json'
            suite.write_text(json.dumps({'schemaVersion': '1.0', 'suiteId': 'dds-unapproved',
                                         'cases': [{'id': 'no-approval', 'profileId': 'dds-anchor'}]}))
            with patch('verifier.runner.subprocess.Popen') as popen:
                result = run_suite(suite, root / 'evidence', sut, oracle=oracle)
            popen.assert_not_called()
            self.assertEqual(result['counts']['BLOCKED'], 1)
            self.assertEqual(verify_suite(root / 'evidence'), [])

    def test_forged_dds_success_without_approval_is_not_verified(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            oracle = fake_sut(root / 'oracle')
            run('dds-anchor', sut, root / 'evidence', oracle=oracle)
            path = root / 'evidence/verdict.json'
            data = json.loads(path.read_text())
            data['result'], data['exitCode'] = 'PASS', 0
            path.write_text(json.dumps(data))
            (root / 'evidence/evidence-index.json').write_text(json.dumps({
                'schemaVersion': '1.0', 'verdictSha256': sha256(path)}))
            self.assertIn('DDS success without declared test network approval',
                          verify_run(root / 'evidence'))


class ExchangeTests(unittest.TestCase):
    def test_single_run_export_has_no_normative_claims(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            run('stdio-fixture', sut, root / 'evidence', scenario=SCENARIO)
            target = export_results(root / 'evidence', root / 'out/result.json')
            data = json.loads(target.read_text())
            self.assertEqual(data['sourceType'], 'run')
            self.assertEqual(data['result'], 'PASS')
            self.assertEqual(len(data['cases']), 1)
            self.assertEqual(data['normativeRequirementRefs'], [])
            self.assertFalse(data['conformanceClaim'])
            self.assertEqual(data['sourceEvidenceDigestSha256'],
                             sha256(root / 'evidence/verdict.json'))
            self.assertFalse(data['cases'][0]['conformanceClaim'])
            self.assertIn('sut', data['cases'][0]['inputDigests'])
            self.assertIn('stdout', data['cases'][0]['artifactDigests'])
            try:
                from jsonschema import validate
            except ImportError:
                pass  # JSON schema validation is optional; no runtime dependency
            else:
                schema = json.loads((ROOT / 'verifier/schemas/studio-result-v1.schema.json').read_text())
                validate(instance=data, schema=schema)
            with self.assertRaises(FileExistsError):
                export_results(root / 'evidence', target)

    def test_suite_export_preserves_aggregate_result(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            suite = root / 'suite.json'
            suite.write_text(json.dumps({'schemaVersion': '1.0', 'suiteId': 'single-case',
                                         'cases': [{'id': 'normal', 'profileId': 'stdio-fixture',
                                                    'scenario': 'simulator/scenarios/normal.json'}]}))
            run_suite(suite, root / 'evidence', sut)
            target = export_results(root / 'evidence', root / 'result.json')
            data = json.loads(target.read_text())
            self.assertEqual(data['sourceType'], 'suite')
            self.assertEqual(data['suiteId'], 'single-case')
            self.assertEqual(data['result'], 'PASS')
            self.assertEqual(data['cases'][0]['caseId'], 'normal')
            self.assertEqual(data['cases'][0]['contractId'], 'dev/stdio-line-trace')

    def test_corrupt_evidence_not_exported(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            run('stdio-fixture', sut, root / 'evidence', scenario=SCENARIO)
            (root / 'evidence/runner.stdout.log').write_text('FORGED')
            with self.assertRaisesRegex(ValueError, 'integrity'):
                export_results(root / 'evidence', root / 'out.json')
            self.assertFalse((root / 'out.json').exists())


if __name__ == '__main__':
    unittest.main()
