"""Reproducible local suite orchestration with per-case immutable evidence."""
from datetime import datetime, timezone
import json
from pathlib import Path
import uuid
from typing import Optional

from .contracts import load_suite, sha256, REGISTRY
from .evidence import verify_run
from .runner import run, ROOT


# Do not collapse failed/blocking verdicts into PASS.
AGGREGATE_PRIORITY = ('ERROR', 'FAIL', 'BLOCKED', 'PASS')


def run_suite(suite_path: Path, output_dir: Path, sut: Path, *,
              oracle: Optional[Path] = None, module: Optional[Path] = None,
              repo_root: Path = ROOT, dds_network_approved: bool = False) -> dict:
    suite = load_suite(suite_path, repo_root)
    output_dir.mkdir(parents=True, exist_ok=False)
    rows = []
    for item in suite['cases']:
        scenario = (repo_root / item['scenario']) if item['scenario'] else None
        result = run(item['profileId'], sut, output_dir / item['id'],
                     oracle=oracle, scenario=scenario, module=module,
                     option=item['option'], timeout=item['timeoutSec'], repo_root=repo_root,
                     dds_network_approved=dds_network_approved)
        errors = verify_run(output_dir / item['id'])
        if errors:
            # Evidence-integrity failure must not be reported as passing.
            result['result'] = 'ERROR'
            result['message'] = 'Evidence integrity failure: ' + '; '.join(errors)
            from .evidence import write_index
            (output_dir / item['id'] / 'verdict.json').write_text(
                json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
            write_index(output_dir / item['id'])
        rows.append({'caseId': item['id'], 'profileId': item['profileId'],
                     'contractId': result['contract']['contractId'], 'result': result['result'],
                     'runId': result['runId'],
                     'verdictSha256': sha256(output_dir / item['id'] / 'verdict.json')})
    counts = {k: sum(r['result'] == k for r in rows) for k in ('PASS', 'FAIL', 'BLOCKED', 'ERROR')}
    overall = next(k for k in AGGREGATE_PRIORITY if counts[k])
    summary = {'schemaVersion': '1.0', 'suiteId': suite['suiteId'],
               'suiteRunId': uuid.uuid4().hex, 'createdAtUtc': datetime.now(timezone.utc).isoformat(),
               'suiteDefinitionSha256': sha256(suite_path),
               'contractRegistrySha256': sha256(REGISTRY),
               'conformanceClaim': False, 'result': overall,
               'total': len(rows), 'counts': counts, 'cases': rows}
    summary_file = output_dir / 'suite-verdict.json'
    summary_file.write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    (output_dir / 'suite-index.json').write_text(json.dumps({
        'schemaVersion': '1.0', 'summarySha256': sha256(summary_file)}, indent=2) + '\n', encoding='utf-8')
    return summary
