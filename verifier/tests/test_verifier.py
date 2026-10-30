"""Offline integration tests for orchestrating existing simulator fixtures."""
import json
import pathlib
import sys
import tempfile
import unittest

from verifier.profiles import PROFILES, command_for
from verifier.runner import ROOT, run


class VerifierProfilesTest(unittest.TestCase):
    def test_all_legacy_runner_paths_exist(self):
        for p in PROFILES.values():
            self.assertTrue((ROOT / p.runner).is_file(), p.runner)

    def test_rejects_non_allowlisted_dds_switch(self):
        p = PROFILES['dds-anchor-extended']
        with self.assertRaisesRegex(ValueError, "Unsupported option"):
            command_for(p, ROOT, pathlib.Path(__file__), pathlib.Path(__file__),
                        None, None, '--injected', pathlib.Path('/tmp/report'), sys.executable)

    def test_rejects_missing_binary(self):
        with self.assertRaisesRegex(ValueError, 'SUT file not found'):
            command_for(PROFILES['stdio-fixture'], ROOT,
                        pathlib.Path('/definitely/not/a/sut'), None,
                        ROOT / 'simulator/scenarios/normal.json', None, None,
                        pathlib.Path('/tmp/report'), sys.executable)


class VerifierSmokeTest(unittest.TestCase):
    def _fake_sut(self, target: pathlib.Path, *, valid: bool):
        normal = json.loads((ROOT / 'simulator/scenarios/normal.json').read_text())
        events = normal['expected_events'] if valid else ['STATUS s1 EXECUTING SUCCEEDED']
        target.write_text('#!' + sys.executable + '\n'
                          'import sys\n'
                          'sys.stdin.read()\n'
                          f'print({chr(10).join(events)!r})\n', encoding='utf-8')
        target.chmod(0o755)

    def test_runner_pass_with_independent_legacy_oracle(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            sut = root / 'valid_sut'
            self._fake_sut(sut, valid=True)
            report = run('stdio-fixture', sut, root / 'evidence',
                         scenario=ROOT / 'simulator/scenarios/normal.json', timeout=25)
            self.assertEqual(report['result'], 'PASS')
            self.assertFalse(report['conformanceClaim'])
            self.assertEqual(report['transport'], 'stdio-fixture')
            self.assertEqual(json.loads((root / 'evidence/verdict.json').read_text())['result'], 'PASS')
            self.assertIn('simulatorReport', report['artifacts'])

    def test_runner_fails_on_illegal_trace(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            sut = root / 'bad_sut'
            self._fake_sut(sut, valid=False)
            report = run('stdio-fixture', sut, root / 'evidence',
                         scenario=ROOT / 'simulator/scenarios/normal.json', timeout=25)
            self.assertEqual(report['result'], 'FAIL')
            self.assertNotEqual(report['exitCode'], 0)

    def test_missing_approved_input_is_blocked_with_evidence(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            report = run('dds-anchor-extended', root / 'missing', root / 'evidence', timeout=25)
            self.assertEqual(report['result'], 'BLOCKED')
            self.assertFalse(report['conformanceClaim'])

    def test_prevents_overwrite_of_existing_evidence(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            target = root / 'existing'
            target.mkdir()
            with self.assertRaises(FileExistsError):
                run('stdio-fixture', root / 'sut', target, timeout=1)


if __name__ == '__main__':
    unittest.main()
