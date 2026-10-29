# UMAA SDK implementation ledger — PR #15

**As-of:** 2026-10-29. This document records development-scope results and
required acceptance gates, never a conformance certificate.

| Capability | State | Evidence and unresolved gate |
|---|---|---|
| Simple typed Provider Component module | PASS (prototype) | C ABI v2 DLL/SO, module loader, Linux/Windows tests; official service runtime incomplete |
| Typed remote CommandHandle | PASS (synthetic real DDS); PARTIAL official | Host `issue_remote` completes/cancels/updates after actual Status+Ack; official UMAA TypeSupport/QoS remain unapproved |
| Report Provider lifecycle | PASS (synthetic development scope) | persistent writer, multiple updates, dispose, acknowledgment, fault fencing; no official all-service policy |
| Per-key Report cache | PASS (mock development scope) | independent keys, per-key timestamp and tombstone; vendor lifecycle proof separately required |
| Cyclone DDS backend facade | PASS (synthetic Command, Report, Collection) | Independent-process real DDS tests; official type coverage incomplete |
| Fast DDS backend facade | PASS (synthetic Command, Report, Collection) | Real Fast DDS 2.x wire tests; GROUP Presentation not implemented/verified |
| Identical binaries across vendors | PASS (synthetic) | Command provider/client and Collection Set/List same binaries on Cyclone or Fast DDS; not mixed network |
| LargeSet / LargeList | PASS (single-sample development fallback); BLOCKED native GROUP | Bounded frame codec, Set/List snapshot/revision/Dispose, explicit recovery, both vendors real wire; native multi-writer coherent/ordered absent |
| Official UMAA 6.0 TypeBindings | BLOCKED ON CONTRACT REVIEW | source IDLs compiled previously; no approved all-service roles, keys and QoS |
| Cross-vendor mixed-network operation | FAIL (strict) | [Mixed Collection test 40885883191](https://github.com/rydolabs/umaa-kit/actions/runs/40885883191): Fast writer ACK unconfirmed and Fast reader cannot recover foreign Dispose key |
| RTI Connext backend | BLOCKED | licensed SDK, runner, TypeSupport and policy verification required |
| Safe module stop/reload | PASS development isolation; PARTIAL production | Cooperatively drain in-flight calls; isolated worker/supervisor with crash quarantine, explicit restart and no automatic command replay; secure durable supervision open |
| Linux/Windows regressions | PASS in prior validated head; recheck final commit | CMake CTest and Python on Linux GCC/Clang/Windows MSVC; final CI must pass |
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

## 2026-10-29 configuration facade

Added `Application::from_config(path)` for a strict middleware/backend/domain
plus optional Component DLL list. Consumer-only startup requires **no local
Component**. Parser checks duplicates, input size, domain range, indentation,
and unknown fields before loading. CTest tests valid and invalid configs.
Official service mapping, richer YAML, security configuration and production
supervision remain open.

## 2026-10-29 collection and remote Command receiver hardening (development)

The public SDK now has two independently testable **vendor-neutral** building
blocks; neither is a completed official UMAA DDS runtime:

- `collection_transport.hpp`: a strict LargeSet/LargeList coherent-batch
  receiver. It stages changes in a private assembler and exposes only complete
  revisions; bounds mutation counts, rejects sequence gaps, stale elements,
  invalid metadata, cycles and ambiguous delivery. Recovery requires an
  explicitly trusted newer-generation full snapshot. The DDS vendor adapter
  still must provide actual coherent/ordered samples, a verified snapshot and
  service-specific QoS; absent those capabilities, official LargeCollection
  remains **PARTIAL / NOT READY**.
- `remote_command_monitor.hpp`: provider/session-scoped CommandStatus and
  CommandAck receiver. Terminal result requires both real ACK and valid,
  timestamp-ordered terminal Status. A dropped ACK, invalid transition or
  ambiguous delivery cannot become a fabricated success. The Component's
  local pending-command mechanism is separate; actual DDS Command/Ack/Status
  writer/reader TypeSupport binding remains **OPEN**.

New tests: `umaa_coherent_collection_receiver_test` and
`umaa_remote_command_monitor_test` (C++17, Linux and Windows). These exercise
transaction/recovery and correlation only, not normative UMAA wire behavior.

## 2026-10-29 Command host bridge and DDS ABI event writer

Additional development functionality:

- `CommandEngine` provides identity-aware optional device callbacks.
- `ComponentCommandProvider<Service>` drives the real Component v2
  business callback through SDK-owned session, Status and Ack transitions.
- `DdsCommandEventTransport<Service>` publishes a declared, explicitly
  codec-bound status/ack stream through `UmaaDdsBackendV1`. It fences
  unknown DDS delivery and refuses automatic replay.
- A mock DDS Reader round-trip feeds timestamped Status/Ack into
  `RemoteCommandMonitor`; terminal success cannot be inferred from Status
  without a matching ACK.
- Multi-key Report `latest()` correctly skips a disposed last-event key.

**Acceptance remains PARTIAL** until the complete remotely issued Command
writer/reader + reviewed official per-service TypeSupport + real Cyclone and
Fast DDS CommandStatus/Ack session tests pass. The new tests are *host-local*
and *mock-transport* evidence, not official UMAA protocol conformance.
`CoherentCollectionReceiver` does not substitute for actual vendor
GROUP-coherent and ordered access.

## 2026-10-29 genuine DDS synthetic Command vertical slice

The new `dds/cyclone/idl/DemoCommandEvent.idl` fixture and two **separate**
Command backend plugins (Cyclone DDS and Fast DDS) connect the common
Component business handler through `CommandEngine` and the DDS Backend
Facade. Independent-process subscriber verification requires exactly four
ordered Status data samples (ISSUED, COMMANDED, EXECUTING, COMPLETED), one
ACK data sample and keyed Dispose of both response topics. The client-side
`RemoteCommandMonitor` correlates actual source timestamps and ACK/Status
to the initiating session. The same command executable + device Component
module are also exercised against both DDS vendors with only the backend
library swapped. No official UMAA conformance is claimed.

**Still incomplete:** incoming official UMAA DDS Command Reader and
client-issued Command Writer, validated status/ack IDL and QoS, correlation
across update/cancel/recovery, full service registry TypeBindings, and
mixed-vendor network interoperability. Do not upgrade the full
CommandClient status beyond PARTIAL based on this fixture.

## 2026-10-29 minimal remote typed Command client (synthetic)

Added `RemoteCommandClient<Service>` with `send`, `poll`,
`wait_for`, `result`, and `finish`. It opens the Command Writer and
Status/Ack Readers inside the SDK, checks decoded Ack against the sent
Command, correlates provider/session/timestamps, requires terminal Ack+Status
before disposal, and waits for the two response disposals. Invalid
samples, transport failures and ambiguous delivery fault the channel.

The independent **real DDS** Cyclone and Fast DDS command fixtures test a
remote client with no vendor DDS imports. A separate same-binary test swaps
the vendor backend without recompiling the typed client or Component.

**Scope and missing work:** synthetic IDL only; the original
`Application::consume<Service>()` Command API is still local. Official
service descriptor mapping, request update/cancel/recovery, mature process
supervision, QoS and all-service conformance are not implemented or certified.

## 2026-10-29 residual work implementation: cancel, update, isolation, collections

**Real synthetic DDS Command cancellation / update:** added
`RemoteCommandClient::request_cancel`, `acknowledged`, `ready_for_update`,
and `update`. Cancellation requires a **real** device-stop outcome,
terminal `CANCELED` Status and original Ack; no success is inferred
from Command Dispose alone. A new Command revision is admitted only after
the prior `EXECUTING` Status and ACK. The provider's handler receives
session identity and confirms hardware cancellation before updating work.
Each vendor's separate-process Command suite now covers completion,
early cancellation and in-flight revision. Evidence:
[Cyclone](https://github.com/rydolabs/umaa-kit/actions/runs/40872063613),
[Fast DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40872063639).
This is synthetic IDL, **not official UMAA conformance**.

**Module process isolation (optional development):**
`umaa_component_ipc_worker` runs a C ABI v2 device module in a child
process; `umaa_process_supervisor.py` validates bounded IPC replies,
quarantines crashes/unknown outcomes, never replays a pending Command, and
requires explicit hardware reconciliation before restart. Tests exercise
ordinary calls, pending-session unload rejection, intentional child crash,
and fresh instance startup. The worker IPC is not authenticated or hardened
for hostile participants, and exact hardware state reconciliation is an
**external operational requirement**. Not yet production-grade hot reload.

**LargeCollection:** `CollectionBatchBuilder<T>` constructs bounded
Set/List snapshots, keyed disposals, ordered revisions, and resync
generations; `CoherentCollectionReceiver<T>` rejects incomplete
revisions and sequence gaps. The core now commits an empty collection
after the last keyed element's Dispose. This is an atomic **SDK-side
transaction model**, not implementation of DDS GROUP coherent/ordered
presentation or official LargeSet/LargeList wire metadata. That gate
remains PARTIAL.

**Remaining uncompromised gates:** official UMAA service-specific
TypeSupport/Key/QoS and reviewed normative profile; real vendor-backed
GROUP coherent/ordered DDS transactions and restart recovery; production
process supervisor with authentication, request deduplication, persistent
session reconciliation and crash isolation across all supported platforms;
actual Cyclone↔Fast DDS mixed-network interop; RTI Connext licensed SDK
and independent Runner; independent formal conformance oracle. These
are not satisfied by synthetic or mock test passes.

## Vendor capability blocker: verified GROUP Presentation QoS

**Fail-closed decision for Fast DDS 2.11 and the current Cyclone fixture:**
both synthetic command adapters advertise only `RELIABLE` and
`INSTANCE_LIFECYCLE`. No `COHERENT_GROUP` or `ORDERED_ACCESS` capability
is advertised, and independent real DDS tests explicitly reject mandatory
GROUP+ORDERED requests. A successful synthetic serialized batch or mock test
**does not prove** DDS coherent presentation across multiple writers.

Fast DDS official documentation states that `PresentationQosPolicy` can
be configured but is **not implemented** in multiple 2.x versions
([Fast DDS 2.14.6 API reference](https://fast-dds.docs.eprosima.com/en/v2.14.6/fastdds/api_reference/dds_pim/core/policy/presentationqospolicy.html)).
The 2.11 library used in CI must therefore NOT claim GROUP coherent support
without new vendor implementation and independent tests. Cyclone DDS exposes
`dds_begin_coherent`/`dds_end_coherent` as described in
[its C API](https://cyclonedds.io/docs/cyclonedds/latest/api/pub.html),
but the current Facade does not provide a coherent transaction handshake or
approved official UMAA collection TypeBinding. This is an actual remaining
architecture and vendor-capability gate, not an issue that a mock test can
close.

## 2026-10-29 async Handle ownership acceptance

Introduced `RemoteCommandHandle<Service>` and the one-step
`Application::issue_remote<Service>()` path. A Handle retains an independent
DDS Domain/Backend until explicitly finished, even after the creating
`Application` is destroyed. It exposes Status/Ack-correlated polling,
bounded waiting, guarded in-flight update, confirmed cancellation and
checked final Dispose. A released Command is not re-issued implicitly.
Both synthetic real DDS plugins and the identical-binary workflow run
the new Handle-based client. **No full official UMAA type/QoS or
production recovery/RTI claim** is made.

## Final continuation scope: real typed Host, single-sample Collection, package

- `DdsCommandRequestServer<Service>` owns DDS Request Reader and business
  Command FSM, publishes correlated real Status/Ack and safely retires
  instances. `Application::start_remote<Service>()` runs the provider
  automatically on a joined thread, while `issue_remote<Service>()`
  returns the lifetime-safe async Client Handle. Cyclone and Fast DDS
  independent-process completion, early cancel and in-flight update pass.
- `CollectionFrameCodec<Service>` validates a 64-KiB, versioned, bounded
  whole-revision Set/List envelope (rejecting wrong kind, malformed length,
  stale sequence and unknown generation). `DdsCollectionPublisher` and
  `DdsCollectionConsumer` expose explicit generation recovery. The vendor
  plugins map an IDL `sequence<octet,65536>` and keyed Dispose through
  genuine DDS; same-binary swapping for both vendors PASS.
- The process worker has a separately installable **release** target without
  its test-only crash injection. The Python Supervisor remains a development
  IPC implementation requiring explicit physical reconciliation for restarts.

**Strict mixed network FAIL:** [40885883191](https://github.com/rydolabs/umaa-kit/actions/runs/40885883191).
Fast DDS publisher samples and Dispose reach Cyclone, but final reliable
Writer ACK remains unconfirmed. Cyclone publisher to Fast DDS subscriber
fails because Fast DDS 2.x does not supply a trustworthy instance key on
the foreign Dispose sample (reader InstanceHandle does not match the
stored data handle). DO NOT substitute a guessed key or claim parity. A
supported DDS version with verified `get_key_value`, XTypes compatibility,
and a strict two-way ACK+Dispose test is required.

**Official UMAA 6.0 remains BLOCKED:** full reviewed topic TypeSupport,
Component Definition roles, Key identity rules, QoS GROUP Presentation,
normative state-table traceability and independent conformance oracle
are not approved. RTI Connext additionally needs a licensed SDK and Runner.
No tests in this section prove official conformance.

## 2026-10-29 lossy-netns Writer retirement hardening

After the `6908153687` documentation-head network regression, the
3-node official BatteryStatus degraded `tc netem` scenario (40ms±10ms,
1% loss) observed 2/3 events: both data updates, but missing terminal
Dispose on one Reader, despite the Writer-side ACK/stop returning success.
The host's `ReportWriter` now accepts an explicit bounded Writer
retirement grace (0–5000 ms; default **zero**), configured to 1000 ms only
for opt-in impaired namespace tests. It retains the real DDS Writer after
the final reliable ACK so late RTPS lifecycle repair remains possible;
it never synthesizes Report data/Dispose or bypasses the independent
subscriber's exact 3/3 event check. This is demo scheduling policy,
not official UMAA QoS/recovery certification.
