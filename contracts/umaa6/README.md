# UMAA 6.0 ICD YAML — reviewed baseline conversion

**Scope:** source-aware, deterministic YAML conversion of the repository's
currently reviewed **six nonexperimental UMAA 6.0 ICD mappings**. This
directory is **generated**; edit the reviewed JSON catalog and regenerate
until YAML becomes the separately validated primary source in a later version.

## What was converted

| ICD | Reviewed Service sections | Exact Topic/IDL identities | Operation names |
|---|---:|---:|---:|
| EO | 14 | 21 | 39 |
| MM | 13 | 49 | 49 |
| MO | 7 | 22 | 22 |
| SA | 14 | 14 | 14 |
| SEM | 3 | 5 | 5 |
| SO | 2 | 2 | 2 |
| **Total** | **53** | **113** | **131** |

- **6** document-resource files: \`icd/{EO,MM,MO,SA,SEM,SO}.yaml\`
- **53** Service-resource files: \`services/<domain>/<name>.yaml\`
- Reviewed ICD section references, literal Service and Operation names, Topic
  identifiers and official IDL type identifiers, declared Topic direction,
  all 5 multi-command groups, 5 MO execution-status side Topics,
  and catalog-listed collection type references are preserved.
- Query-less Request/Reply is modeled as DDS Report subscription, **not**
  an invented Request/Response RPC.
- Each Command group connects only its independently reviewed Command, ACK
  and Status Type identities. SDK state-machine semantics remain unresolved.

## What was NOT converted or approved

The existing reviewed catalog was produced from public, **secondary Markdown
transcriptions**, not a lossless copy of the official AUVSI ICD PDFs.
It contains literal service/operation/topic mappings but does not include
all descriptive paragraphs, data tables, diagrams, equations, SHALL
requirements, operations' exact per-Topic bindings, original verified
IDL keys, mandatory QoS, or verified behavior of Large Collections and
Generalization/Specialization. **Empty lists mean information is absent
from the reviewed mapping, not that the original ICD has no such content.**

Accordingly:
- Document \`sections\` are a **service section index**, not the entire
  ICD hierarchy (common flow-control chapters and appendices are not transcribed).
- Service \`documentation.sections[*].text\` is empty, rather than
  fabricated or copied from unavailable normative source text.
- \`spec.operations[*].kind = unresolved\` with null Topic mapping/clause
  until each exact operation/Topic relationship is reviewed.
- \`ddsTopicName\`, \`keyFields\`, \`qosProfileRef\` are **null**, not
  inferred from IDL type names.
- \`unresolvedCollectionTypes\` preserves catalog references without
  pretending the multi-Topic Set/List relationships are fully defined.
- \`collections\` and \`specializations\` are empty pending normative
  source evidence and actual native DDS runtime binding.
- \`originalSha256\` and official IDL bundle hashes are null until a
  source-specific official digest is independently bound to each ICD.
- All four evidence flags retain the exact states from the reviewed source;
  no formal conformance, native wire or QoS claim is added.

**Not included:** experimental EXP and CO-EXP documents, other UMAA
Component Definitions, unknown services not in the scoped six-ICD catalog,
or unreviewed original-prose sections. The original 6.0 ICD/IDL and
Component Definition/Compliance Specification remain normative.

The reviewed extraction and source provenance are explained at
[ICD_CATALOG_AUDIT.md](../../docs/specification/ICD_CATALOG_AUDIT.md).

## Regenerate and validate

The generator uses standard Python 3.11+ and deliberately emits a readable
safe YAML subset without deserializing arbitrary source YAML.

\`\`\`bash
python -m tools.generate_icd_yaml
python -m tools.generate_icd_yaml --check
python -m pip install PyYAML==6.0.2 jsonschema==4.23.0
python -m tools.validate_icd_yaml
python -m unittest discover -s sdk/tests -p test_icd_yaml_contract.py -v
\`\`\`

- Canonical schema:
  [\`schemas/umaa-icd-v1alpha1.schema.json\`](../../schemas/umaa-icd-v1alpha1.schema.json).
- The validator checks JSON Schema 2020-12, strict YAML parsing (reject
  duplicate keys and aliases), fully deterministic serialization, complete
  source-catalog parity, and no forged official readiness.
- The dedicated CI workflow runs on **Ubuntu** and **Windows**.
- Any manual edit to these generated YAML files will fail \`--check\`;
  user-authored contracts will live in a separate directory.

## Converting complete original ICD documents later

A **separate source-ingestion pipeline** is still needed to preserve all
licensed/permitted original paragraphs, tables, diagrams, figure references,
normative clauses and exact Operation↔Topic binding evidence. It must:

1. Pin each official ICD/IDL source version and SHA-256 independently.
2. Extract original section trees, paragraph/table/figure identifiers, units
   and requirement IDs; distinguish normative text from documentation.
3. Check the extractor's coverage for *every* source paragraph, requirement,
   table and diagram against the original source; do not treat gaps as empty.
4. Human-review all field/key/QoS and operation maps before changing
   \`verification\` flags or emitting official SDK TypeBindings.
5. Independently run two-component real DDS tests with Cyclone and Fast DDS
   as separate standalone backends.

The present mapping is **a complete conversion of the currently reviewed
catalog**, not a complete conversion of all contents of all original UMAA
specification documents.
