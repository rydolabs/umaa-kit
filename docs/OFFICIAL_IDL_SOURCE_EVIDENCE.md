# Externally supplied UMAA IDL → source-audited ICD evidence

The reviewed UMAA 6.0 YAML catalog (6 ICD / 53 Services / 113 direct
Topic/Type identities) was originally mapped from **secondary text**.
It intentionally leaves official Topic strings, IDL `@key`, normative QoS,
and Operation mapping unresolved. Do **not** change official readiness flags
solely because an IDL field can be parsed.

## Local-only source inspection (licensed input stays external)

```bash
python -m pip install PyYAML==6.0.2 jsonschema==4.23.0
python -m tools.official_icd_evidence \
  --zip /private/umaa-6.0-idl-with-exp-services.zip \
  --yaml-root contracts/umaa6 \
  --expect-sha256 <independently-recorded-64-hex-hash> \
  --out /private/umaa-idl-evidence.json
```

The source archive must include its original `LICENSE`. The tool does not
extract or upload any original IDL source. ZIP traversal, duplicate member
names, symlinks, encryption and excessive uncompressed sizes are rejected.
It uses the existing strict JSON Schema/source-reference YAML loader.

For each reviewed Service Topic, the tool checks:

1. An externally supplied IDL file actually declares the full namespace/type.
2. Exactly one `const string <Type>Topic` has the exact fully qualified
   DDS Topic name. No guessed names from basename or service catalog.
3. The matching DDS struct's ordered `@key` fields and field *type names*
   are extracted, with IDL-source and YAML byte hashes.
4. A YAML `ddsTopicName` or `keyFields` that already has a value must agree
   exactly. Conflicts fail. Unresolved YAML fields are reported, not filled.
5. `unresolvedCollectionTypes` are independently audited, including large
   Set/List elements **declared inside their parent Report IDL**, without
   pretending to have established collection protocol or ordered DDS QoS.

The output may contain short declaration identifiers and hashes and should
remain **outside the public repo** while source/redistribution rules are
reviewed. It never contains original IDL code, ICD prose or PDF contents.

## Generate separate author-reviewable contracts

```bash
python -m tools.official_icd_overlay \
  --zip /private/umaa-6.0-idl-with-exp-services.zip \
  --yaml-root contracts/umaa6 \
  --expect-sha256 <independently-recorded-64-hex-hash> \
  --out-root /private/umaa6-source-backed-candidates
```

This outputs 53 separately authored Service YAML candidates (not the canonical
`contracts/umaa6`) with **only** IDL-declared `ddsTopicName`,
`keyFields` and source archive hash populated. `verification`, all
Operation mappings, QoS, unknown Collection semantics, and original PDF
evidence remain unchanged. The output is independently validated against the
same strict YAML schema and is not written if any input declaration conflicts.
Existing destination paths, mismatched pins, or attempts to write into the
generated canonical area fail closed. Keep the authored candidates private
until rights and original normative contracts have been reviewed.

## Scope of source evidence vs runtime readiness

- A source audit is **not** a full OMG IDL parser, official DDS compiler,
  reviewed runtime TypeBinding, approved QoS policy, or wire conformance.
- `approvedDdsQos`, `approvedOperationSemantics` and
  `nativeDdsTypeSupportGenerated` always remain false.
- Real Service Providers/Consumers still require complete Command/ACK/Status,
  report lifecycle, Large Collections, DDS QoS and native TypeSupport tests.
- The official ICD PDFs and Component Definitions document must separately
  justify each Operation mapping and protocol state transition, with pages,
  clauses and figure/table anchors. An IDL alone cannot establish this.
- Refer to `T0300-BE-IDS-010 Rev 1`, section 1-1 and 1-3 for the
  distinction between type compatibility and Component compliance.

## Tests

```bash
python -m unittest discover -s sdk/tests -p test_official_icd_evidence.py -v
```

GitHub Actions runs synthetic-only regressions. No user-supplied AUVSI ZIP,
ICD PDF or licensing terms are committed or requested by the CI workflow.
A private, independently verified source audit is a separate acceptance gate.

## GitHub-hosted Actions Runner: complete official archive, all types, actual wire

CI workflow: `.github/workflows/official-idl-runner-integration.yml`.

Verified against **the identical SHA-256 of the externally supplied full
UMAA 6.0 package with EXP**:

`15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`

The Runner obtains the archive exclusively from the official AUVSI
Distribution Statement A download URL. It checks the exact SHA, ZIP CRC,
LICENSE inclusion and the 594-IDL inventory, then uses only temporary storage.

Each workflow run performs all of the following:

1. Source identity/import/Topic/Key audit of all 53 catalog services, 113
   direct Topic/Type bindings and BatteryStatus embedded collection type.
2. Source-checked generation of 53 separate YAML candidates, preserving
   the not-yet-verified QoS, Operation, original ICD source proof and review
   flags. None are committed or uploaded.
3. Official Cyclone DDS `idlc` generation **and C compilation of 594/594 IDLs**
   using `tools/official_idl_coverage.py`. Source and binaries stay in
   ephemeral Runner temp; only counts appear in the logs.
4. Focused native TypeSupport compilation for BatteryStatus, EngineControl
   Command, ACK, Status; then existing real-wire integration CTests:
   `official_umaa_battery_dds_wire`,
   `official_umaa_engine_command_flow`,
   `official_umaa_engine_update_flow`.

First recorded full-EXP success: [Actions run 41345796581](https://github.com/rydolabs/umaa-kit/actions/runs/41345796581)
(594 generated/compiled, 53 YAML candidates, 3/3 real DDS integration tests).

This **does not prove** that `umaa_icd_component_codegen.py` can
generate complete official Provider/Consumer services: the original ICD
operation semantics/QoS and the fully vendor-neutral runtime linkage
are still separate, gated work. Full IDL C compilation alone does not
establish independent vendor interchange or 6.0 Component compliance.
