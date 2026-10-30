# Verifier V2 — approved-test-network gate and traceability exchange

Status: **scoped offline verifier development increment**, not formal UMAA
conformance and not a deployable multi-user verification service.

## Design decisions

- Continue `verifier/` as standalone orchestrator; retain `simulator/` as
  independently executed external runner/oracle; leave `sdk/`, `dds/` untouched.
- Keep *development-unreviewed* contracts and `conformanceClaim=false`.
- Block DDS subprocess execution unless a trusted local operator explicitly
  declares the test network approved (`--dds-network-approved`). This is a
  **procedural safety acknowledgement, not network enforcement**.
- Restrict stdio fixture selection to the repository fixture directory.
- Export a read-only, evidence-verified JSON interchange for a future Studio
  to ingest. This exchange contains neither acceptance overrides nor expected
  PASS values; Studio must treat it as evidence and display its non-normative
  scope verbatim.
- Report the exact unapproved scope inventory using `scope-report` backed by
  the original catalog, without inferring conformance from the candidate list.

## V2 acceptance cases

| ID | Criterion | Evidence |
|---|---|---|
| V2-01 | DDS requires explicit operator acknowledgement | isolated unit test mocks process spawn, expects BLOCKED and zero launches |
| V2-02 | DDS suite also blocks without acknowledgement | suite and individual-case verdict, no subprocess launches |
| V2-03 | Forged DDS PASS lacking operator acknowledgement rejected | recomputed checksum negative test detects semantic inconsistency |
| V2-04 | Evidence-verified JSON result exchange preserves cases and source digests | single and suite export tests, optional JSON schema test |
| V2-05 | Corrupt evidence is rejected before exporting | tampered stdout log test |
| V2-06 | StdIO scenarios cannot come from untrusted paths | invalid external scenario path test |
| V2-07 | Source conformance coverage is transparently BLOCKED | catalog-backed scope-report test, zero approved normative contracts |
| V2-08 | V1 and original SDK/Simulator regression unaffected | full local regression scripts and CI fixture suite |

## Critical remaining requirements

- **Official compliance:** catalog has zero reviewer-approved normative
  requirements; Component applicability/ICD semantics are not approved.
- **Real DDS:** no Cyclone DDS runtime and approved official TypeSupport/SUT
  binaries available in this environment, so no real wire test is asserted.
  Actual real DDS must be run separately on a controlled, authorized test net.
- **Security:** caller-supplied executable paths are suitable only for the
  existing trusted offline operator workflow. No sandbox, multi-user RBAC,
  PostgreSQL, production UI or signed immutable evidence store is delivered.
- **Vendor interop:** RTI licensed environment absent.

Completion of a V2 increment does not mean completion of the full UMAA
verification product. Only real tests with approved requirements can retire
those scope blockers.
