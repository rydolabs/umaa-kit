"""Read-only, non-normative result interchange with the future Studio.

No imported project, SDK-generated expectation or user-supplied result can change
an independent verifier verdict. SHA-256 proves consistency, not authenticity.
"""
import json
from datetime import datetime, timezone
from pathlib import Path

from .contracts import sha256
from .evidence import verify_run, verify_suite


def export_results(evidence_dir: Path, output_file: Path) -> Path:
    """Export digest-checked evidence as JSON, never overwrite an existing file."""
    is_suite = (evidence_dir / 'suite-verdict.json').is_file()
    errors = verify_suite(evidence_dir) if is_suite else verify_run(evidence_dir)
    if errors:
        raise ValueError('Evidence integrity verification failed: ' + '; '.join(errors))
    if output_file.exists():
        raise FileExistsError(str(output_file))
    input_file = evidence_dir / ('suite-verdict.json' if is_suite else 'verdict.json')
    data = json.loads(input_file.read_text(encoding='utf-8'))
    if is_suite:
        cases = []
        for row in data['cases']:
            d = json.loads((evidence_dir / row['caseId'] / 'verdict.json').read_text(encoding='utf-8'))
            cases.append(_case(d, row['caseId'], row['verdictSha256']))
        suite_id = data['suiteId']
    else:
        cases = [_case(data, 'single-run', sha256(input_file))]
        suite_id = None
    doc = {
        'schemaVersion': '1.0', 'kind': 'umaa-verifier-development-result',
        'exportedAtUtc': datetime.now(timezone.utc).isoformat(),
        'sourceType': 'suite' if is_suite else 'run',
        'sourceEvidenceDigestSha256': sha256(input_file),
        'suiteId': suite_id, 'result': data['result'],
        'conformanceClaim': False, 'normativeRequirementRefs': [],
        'contractReviewStatus': 'development-unreviewed',
        'explanation': 'Development-only test evidence; NOT official UMAA conformance',
        'cases': cases,
    }
    output_file.parent.mkdir(parents=True, exist_ok=True)
    # x mode prevents accidental overwrites even if another process creates it now.
    with output_file.open('x', encoding='utf-8') as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write('\n')
    return output_file


def _case(data: dict, case_id: str, sha: str) -> dict:
    return {
        'caseId': case_id, 'runId': data['runId'],
        'profileId': data['profileId'],
        'contractId': data['contract']['contractId'],
        'transport': data['transport'], 'result': data['result'],
        'verdictDigestSha256': sha, 'conformanceClaim': False,
        'normativeRequirementRefs': [],
        'ddsNetworkApprovalDeclared': data.get('ddsNetworkApprovalDeclared', False),
        'startedAtUtc': data.get('startedAtUtc'), 'durationMs': data.get('durationMs'),
        'inputDigests': {name: item.get('sha256') for name, item in data.get('inputs', {}).items()},
        'artifactDigests': {name: item['sha256'] for name, item in data.get('artifacts', {}).items()},
    }
