# UMAA SDK modular component architecture (development milestone)

**Scope:** `sdk/` + `dds/` modularization. This document is an engineering
contract, **not** an assertion of UMAA 6.0 conformance. All official ICD,
Component Definition, security and QoS gates remain independently applicable.

## Developer goal

Write only device/business handlers; neither DDS `Participant`/`Topic`/
`Reader`/`Writer` nor UMAA CommandStatus/Ack/session/state-machine logic
belongs in application code. A component may provide/consume multiple services.

```cpp
class Engine : public umaa::CommandHandler<EngineControl> {
public:
    umaa::Result execute(const EngineControl::Command& command) override {
        return motor.start(command.target_rpm) ? umaa::Result::Completed
                                                : umaa::Result::Rejected;
    }
    umaa::Result cancel() override { return motor.stop() ? umaa::Result::Completed
                                                           : umaa::Result::Rejected; }
};
void register_services(umaa::Component& component) {
    static Engine engine;
    component.provide<EngineControl>(engine);
}
UMAA_EXPORT_COMPONENT_V2("engine", register_services)
```

`EngineControl` above is a generated service contract, **not** the sample fixture
class. The module-export macro hides all C ABI boilerplate; a future build
`ServiceBinding` generator will produce official schema codecs and type maps.
The present demo under `sdk/examples/modules/vehicle_component.cpp` uses three
synthetic, intentionally `demo:`-prefixed types, not genuine UMAA wire types.

## Binary responsibilities

```text
umaa-host(.exe)                         process orchestration (planned)
  |-- umaa-runtime(.dll/.so)             one common host runtime (MVP built)
  |     |-- Component ABI v2             sdk/include/umaa/module_api_v2.h
  |     |-- DDS Backend ABI v1           sdk/include/umaa/internal/dds_backend.h
  |     |-- protocol core               sdk/include/umaa/command_engine.hpp, etc.
  |     `-- service binding registry     official IDL/ICD mapping (future)
  |-- vehicle-control(.dll/.so)         user Handler(s), no DDS headers (MVP built)
  |-- navigation(.dll/.so)              optional user component
  `-- umaa-dds-<vendor>(.dll/.so)       ONE backend active per runtime/domain
        |-- cyclone                     synthetic Report ABI plugin verified
        |-- fastdds                     synthetic Report ABI plugin verified
        `-- rti                         requires SDK/license, not yet implemented
```

One module can host many services. **Service** is a logical contract;
**DLL/SO** is a deployment and lifecycle boundary, not necessarily 1:1.
Device and vendor libraries are independently loadable without changing their
source. These are in-process plugins; process crashes and memory corruption are
NOT isolated. Out-of-process safety is a separate later milestone.

## Single-entry DDS vendor contract

Read **only** `sdk/include/umaa/internal/dds_backend.h` to see every required
vendor callback and the exact ownership/capability semantics.

A vendor implementation directory should contain:

```text
dds/vendors/<name>/
  adapter.cpp                 implements umaa_get_dds_backend_v1()
  type_bindings/              generated IDL TypeSupport, schema/key mapping
  CMakeLists.txt              self-contained vendor SDK dependency
  README.md                   supported QoS and capabilities evidence
```

Only that directory plus a build/package registration is needed for a new
vendor. No edits to `Component`, `CommandEngine`, `ReportEngine` or user modules.
**Important limitation:** official UMAA C/C++ TypeSupport/keys/QoS mappings
are NOT interchangeable raw DDS byte arrays. A backend translates explicitly
versioned vendor-neutral *schema-encoded* samples to the generated native DDS
sample type. Missing type binding, unreviewed policy or mandatory QoS is an
error, never an implicit downgrade. Source timestamps, keyed instance
lifecycle, reliable ACK, discovery and GROUP coherent/ordered presentation
are capabilities and require independent real-wire verification.

`dds_backend.h` intentionally includes no DDS vendor header. Its currently
implemented `mock-contract-only` plugin lives in `sdk/tests/plugins/` and
rejects any schema without the `demo:` prefix. It proves ABI separation, NOT
DDS network behavior or cross-vendor interoperability.

## Component module contract

`module_api_v2.h` is a versioned, C-compatible table:

- Entry `umaa_get_component_v2(version)` returns ABI v2 metadata.
- One module declares multiple `(service_id, schema_id, action mask)` bindings.
- `create/start/call/stop/destroy` use module-owned opaque context and
  caller-owned bounded buffers, not C++ object/allocator ownership.
- The module dispatches only business results. SDK host remains responsible
  for Command/Ack/Status and other normative state transitions.
- Schema and service IDs must match *before* invoking the handler.
- No C++ exceptions across ABI; callbacks must drain before successful stop.
- Failed stop means **never unload** the module; quarantine instead.
- Cross-ABI command callbacks MUST NOT be retried due to output buffer sizing
  or ambiguous delivery, because hardware actions may have side effects.
- ABI v1 (`module_api.h`) remains available for legacy demo modules during
  migration; it only supports a scalar `invoke` fixture and is not a real
  UMAA typed/wire module contract.

## Build and local acceptance

```sh
cmake -S sdk -B build/sdk-modular -DCMAKE_BUILD_TYPE=Release
cmake --build build/sdk-modular --parallel
ctest --test-dir build/sdk-modular --output-on-failure
cmake --install build/sdk-modular --prefix build/sdk-install
```

The CMake target `UMAA::Runtime` is a genuine shared runtime host library,
while `UMAA::Core` is still a C++17 header-only/INTERFACE target. Build outputs:
`umaa_sdk_runtime` shared library, `umaa_demo_vehicle_v2` module,
`umaa_mock_dds_backend` module. The runtime verifies multiple service bindings,
ABI/schema mismatch, and a mock transport boundary before calling business
logic. Native loader tests run on Linux and Windows using the same code.

All test artifacts here are development/synthetic fixtures. Do not call this
complete UMAA service support. The existing genuine Cyclone DDS suites remain
separate and continue to gate non-regression.

## Critical next milestones, in order

1. **MOD-03B**: Reviewed contract/codegen delivers official per-service
   `ServiceTraits`/typed codecs, stable schema hashes and official DDS type keys.
2. **MOD-04B/05**: Replace mock with genuine Cyclone implementation of
   `dds_backend.h`, migrate BatteryStatus and EngineControl typed runtime
   entirely off the old host's manual Reader/Writer assembly, integrate
   CommandEngine event pipeline. Verify independent real DDS oracle, correct
   QoS/keys and failure paths.
3. **MOD-06**: Implement Fast DDS backend using the identical contract and
   approved TypeSupport; run the exact same unmodified application module on
   both Cyclone and Fast DDS, plus negative QoS/Dispose/recovery tests.
4. **MOD-07**: Optional RTI backend after licensed SDK and QA environment,
   cross-vendor wire tests (both directions).
5. **MOD-08**: Out-of-process module isolation, crash recovery, service
   supervision, hot-upgrade/session draining, fuzzing, real safety behavior.

For all milestones: CTest, Linux GCC/Clang, Windows MSVC, installed consumers,
correct ABI rejection and DDS-source isolation must pass on GitHub Actions.


## Phase 2 status: modular backend real-wire development slice

The SDK now has a small host-side `Application::consume<Service>()` API.
A typed Report client can use the independently loaded Cyclone
`UmaaDdsBackendV1` module to poll keyed samples and clear its cache on Dispose.
The consumer does not include Cyclone headers. Backend callback, domain and
library lifetime are retained safely across the ReportSubscription lifetime.

Only synthetic `demo:battery-v1` mapping is compiled into the new Cyclone
and Fast DDS plugins. The identical device module and Consumer binary run
against either vendor in CI. A synthetic Report callback/watch loop and
periodic ReportSource scheduler are implemented in the common runtime;
official UMAA QoS binding and DDS CommandStatus/Ack correlation remain OPEN.
RTI Adapter validation remains BLOCKED pending licensed SDK/Runner.

## Incremental user API and module safety evidence — 2026-10-29

- **Application callback API:** `consume<Report>().on_change(data,dispose)`
  dispatches keyed Report events on an SDK-managed Reader thread; no app
  Reader/Writer code. Callback failures are exposed, and manual concurrent
  polling is prohibited. Development TypeBinding only.
- **Automatic Report Provider:** `ReportPublisher::start_periodic(interval)`
  asks the registered business-only `ReportSource::read()` for updates.
  The SDK owns the persistent writer, periodic thread and final ACK on close.
- **ABI v2 safe stop:** `stop()` waits for active module calls; Pending
  device Command sessions block unload until terminal. Failed stop remains
  quarantined; the host does not retry unsafe shutdown. Process-level isolation
  is NOT included.
- **Vendor facade:** Cyclone DDS and Fast DDS real-wire synthetic
  BatteryStatus testing and unchanged-binary parity have Actions evidence.
  No official UMAA full-service conformance or mixed-vendor wire proof.

The old milestones above are historical design targets; the
[implementation ledger](SDK_IMPLEMENTATION_BACKLOG.md) is the current
acceptance source of truth.


## Shared host DDS EndpointContext (2026-11-07 — isolated refactor candidate)

**Goal:** remove duplicate native DDS backend/Domain allocation from the
`Application` remote **Command** paths without introducing a second Runtime
or a service/Topic name inference engine.

- `ComponentRuntime::load_backend` creates one `DdsEndpointContext`.
  Existing Report paths and `Application::consume_remote`,
  `serve_remote`/`start_remote`, `issue_remote` reuse this exact session.
  Standalone `RemoteCommandClient` and `DdsCommandRequestServer` constructors
  remain supported and own a private context for compatibility.
- `DdsCommandEventTransport` accepts the same context, so the Command Reader,
  Status Writer and Ack Writer do not silently open a second Domain within
  the same application. Their existing engine/protocol logic is unchanged.
- Explicit host QoS profiles are shared and frozen when the first context
  Reader/Writer or existing Report endpoint opens. When profiles are provided
  the context requires V2 QoS extension and refuses unknown profiles or
  unsupported capabilities: **no implicit V1 downgrade**.
- When no profile is provided, the current legacy V1 development-only fallback
  remains. It is **NOT** evidence of any official UMAA Service QoS or native
  TypeSupport. Full original per-Service/Component QoS still needs reviewed
  source-backed bindings and separate admission.
- Remote Command handles may outlive `Application`. Shared ownership keeps
  the plugin mapping and Domain live until their endpoints close. The plugin
  must be the last native resource released.

**Not included:** Collection/SplitCollection/Specialization constructors
continue their existing independently owned Domain paths for now. A later
migration must preserve their strict ACK/dispose/fail-closed tests. There is
no new public general Runtime API, no breaking V1/V2 C ABI change, and no
claim of mixed-vendor networking or normative conformance.

Acceptance: compile existing Linux/Windows SDK, run all existing remote
Command/DDS real-wire regression suites, and the new
`umaa_dds_endpoint_context_test` for shared domain, parent lifetime, QoS
freeze and V2 rejection. Do not merge before exact-head CI evidence.


### Single QoS owner refinement (subsequent code commit)

The `ComponentRuntime::State`'s duplicate `backend`, `domain` and
`qos_profiles` fields were removed after the first passing CI checkpoint.
The shared `DdsEndpointContext` is now the **single owner** of backend,
Domain, and QoS policy state. Both existing Report fixtures and all migrated
remote Command endpoints open Reader/Writer through the Context's common
admission methods. A source-reviewed `qos::Requirements` object is passed
explicitly for Report endpoints and cannot silently use no-profile V1 mode.
No additional Dispatcher, Runtime wrapper, or endpoint lifecycle adapter
was introduced; the now-unused `note_endpoint_start` API was deleted.

The earlier 8/8 workflow evidence applies only to the first code revision,
not automatically to this follow-up. Require a new exact-HEAD CI PASS.


## Shared native DDS session for Collection and SplitCollection (2026-11-07)

**Stacked on shared Command/Report Context PR #77.** This change migrates
`Application::publish_collection/consume_collection` and
`publish_split_collection/consume_split_collection` onto the same
host-owned `DdsEndpointContext` already shared by the Command and Report
endpoints. The old standalone `(backend_path, domain_id, key, cap)`
constructors delegate to a privately owned Context for source compatibility.

The Context creates and owns a single vendor plugin and Domain for each
`Application`. All four Collection endpoint classes now take
`std::shared_ptr<DdsEndpointContext>` and create Readers/Writers through
`context->reader/writer`, which enforces identical QoS admission as the
other migrated transports. An explicit profile requires vendor QoS V2,
and missing profiles are rejected without fallback; configuration freezes
after the first successful endpoint opening. Endpoint handles keep the
Context alive even after their parent Application goes out of scope.

A new mock-backed cross-endpoint CTest checks actual same-Domain sample
delivery, ordered split metadata/element snapshot commit, revisions,
Dispose, retained resource lifetimes, legacy constructors, null contexts,
and refusal of both configured-V2-on-V1 and missing-profile combinations.
Existing genuine Cyclone and Fast DDS Collection/SplitCollection tests
remain mandatory.

**Scope:** the Collection transport is still the **synthetic** bounded
single-frame or two-frame application commit-fence model. This does NOT
supply original UMAA multi-topic native DDS GROUP presentation, normative
Key/QoS, full TypeSupport or conformance. General
Specialization and optional process Supervisor remain unchanged. No
Component ABI or DDS plugin ABI change; no additional public Runtime
factory/dispatcher was added.
