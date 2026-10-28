# UMAA Studio — offline generators (V4/V5) + V6 web foundation

This module **does not edit Sparx EA** or provide the production React/PostgreSQL
platform yet. The source QEA (closed/copied before import) remains authoritative.

## Available paths

- `import-qea`: read-only import of a documented **subset** of the QEA SQLite
  structure; saves a JSON snapshot with original QEA checksum.
- `diff`: compare two snapshots using EA GUIDs. A diff is a review cue, **not**
  an automatic migration of mapped metadata or code.
- `generate-docx`: generate an unapproved development **model inventory DOCX**
  from a snapshot and optional **trusted local DOCX template**; create a JSON
  manifest containing source/model/template/report SHA-256 values.
- `verify-docx`: check those files' SHA-256 consistency; not provenance
  authenticity or official document approval.
- `link-verification`: associate an imported model snapshot with a Verifier V2+
  development result export in JSON by digest, **without claiming** that the
  SUT tested by Verifier matches this model.
- `verify-link`: check model/result digest consistency.

Install the offline generator dependencies: `python -m pip install -r studio/requirements.txt`.

## CLI example (from repository root)

```sh
python -m studio import-qea --input /path/to/closed-copy.qea --output /tmp/model.json
python -m studio generate-docx --snapshot /tmp/model.json \
  --output /tmp/model-inventory.docx --manifest /tmp/model-inventory.json \
  --title 'Model inventory (unapproved draft)'
python -m studio verify-docx --snapshot /tmp/model.json \
  --output /tmp/model-inventory.docx --manifest /tmp/model-inventory.json
# Optional trusted DOCX template: add --template /path/to/approved-template.docx
python -m studio link-verification --snapshot /tmp/model.json \
  --verifier-export /path/to/verifier-studio-result.json \
  --output /tmp/model-verifier-link.json --project-id demo
python -m studio verify-link --snapshot /tmp/model.json \
  --verifier-export /path/to/verifier-studio-result.json \
  --output /tmp/model-verifier-link.json
python -m unittest discover -s studio/tests -v
```

### Document template subset

Use plain text placeholders in paragraphs (also permitted in header/footer):
`{{MODEL_TITLE}}`, `{{MODEL_SOURCE_SHA256}}`, `{{MODEL_VALIDATION}}`,
`{{MODEL_APPROVAL_STATUS}}`, and `{{MODEL_PACKAGES_COUNT}}`,
`{{MODEL_ELEMENTS_COUNT}}`, `{{MODEL_CONNECTORS_COUNT}}`,
`{{MODEL_ATTRIBUTES_COUNT}}`, `{{MODEL_DIAGRAMS_COUNT}}`.

Place a dedicated paragraph containing `[[MODEL_TABLE:elements]]` (or
`packages`, `connectors`, `attributes`, `diagrams`) to insert a model inventory
Word table. Other template content and styling remain with the template;
placeholders that cross multiple runs are supported. This is **not** a general
Word layout engine and does not render EA diagram graphics or all EA extension
metadata. Template uploads and external links are **not allowed** for this
trusted-local offline PoC; a production multi-user worker must add strong
isolation and document policy checks.

### Explicit limitations / acceptance gates

- Tests use synthetic EA-like SQLite records. Validate a **real** QEA against
  EA's own model export before claiming QEA interoperability. QEA is opened
  read-only with SQLite immutable mode; import only a frozen copy.
- The generated Word document is a **development inventory draft** and must
  not be presented as an ICD, a validated interface specification, or UMAA
  conformance evidence. Original UMAA IDL is **not** generated or rewritten.
- SHA-256 checks detect inconsistent bytes, **not** a forged source or approval;
  a valid report is not proof of model correctness.
- Verification links are administrative evidence associations, **not** proof of
  code/model identity or conformance. Verifier computes its verdict independently.
- No live PostgreSQL, authentication/RBAC, sandboxed document workers, real EA
  diagrams, real Cyclone DDS, or Web UI are claimed here.

## V5: QEA snapshot → project-extension DDS IDL draft (offline)

This is **not** a generator for the AUVSI UMAA 6.0 official IDL; it only
supports operator-selected, project-specific extension structures. It produces
**non-approved drafts** from a frozen QEA snapshot and an explicit model-pinned
selection profile. Do not use this output as a certified UMAA type definition.

```bash
# First import a closed/copy of a local EA QEA file (still a synthetic-fixture PoC).
python -m studio import-qea --input /path/to/frozen.qea --output /tmp/model.json
# The selected element GUIDs below must be actual Class/DataType EA GUIDs
# from that snapshot. Namespace must be project-specific (NOT UMAA/AUVSI).
python -m studio draft-idl-profile --snapshot /tmp/model.json \
    --module ProjectTypes --element-guid '{SELECTED-EA-GUID}' \
    --output /tmp/idl-profile.json
python -m studio generate-idl --snapshot /tmp/model.json \
    --profile /tmp/idl-profile.json --output-dir /tmp/generated-extension
python -m studio verify-idl --snapshot /tmp/model.json \
    --profile /tmp/idl-profile.json --output-dir /tmp/generated-extension
# Optional: requires trusted, locally installed Cyclone DDS idlc; otherwise BLOCKED/exit 2
python -m studio check-idlc --snapshot /tmp/model.json \
    --profile /tmp/idl-profile.json --output-dir /tmp/generated-extension \
    --report /tmp/idlc-check.json
```

### Explicitly supported subset

- `Class` or `DataType` elements → one IDL `struct` per selected EA GUID.
- Scalar `t_attribute` members with stable EA GUID and **unique numeric EA
  `t_attribute.Pos`**: field order follows `Pos` (never sorted by GUID).
- `boolean`, `octet`, `char`, `string`, signed/unsigned 16/32/64-bit integers,
  `float`, `double`, and references to other **selected** generated structs.
- Supported dependent structs are generated before their referencing structs.
- Case-insensitive identifier collisions, reserved words, unknown types,
  unselected references, unknown Classifiers, duplicate/missing `Pos` values,
  self-reference and dependency cycles are rejected **before any output**.
- `manifest.json` pins source QEA checksum (as declared by the imported model),
  snapshot checksum, selection profile checksum, generated `.idl` hash and EA
  element/attribute GUID mappings. Verification re-renders deterministic IDL;
  it is consistency evidence, not source authenticity or approval.
- Generated output directories are created fresh and never overwritten.

### Outside the supported subset

UMAA normative IDL/types, DDS key/QoS, enums, unions, bounded strings, arrays,
sequences, inherited fields, operations, stereotypes, tagged-value mapping,
official `Component Definitions`, large model import, PostgreSQL persistence,
React UI, real customer QEA compatibility, generated C compilation, real DDS
wire communication, and all conformance verdicts remain outside this V5 gate.
The optional `check-idlc` uses a local compiler in a **trusted offline**
process, not a sandbox suitable for processing arbitrary web-uploaded models.
Real production ingestion must use isolated workers with resource limits,
network controls and authorization. If `idlc` is unavailable, the command
returns `BLOCKED` and no compilation claim is recorded in the draft Manifest.


## V6: API, SQL and React browser workspace

See [`PLATFORM_V6.md`](PLATFORM_V6.md) for API server, PostgreSQL schema,
queued workers, test commands, development-only auth and acceptance gates.
The earlier offline `python -m studio` commands remain supported.
