# V4 scoped progress and acceptance gates (local)

## Increment

- `studio/documents.py`: read-only model inventory DOCX generation, trusted
  optional Word template markers, SHA-256 manifest, re-verification, no overwrite.
- `studio/verification_links.py`: validated Verifier development exchange
  association to a particular Studio snapshot via immutable digests.
- `studio/qea.py`: close the SQLite connection deterministically after import.
- CLI, tests, CI dependency installation and documentation updated.
- `verifier/`, existing `sdk/`, `dds/`, `simulator/` and `catalog/` runtime
  implementations are **not changed**. V3 suite/Oracle semantics preserved.

## What a PASS means

- Studio unit tests prove selected synthetic QEA-like model inputs can yield a
  Word inventory; negative tests demonstrate invalid templates, mismatched
  counts and tampered artifacts are rejected.
- DOCX rendering and manual visual checks are required before shipping a
  generated example. The supplied example is illustrative, not a customer spec.
- Verifier development suite PASS means a prebuilt fixture behaves as expected
  for that synthetic scenario only, not UMAA 6.0 conformance.

## Gates still BLOCKED / not yet tested

1. **Sparx EA acceptance:** representative real QEA and model/tagged-value/
   diagram reference export unavailable.
2. **UMAA normative approval:** no independently approved official requirement
   corpus; cannot assert service conformance or automatically generate official
   standard contracts.
3. **Real DDS / RTI:** Cyclone DDS TypeSupport and authorized test network,
   RTI vendor runtime not present in local environment. Prior CI evidence is
   separate; no false fresh PASS claims.
4. **PostgreSQL/Web Studio:** production database, migrations, service API,
   roles, isolated file-processing worker and React Web UI not delivered.
5. **Formal document templates:** customer-specific templates and acceptance
   rules unavailable; generated document is a draft model inventory only.

The next useful gate is to validate a real frozen QEA + reference EA export,
followed by approved document template mapping and restricted worker deployment.
