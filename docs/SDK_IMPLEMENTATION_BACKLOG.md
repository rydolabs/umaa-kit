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
| Fast DDS backend facade | PASS (synthetic Report only) | Ubuntu Fast DDS 2.11 independent-process [wire run 40810503339](https://github.com/rydolabs/umaa-kit/actions/runs/40810503339) SUCCESS; official UMAA mappings not complete |
| Identical client and module binaries across vendors | PASS (synthetic Report only) | [parity run 40810503302](https://github.com/rydolabs/umaa-kit/actions/runs/40810503302) SUCCESS: same Consumer executable + same device .so, separate Cyclone/Fast DDS backends |
| LargeSet / LargeList | PARTIAL | CollectionAssembler core only; group coherent/ordered DDS plus recovery not verified |
| Official UMAA 6.0 TypeBindings | BLOCKED ON CONTRACT REVIEW | source IDLs compiled previously; no approved all-service roles, keys and QoS |
| Cross-vendor mixed-network operation | NOT PROVEN | same-binary parity is not Cyclone-to-Fast DDS mixed-network interoperability |
| RTI Connext backend | BLOCKED | licensed SDK, runner, TypeSupport and policy verification required |
| Safe module stop/reload | PARTIAL | in-process ABI checks and failed-stop mapping retention; pending-session draining and crash isolation still absent |
| Linux/Windows regressions | PASS [Actions run 40810503199](https://github.com/rydolabs/umaa-kit/actions/runs/40810503199) | GCC/Clang/MSVC success on `afa5b269`; final docs-only commit recheck required |
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

## 2026-10-29 verified multi-vendor development slice

Commit `afa5b26960f7a0353d172a40355ef87d6d4f5311` completed all six
GitHub-hosted workflows: Linux/Windows SDK, Verifier, legacy synthetic
Cyclone DDS, new Cyclone plugin, new Fast DDS plugin, and same-binary
vendor-parity test. The suite covers **only synthetic DemoBattery** in the
new plugin. It does not cover official UMAA ServiceTraits, COMMAND wire,
LargeCollection coherent DDS, mixed Cyclone/Fast network interoperability,
physical equipment or RTI Connext. No formal conformance approval is implied.

## 2026-10-29 module command-drain hardening

Module ABI v2 host admission, session tracking and cooperative teardown are
updated: all in-flight ABI invocations drain before successful `stop()`;
Pending device sessions reject unload and duplicate command replay;
completion/cancellation permits subsequent stop. Per-service session identity
avoids same-number collisions. An ambiguous exception retains the session
reservation (fail closed). New CTest verifies the scoped safety behavior.

**Still OPEN:** true out-of-process crash isolation, supervisor restart,
active DDS CommandStatus/Ack sessions and formal UMAA conformance.

## 2026-10-29 in-flight ABI lifetime test

An independent native DLL/SO fixture holds a device callback for 180ms and
signals when the callback is actually inside module code. The host's stop
must block until this invocation finishes. The fixture also injects a failing
stop and checks one-shot quarantine (no second stop, no new invocation).
A `Pending` command still blocks unload, but a *reserved in-flight* command
is first allowed to finish before determining whether it remains Pending.
Local Linux validation passed; GitHub Linux/Windows CTest is the acceptance
gate. In-process crashing-module isolation remains OPEN.

## 2026-10-29 callback-style Report consumer

Added callback-based `ReportConsumer::on_change(on_report,on_dispose)`
and `stop_watch()` lifecycle. Events are keyed and delivered serially on a
private thread, without user DDS Reader/Writer code. Equal-timestamp replays
are dropped. CTest covers two updates and Dispose via the mock Backend.
**OPEN:** official per-service Report TypeSupport, QoS mapping, full DDS
reader recovery, resource budgets and production concurrency/fuzzing.

## 2026-10-29 Fast DDS intermediate ACK behavior

Fast DDS 2.11 sometimes returns an error on a *mid-stream* reliable ACK
query even though the independent subscriber receives the data and Dispose.
The synthetic Report smoke now records this query diagnostically, then
requires the **final ACK after Dispose** to succeed before PASS. This mirrors
`ReportPublisher::close()` and does not replay any command or sample.
Final ACK failure remains FAIL; this does not assert official UMAA QoS.

## 2026-10-29 ReportProvider scheduled update

A bounded, cancellable Report sampling thread supports persistent Writer
updates from business-only `ReportSource::read()` and fail-closed async
errors. Mock backend CTest verifies repeated updates and Dispose on teardown.
**OPEN:** official service-specific scheduling policies, Startup/Recovery
QoS and full multi-service performance qualification.

## 2026-10-29 cooperative Component reload (development)

`ComponentRuntime::unload_component(module_id)` now rejects active Pending
device Command sessions and retained ReportPublisher ownership. It gates
new service calls during the stop sequence and requires a successful
`ComponentModule::stop()` before removing service registry entries.
Afterwards the same DLL/SO can be reloaded as a fresh component instance.
CTest covers negative pending/publisher gates, unload and reload.

**Limit:** this is a cooperative in-process reload, not automatic hot-upgrade
with zero downtime, process crash isolation or supervisor-controlled IPC.
The module administrative API must be serialized by the host orchestrator.

## 2026-10-29 typed Application Report publishing

Added public `Application::publish<Service>(key)` and
`Application::unload_component(id)`, avoiding internal Runtime APIs and
manual Topic strings for user-level Report publishing and cooperative reload.
CTest covers background publish to typed consume and refuses to unload an
active module publisher. **Scope:** synthetic `demo:battery-v1` only; no
normative UMAA service mapping or lifecycle claim.

## 2026-10-29 experimental mixed vendor results

Strict cross-vendor network tests are **NOT PROVEN**:
[Actions 40831127878](https://github.com/rydolabs/umaa-kit/actions/runs/40831127878)
saw FastDDS→Cyclone data/Dispose but no confirmed writer final ACK, and
Cyclone→FastDDS subscriber missed the required full data/Dispose condition.
This must not be conflated with same-binary vendor *substitution*, which
passed. Reproduction and exact scope:
[Mixed network evidence](SDK_DDS_MIXED_NETWORK_LIMITS.md).

## 2026-10-29 independent remote-only Report Consumer

Removed the incorrect requirement that a network Consumer also load a local
Provider device DLL. Real DDS Consumer fixtures now subscribe with the Backend
plugin alone, and a new mock CTest guards remote-only subscription.
Backend TypeBinding/QoS validation remains mandatory and fail-closed.
Official per-service Provider/Consumer roles require reviewed generated
ServiceTraits; this demo does not establish that acceptance.
