"""Negative/positive tests for contract policy, artifact integrity and suites."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

from verifier.contracts import REGISTRY, ROOT, load_contracts, load_suite
from verifier.evidence import verify_run, verify_suite
from verifier.runner import run
from verifier.suite import run_suite

SUITE = ROOT / 'verifier/suites/stdio-regression.json'
SCENARIO = ROOT / 'simulator/scenarios/normal.json'


def fake_sut(path: Path, mode='valid'):
    events = json.loads(SCENARIO.read_text())['expected_events']
    if mode == 'bad':
        events = ['STATUS s1 EXECUTING SUCCEEDED']
    body = ('import time\ntime.sleep(20)\n' if mode == 'slow' else
            'import sys\nsys.stdin.read()\nprint(' + repr('\n'.join(events)) + ')\n')
    path.write_text('#!' + sys.executable + '\n' + body)
    path.chmod(0o755)
    return path


def write_json(path: Path, data: dict):
    path.write_text(json.dumps(data))
    return path


class ContractsTest(unittest.TestCase):
    def test_legacy_contracts_are_explicit_and_not_normative(self):
        doc = json.loads(REGISTRY.read_text())
        entries = load_contracts()
        self.assertEqual(doc['normativeRequirementRefs'], [])
        self.assertEqual(len(entries), 6)
        self.assertTrue(all(not x['conformanceEligible'] for x in entries.values()))

    def test_invalid_registry_claim_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            obj = json.loads(REGISTRY.read_text())
            obj['status'] = 'approved'
            with self.assertRaisesRegex(ValueError, 'unsafe legacy'):
                load_contracts(write_json(Path(td) / 'reg.json', obj))

    def test_suite_has_four_known_cases(self):
        suite = load_suite(SUITE)
        self.assertEqual(len(suite['cases']), 4)
        self.assertEqual([x['id'] for x in suite['cases']],
                         ['normal', 'update_cancel', 'invalid_payload', 'replay_recovery'])

    def test_duplicate_case_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            data = json.loads(SUITE.read_text())
            data['cases'].append(data['cases'][0])
            with self.assertRaisesRegex(ValueError, 'duplicate'):
                load_suite(write_json(Path(td) / 'bad.json', data))

    def test_scenario_traversal_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            data = json.loads(SUITE.read_text())
            data['cases'][0]['scenario'] = 'simulator/scenarios/../../catalog/umaa6-six-icd-reviewed.json'
            with self.assertRaisesRegex(ValueError, 'Scenario'):
                load_suite(write_json(Path(td) / 'bad.json', data))

    def test_unapproved_profile_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            data = json.loads(SUITE.read_text())
            data['cases'][0]['profileId'] = 'my-injected-script'
            with self.assertRaisesRegex(ValueError, 'Unregistered'):
                load_suite(write_json(Path(td) / 'bad.json', data))

    def test_non_allowlisted_option_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            data = json.loads(SUITE.read_text())
            data['cases'][0]['option'] = '--bad-destination'
            with self.assertRaisesRegex(ValueError, 'Unsupported option'):
                load_suite(write_json(Path(td) / 'bad.json', data))


class EvidenceTest(unittest.TestCase):
    def test_passed_run_records_input_provenance(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            report = run('stdio-fixture', sut, root / 'run', scenario=SCENARIO, timeout=25)
            self.assertEqual(report['result'], 'PASS')
            self.assertFalse(report['conformanceClaim'])
            self.assertEqual(report['contract']['normativeRequirementRefs'], [])
            self.assertIsNotNone(report['inputs']['sut']['sha256'])
            self.assertIsNotNone(report['inputs']['runner']['sha256'])
            self.assertIsNotNone(report['inputs']['scenario']['sha256'])
            self.assertFalse(verify_run(root / 'run'))

    def test_detect_log_corruption(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            report = run('stdio-fixture', fake_sut(root / 'sut'), root / 'run',
                         scenario=SCENARIO, timeout=25)
            self.assertEqual(report['result'], 'PASS')
            (root / 'run' / 'runner.stdout.log').write_text('ALTERED')
            self.assertIn('artifact mismatch', str(verify_run(root / 'run')))

    def test_detect_verdict_corruption(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run('stdio-fixture', fake_sut(root / 'sut'), root / 'run', scenario=SCENARIO, timeout=25)
            v = root / 'run' / 'verdict.json'
            obj = json.loads(v.read_text())
            obj['conformanceClaim'] = True
            write_json(v, obj)
            errors = verify_run(root / 'run')
            self.assertIn('verdict digest mismatch', errors)
            self.assertIn('unexpected conformance claim', errors)

    def test_blocked_run_has_complete_index_and_logs(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            verdict = run('dds-engine', root / 'not-here', root / 'run', timeout=2)
            self.assertEqual(verdict['result'], 'BLOCKED')
            self.assertFalse(verify_run(root / 'run'))

    def test_invalid_sut_fails_and_is_not_conformance(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            verdict = run('stdio-fixture', fake_sut(root / 'sut', 'bad'), root / 'run',
                          scenario=SCENARIO, timeout=25)
            self.assertEqual(verdict['result'], 'FAIL')
            self.assertFalse(verify_run(root / 'run'))
            self.assertFalse(verdict['conformanceClaim'])

    def test_timeout_kills_child_group_and_returns_error(self):
        if sys.platform == 'win32':
            self.skipTest('Portable kill behavior is different on Windows')
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            verdict = run('stdio-fixture', fake_sut(root / 'sut', 'slow'), root / 'run',
                          scenario=SCENARIO, timeout=1)
            self.assertEqual(verdict['result'], 'ERROR')
            self.assertIn('exceeded', verdict['message'])
            self.assertFalse(verify_run(root / 'run'))

    def test_suite_aggregates_failure_and_detects_tampering(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut', 'valid')
            result = run_suite(SUITE, root / 'suite', sut)
            # The fake SUT produces the normal trace, and fails the other three.
            self.assertEqual(result['result'], 'FAIL')
            self.assertEqual(result['counts']['PASS'], 1)
            self.assertEqual(result['counts']['FAIL'], 3)
            self.assertFalse(verify_suite(root / 'suite'))
            (root / 'suite' / 'normal' / 'runner.stdout.log').write_text('TAMPER')
            self.assertIn('normal:', str(verify_suite(root / 'suite')))

    def test_suite_wont_overwrite_existing_evidence(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            target = root / 'exists'
            target.mkdir()
            with self.assertRaises(FileExistsError):
                run_suite(SUITE, target, fake_sut(root / 'sut'))


if __name__ == '__main__':
    unittest.main()

# Keep report tests independent of the actual SDK build environment.
class ReadOnlyReportTest(unittest.TestCase):
    def test_report_renders_escaped_summary_without_conformance_claim(self):
        from verifier.report import render
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            run('stdio-fixture', sut, root / 'evidence', scenario=SCENARIO, timeout=25)
            out = render(root / 'evidence', root / 'viewer.html')
            html = out.read_text()
            self.assertIn('NOT UMAA conformance certification', html)
            self.assertIn('development-unreviewed', html)
            self.assertIn('PASS', html)
            with self.assertRaises(FileExistsError):
                render(root / 'evidence', out)

    def test_report_refuses_corrupted_run(self):
        from verifier.report import render
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            sut = fake_sut(root / 'sut')
            run('stdio-fixture', sut, root / 'evidence', scenario=SCENARIO, timeout=25)
            (root / 'evidence' / 'runner.stdout.log').write_text('spoofed')
            with self.assertRaisesRegex(ValueError, 'integrity'):
                render(root / 'evidence', root / 'viewer.html')

class AggregateIntegrityTest(unittest.TestCase):
    def test_suite_counters_cannot_be_inconsistent(self):
        from verifier.contracts import sha256
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run_suite(SUITE, root / 'suite', fake_sut(root / 'sut'))
            verdict = root / 'suite/suite-verdict.json'
            value = json.loads(verdict.read_text())
            value['counts']['PASS'] = 4
            write_json(verdict, value)
            # Even after recalculating the digest index, internal inconsistency is detected.
            write_json(root / 'suite/suite-index.json',
                       {'schemaVersion': '1.0', 'summarySha256': sha256(verdict)})
            self.assertIn('incorrect suite result counts', verify_suite(root / 'suite'))

    def test_synthetic_normative_claim_is_rejected(self):
        from verifier.contracts import sha256
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run('stdio-fixture', fake_sut(root / 'sut'), root / 'run', scenario=SCENARIO, timeout=25)
            path = root / 'run/verdict.json'
            d = json.loads(path.read_text())
            d['contract']['normativeRequirementRefs'] = ['unapproved shall']
            write_json(path, d)
            write_json(root / 'run/evidence-index.json',
                       {'schemaVersion': '1.0', 'verdictSha256': sha256(path)})
            self.assertIn('unsafe/unreviewed contract claim', verify_run(root / 'run'))

class MalformedEvidenceTest(unittest.TestCase):
    def test_malformed_json_is_reported_not_uncaught(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            d = root / 'run'
            d.mkdir()
            (d / 'verdict.json').write_text('[]')
            (d / 'evidence-index.json').write_text('{}')
            self.assertTrue(verify_run(d))

    def test_malformed_suite_is_reported_not_uncaught(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            d = root / 'suite'
            d.mkdir()
            (d / 'suite-verdict.json').write_text('[]')
            (d / 'suite-index.json').write_text('{}')
            self.assertTrue(verify_suite(d))
