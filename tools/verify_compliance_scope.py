#!/usr/bin/env python3
"""Fail-closed UMAA scope integrity check; this is not a conformance test."""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'catalog/umaa6-six-icd-reviewed.json'
SCOPE = ROOT / 'catalog/umaa6-conformance-scope.json'
GROUPS = {'provider_topics': 'provider', 'consumer_topics': 'consumer', 'collection_topics': 'collection'}
MANDATORY_SOURCES = {'COMPLIANCE_REV1', 'COMPONENT_DEFINITIONS_1_0'} | {'ICD_%s_6_0' % n for n in ('EO', 'MO', 'MM', 'SA', 'SEM', 'SO', 'CO', 'EXP')}
APPLICABILITY = {'applicable', 'conditional', 'not_applicable', 'undetermined'}


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def normalize_topics(original):
    return sorted(({'name': t['name'], 'type': t['type'], 'role': t['role'], 'direction': direction}
                   for group, direction in GROUPS.items() for t in original.get(group, [])),
                  key=lambda t: (t['name'], t['type'], t['role'], t['direction']))


def validate(catalog, scope):
    errors = []
    profile = scope.get('scope_profile', {})
    if scope.get('schema_version') != '1.0.0':
        errors.append('unsupported schema')
    if profile.get('claimed_full_umaa_conformance') is not False:
        errors.append('unreviewed full compliance claim')
    if profile.get('applicability_locked') and not profile.get('declared_umaa_component'):
        errors.append('applicability locked without selected Component')
    if profile.get('experimental_default') != 'excluded_unless_explicitly_required':
        errors.append('experimental scope changed without explicit contract')
    sources = scope.get('sources', [])
    source_ids = [s.get('id') for s in sources]
    if set(source_ids) != MANDATORY_SOURCES or len(source_ids) != len(set(source_ids)):
        errors.append('incomplete or duplicated source authority list')
    source_map = {s.get('id'): s for s in sources}
    if source_map.get('COMPONENT_DEFINITIONS_1_0', {}).get('kind') != 'component_definitions_not_requirements':
        errors.append('Component Definitions are not the requirements specification')
    for s in sources:
        if not s.get('url', '').startswith('https://'):
            errors.append('source URL missing for %s' % s.get('id'))
    components = scope.get('component_candidates', [])
    if len(components) != 9 or len({c.get('name') for c in components}) != 9:
        errors.append('public v1.0 component name candidates incomplete')
    if any(c.get('assignment_reviewed') and not c.get('service_assignments') for c in components):
        errors.append('reviewed Component membership has no services')
    originals = {s['name']: s for s in catalog.get('services', [])}
    candidates = scope.get('service_candidates', [])
    cand_names = [s.get('name') for s in candidates]
    if len(cand_names) != len(set(cand_names)) or set(cand_names) != set(originals):
        errors.append('candidate service inventory differs from reviewed catalog')
    total = 0
    topics = []
    for s in candidates:
        name = s.get('name', '')
        original = originals.get(name)
        if original is None:
            continue
        if s.get('icd') != name.split('::')[1] or s.get('source_clauses') != original.get('clauses', []):
            errors.append('source/ICD mapping drift: ' + name)
        try:
            actual = sorted(s.get('topics', []), key=lambda t: (t['name'], t['type'], t['role'], t['direction']))
            if actual != normalize_topics(original):
                errors.append('Topic role/type/direction drift: ' + name)
        except (TypeError, KeyError):
            errors.append('malformed candidate Topic: ' + name)
        total += len(s.get('topics', []))
        topics.extend(t.get('name') for t in s.get('topics', []))
        if s.get('applicability') not in APPLICABILITY:
            errors.append('invalid applicability: ' + name)
        if s.get('applicability') == 'not_applicable' and not (s.get('applicability_rationale') and s.get('component_membership_reviewed')):
            errors.append('unreviewed not_applicable: ' + name)
        if s.get('conformance_verdict') == 'pass' and not (s.get('authoritative_icd_reconciled') and s.get('component_membership_reviewed') and s.get('normative_requirements') and s.get('applicability') == 'applicable'):
            errors.append('unsupported conformance PASS: ' + name)
    stats = scope.get('existing_six_icd_catalog', {})
    if stats.get('actual_service_count') != len(originals) or stats.get('actual_topic_entries') != total or stats.get('observed_unique_topic_names') != len(set(topics)):
        errors.append('candidate count drift')
    if stats.get('documented_service_count') != 53:
        errors.append('legacy service count changed without review')
    mismatch = total != stats.get('documented_topic_count')
    discrepancies = [d for d in scope.get('known_discrepancies', []) if d.get('id') == 'TOPIC_COUNT_001' and d.get('status') == 'unresolved' and d.get('catalog_documented_count') == stats.get('documented_topic_count') and d.get('service_topic_entries') == total]
    if mismatch and len(discrepancies) != 1:
        errors.append('unrecorded Topic count discrepancy')
    if not mismatch and discrepancies:
        errors.append('stale resolved Topic discrepancy')
    normative = scope.get('normative_requirements', [])
    ids = [r.get('requirement_id') for r in normative]
    if len(ids) != len(set(ids)):
        errors.append('duplicated normative requirement ID')
    for requirement in normative:
        rid = requirement.get('requirement_id', '<missing>')
        if requirement.get('applicability') not in APPLICABILITY:
            errors.append('unknown normative applicability ' + rid)
        if requirement.get('applicability') == 'not_applicable' and not all(requirement.get(x) for x in ('applicability_justification', 'reviewer', 'clause')):
            errors.append('unjustified normative N/A ' + rid)
        if requirement.get('verdict') == 'pass':
            fields = ('authoritative_document','document_version','clause','source_sha256','reviewer','independent_test','evidence_commit','evidence_run_url')
            if not all(requirement.get(x) for x in fields) or not source_map.get(requirement.get('authoritative_document'), {}).get('full_text_reviewed'):
                errors.append('unproven normative PASS ' + rid)
    blocked = bool(not normative or not profile.get('applicability_locked') or mismatch or any(s.get('applicability') == 'undetermined' for s in candidates) or any(not source_map.get(x, {}).get('full_text_reviewed') for x in ('COMPLIANCE_REV1','COMPONENT_DEFINITIONS_1_0','ICD_CO_6_0')))
    if blocked and scope.get('acceptance', {}).get('claim_status') != 'BLOCKED':
        errors.append('false compliance accepted status')
    return {'candidate_services': len(candidates), 'candidate_topic_entries': total,
            'documented_topic_count': stats.get('documented_topic_count'),
            'count_discrepancy_open': mismatch,
            'published_component_name_candidates': len(components),
            'normative_requirements_traced': len(normative),
            'source_full_compliance_reviewed': source_map.get('COMPLIANCE_REV1', {}).get('full_text_reviewed',False),
            'claim_blocked': blocked, 'errors': errors}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',type=Path,default=CATALOG)
    parser.add_argument('--scope',type=Path,default=SCOPE)
    parser.add_argument('--report',type=Path)
    parser.add_argument('--strict',action='store_true')
    args=parser.parse_args(argv)
    report=validate(load_json(args.catalog),load_json(args.scope))
    payload=json.dumps(report,sort_keys=True,indent=2)+'\n'
    print(payload,end='')
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(payload,encoding='utf-8')
    return 2 if report['errors'] else 3 if args.strict and report['claim_blocked'] else 0

if __name__=='__main__':
    sys.exit(main())
