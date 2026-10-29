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
The present demo under `sdk/examples/modules/vehicle_component.cpp` uses two
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
        |-- cyclone                     legacy adapter exists; ABI plugin not yet migrated
        |-- fastdds                     not yet implemented
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
