# Previous repository README — preserved implementation history

The following text is the root README before the Component-platform landing-page redesign. For current feature status, consult the root README and individual verified evidence documents; historical assertions retain their original date/scope.

---

# UMAA Kit — C++ SDK and Independent Simulator

**Active milestone scope (2026-10-30):** independent **Cyclone DDS** and
**Fast DDS** deployments, including unchanged-binary plugin substitution.
Mixed Cyclone↔Fast DDS network communication and RTI Connext adapter /
independent validation are **future objectives only**, excluded from current
implementation and acceptance. Historical mixed-network failures remain
recorded (not silently passed); RTI is not verified. This change does not
establish formal UMAA compliance or production readiness.
[Current-phase scope and acceptance](docs/CURRENT_PHASE_SCOPE.md).

**Canonical repository:** https://github.com/rydolabs/umaa-kit  
**Status:** experimental UMAA 6.0 implementation subset; **not** official
UMAA/K-MOSA conformance or a complete 53-service runtime. Prior
`nssang00/umaa-dev` links below are historical evidence, not the canonical
repository. See [release gates](docs/DELIVERY_GATES.md), [the SDK quick start](docs/QUICKSTART.md)
and the [open implementation roadmap](https://github.com/rydolabs/umaa-kit/issues/2).


## Current modular SDK development gates — 2026-10-29

**Verified synthetic real DDS:** a user Component DLL/SO implements only
`provide<Service>(handler)`. `Application::start_remote<Service>()`
automatically polls incoming Command requests and drives the SDK-owned
Status/Ack FSM on a background worker. `Application::issue_remote<Service>()`
returns a typed `RemoteCommandHandle` with completion, cancellation and
guarded update. The typed `publish_collection/consume_collection` paths
transfer a **whole bounded Set/List revision in one reliable DDS sample**,
validating generation, sequence, disposal and resync. Both Cyclone DDS and
Fast DDS vendor plugins have real-wire, independent-process tests; the
same physical client, provider and device modules run on either vendor.

**Not complete:** the Collection frame is an application-level atomic
fallback, **not** DDS native GROUP Coherent/Ordered Presentation. Reviewed
official UMAA 6.0 TypeSupport plus role/key/QoS contract remains a separate
approval gate. Mixed Cyclone/Fast network interoperation failed strict
ACK/Dispose checks on actual GitHub-hosted Runners, and RTI Connext requires
a licensed SDK. Optional process isolation is a development IPC tool,
not production-grade durable hardware recovery.

See [current implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md),
[mixed-network failures](docs/SDK_DDS_MIXED_NETWORK_LIMITS.md) and
[how to implement another backend](dds/vendors/ADDING_A_BACKEND.md).

## Modular SDK architecture (MVP, 2026-10-29)

**Design rule:** applications only register UMAA service handlers. DDS Reader/Writer,
Command/Ack/Status and QoS belong to the internal runtime, not device code.
A module can expose several services through one DLL/SO; vendor backends are
separate DLL/SO plugins behind a single versioned C ABI.

| Binary / source | Responsibility | Current implementation |
| --- | --- | --- |
| `UMAA::Core` | command/report/collection protocol core | Existing C++17 INTERFACE library |
| `UMAA::Runtime` | shared host runtime and ABI loaders | **Built DLL/SO**, development orchestration |
| `vehicle_component` DLL/SO | user-created Engine/Battery typed Handler(s) | **Built** from synthetic fixture |
| `dds_backend.h` | **single DDS vendor contract** | Versioned C ABI, capabilities/fail-closed |
| `mock_backend` DLL/SO | ABI/loader contract regression | **Test-only**, NOT real DDS |
| Cyclone DDS plugin | synthetic BatteryStatus through the new ABI | **PASS for scoped synthetic real-wire**, official service migration open |
| Fast DDS / RTI plugins | new-vendor ABI without app/core changes | **Fast DDS synthetic real-wire PASS**; RTI SDK/license blocked |

See [full architecture, DLL/SO ownership and new-vendor instructions](docs/SDK_MODULAR_ARCHITECTURE.md),
[implementation/evidence scope](docs/MODULAR_IMPLEMENTATION_EVIDENCE.md), and
[existing delivery gates](docs/DELIVERY_GATES.md). This modularization milestone
**does not claim official UMAA 6.0 conformance**. Synthetic Report vendor swapping is verified; full-service switching is not.

Two **separate projects** for development and independent verification of UMAA services:

- [sdk/](sdk/): strict synthetic-subset generator and **official-EngineControl-derived ignition Handler generator**, Command/Report/Collection prototype, versioned native service module loader (DLL/SO), installable CMake package and tests.
- [simulator/](simulator/): independent scenario runner and protocol trace verifier. Baseline synthetic fixture exchanges JSON over stdio. Additional native DDS simulator programs in `simulator/dds/` independently receive samples and Dispose via **real Cyclone DDS**.

## Where things live

The stable implementation directories are `sdk/` (vendor-neutral core),
`dds/cyclone/` (real DDS transport), `simulator/` (independent verification),
`catalog/` (reviewed contracts), and `tools/` (build and audit utilities).
Start at the [documentation index](docs/README.md) and the
[complete repository map](docs/PROJECT_LAYOUT.md) for file ownership,
verification evidence, and generated-data rules.

## Cross-platform GitHub CI — executed

| GitHub-hosted runner | Result |
| --- | --- |
| Ubuntu 24.04 / GCC | PASS |
| Ubuntu 24.04 / Clang | PASS |
| Windows Server 2022 / MSVC | PASS |

**[Full SDK + Simulator Actions run](https://github.com/nssang00/umaa-dev/actions/runs/39962717654)**: all three matrix jobs passed, verified from GitHub-hosted job logs. Each job performed 9 Python codegen unit tests, 6 independent simulator oracle unit tests, a CMake build, CTest **4/4**, CMake install and external consumer build, and four cross-process simulator scenarios (**32 observed events total**). A separate earlier smoke workflow also verified native DLL/SO modules on the same three environments.

## Local reproduction

Requires Python 3.10+, CMake 3.20+, a C++17 compiler:

```sh
python tools/ci_verify.py --compiler default
# Linux/Clang only:
python tools/ci_verify.py --compiler clang
```

The verifier writes command logs and JSON summaries under `evidence/ci/<platform>/<compiler>/`, while the GitHub workflow uploads job artifacts.

## CycloneDDS real-wire proof (official and synthetic)

- [Linux official AUVSI UMAA 6.0 BatteryReportType + SO module → Cyclone DDS → separate Simulator test](https://github.com/nssang00/umaa-dev/actions/runs/39952219497) — PASS.
- [Windows vcpkg Cyclone DDS 0.10.5 + DLL plugin + synthetic test IDL → separate Simulator](https://github.com/nssang00/umaa-dev/actions/runs/39952395220) — PASS.
- [Windows official UMAA 6.0 BatteryReportType + DLL module + live DDS + separate Simulator](https://github.com/nssang00/umaa-dev/actions/runs/39953508112) — PASS.
- **[Official EngineControl Command/Ack/Status, Cancel/Dispose, later-Timestamp UPDATE](https://github.com/nssang00/umaa-dev/blob/main/docs/ENGINECONTROL_EVIDENCE.md)** — Linux CTest 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962602416); Windows CTest 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962717265).
- **[All 352 original AUVSI 6.0 IDLs idlc-generated and C TypeSupport compiled](https://github.com/nssang00/umaa-dev/actions/runs/39963083783)** — 352/352; this is NOT 352 generated service APIs or compliance certification.
- [Official AUVSI IDL archive verification and Engine Command/Status TypeSupport compilation](https://github.com/nssang00/umaa-dev/actions/runs/39952552116) — PASS.

The official IDL ZIP is downloaded to ephemeral CI space for each applicable run; it is **not committed or redistributed**. UMAA IDL TOU must be reviewed for any downstream licensing/distribution.

**Conformance boundaries:** the BatteryStatus adapter currently uses manual C TypeSupport glue and a shortened DDS topic string for testing, with some metadata fields unset. It is a valid on-wire **type and lifecycle proof**, *not an UMAA ICD service conformance verdict*. LargeCollection QoS, startup/recovery, timestamps, service addressing, official Topic naming, CycloneDDS-CXX parity, and EngineControl **all-field/full-conformance** coverage remain open.

## Scope / limits

This repository contains a **technology prototype**, not a complete UMAA 6.0 implementation or official K-MOSA/UMAA certification. `sdk/fixtures/demo.idl` is synthetic, the generator implements a deliberately restricted IDL subset, and the independent simulator uses a test-fixture stdio protocol. **Real Cyclone DDS tests are now available:** Linux and Windows synthetic-IDL module-backed cross-process samples; **both Linux and Windows** official AUVSI UMAA 6.0 `BatteryReportType` compiled from downloaded IDL and verified over DDS with an independent simulator (20→80→Dispose). Official `EngineCommandType`, `EngineCommandStatusType` and `EngineCommandAckReportType` are now used in live real DDS integration tests for command, cancellation and update on **both** operating systems. The generated user module API is limited to the ignition-state field. RTI Connext DDS and mixed-vendor tests have not been executed. The generic IDL generator is still restricted to synthetic types. A separate fail-closed official `EngineControl` subset generator now creates a DDS-free user Handler and C ABI plugin glue. The native module ABI is project-defined, not an official K-MOSA ABI, and does not implement live hot swap of active commands.

Read [delivery gates](docs/DELIVERY_GATES.md), [architecture](docs/ARCHITECTURE.md), [real DDS evidence](docs/REAL_DDS_EVIDENCE.md) and [validation evidence](docs/CROSS_PLATFORM_VALIDATION.md) before treating any subset as production-ready.


## 2026-10-26 P1 Cyclone writer checkpoint (local only)

A reusable CycloneDDS C CommandStatus/Ack writer adapter and an EngineControl
example-host integration are now in the tree. CMake can optionally export the
`UMAA::Cyclone` target (`-DUMAA_ENABLE_CYCLONE_ADAPTER=ON`) when CycloneDDS
is installed. Adapter behavior was checked
using a test-only DDS ABI mock. **No official DDS or Windows CI run has
validated these new changes yet.** For precise evidence, scope, and remaining
acceptance criteria see `docs/implementation/P1_CYCLONE_ADAPTER_STATUS.md`.

## Latest structure baseline

The repository layout is documented at [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md).
The independently verified P1 Reader/Writer checkpoint is [PR #3](https://github.com/nssang00/umaa-dev/pull/3)
(commit `cb9a87f`); older sections above remain as dated historical notes.
Structure maintenance does not expand the current UMAA service-conformance scope.

## P2 Cyclone Command input Reader (2026-10-27)

`dds/cyclone/include/umaa/cyclone_sample_reader.hpp` provides a reusable
RAII sample Reader shared by Command and Report; the official EngineControl
host consumes its Command input through `CommandReader`. A rejected command,
DDS failure or exception faults the epoch and releases sample-owned data.
Refer to [implementation notes](docs/implementation/P2_COMMAND_READER_STATUS.md).
This remains an EngineControl ignition-only and BatteryStatus Report subset,
not complete UMAA service coverage.

## P3-reviewed service entity guard (development checkpoint)

`ServiceContext` validates the logical ICD Catalog service, Topic, TypeSupport
*identity supplied by caller*, and DDS direction before allocating Cyclone DDS
Topics. The generated 53-service/113-Topic allow-list is checked in CI and
**strict mode refuses all currently unreviewed per-service QoS**. Existing
BatteryStatus/EngineControl vertical slices explicitly use `PrototypeOnly`;
this does not confer UMAA normative QoS conformance. See
[details](docs/implementation/P3_SERVICE_CONTEXT_STATUS.md).

## P3 exact-source service-binding proof (prototype, 2026-10-27)

`umaa::cyclone::ServiceContext` now checks the generated **53-service / 113-Topic**
Catalog identity and role before allocating actual CycloneDDS Topics. It is
fail-closed for every current unreviewed normative QoS profile. Two *additional*
official Topic types, `AnchorSpecsReportType` and `AnchorCommandType`, passed
independent Linux/Windows discovery and default-valued DDS message transport.
**This is a transport/type smoke, not full service behavior**: AnchorSpecs
semantic Report values, AnchorControl execution/Ack/Status/Dispose and required
QoS remain unimplemented. See the [P3 evidence](docs/implementation/P3_SERVICE_CONTEXT_STATUS.md)
and [open gates](docs/DELIVERY_GATES.md).

## P4 — Anchor semantic DDS subset (2026-10-27)

The official `AnchorSpecsReportType` publishes two distinct non-zero
`anchorHoldingPower`/`rodeLength` samples, with verified UUID/timestamps,
then DDS Dispose. An **independent C Simulator** checks exact values and
sequence. `AnchorControl` uses real `AnchorCommandType`, Status and Ack
TypeSupport with virtual LOWER, RAISE, STOP actions, later-timestamp UPDATE,
Command Dispose cancellation, Status/Ack/Dispose and rejected Destination.
All these tests run against pinned official original UMAA IDL in Linux and
Windows GitHub Actions (source commit `5ad1131`; merged PR
[#6](https://github.com/rydolabs/umaa-kit/pull/6)).
The new field values are fixtures, **not** normative units or a real device.
See [precise verification limits](docs/implementation/P4_ANCHOR_SEMANTIC_WIRE.md)
and [delivery gates](docs/DELIVERY_GATES.md).

## P4.1 Anchor semantic values and virtual terminal protocol evidence

Verified merged-main commit `d59612c`: [Linux real DDS 22/22](https://github.com/rydolabs/umaa-kit/actions/runs/40451709970)
and [Windows real DDS 21/21](https://github.com/rydolabs/umaa-kit/actions/runs/40451709996).
Includes non-default AnchorSpecs report data with update/Dispose and independently
asserted AnchorControl Ack/Status, LOWER/RAISE/STOP Update, Cancel, COMPLETED,
FAILED/OBJECTIVE_FAILED and terminal Dispose. COMPLETED/FAILED are explicitly
injected **virtual-device outcomes**, not physical device proof; full normative
ICD roles, QoS, security and RTI interoperability remain open.

## Independent Verifier V1 (offline, 2026-10-28)

`verifier/` is a **separate CLI verification orchestration product**, built
around the existing external `simulator/` oracle and process supervisor, not
a replacement for it. It supports registered test profiles, batch suites,
scoped evidence, file digest consistency checks and an offline HTML results
viewer. This increment verifies **four development-only stdio fixture scenarios**
with the existing built C++ host; its results are **not official DDS or UMAA
conformance**. See [Verifier quick start](verifier/README.md) and
[acceptance matrix](docs/verifier/ACCEPTANCE_MATRIX.md).

## Verifier V3 and Studio QEA PoC (local 2026-10-28)

`verifier/` now re-evaluates independent simulator PASS markers and retains a
self-contained stdio scenario snapshot for stronger offline evidence consistency.
Five scoped DDS suite definitions are prepared, but **no local real DDS test
was performed**. See [V3 acceptance scope](docs/verifier/V3_INCREMENT.md).

`studio/` begins with a **read-only QEA SQLite model extractor and GUID-based
diff prototype**. Tests use only synthetic EA-like repositories; real QEA,
PostgreSQL, DOCX/IDL generation and Web UI remain unverified or unimplemented.
See [Studio scope](studio/README.md) and [remaining gates](docs/UMAA_ROADMAP_STATUS_V3.md).

## Local V4 extension — Studio documents and Verifier association (2026-10-28)

New `studio/` is an **offline development prototype**, not a PostgreSQL/React
Studio product. V4 adds draft DOCX model inventory generation and SHA-256
manifests from a frozen QEA snapshot, plus a **non-normative administrative**
model-to-Verifier-export association. See [Studio CLI](studio/README.md) and
[V4 status/gates](docs/UMAA_ROADMAP_STATUS_V4.md). Verifier and Simulator
continue to operate independently. No real Sparx QEA acceptance, live Cyclone
DDS, normative conformance, or PostgreSQL operation is claimed for V4.

## Local V5 increment — offline EA-snapshot project extension IDL draft

`studio/` now includes a **limited, fail-closed** model-pinned IDL struct draft
generator with deterministic regeneration checks and an optional Cyclone
`idlc` syntax gate. It does **not** rewrite or supersede the official UMAA IDL.
See [Studio scope and commands](studio/README.md) and
[V5 acceptance scope](docs/UMAA_ROADMAP_STATUS_V5.md).


## SDK module facade phase 2 — typed local consumer

The experimental `umaa::Application` in `sdk/include/umaa/application.hpp` adds a
small `consume<Service>()` API over an ABI-v2 device component for **host-local**
calls. `CommandHandle::wait()` refuses a Pending result until an actual
UMAA ACK/Status tracking implementation is attached. `read_local()` is not
DDS subscription; see [scope and remaining gates](docs/SDK_FACADE_PHASE2.md).
All official UMAA vendor, QoS, runtime and conformance gates remain open.


### Phase 2 — typed Report consumer over an independent DDS plugin

The hosted [modular Cyclone plugin real-wire test](https://github.com/rydolabs/umaa-kit/actions/workflows/sdk-dds-backend-wire.yml)
now compiles a **separate** `libumaa_dds_cyclone_demo.so` and user component
`libumaa_cyclone_vehicle_component.so`. A publisher process uses the device
component's synthetic BatteryStatus Handler, while a second process uses
`Application::consume<BatteryStatus>().poll()/latest()` with **no DDS include**
to observe a keyed Report and subsequent Dispose.

This is **synthetic demo:battery-v1**, not official UMAA TypeSupport, not a
full multi-service Cyclone adapter, and now verified across Cyclone/Fast DDS using identical client/device binaries; RTI remains unverified.
The true UMAA Command ACK/Status async client, official service binding
generation and safe device hot-swap remain open. See
[SDK Facade Phase 2](docs/SDK_FACADE_PHASE2.md).

## SDK implementation gates (active PR #15)

The SDK uses a **device Component DLL/SO** loaded by a shared UMAA runtime and a
separate **DDS backend plugin DLL/SO**. User device code implements a typed
`Component::provide<Service>(handler)`; the loader owns the module ABI v2.
The host-side `Application::consume<Service>()` currently supports local
Command results and typed synthetic Report subscription. DDS vendor code is
isolated under `dds/vendors/<vendor>/`, behind
`sdk/include/umaa/internal/dds_backend.h`. New vendors MUST NOT change
application handlers or the common protocol FSM.

**Proven:** Linux/Windows SDK builds, device-module load/unload ABI, Cyclone DDS
synthetic BatteryStatus cross-process wire with value/Dispose, and scoped
legacy official service tests. Development-only Report Writer reuse and
per-instance cache are covered by CTest.
**Still open:** actual UMAA Command/Ack/Status consumer wire, approved
TypeSupport/QoS for all scoped services, coherent LargeSet/LargeList,
production module isolation, RTI licensed validation and normative conformance.
See [module architecture](docs/SDK_MODULAR_ARCHITECTURE.md) and the
[implementation evidence and blockers](docs/SDK_IMPLEMENTATION_BACKLOG.md).
Never infer formal compliance from synthetic IDL or service-name catalogs.

## SDK v2 modular runtime — verified 2026-10-29 development increment

The **same user Component DLL/SO** and the **same typed Consumer binary**
can use the Cyclone DDS or Fast DDS backend without recompilation for a
synthetic BatteryStatus Report. Both implementations use the same single
`dds_backend.h` C ABI, with vendor TypeSupport confined to their own
`dds/vendors/<vendor>/` directory. This establishes ABI-based vendor
substitution for the tested development fixture only, not mixed-vendor DDS
wire interoperability or official UMAA service compatibility.

User application code may register multiple typed Handlers in one native
module with `Component::provide<Service>(handler)`. Host applications may
use `Application::consume<Service>()`, with synthetic Report
`on_change(data_handler,dispose_handler)` callbacks. SDK host Report
publishers have a cancellable `start_periodic()` scheduler that invokes
only the user's `ReportSource::read()` implementation. Device business
modules contain no DDS Reader/Writer/QoS code.

The ABI v2 host now **prevents unload during in-flight calls and Pending
device commands**. Duplicate Command session execution and unsafe shutdown
are refused, not retried. CTest checks pending session draining and 180ms
in-flight callback completion before unload, including injected stop failure.
These are development safety gates, not physical hardware or process-crash
isolation proofs.

**Still required:** actual UMAA 6.0 CommandStatus/Ack on-wire integration,
reviewed official IDL ServiceBindings/keys/QoS, LargeSet/List coherent/ordered
semantics, RTI licensed adapter and mixed-network tests, hard process
isolation, and independently approved normative conformance scope. See
[SDK implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md) and
[modular architecture](docs/SDK_MODULAR_ARCHITECTURE.md). PR #15 remains
Draft and `main` is not implicitly merged.

### SDK transport protocol hardening (experimental)

The modular SDK now contains strict, vendor-neutral building blocks for
coherent LargeCollection batch reconciliation and remote Command Ack/Status
correlation. Both are covered by standalone CTest and are **not** substitutes
for reviewed UMAA TypeBindings, DDS coherent/ordered QoS, or the missing
on-wire Command runtime. See
[SDK implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md) for the
remaining completion gates.

### Modular SDK Command transport integration (development)

The SDK now provides a host-side path from a user Component v2 business
handler to `CommandEngine`, typed Status/Ack events, and a pluggable
`UmaaDdsBackendV1` event writer. A separate mock-backend Reader test
checks that a client does not report terminal success from Status alone.
This **is not a completed network Command client**: official Command
TypeSupport, incoming DDS Command readers, reviewed status/ack schemas,
recovery policy and real Cyclone/Fast DDS Command wire tests remain open.
The new `CoherentCollectionReceiver` similarly provides atomic, bounded
revision processing but still requires vendor Coherent/Ordered DDS bindings.
See [the implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md).

### Real DDS Command transport development slice (synthetic)

Separate Cyclone DDS and Fast DDS `UmaaDdsBackendV1` plugins now publish
typed synthetic Command Status and Ack, with independent-process readers
verifying the ordered 4 Status events, 1 Ack and 2 keyed Dispose events.
The same command client executable and Component module binary are tested
against each vendor backend without rebuilding user code. This is a
**synthetic Command example only**: it is not the full official UMAA
Command topic/client/session implementation. See the
[SDK implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md).

### Typed remote Command client (synthetic real-DDS acceptance)

`sdk/include/umaa/remote_command_client.hpp` exposes a typed
`RemoteCommandClient<Service>` to send a Command and await a correlated
Status+Ack without application DDS Reader/Writer code. The client explicitly
disposes the initiating Command after terminal confirmation and waits for
provider response cleanup. Independent-process Cyclone and Fast DDS tests
exercise this flow using `DemoCommandEvent.idl`; the parity workflow uses
the **same executable** and device Component against either vendor plugin.
This is a non-normative demo contract only; the official UMAA TypeBinding
and QoS acceptance gates remain unresolved.

### SDK Command cancellation, revision and crash isolation development update

The modular SDK includes a **typed remote Command** path with confirmed
device cancel, status/ack correlation, and guarded in-flight Command update.
[Real Cyclone DDS synthetic Command tests](https://github.com/rydolabs/umaa-kit/actions/runs/40872063613)
and [Fast DDS synthetic Command tests](https://github.com/rydolabs/umaa-kit/actions/runs/40872063639)
exercise the same `send → update/cancel → wait_for → finish` concepts in
separate processes. Both use non-normative `DemoCommandEvent.idl`.

An optional, explicitly supervised out-of-process Component Worker has also
been added for crash fault containment and **no automatic Command replay**.
The sender/receiver pair for LargeSet/LargeList now supports deterministic
bounded collection batches, final-element Dispose and generation recovery.
Neither the child-process experiment nor the collection transaction API
is a formal UMAA 6.0 conformance result. See the
[implementation ledger](docs/SDK_IMPLEMENTATION_BACKLOG.md).

### Remote asynchronous CommandHandle (typed application entrypoint)

Use `Application::issue_remote<Service>(command,session,source,destination)`
to obtain a `RemoteCommandHandle<Service>` with SDK-owned DDS Reader/Writer
lifetime. The handle supports `state()`, `poll()`, `wait_for()`,
`request_cancel()`, `ready_for_update()`, `update()` and `finish()`.
It remains usable after destruction of the configuration `Application`.
Only explicit, non-normative synthetic service bindings are proven by the
Cyclone/Fast DDS end-to-end tests; an official UMAA CommandHandle contract
still requires service-specific IDL and QoS review.
