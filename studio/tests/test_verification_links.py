"""Verification association only: fixtures contain no approved UMAA requirements."""
import json
from pathlib import Path
import tempfile
import unittest

from studio.qea import extract, save_snapshot
from studio.verification_links import link_verification, verify_link
from test_qea import qea_fixture


def unreviewed_export():
    # Schema-equivalent minimal development-only exchange, not an oracle run.
    return {
        'schemaVersion': '1.0', 'kind': 'umaa-verifier-development-result',
        'exportedAtUtc': '2026-10-28T00:00:00+00:00', 'sourceType': 'run',
        'sourceEvidenceDigestSha256': '0'*64, 'suiteId': None, 'result': 'PASS',
        'conformanceClaim': False, 'normativeRequirementRefs': [],
        'contractReviewStatus': 'development-unreviewed',
        'explanation': 'fixture only',
        'cases': [{
            'caseId': 'single-run', 'runId': 'fake', 'profileId': 'stdio-fixture',
            'contractId': 'dev/stdio-line-trace', 'transport': 'stdio-fixture',
            'result': 'PASS', 'verdictDigestSha256': '0'*64,
            'conformanceClaim': False, 'normativeRequirementRefs': [],
            'ddsNetworkApprovalDeclared': False, 'startedAtUtc': None,
            'durationMs': 0, 'inputDigests': {}, 'artifactDigests': {}
        }]
    }


class VerificationLinkTests(unittest.TestCase):
    def _setup(self, root):
        snap = root / 'model.json'
        save_snapshot(extract(qea_fixture(root / 'm.qea')), snap)
        source = root / 'verifier.json'
        source.write_text(json.dumps(unreviewed_export()))
        return snap, source

    def test_link_unreviewed_and_detect_modification(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            snap, src = self._setup(root)
            target = root / 'links' / 'result.json'
            doc = link_verification(snap, src, target, project_id='project-1')
            self.assertFalse(doc['sutModelIdentityValidated'])
            self.assertFalse(doc['sourceEvidenceRevalidated'])
            self.assertFalse(doc['conformanceClaim'])
            self.assertEqual(doc['verifierReportedResult'], 'PASS')
            self.assertEqual(verify_link(snap, src, target), [])
            modified = json.loads(target.read_text())
            modified['verifierReportedResult'] = 'FAIL'
            target.write_text(json.dumps(modified))
            self.assertIn('Link mismatch: verifierReportedResult', verify_link(snap, src, target))
            modified['verifierReportedResult'] = 'PASS'
            target.write_text(json.dumps(modified))
            src.write_text(src.read_text()+' ')
            self.assertIn('Link mismatch: verifierExchangeSha256', verify_link(snap, src, target))

    def test_rejects_normative_and_additional_fields(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            snap, src = self._setup(root)
            data = json.loads(src.read_text())
            data['conformanceClaim'] = True
            src.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'schema invalid'):
                link_verification(snap, src, root / 'x.json', project_id='p')
            self.assertFalse((root / 'x.json').exists())
            data['conformanceClaim'] = False
            data['unexpected'] = 'payload'
            src.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'schema invalid'):
                link_verification(snap, src, root / 'x.json', project_id='p')

    def test_invalid_project_id_and_existing_output(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            snap, src = self._setup(root)
            with self.assertRaisesRegex(ValueError, 'Project identifier'):
                link_verification(snap, src, root / 'out.json', project_id='../escape')
            link_verification(snap, src, root / 'out.json', project_id='approved-demo')
            with self.assertRaises(FileExistsError):
                link_verification(snap, src, root / 'out.json', project_id='approved-demo')


if __name__=='__main__': unittest.main()
