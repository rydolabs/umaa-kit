"""Explicit non-normative legacy contract registry and verified suite definitions.

The registry is a declared mapping to existing simulator oracles, NOT an
independently approved UMAA compliance requirements corpus.
"""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from typing import Any, Dict

from .profiles import PROFILES

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = Path(__file__).resolve().parent / 'contracts' / 'legacy-profiles.json'
_ID = re.compile(r'^[a-zA-Z0-9][a-zA-Z0-9_.-]{0,63}$')


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for part in iter(lambda: f.read(1024 * 1024), b''):
            h.update(part)
    return h.hexdigest()


def _object(path: Path) -> dict:
    with path.open(encoding='utf-8') as f:
        item = json.load(f)
    if not isinstance(item, dict):
        raise ValueError(f'Expected JSON object: {path}')
    return item


def load_contracts(path: Path = REGISTRY) -> Dict[str, Dict[str, Any]]:
    doc = _object(path)
    if (doc.get('schemaVersion') != '1.0' or doc.get('status') != 'development-unreviewed'
            or doc.get('normativeRequirementRefs') != [] or not isinstance(doc.get('cases'), list)):
        raise ValueError('Unsupported/unsafe legacy contract registry: normative approval is not established')
    entries = {}
    for item in doc['cases']:
        if not isinstance(item, dict):
            raise ValueError('Contract entry must be an object')
        pid = item.get('profileId')
        if (pid not in PROFILES or pid in entries or
                not isinstance(item.get('contractId'), str) or
                not item['contractId'].startswith('dev/') or
                not isinstance(item.get('basis'), str) or not item['basis'] or
                item.get('conformanceEligible') is not False):
            raise ValueError(f'Invalid/unreviewed legacy contract mapping: {pid}')
        entries[pid] = item
    if set(entries) != set(PROFILES):
        raise ValueError('Legacy contract registry and available profiles disagree')
    return entries


def load_suite(path: Path, repo_root: Path = ROOT) -> dict:
    doc = _object(path)
    cases = doc.get('cases')
    if (doc.get('schemaVersion') != '1.0' or not isinstance(doc.get('suiteId'), str)
            or not _ID.fullmatch(doc['suiteId']) or not isinstance(cases, list)
            or not 1 <= len(cases) <= 100):
        raise ValueError('Invalid suite format, identifier or case count (1..100)')
    contracts = load_contracts()
    seen = set()
    validated = []
    allowed_scenarios = (repo_root / 'simulator' / 'scenarios').resolve()
    for case in cases:
        if not isinstance(case, dict):
            raise ValueError('Suite case must be an object')
        cid = case.get('id')
        pid = case.get('profileId')
        timeout = case.get('timeoutSec', 90)
        opt = case.get('option')
        if not isinstance(cid, str) or not _ID.fullmatch(cid) or cid in seen:
            raise ValueError(f'Invalid/duplicate case identifier: {cid}')
        if pid not in contracts:
            raise ValueError(f'Unregistered test profile: {pid}')
        if type(timeout) is not int or not 1 <= timeout <= 180:
            raise ValueError(f'Invalid timeout for {cid}')
        if opt is not None and opt not in PROFILES[pid].choices:
            raise ValueError(f'Unsupported option for {cid}')
        s = case.get('scenario')
        if PROFILES[pid].needs_scenario:
            if (not isinstance(s, str) or not s.endswith('.json') or
                    PurePosixPath(s).is_absolute() or '..' in PurePosixPath(s).parts or
                    not s.startswith('simulator/scenarios/')):
                raise ValueError(f'Scenario path required for {cid}')
            # No absolute paths, symlinks or traversal to arbitrary external input.
            path = (repo_root / s).resolve()
            if path.parent != allowed_scenarios or not path.is_file():
                raise ValueError(f'Scenario must exist directly under simulator/scenarios: {s}')
            scenario = _object(path)
            if (not isinstance(scenario.get('name'), str) or
                not isinstance(scenario.get('actions'), list) or
                not all(isinstance(a, str) for a in scenario['actions']) or
                not isinstance(scenario.get('expected_events'), list) or
                not all(isinstance(a, str) for a in scenario['expected_events'])):
                raise ValueError(f'Invalid scenario data: {s}')
        elif s is not None:
            raise ValueError(f'Scenario not supported for {pid}')
        seen.add(cid)
        validated.append(dict(id=cid, profileId=pid, timeoutSec=timeout,
                              option=opt, scenario=s))
    return {'schemaVersion': '1.0', 'suiteId': doc['suiteId'],
            'description': doc.get('description', ''), 'cases': validated}
