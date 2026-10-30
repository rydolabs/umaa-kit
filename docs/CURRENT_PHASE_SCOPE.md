# Current development phase scope — 2026-10-30

This file is the **scope decision for the current modular UMAA Kit development milestone**, not a modification of official UMAA requirements or a claim of certification. The approved long-term end-state in [PROJECT.md](../PROJECT.md) remains aspirational.

## Current phase: IN SCOPE

- One vendor-neutral user Component/SDK API and independently loadable DDS backend plugins.
- **Standalone Cyclone DDS** operation and **standalone Fast DDS** operation, each validated in its own deployment/network with independent-process real DDS tests as applicable.
- Running the **same compiled user Component and client binaries** against either vendor backend **in separate single-vendor configurations**. This is *backend substitution*, **not** communicating between the two vendors on one DDS network.
- Typed Command, Report, Set/List single-frame development fallback, bounded error handling, lifecycle, keyed Dispose, independent Simulator/Verifier and Linux/Windows regression within the declared official-vs-synthetic scope.
- Supervisor durability, fail-closed command handling, official service metadata/TypeSupport/Key/QoS review and selected official per-service single-vendor wire tests. Claims remain explicitly scoped to what is actually verified.
- Required CI checks for the above must remain enabled; failing tests must never be silently disabled or relabeled PASS.

## Later objectives only: OUT OF CURRENT SCOPE

1. **Mixed Cyclone DDS ↔ Fast DDS network interoperability.** No current requirement to exchange data between the vendors in either direction or pass cross-vendor reliable ACK, key/Dispose, XTypes or QoS trials. Historical strict mixed-vendor tests **FAILED**; the evidence remains intact in [SDK_DDS_MIXED_NETWORK_LIMITS.md](SDK_DDS_MIXED_NETWORK_LIMITS.md). Existing opt-in manual workflows are retained for future investigation, but their failures are **not current phase acceptance blockers**. Never change a FAIL to PASS.
2. **RTI Connext DDS adapter implementation and independent verification**, including licensed SDK/rtiddsgen setup, dedicated CI Runner, build/run, single-vendor RTI functional validation, and RTI↔other DDS interoperability. No RTI implementation or verification is required for this phase. G9/G10 remain **DEFERRED (not verified)**, not completed.

Both later objectives may be rescheduled only under a separately approved milestone with explicit dependencies, tests, evidence and resources. Historical standards traceability is preserved.

## Current-phase acceptance boundary

- PASS requires exact-commit CI evidence for the scoped SDK/Component/Simulator and **each supported standalone DDS backend**, the same-binary replacement scenarios, safety and regression tests. Do not require mixed-vendor experiments or RTI Runner for current-phase closure.
- A development-scope PASS **does not establish UMAA 6.0 conformance, production readiness, security assurance or real-hardware safety**. Official normative full-service, key, QoS, native coherent/ordered multi-writer Collection, protocol/recovery and compliance gates stay independently OPEN unless actually approved/tested.
- The previous PR #15 mixed-vendor strict failure remains **historical FAIL** despite being deferred as a schedule priority. Future work is not marked PASS, N/A for official conformance, or deleted.
- Keep Draft PR #14/#15/#16 review and merge approval boundaries intact. No silent change to original `main` or a normative acceptance profile.

See [SDK implementation backlog](SDK_IMPLEMENTATION_BACKLOG.md), [delivery gates](DELIVERY_GATES.md), [current handoff](../HANDOFF/CURRENT_2026-10-30.md) and [issue #2](https://github.com/rydolabs/umaa-kit/issues/2).
