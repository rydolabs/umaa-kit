"""Administrative association of Studio model snapshots and Verifier exports.

This does NOT attribute a verifier PASS to the model, check SUT/model identity,
validate external normative requirements, or make any conformance claim.
"""
from datetime import datetime, timezone
import json
from pathlib import Path
import re

from .documents import _load_snapshot
from .qea import sha

SCHEMA_FILE = Path(__file__).resolve().parents[1] / 'verifier' / 'schemas' / 'studio-result-v1.schema.json'
PROJECT_ID = re.compile(r'^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$')


def link_verification(snapshot_path: Path, verifier_export_path: Path,
                      output_path: Path, *, project_id: str) -> dict:
    """Link two immutable evidence identities without modifying verdicts."""
    if not isinstance(project_id, str) or not PROJECT_ID.fullmatch(project_id):
        raise ValueError('Project identifier must be 1..64 safe characters')
    snapshot_path, verifier_export_path, output_path = map(
        Path, (snapshot_path, verifier_export_path, output_path))
    if len({p.resolve() for p in (snapshot_path, verifier_export_path, output_path)}) != 3:
        raise ValueError('Input and output paths must differ')
    if output_path.exists():
        raise FileExistsError(str(output_path))
    snapshot = _load_snapshot(snapshot_path)
    if verifier_export_path.stat().st_size > 5 * 1024 * 1024:
        raise ValueError('Verifier result export too large')
    doc = json.loads(verifier_export_path.read_text(encoding='utf-8'))
    schema = json.loads(SCHEMA_FILE.read_text(encoding='utf-8'))
    try:
        from jsonschema import Draft202012Validator, FormatChecker
    except ImportError as exc:
        raise RuntimeError('jsonschema is required to validate the Verifier exchange contract') from exc
    errors = list(Draft202012Validator(schema, format_checker=FormatChecker()).iter_errors(doc))
    if errors:
        raise ValueError('Verifier exchange schema invalid: ' + '; '.join(e.message for e in errors[:3]))
    if any(case['conformanceClaim'] is not False or case['normativeRequirementRefs'] != []
           for case in doc['cases']):
        raise ValueError('Normative claims are not permitted in development result links')
    result = {
        'schemaVersion': '1.0', 'kind': 'studio-verifier-development-link',
        'createdAtUtc': datetime.now(timezone.utc).isoformat(),
        'projectId': project_id,
        'modelSnapshotSha256': sha(snapshot_path),
        'sourceQeaSha256AsDeclaredBySnapshot': snapshot['sourceSha256'],
        'verifierExchangeSha256': sha(verifier_export_path),
        'sourceEvidenceDigestSha256AsDeclaredByExport': doc['sourceEvidenceDigestSha256'],
        'verifierReportedResult': doc['result'],
        'verifierReportedCases': len(doc['cases']),
        'associationType': 'administrative-only',
        'sutModelIdentityValidated': False,
        'sourceEvidenceRevalidated': False,
        'conformanceClaim': False,
        'reviewStatus': 'development-unreviewed',
        'notice': 'Association is not proof the verifier tested this model or an equivalent SUT.',
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
        f.write('\n')
    return result


def verify_link(snapshot_path: Path, verifier_export_path: Path, link_path: Path) -> list[str]:
    errors = []
    try:
        doc = json.loads(Path(link_path).read_text(encoding='utf-8'))
        snapshot = _load_snapshot(Path(snapshot_path))
        export = json.loads(Path(verifier_export_path).read_text(encoding='utf-8'))
        for key, expected in (
            ('schemaVersion', '1.0'),
            ('kind', 'studio-verifier-development-link'),
            ('modelSnapshotSha256', sha(Path(snapshot_path))),
            ('sourceQeaSha256AsDeclaredBySnapshot', snapshot['sourceSha256']),
            ('verifierExchangeSha256', sha(Path(verifier_export_path))),
            ('sourceEvidenceDigestSha256AsDeclaredByExport', export['sourceEvidenceDigestSha256']),
            ('verifierReportedResult', export['result']),
            ('verifierReportedCases', len(export['cases'])),
            ('associationType', 'administrative-only'),
            ('sutModelIdentityValidated', False),
            ('sourceEvidenceRevalidated', False),
            ('conformanceClaim', False),
            ('reviewStatus', 'development-unreviewed'),
        ):
            if doc.get(key) != expected:
                errors.append('Link mismatch: ' + key)
    except (OSError, ValueError, TypeError) as exc:
        errors.append('Invalid verification link: ' + str(exc))
    return errors
