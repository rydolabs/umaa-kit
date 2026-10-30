# UMAA Kit V5 — Scoped Studio IDL increment (2026-10-28)

## Implemented locally

- `studio/idl.py`: fail-closed, explicit project-extension-only IDL generation
  from a SHA-256-pinned read-only imported QEA snapshot and selection profile.
- Support limited to scalar fields in Class/DataType structs with selected
  references; stable EA GUIDs and numeric `t_attribute.Pos` mandatory.
- Generate deterministic source and a canonical Manifest with `approved:false`,
  `conformanceClaim:false`, `idlCompilerValidated:false`.
- Re-render IDL to verify source/Manifest integrity, reject tampering,
  collisions, cycles, unsupported types, overwrite and pin mismatches.
- Add `draft-idl-profile`, `generate-idl`, `verify-idl`, optional `check-idlc`
  CLI actions, and synthetic EA-like fixture tests.
- Preserve existing SDK, DDS Adapter, Simulator and Verifier runtime source.

## Test and evidence meaning

- Studio tests exercise synthetic EA-schema SQLite import and a draft IDL source
  generator; no actual EA `.qea` compatibility or real DDS wire behavior is
  inferred from the tests.
- The generated IDL syntax resembles Cyclone DDS `idlc`-accepted module/struct
  grammar, but **no actual compiler acceptance is claimed without `idlc`**.
- The executable `check-idlc` gate returns `BLOCKED` when no local idlc exists.
- A model import with structural issues or without explicit field ordering is
  not eligible for generation. An official UMAA/AUVSI namespace is prohibited
  in this draft-extension mode. This does not certify absence of all possible
  external type name collisions.

## Unresolved acceptance gates

1. Real Sparx `.qea` file + independent EA reference export needed to validate
   actual table layout/Tagged Values/ownership/attribute ordering.
2. Cyclone DDS `idlc` + generated C TypeSupport build required for compiler
   and runtime integration acceptance. Does not establish UMAA conformance.
3. Official normative UMAA ICD/IDL/Component/Compliance requirement approval
   needed for any official service contract or conformance claim.
4. PostgreSQL/RBAC, React Web UI, isolated generation workers, production
   upload policy and QEA model security remain unimplemented.
5. Full IDL constructs, keys, QoS and service protocol mappings require
   separate reviewed requirements and design.

## Run locally

```sh
python -m unittest discover -s studio/tests -v
python -m unittest discover -s verifier/tests -v
python -m unittest discover -s simulator/tests -v
python -m unittest discover -s simulator/dds -p 'test*.py' -v
python tools/ci_verify.py --compiler default
```

Each reported PASS must be tied to a dated log and exact source hash; do not
merge older external GitHub Actions counts into a claim of a new live DDS run.
