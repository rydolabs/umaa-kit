# Verifier V1-offline: acceptance matrix

The objective of **this increment** is a reproducible, offline and **narrowly
scoped** verification orchestrator over *existing* simulator runners. It is not
a completed UMAA 6.0 conformance tool or a hardened multi-user service.

| ID | Acceptance condition | Verification |
|---|---|---|
| V1-01 | Six known simulator profiles, no arbitrary plugin/command execution | `python -m verifier list`; registry negative tests |
| V1-02 | All six profiles have explicit, **non-normative** contracts | `verifier/contracts/legacy-profiles.json`; registry policy tests |
| V1-03 | Existing C++ mock SUT runs all 4 stdio scenarios with independent oracle | CMake build; `python -m verifier run-suite` |
| V1-04 | PASS and FAIL are distinguished; invalid trace must FAIL | verifier test suite, invalid fake SUT |
| V1-05 | Missing binary BLOCKED, timeout ERROR; never PASS by default | verifier negative tests |
| V1-06 | Inputs, runner, contract registry and output hashes recorded | `verdict.json` and `evidence-index.json` |
| V1-07 | Corrupted log/verdict/case evidence detected | `verify-evidence` and tamper tests |
| V1-08 | Existing simulator unit tests unaffected | `unittest discover` in `simulator/tests` and `simulator/dds` |
| V1-09 | Existing SDK builds & CTest regression unaffected | `cmake --build`; `ctest` |
| V1-10 | All reports explicit `conformanceClaim=false` | contract/schema tests; JSON inspection |

## Deferred; deliberately NOT satisfied

- Full UMAA 6.0 ICD, Component, Compliance requirement test coverage: **blocked
  on reviewed normative contracts and scope; G6/G11 remain OPEN**.
- Independent official-IDL Cyclone DDS run in this environment: **not run**;
  relies on external installation of library, pinned source and binaries.
- RTI/vendor interoperability: **blocked** (licensed RTI inputs unavailable).
- Arbitrary uploaded/untrusted executables: **unsafe**; worker/container isolation,
  quotas, authn/authz, isolation and audit must precede a web API.
- PostgreSQL, multi-user jobs, dedicated Verifier UI and Studio integration:
  **not part of this offline increment**.
- Integrity checksum is **consistency only**, not immutable storage, signature,
  verified provenance or third-party attestation.

## Future tool boundaries

`studio/` handles QEA model import and artifact generation. `verifier/` uses
approved independent test criteria. Studio-supplied source IDs can assist in
traceability, but do not define an expected PASS. `simulator/` remains an
external oracle and stimulus runtime. No directory is renamed or duplicated.

## V2 incremental acceptance (local, 2026-10-28)

- DDS network opt-in: real-wire profiles are `BLOCKED` by default; no
  DDS-capable process starts without an explicit operator declaration.
  This is *not* isolation, authentication or authorization enforcement.
- Verifier result export: digest-checked JSON (`studio-result-v1.schema.json`)
  for future Studio integration; no normative success is inferred.
- Scope diagnostic and `--require-ready` release gate: selected source catalog
  contains 53 service candidates, 26 unreviewed compliance-clause locators,
  zero approved normative requirements, and no applicable target Component.
  Formal release-gate result remains `BLOCKED` (exit code 2).
- Stdio scenarios now confined to repository fixture paths; tampering before
  export fails integrity checks. New tests cover the DDS gate, result export,
  scope gate and invalid external scenario handling.

See `docs/verifier/V2_INCREMENT.md` for tests, boundaries and blocked targets.
