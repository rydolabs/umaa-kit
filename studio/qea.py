"""Read-only extraction of selected EA repository objects from a local QEA snapshot.

This adapter intentionally supports a small, explicit subset of the EA schema;
it does not attempt to interpret proprietary diagram content or write to EA.
Real customer QEA files still require a compatibility/acceptance test.
"""
from datetime import datetime, timezone
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3

SCHEMA = {
    'packages': ('t_package', ('Package_ID', 'ea_guid', 'Name', 'Parent_ID', 'Notes'),
                 ('Package_ID', 'ea_guid', 'Name', 'Parent_ID')),
    'elements': ('t_object', ('Object_ID', 'ea_guid', 'Name', 'Object_Type', 'Package_ID', 'Stereotype', 'Note'),
                 ('Object_ID', 'ea_guid', 'Name', 'Object_Type', 'Package_ID')),
    'connectors': ('t_connector', ('Connector_ID', 'ea_guid', 'Name', 'Connector_Type', 'Start_Object_ID', 'End_Object_ID'),
                   ('Connector_ID', 'ea_guid', 'Connector_Type', 'Start_Object_ID', 'End_Object_ID')),
    'attributes': ('t_attribute', ('ID', 'ea_guid', 'Object_ID', 'Name', 'Type', 'Classifier', 'Pos'),
                   ('ID', 'Object_ID', 'Name', 'Type')),
    'diagrams': ('t_diagram', ('Diagram_ID', 'ea_guid', 'Name', 'Package_ID', 'Diagram_Type'),
                 ('Diagram_ID', 'Name', 'Package_ID')),
}
MAX_QEA_BYTES = 1024 * 1024 * 1024
MAX_TABLE_ROWS = 200_000


def sha(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def _fetch(conn, kind, table, columns, required):
    # Only known, statically defined table names and projected columns are used.
    present = {row[1].lower(): row[1] for row in conn.execute(f'PRAGMA table_info("{table}")')}
    missing = [c for c in required if c.lower() not in present]
    if missing:
        raise ValueError(f'Unsupported QEA schema {table}: missing {missing}')
    selected = [c for c in columns if c.lower() in present]
    quote = lambda v: '"' + v.replace('"', '""') + '"'
    cursor = conn.execute(f'SELECT {", ".join(quote(present[c.lower()]) for c in selected)} FROM {quote(table)}')
    rows = cursor.fetchmany(MAX_TABLE_ROWS + 1)
    if len(rows) > MAX_TABLE_ROWS:
        raise ValueError(f'QEA {kind} exceeds supported safety limit')
    entries = [{c: value for c, value in zip(selected, row)} for row in rows]
    entries.sort(key=lambda x: (str(x.get(required[0]) or ''), str(x.get('ea_guid') or '')))
    return entries


def extract(path: Path) -> dict:
    if path.suffix.lower() != '.qea' or not path.is_file():
        raise ValueError('Expected a regular local .qea file')
    path = path.resolve(strict=True)
    if path.stat().st_size > MAX_QEA_BYTES:
        raise ValueError('QEA exceeds local import size limit')
    before = sha(path)
    # Restrict the SQLite handle to reads. Production imports must copy an EA-
    # closed file into an isolated, untrusted-input worker before extraction.
    db_uri = path.as_uri() + '?mode=ro&immutable=1'
    try:
        with closing(sqlite3.connect(db_uri, uri=True, timeout=3)) as conn:
            conn.execute('PRAGMA query_only=ON')
            conn.execute('PRAGMA trusted_schema=OFF')
            check = conn.execute('PRAGMA quick_check(1)').fetchone()
            if check is None or check[0] != 'ok':
                raise ValueError('QEA SQLite quick_check did not pass')
            tables = {r[0].lower(): r[0] for r in conn.execute(
                "SELECT name FROM sqlite_master WHERE type='table'")}
            if not {'t_package', 't_object', 't_connector'}.issubset(tables):
                raise ValueError('Not an expected Enterprise Architect repository: required tables absent')
            result, unsupported = {}, []
            for kind, (table, columns, required) in SCHEMA.items():
                if table.lower() not in tables:
                    if kind in ('packages', 'elements', 'connectors'):
                        raise ValueError('Required table missing: ' + table)
                    unsupported.append(table)
                    result[kind] = []
                else:
                    result[kind] = _fetch(conn, kind, tables[table], columns, required)
    except sqlite3.DatabaseError as exc:
        raise ValueError('Invalid/unsupported QEA SQLite repository: ' + str(exc)) from exc
    if sha(path) != before:
        raise ValueError('QEA changed during read-only import; close EA and retry with a frozen copy')
    issues = []
    for kind, records in result.items():
        if kind in ('packages', 'elements', 'connectors'):
            guids = [v.get('ea_guid') for v in records if v.get('ea_guid')]
            if len(guids) != len(set(guids)):
                issues.append(kind + ' contains duplicate EA GUIDs')
            if len(guids) != len(records):
                issues.append(kind + ' contains entries without stable EA GUIDs')
    package_ids = {p['Package_ID'] for p in result['packages']}
    element_ids = {e['Object_ID'] for e in result['elements']}
    for e in result['elements']:
        if e['Package_ID'] not in package_ids:
            issues.append('element missing parent package: ' + str(e['Object_ID']))
    for c in result['connectors']:
        if c['Start_Object_ID'] not in element_ids or c['End_Object_ID'] not in element_ids:
            issues.append('connector has unresolved endpoint: ' + str(c['Connector_ID']))
    for a in result['attributes']:
        if a['Object_ID'] not in element_ids:
            issues.append('attribute has unresolved owner: ' + str(a['ID']))
    return {
        'schemaVersion': '0.1', 'sourceKind': 'sparx-ea-qea',
        'sourceSha256': before, 'extractedAtUtc': datetime.now(timezone.utc).isoformat(),
        'isReadOnlySnapshot': True, 'generationApproved': False,
        'modelValidation': 'WARNINGS' if issues else 'STRUCTURAL_CHECKS_PASSED',
        'warnings': unsupported, 'issues': issues,
        'counts': {kind: len(v) for kind,v in result.items()}, 'records': result,
    }


def save_snapshot(model: dict, output: Path):
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as stream:
        json.dump(model, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    return output


def model_diff(previous: dict, current: dict):
    if previous.get('schemaVersion') != '0.1' or current.get('schemaVersion') != '0.1':
        raise ValueError('Unsupported model snapshot schema')
    changes = {}
    for kind in SCHEMA:
        def index(doc):
            records = doc.get('records', {}).get(kind, [])
            if not isinstance(records, list):
                raise ValueError('Invalid snapshot records')
            result = {}
            for r in records:
                guid = r.get('ea_guid')
                if not isinstance(guid, str) or not guid or guid in result:
                    raise ValueError('Model diff requires unique EA GUID for each ' + kind)
                result[guid] = r
            return result
        p, c = index(previous), index(current)
        changes[kind] = {
            'added': sorted(c.keys() - p.keys()),
            'removed': sorted(p.keys() - c.keys()),
            'changed': sorted(k for k in c.keys() & p.keys() if p[k] != c[k]),
        }
    return {'schemaVersion':'0.1','previousSha256':previous['sourceSha256'],
            'currentSha256':current['sourceSha256'], 'changes':changes,
            'requiresArtifactReview':any(any(v.values()) for v in changes.values()),
            'note':'Potential impact only. No generated IDL/DOCX is updated automatically.'}
