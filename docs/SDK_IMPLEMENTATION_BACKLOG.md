# UMAA SDK implementation ledger — PR #15

**As-of:** 2026-10-29. This document records development-scope results and
required acceptance gates, never a conformance certificate.

| Capability | State | Evidence and unresolved gate |
|---|---|---|
| Simple typed Provider Component module | PASS (prototype) | C ABI v2 DLL/SO, module loader, Linux/Windows tests; official service runtime incomplete |
| Typed Consumer/CommandHandle | PARTIAL | local session poll/cancel/wait; not actual UMAA on-wire Ack/Status |
| Report Provider lifecycle | PASS (synthetic development scope) | persistent writer, multiple updates, dispose, acknowledgment, fault fencing; no official all-service policy |
| Per-key Report cache | PASS (mock development scope) | independent keys, per-key timestamp and tombstone; vendor lifecycle proof separately required |
| Cyclone DDS backend facade | PARTIAL | synthetic BatteryStatus real-wire; official services remain legacy DDS hosts |
| Fast DDS backend facade | PARTIAL / CHECK CI | Fast DDS 2.x synthetic BatteryStatus, real-wire and parity Actions are acceptance gates, not evidence until SUCCESS |
| Identical client and module binaries across vendors | CHECK CI | `sdk-dds-vendor-parity.yml` must run both on one exact set of binaries |
| LargeSet / LargeList | PARTIAL | CollectionAssembler core only; group coherent/ordered DDS plus recovery not verified |
| Official UMAA 6.0 TypeBindings | BLOCKED ON CONTRACT REVIEW | source IDLs compiled previously; no approved all-service roles, keys and QoS |
| Cross-vendor mixed-network operation | NOT PROVEN | same-binary parity is not Cyclone-to-Fast DDS mixed-network interoperability |
| RTI Connext backend | BLOCKED | licensed SDK, runner, TypeSupport and policy verification required |
| Safe module stop/reload | PARTIAL | in-process ABI checks and failed-stop mapping retention; pending-session draining and crash isolation still absent |
| Linux/Windows regressions | PASS as of [Actions run 40810011629](https://github.com/rydolabs/umaa-kit/actions/runs/40810011629) | test suite and source commit scoped; subsequent heads require rerun |
| Formal UMAA compliance | BLOCKED | reviewer-approved profile, normative SHALL traceability, independent oracle and acceptance |

## Source of truth for extension

- `sdk/include/umaa/internal/dds_backend.h` — exact stable C backend ABI
- `sdk/include/umaa/module_api_v2.h` — user Component module ABI
- `sdk/include/umaa/component.hpp` and `application.hpp` — minimal public C++ API
- `dds/vendors/<vendor>/` — all vendor-specific headers, compiler and TypeSupport
- `sdk/tests/test_report_publisher_v2.cpp` — multikey, persistent writer, resource ownership
- `.github/workflows/sdk-dds-vendor-parity.yml` — identical binaries against two DDS plugins

## Fail-closed conditions

Reject unknown service, schema, topic, or QoS; reject mandatory QoS
capabilities missing in a vendor; reject stale and malformed keyed samples.
Unknown delivery outcomes fault the publisher and must never trigger automatic
device-command replay. A Pending business Command is not a terminal UMAA
CommandStatus. All in-process module termination is cooperative; a crashing
device module can crash its host.

**Do not call this SDK fully UMAA conformant or production-ready.**
