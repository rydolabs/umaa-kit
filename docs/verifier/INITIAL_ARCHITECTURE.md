# Independent Verifier: initial decision and implementation boundary

Decision: maintain `simulator/` as independent proof assets, add `verifier/`
as the separately managed verification product. Reuse the simulator **test
execution method** (external DDS or development-fixture processes with a
separate oracle), not its name as the verifier's general architecture.

1. Existing narrow scenarios remain in `simulator/`; their outcomes cannot be
   enlarged into automatic UMAA conformance conclusions.
2. `verifier/` controls test profiles, trusted execution, result semantics and
   evidence. An initial offline CLI wraps existing runners by subprocess.
3. Later `verifier/engine/` should add independent specification-derived
   requirement checks. No SDK production-state-machine imports.
4. Future `verifier/backend/` and `verifier/frontend/` are separate from
   `studio/` deployment and access-control decisions.
5. Studio can link a model GUID and selected normative contract ID to a test;
   verifier maintains its own approved expected results, requirement evidence
   and verdicts.

## Phase gates

- V0 (present scaffold): list legacy test profiles, execute stdio-fixture or
  selected existing real-DDS harness when binaries are supplied, produce scoped
  machine-readable PASS/FAIL/BLOCKED/ERROR evidence with no conformance claim.
- V1: authenticated/test-isolated job service, artifact provenance, immutable
  evidence, timeouts/quotas on Linux/Windows; contract schemas and API.
- V2: independent approval workflow for normative UMAA requirements, Component
  scope, IDL/ICD roles, QoS, protocol expectations and per-requirement tests.
- V3: full audited UI, multi-DDS environment matrix and cross-vendor results.

**Not implemented:** V1-V3, independent normative approvals, whole-service
conformance, vendor interoperability, production safety and real hardware tests.

Repo root `docs/DELIVERY_GATES.md` continues to describe SDK delivery gates.
Verifier milestones above do not weaken those gates.
