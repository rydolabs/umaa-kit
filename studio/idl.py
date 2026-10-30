"""Fail-closed, offline EA-snapshot -> *project extension* DDS IDL prototype.

NOT a generator of official UMAA 6.0 IDL and NOT an ICD mapping engine.
Only a deliberately small subset (structs and scalar fields) is supported.
The output is a review draft, not approved wire types or conformance evidence.
"""
from __future__ import annotations

from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

from .qea import sha

GENERATOR_VERSION = 'studio-extension-idl-0.1.0'
MAX_JSON_BYTES = 32 * 1024 * 1024
MAX_STRUCTS = 100
MAX_FIELDS_PER_STRUCT = 200
IDENTIFIER = re.compile(r'^[A-Za-z][A-Za-z0-9_]*$')
SHA256 = re.compile(r'^[0-9a-f]{64}$')
IDL_RESERVED = set(('abstract any attribute boolean case char component const consumes context custom default '
                    'double enum exception emits eventtype factory false finder fixed float getraises home import '
                    'in inout interface local long module multiple native object octet oneway out primarykey '
                    'private provides public publishes raises readonly sequence setraises short string struct '
                    'supports switch true truncatable typedef typeid typeprefix unsigned union uses valuetype '
                    'void wchar wstring').split())
TYPE_MAP = {
    'bool': 'boolean', 'boolean': 'boolean',
    'byte': 'octet', 'octet': 'octet',
    'char': 'char', 'string': 'string',
    'int16': 'short', 'short': 'short',
    'uint16': 'unsigned short', 'unsigned short': 'unsigned short',
    'int32': 'long', 'int': 'long', 'long': 'long',
    'uint32': 'unsigned long', 'unsigned int': 'unsigned long', 'unsigned long': 'unsigned long',
    'int64': 'long long', 'long long': 'long long',
    'uint64': 'unsigned long long', 'unsigned long long': 'unsigned long long',
    'float': 'float', 'double': 'double',
}


def _read_json(path: Path) -> dict:
    path = Path(path)
    if not path.is_file() or path.stat().st_size > MAX_JSON_BYTES:
        raise ValueError('Input is absent or exceeds supported JSON size limit')
    with path.open('r', encoding='utf-8') as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError('Expected a JSON object')
    return data


def _symbol(symbol: str, description: str) -> str:
    if not isinstance(symbol, str) or not IDENTIFIER.fullmatch(symbol) or symbol.lower() in IDL_RESERVED:
        raise ValueError('Invalid or reserved IDL identifier for ' + description + ': ' + str(symbol))
    return symbol


def _validate_snapshot(snapshot: dict):
    if (snapshot.get('schemaVersion') != '0.1' or snapshot.get('sourceKind') != 'sparx-ea-qea'
            or snapshot.get('isReadOnlySnapshot') is not True
            or snapshot.get('generationApproved') is not False
            or snapshot.get('modelValidation') != 'STRUCTURAL_CHECKS_PASSED'
            or snapshot.get('issues') != []):
        raise ValueError('Snapshot is not a structurally checked, read-only EA import draft')
    if not isinstance(snapshot.get('sourceSha256'), str) or not SHA256.fullmatch(snapshot['sourceSha256']):
        raise ValueError('Invalid declared source QEA SHA-256')
    records, counts = snapshot.get('records'), snapshot.get('counts')
    if not isinstance(records, dict) or not isinstance(counts, dict):
        raise ValueError('Missing model records/counts')
    for kind in ('packages', 'elements', 'connectors', 'attributes', 'diagrams'):
        rows = records.get(kind)
        if not isinstance(rows, list) or type(counts.get(kind)) is not int or len(rows) != counts[kind]:
            raise ValueError('Invalid or inconsistent snapshot count: ' + kind)
        if any(not isinstance(r, dict) for r in rows):
            raise ValueError('Invalid snapshot row: ' + kind)
    return records


def _validate_profile(profile: dict, snapshot: dict, snapshot_hash: str):
    if (profile.get('schemaVersion') != '1.0' or profile.get('kind') != 'studio-extension-idl-profile'
            or profile.get('scope') != 'project-extension-only'):
        raise ValueError('Expected an explicit project-extension-only IDL profile')
    module = _symbol(profile.get('module'), 'module')
    if module.lower().startswith(('umaa', 'auvsi')):
        raise ValueError('Official UMAA/AUVSI-like module names are prohibited in extension mode')
    for key, expected in [('expectedSnapshotSha256', snapshot_hash),
                          ('expectedSourceQeaSha256', snapshot['sourceSha256'])]:
        if profile.get(key) != expected:
            raise ValueError('Profile model pin mismatch: ' + key)
    guids = profile.get('selectedElementGuids')
    if (not isinstance(guids, list) or not guids or len(guids) > MAX_STRUCTS
            or any(not isinstance(g, str) or not g for g in guids)
            or len(guids) != len(set(guids))):
        raise ValueError('Profile must select 1..100 distinct EA element GUIDs')
    if profile.get('approved') is not False or profile.get('conformanceClaim') is not False:
        raise ValueError('Draft profile must explicitly deny approval and conformance')
    return module, guids


def draft_profile(snapshot_path: Path, module: str, guids: list[str], output: Path) -> dict:
    snapshot_path, output = Path(snapshot_path), Path(output)
    snapshot = _read_json(snapshot_path)
    _validate_snapshot(snapshot)
    data = {
        'schemaVersion': '1.0', 'kind': 'studio-extension-idl-profile',
        'scope': 'project-extension-only', 'module': module,
        'selectedElementGuids': guids,
        'expectedSnapshotSha256': sha(snapshot_path),
        'expectedSourceQeaSha256': snapshot['sourceSha256'],
        'approved': False, 'conformanceClaim': False,
    }
    _validate_profile(data, snapshot, sha(snapshot_path))
    if output.resolve() == snapshot_path.resolve():
        raise ValueError('Cannot overwrite snapshot')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('x', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write('\n')
    return data


def build_idl(snapshot: dict, profile: dict, snapshot_hash: str) -> tuple[str, dict]:
    """Render deterministic IDL bytes from pinned model/profile, without side effects."""
    records = _validate_snapshot(snapshot)
    module, chosen = _validate_profile(profile, snapshot, snapshot_hash)
    elements_by_guid = {}
    for element in records['elements']:
        guid = element.get('ea_guid')
        if not isinstance(guid, str) or not guid or guid in elements_by_guid:
            raise ValueError('Missing/duplicate EA element GUID')
        elements_by_guid[guid] = element

    selected = {}
    names = {}
    for guid in chosen:
        element = elements_by_guid.get(guid)
        if element is None:
            raise ValueError('Unknown selected EA element GUID: ' + guid)
        if element.get('Object_Type') not in ('Class', 'DataType'):
            raise ValueError('Unsupported EA element type for IDL struct: ' + str(element.get('Object_Type')))
        name = _symbol(element.get('Name'), 'element ' + guid)
        if any(name.lower() == existing.lower() for existing in names):
            raise ValueError('Duplicate/case-colliding generated struct name: ' + name)
        names[name], selected[guid] = guid, element

    fields = defaultdict(list)
    owners = {selected[g].get('Object_ID'): g for g in selected}
    if len(owners) != len(selected):
        raise ValueError('Duplicate EA Object_ID among selected elements')
    attribute_guid_set = set()
    for attr in records['attributes']:
        owner = owners.get(attr.get('Object_ID'))
        if owner is None:
            continue
        guid = attr.get('ea_guid')
        if not isinstance(guid, str) or not guid or guid in attribute_guid_set:
            raise ValueError('Generated fields require distinct EA attribute GUIDs')
        attribute_guid_set.add(guid)
        fields[owner].append(attr)

    dependencies = {g: set() for g in chosen}
    rendered_fields = {}
    for guid in chosen:
        attrs = fields[guid]
        if not attrs or len(attrs) > MAX_FIELDS_PER_STRUCT:
            raise ValueError('Selected struct needs 1..200 scalar attributes: ' + guid)
        # DDS struct member order is semantically significant. Never infer it from
        # EA GUID or database insertion order: require explicit EA t_attribute.Pos.
        positions = [a.get('Pos') for a in attrs]
        if (any(type(pos) is not int or pos < 0 for pos in positions)
                or len(set(positions)) != len(positions)):
            raise ValueError('IDL field order requires unique non-negative EA attribute Pos values: ' + guid)
        attrs.sort(key=lambda a: a['Pos'])
        seen_field_names = set()
        target = []
        for attr in attrs:
            name = _symbol(attr.get('Name'), 'attribute ' + str(attr['ea_guid']))
            if name.lower() in seen_field_names:
                raise ValueError('Duplicate/case-colliding field name: ' + name)
            seen_field_names.add(name.lower())
            raw_type = attr.get('Type')
            if not isinstance(raw_type, str):
                raise ValueError('Missing EA attribute type: ' + name)
            clean = raw_type.strip()
            type_name = TYPE_MAP.get(clean.lower())
            classifier = attr.get('Classifier')
            has_classifier = classifier is not None and str(classifier).strip() not in ('', '0')
            if type_name is not None and has_classifier:
                raise ValueError('Primitive field has an unresolved EA Classifier: ' + name)
            if type_name is None:
                if clean not in names:
                    raise ValueError('Unmapped/non-selected type ' + repr(clean) + ' for field ' + name)
                dep = names[clean]
                if has_classifier and str(classifier).strip() != str(selected[dep]['Object_ID']):
                    raise ValueError('EA Classifier disagrees with selected type: ' + name)
                if dep == guid:
                    raise ValueError('Recursive struct fields are not supported: ' + name)
                dependencies[guid].add(dep)
                type_name = clean
            target.append((name, type_name, attr['ea_guid']))
        rendered_fields[guid] = target

    # Dependency-first topological order. Cycles deliberately fail closed.
    ordered, visiting, visited = [], set(), set()
    def visit(guid):
        if guid in visiting:
            raise ValueError('Circular IDL struct dependency')
        if guid in visited:
            return
        visiting.add(guid)
        for dep in sorted(dependencies[guid], key=lambda x: selected[x]['Name']):
            visit(dep)
        visiting.remove(guid)
        visited.add(guid)
        ordered.append(guid)
    for guid in sorted(chosen, key=lambda g: selected[g]['Name']):
        visit(guid)

    lines = [f'// Generated project-extension IDL draft by {GENERATOR_VERSION}',
             '// NOT official UMAA IDL. No normative conformance or approval.',
             f'module {module} {{']
    summary = []
    for guid in ordered:
        element = selected[guid]
        lines.append(f'  struct {element["Name"]} {{')
        summary.append({'eaGuid': guid, 'name': element['Name'], 'fields': [
            {'eaGuid': ag, 'name': fn, 'idlType': ft} for fn, ft, ag in rendered_fields[guid]]})
        for field_name, field_type, _ in rendered_fields[guid]:
            lines.append(f'    {field_type} {field_name};')
        lines.extend(['  };', ''])
    lines.extend(['};', ''])
    return '\n'.join(lines), {'module': module, 'types': summary, 'scope': 'project-extension-only'}


def generate_idl(snapshot_path: Path, profile_path: Path, output_dir: Path) -> dict:
    snapshot_path, profile_path, output_dir = map(Path, (snapshot_path, profile_path, output_dir))
    if output_dir.exists():
        raise FileExistsError('Refusing to overwrite existing IDL output directory')
    snapshot, profile = _read_json(snapshot_path), _read_json(profile_path)
    rendered, meta = build_idl(snapshot, profile, sha(snapshot_path))
    if output_dir.resolve() in (snapshot_path.resolve(), profile_path.resolve()):
        raise ValueError('Invalid IDL output path')
    parent = output_dir.resolve().parent
    parent.mkdir(parents=True, exist_ok=True)
    encoded = rendered.encode('utf-8')
    manifest = {
        'schemaVersion': '1.0', 'kind': 'studio-extension-idl-draft',
        'generatorVersion': GENERATOR_VERSION, 'scope': 'project-extension-only',
        'snapshotSha256': sha(snapshot_path), 'profileSha256': sha(profile_path),
        'sourceQeaSha256AsDeclaredBySnapshot': snapshot['sourceSha256'],
        'idlFile': meta['module'] + '.idl',
        'idlSha256': hashlib.sha256(encoded).hexdigest(),
        'types': meta['types'], 'approved': False, 'conformanceClaim': False,
        'idlCompilerValidated': False,
        'limitations': ['Project-extension scalar structs only',
                        'Not official UMAA IDL', 'No idlc compile or live DDS verification'],
    }
    # Stage both outputs on same filesystem. Publication only after both are written.
    with tempfile.TemporaryDirectory(prefix='.studio-idl-', dir=parent) as tmp:
        stage = Path(tmp) / 'payload'
        stage.mkdir()
        (stage / manifest['idlFile']).write_bytes(encoded)
        (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        if output_dir.exists():
            raise FileExistsError('IDL output directory already exists')
        os.rename(stage, output_dir)
    return manifest


def verify_idl(snapshot_path: Path, profile_path: Path, output_dir: Path) -> list[str]:
    errors = []
    try:
        snapshot_path, profile_path, output_dir = map(Path, (snapshot_path, profile_path, output_dir))
        snapshot, profile = _read_json(snapshot_path), _read_json(profile_path)
        text, meta = build_idl(snapshot, profile, sha(snapshot_path))
        manifest = _read_json(output_dir / 'manifest.json')
        expected = {
            'schemaVersion': '1.0', 'kind': 'studio-extension-idl-draft',
            'generatorVersion': GENERATOR_VERSION, 'scope': 'project-extension-only',
            'snapshotSha256': sha(snapshot_path), 'profileSha256': sha(profile_path),
            'sourceQeaSha256AsDeclaredBySnapshot': snapshot['sourceSha256'],
            'idlFile': meta['module'] + '.idl',
            'idlSha256': hashlib.sha256(text.encode('utf-8')).hexdigest(),
            'types': meta['types'], 'approved': False, 'conformanceClaim': False,
            'idlCompilerValidated': False,
            'limitations': ['Project-extension scalar structs only',
                            'Not official UMAA IDL', 'No idlc compile or live DDS verification'],
        }
        for key in sorted(set(expected) | set(manifest)):
            if manifest.get(key) != expected.get(key) or (key in manifest) != (key in expected):
                errors.append('Manifest mismatch: ' + key)
        path = output_dir / expected['idlFile']
        if sha(path) != expected['idlSha256'] or path.read_bytes() != text.encode('utf-8'):
            errors.append('IDL file differs from deterministic source/model rendering')
        if sorted(p.name for p in output_dir.iterdir()) != sorted([expected['idlFile'], 'manifest.json']):
            errors.append('Unexpected files in generated IDL directory')
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as exc:
        errors.append('IDL evidence cannot be verified: ' + str(exc))
    return errors


def check_idlc(snapshot_path: Path, profile_path: Path, output_dir: Path,
               *, compiler: str = 'idlc', timeout_seconds: int = 30) -> dict:
    """Optional local Cyclone idlc syntax gate; untrusted inputs need sandboxing.

    The returned PASS refers *only* to the named local compiler accepting the
    generated IDL source, not to generated C compilation or DDS conformance.
    """
    import subprocess
    import shutil
    if not 1 <= timeout_seconds <= 120:
        raise ValueError('Invalid idlc timeout')
    errors = verify_idl(snapshot_path, profile_path, output_dir)
    if errors:
        return {'status': 'FAIL', 'reason': 'IDL evidence verification failed', 'errors': errors,
                'idlcAccepted': False, 'conformanceClaim': False}
    compiler_path = shutil.which(compiler)
    if not compiler_path:
        return {'status': 'BLOCKED', 'reason': 'Cyclone DDS idlc executable not found',
                'idlcAccepted': False, 'conformanceClaim': False}
    compiler_path = str(Path(compiler_path).resolve())
    file = Path(output_dir).resolve() / (_read_json(Path(output_dir) / 'manifest.json')['idlFile'])
    try:
        with tempfile.TemporaryDirectory(prefix='studio-idlc-') as td:
            # -S means compile/validate only (no output binding code).
            process = subprocess.run([compiler_path, '-l', 'c', '-S', str(file)],
                                     cwd=td, capture_output=True, text=True,
                                     timeout=timeout_seconds, check=False)
        return {
            'status': 'PASS' if process.returncode == 0 else 'FAIL',
            'reason': 'Compiler accepted IDL' if process.returncode == 0 else 'Compiler rejected IDL',
            'compilerPath': compiler_path, 'compilerSha256': sha(Path(compiler_path)),
            'idlSha256': sha(file), 'exitCode': process.returncode,
            'compilerOutputTail': (process.stdout + '\n' + process.stderr)[-8192:],
            'idlcAccepted': process.returncode == 0, 'conformanceClaim': False,
        }
    except subprocess.TimeoutExpired:
        return {'status': 'FAIL', 'reason': 'idlc timeout', 'idlcAccepted': False, 'conformanceClaim': False}
    except OSError as exc:
        return {'status': 'BLOCKED', 'reason': 'Cannot execute idlc: ' + str(exc),
                'idlcAccepted': False, 'conformanceClaim': False}
