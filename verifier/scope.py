"""Read-only capability/coverage gap report. Never infers normative approval."""
import json
from pathlib import Path

from .contracts import ROOT, load_contracts, sha256


def assess_scope(repo_root: Path = ROOT) -> dict:
    """Report only the facts expressed by the repo's candidate compliance catalog."""
    path = repo_root / 'catalog/umaa6-conformance-scope.json'
    doc = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(doc, dict) or doc.get('schema_version') != '1.0.0':
        raise ValueError('Unknown conformance scope catalog schema')
    requirements = doc.get('normative_requirements')
    candidates = doc.get('service_candidates')
    clauses = doc.get('normative_clause_candidates')
    accept = doc.get('acceptance')
    scope = doc.get('scope_profile')
    if not (isinstance(requirements, list) and isinstance(candidates, list)
            and isinstance(clauses, list) and isinstance(accept, dict)
            and isinstance(scope, dict)):
        raise ValueError('Incomplete conformance scope catalog')
    contracts = load_contracts()
    # Do not automatically produce READY even after adding some clauses: a
    # separately reviewed applicability and release gate is still required.
    return {
        'schemaVersion': '1.0',
        'sourceCatalogSha256': sha256(path),
        'gateStatus': 'BLOCKED', 'officialConformanceClaim': False,
        'profileCoverage': {
            'registeredSimulatorProfiles': len(contracts),
            'approvedNormativeTestContracts': 0,
            'developmentOnlyProfiles': len(contracts),
        },
        'candidateInventory': {
            'serviceCandidateCount': len(candidates),
            'unreviewedClauseLocatorCount': len(clauses),
            'approvedNormativeRequirementCount': len(requirements),
        },
        'blockers': {
            'targetComponentUnselected': scope.get('declared_umaa_component') is None,
            'applicabilityUnlocked': scope.get('applicability_locked') is not True,
            'primaryDocumentsUnreviewed': accept.get('unreviewed_primary_documents') is True,
            'requirementsUnmapped': accept.get('unmapped_normative_requirements') is True,
            'normativeTestCorpusNotApproved': True,
        },
        'notice': 'Candidate inventory and simulator pass results are NOT official UMAA conformance',
    }
