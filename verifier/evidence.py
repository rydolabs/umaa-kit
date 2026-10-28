"""Offline integrity consistency checks; not a cryptographic signature/attestation."""
import json
from pathlib import Path
from typing import List

from .contracts import sha256
from .assertions import success_errors


def write_index(evidence_dir: Path) -> None:
    verdict = evidence_dir / 'verdict.json'
    data = {'schemaVersion': '1.0', 'verdictSha256': sha256(verdict)}
    (evidence_dir / 'evidence-index.json').write_text(
        json.dumps(data, indent=2) + '\n', encoding='utf-8')


def _safe_file(base: Path, rel: str) -> Path:
    if not isinstance(rel, str) or not rel or Path(rel).name != rel:
        raise ValueError('Evidence file must be a basename')
    path = base / rel
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'Missing or symlink evidence: {rel}')
    return path


def verify_run(evidence_dir: Path) -> List[str]:
    """Return detected corruptions; no claim of authenticity against a malicious editor."""
    errors = []
    try:
        index = json.loads(_safe_file(evidence_dir, 'evidence-index.json').read_text(encoding='utf-8'))
        data_path = _safe_file(evidence_dir, 'verdict.json')
        data = json.loads(data_path.read_text(encoding='utf-8'))
        if index.get('schemaVersion') != '1.0' or index.get('verdictSha256') != sha256(data_path):
            errors.append('verdict digest mismatch')
        if data.get('conformanceClaim') is not False:
            errors.append('unexpected conformance claim')
        if data.get('result') not in ('PASS', 'FAIL', 'BLOCKED', 'ERROR'):
            errors.append('invalid result')
        if data.get('result') == 'PASS' and data.get('exitCode') != 0:
            errors.append('success without zero process exit code')
        if data.get('schemaVersion') == '1.1' and data.get('result') == 'PASS':
            files = data.get('artifacts', {})
            try:
                stdout = _safe_file(evidence_dir, files['stdout']['file'])
                report = (_safe_file(evidence_dir, files['simulatorReport']['file'])
                          if 'simulatorReport' in files else None)
                snapshot = (_safe_file(evidence_dir, files['scenarioSnapshot']['file'])
                            if 'scenarioSnapshot' in files else None)
                errors.extend(success_errors(data.get('profileId'), data.get('option'),
                                             stdout, report, snapshot))
                if data.get('profileId') == 'stdio-fixture':
                    scenario_input = data.get('inputs', {}).get('scenario', {})
                    if snapshot is None or sha256(snapshot) != scenario_input.get('sha256'):
                        errors.append('scenario snapshot does not match recorded input hash')
            except (OSError, ValueError, TypeError, KeyError) as exc:
                errors.append('missing/invalid PASS evidence: ' + str(exc))
        if data.get('schemaVersion') not in ('1.0', '1.1'):
            errors.append('unsupported run evidence schema')
        if data.get('transport') == 'cyclonedds' and data.get('result') == 'PASS' and data.get('ddsNetworkApprovalDeclared') is not True:
            errors.append('DDS success without declared test network approval')
        if data.get('transport') != 'cyclonedds' and data.get('ddsNetworkApprovalDeclared') is not False:
            errors.append('invalid DDS approval declaration on non-DDS evidence')
        contract = data.get('contract')
        if not isinstance(contract, dict) or contract.get('reviewStatus') != 'development-unreviewed' or contract.get('conformanceEligible') is not False or contract.get('normativeRequirementRefs') != []:
            errors.append('unsafe/unreviewed contract claim')
        for kind, entry in data.get('artifacts', {}).items():
            if not isinstance(entry, dict):
                errors.append('invalid artifact entry: ' + kind)
                continue
            try:
                path = _safe_file(evidence_dir, entry.get('file'))
                if sha256(path) != entry.get('sha256') or path.stat().st_size != entry.get('bytes'):
                    errors.append('artifact mismatch: ' + kind)
            except (OSError, ValueError, TypeError):
                errors.append('missing/unsafe artifact: ' + kind)
        if 'stdout' not in data.get('artifacts', {}) or 'stderr' not in data.get('artifacts', {}):
            errors.append('missing stdout/stderr evidence records')
        if data.get('profileId') == 'stdio-fixture' and data.get('result') == 'PASS' and 'simulatorReport' not in data.get('artifacts', {}):
            errors.append('missing successful stdio oracle report')
        if not isinstance(data.get('inputs'), dict) or not isinstance(data.get('contract'), dict):
            errors.append('missing input/contract provenance')
    except (OSError, ValueError, TypeError, AttributeError, KeyError, json.JSONDecodeError) as exc:
        errors.append('invalid evidence envelope: ' + str(exc))
    return errors


def verify_suite(evidence_dir: Path) -> List[str]:
    errors = []
    try:
        index = json.loads(_safe_file(evidence_dir, 'suite-index.json').read_text(encoding='utf-8'))
        summary_path = _safe_file(evidence_dir, 'suite-verdict.json')
        summary = json.loads(summary_path.read_text(encoding='utf-8'))
        if index.get('schemaVersion') != '1.0' or index.get('summarySha256') != sha256(summary_path):
            errors.append('suite summary digest mismatch')
        observed_counts = {name: 0 for name in ('PASS', 'FAIL', 'BLOCKED', 'ERROR')}
        seen_case_ids = set()
        for case in summary.get('cases', []):
            cid = case.get('caseId')
            if not isinstance(cid, str) or not cid or Path(cid).name != cid or '..' in cid:
                errors.append('invalid case identifier')
                continue
            if cid in seen_case_ids:
                errors.append('duplicate suite case: ' + cid)
            seen_case_ids.add(cid)
            if case.get('result') in observed_counts:
                observed_counts[case['result']] += 1
            else:
                errors.append('invalid suite case result: ' + cid)
            subdir = evidence_dir / cid
            if subdir.is_symlink() or not subdir.is_dir():
                errors.append('missing/unsafe case evidence: ' + cid)
                continue
            errors.extend(cid + ': ' + e for e in verify_run(subdir))
            try:
                verdict = subdir / 'verdict.json'
                if sha256(verdict) != case['verdictSha256']:
                    errors.append('case summary digest mismatch: ' + cid)
                d = json.loads(verdict.read_text(encoding='utf-8'))
                if case.get('result') != d.get('result'):
                    errors.append('case result mismatch: ' + cid)
                if (case.get('runId') != d.get('runId') or case.get('profileId') != d.get('profileId') or case.get('contractId') != d.get('contract', {}).get('contractId')):
                    errors.append('case metadata mismatch: ' + cid)
            except (KeyError, OSError, ValueError):
                errors.append('unreadable case verdict: ' + cid)
        if summary.get('conformanceClaim') is not False:
            errors.append('unexpected suite conformance claim')
        if len(summary.get('cases', [])) != summary.get('total'):
            errors.append('incorrect suite case count')
        if summary.get('counts') != observed_counts:
            errors.append('incorrect suite result counts')
        expected = next((name for name in ('ERROR', 'FAIL', 'BLOCKED', 'PASS') if observed_counts[name]), None)
        if summary.get('result') != expected:
            errors.append('incorrect suite aggregate verdict')
    except (OSError, ValueError, TypeError, AttributeError, KeyError, json.JSONDecodeError) as exc:
        errors.append('invalid suite evidence: ' + str(exc))
    return errors
