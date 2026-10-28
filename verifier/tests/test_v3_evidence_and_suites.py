"""V3: defend against spurious PASS and validate packaged, scoped DDS plans."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from verifier.assertions import success_errors
from verifier.contracts import ROOT, load_suite, sha256
from verifier.evidence import verify_run
from verifier.runner import run

SCENARIO = ROOT / 'simulator/scenarios/normal.json'


def good_sut(path):
    events = json.loads(SCENARIO.read_text())['expected_events']
    path.write_text('#!' + sys.executable + '\nimport sys\nsys.stdin.read()\nprint(' + repr('\n'.join(events)) + ')\n')
    path.chmod(0o755)
    return path


def reindex(output):
    verdict = output / 'verdict.json'
    (output / 'evidence-index.json').write_text(json.dumps({
        'schemaVersion': '1.0', 'verdictSha256': sha256(verdict)}))


class SuccessEvidenceV3(unittest.TestCase):
    def test_stdio_evidence_is_self_contained(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            d=run('stdio-fixture',good_sut(root/'sut'),root/'evidence',scenario=SCENARIO,timeout=25)
            self.assertEqual(d['result'],'PASS')
            self.assertEqual(d['schemaVersion'],'1.1')
            self.assertEqual(d['artifacts']['scenarioSnapshot']['sha256'],sha256(SCENARIO))
            self.assertEqual(verify_run(root/'evidence'),[])

    def test_forged_report_rejected_even_if_all_envelope_hashes_recalculated(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            run('stdio-fixture',good_sut(root/'sut'),root/'evidence',scenario=SCENARIO,timeout=25)
            evidence=root/'evidence'
            path=evidence/'simulator-result.json'
            report=json.loads(path.read_text())
            report['trace']=['FORGED']
            path.write_text(json.dumps(report))
            verdict=evidence/'verdict.json'
            data=json.loads(verdict.read_text())
            data['artifacts']['simulatorReport']['sha256']=sha256(path)
            data['artifacts']['simulatorReport']['bytes']=path.stat().st_size
            verdict.write_text(json.dumps(data))
            reindex(evidence)
            self.assertTrue(any('stdio oracle report does not match' in e for e in verify_run(evidence)))

    def test_scenario_snapshot_must_match_original_input(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            run('stdio-fixture',good_sut(root/'sut'),root/'evidence',scenario=SCENARIO,timeout=25)
            evidence=root/'evidence'
            path=evidence/'scenario.snapshot.json'
            scenario=json.loads(path.read_text())
            scenario['description']='Edited after execution'
            path.write_text(json.dumps(scenario))
            verdict=evidence/'verdict.json'
            data=json.loads(verdict.read_text())
            data['artifacts']['scenarioSnapshot']['sha256']=sha256(path)
            data['artifacts']['scenarioSnapshot']['bytes']=path.stat().st_size
            verdict.write_text(json.dumps(data))
            reindex(evidence)
            self.assertIn('scenario snapshot does not match recorded input hash',verify_run(evidence))

    def test_dds_exit_zero_without_expected_markers_must_fail(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            sut=good_sut(root/'sut')
            oracle=good_sut(root/'oracle')
            with patch('verifier.runner.command_for',return_value=[sys.executable,'-c',"print('NO DDS ORACLE')"]):
                d=run('dds-anchor',sut,root/'evidence',oracle=oracle,
                      dds_network_approved=True,timeout=20)
            self.assertEqual(d['exitCode'],0)
            self.assertEqual(d['result'],'FAIL')
            self.assertIn('missing DDS supervisor/oracle marker',d['message'])
            self.assertEqual(verify_run(root/'evidence'),[])

    def test_semantic_discovery_needs_both_semantic_markers(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'stdout'
            p.write_text('REVIEWED_SERVICE_DISCOVERY_HOST_PASS\nREVIEWED_SERVICE_DISCOVERY_SIM_PASS\n'
                         'REVIEWED_SERVICE_TYPED_WIRE_HOST_PASS\nREVIEWED_SERVICE_TYPED_WIRE_SIM_PASS\n')
            errors=success_errors('dds-reviewed-discovery','--semantic',p)
            self.assertTrue(any('ANCHOR_SPECS_SEMANTIC_HOST_PASS' in e for e in errors))


class PackagedDdsSuiteV3(unittest.TestCase):
    def test_packaged_dds_suites_are_strictly_valid_and_dev_only(self):
        paths=sorted((ROOT/'verifier/suites').glob('dds-*-development.json'))
        self.assertEqual(len(paths),5)
        for path in paths:
            with self.subTest(path=path.name):
                doc=load_suite(path)
                self.assertTrue(doc['cases'])
                self.assertTrue(all(c['profileId'].startswith('dds-') for c in doc['cases']))
                self.assertTrue(all(c['scenario'] is None for c in doc['cases']))
                self.assertIn('NOT UMAA conformance',path.read_text())

if __name__ == '__main__':
    unittest.main()
