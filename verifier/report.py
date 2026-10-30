"""Portable, read-only HTML view of verified scoped evidence (no web server)."""
from html import escape
import json
from pathlib import Path

from .evidence import verify_run, verify_suite

_STYLE = '''
:root{font-family:system-ui,-apple-system,Segoe UI,sans-serif;color:#172536;background:#f5f7fa}
body{max-width:1060px;margin:36px auto;padding:0 22px;line-height:1.5}
h1{letter-spacing:-.025em;margin-bottom:0}p{color:#475467}
section{background:#fff;border:1px solid #dce2e9;border-radius:12px;padding:20px;margin:18px 0}
table{width:100%;border-collapse:collapse;text-align:left}th,td{padding:12px;border-bottom:1px solid #e7ebef}
th{font-size:13px;color:#475467}td{font-size:14px}
code{background:#f1f3f5;padding:2px 5px;border-radius:4px}
.pill{font-weight:700;border:1px solid currentColor;border-radius:14px;padding:2px 10px;font-size:12px}
.PASS{color:#12613f}.FAIL{color:#9f252b}.ERROR{color:#9f252b}.BLOCKED{color:#875510}
.meta{display:flex;gap:20px;flex-wrap:wrap}.meta>div{min-width:140px}
small{color:#667085}caption{text-align:left;margin-bottom:10px;font-size:13px;color:#475467}
'''


def _safe(s):
    return escape(str(s), quote=True)


def _row(*items):
    return '<tr>' + ''.join('<td>' + _safe(x) + '</td>' for x in items) + '</tr>'


def render(evidence_dir: Path, output_file: Path) -> Path:
    """Refuse to present inconsistent evidence as a trustworthy success report."""
    is_suite = (evidence_dir / 'suite-verdict.json').is_file()
    errors = verify_suite(evidence_dir) if is_suite else verify_run(evidence_dir)
    if errors:
        raise ValueError('Evidence integrity verification failed: ' + '; '.join(errors))
    if output_file.exists():
        raise FileExistsError(str(output_file))
    data = json.loads((evidence_dir / ('suite-verdict.json' if is_suite else 'verdict.json')).read_text())
    status = data['result']
    identity = data.get('suiteId', data.get('profileId', 'unknown'))
    if is_suite:
        counts = data['counts']
        detail = ('<section><h2>Execution summary</h2><div class="meta">' +
                  ''.join(f'<div><strong>{_safe(counts[key])}</strong><br><small>{key}</small></div>'
                          for key in ('PASS', 'FAIL', 'BLOCKED', 'ERROR')) +
                  '</div></section><section><h2>Cases</h2><table><thead><tr>' +
                  '<th>Case</th><th>Profile</th><th>Contract</th><th>Result</th><th>Run ID</th>' +
                  '</tr></thead><tbody>' + ''.join(
                      _row(item['caseId'], item['profileId'], item['contractId'],
                           item['result'], item['runId']) for item in data['cases']) +
                  '</tbody></table></section>')
    else:
        detail = ('<section><h2>Run details</h2><table><tbody>' + ''.join(
            _row(label, data.get(key)) for label, key in (
                ('Profile', 'profileId'), ('Transport', 'transport'), ('Scope', 'scope'),
                ('Run ID', 'runId'), ('Started UTC', 'startedAtUtc'),
                ('Duration (ms)', 'durationMs'), ('Message', 'message'))) +
                  '</tbody></table></section>')
    html = ('<!doctype html><html lang="en"><head><meta charset="utf-8">' +
            '<meta name="viewport" content="width=device-width,initial-scale=1">' +
            '<title>UMAA Verifier — scoped evidence</title><style>' + _STYLE +
            '</style></head><body><header><h1>UMAA Verifier</h1>' +
            '<p>Scoped offline execution evidence · <strong>NOT UMAA conformance certification</strong></p>' +
            '<p>Profile / suite: <code>' + _safe(identity) + '</code> · ' +
            '<span class="pill ' + _safe(status) + '">' + _safe(status) + '</span></p></header>' +
            detail + '<section><h2>Evidence interpretation</h2>' +
            '<p>These results describe only the specified existing simulator test cases. ' +
            'The contracts are development-unreviewed, normative requirement coverage is empty, ' +
            'and conformanceClaim is false. Digest checks establish consistency, not origin ' +
            'authenticity or immutable audit.</p></section></body></html>')
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(html, encoding='utf-8')
    return output_file
