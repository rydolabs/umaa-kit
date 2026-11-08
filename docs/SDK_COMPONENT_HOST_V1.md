# Runnable UMAA SDK Component Host v1 — business module orchestration

The SDK now builds and installs an executable **umaa_component_host**. This
closes the *process orchestration* link for one development-only Report
Service using the existing Component ABI v2, UMAA::Runtime and DDS backend.

The developer selects a Service, generates a Component project and CMake
MODULE, implements only a typed `ReportSource::read()` business handler,
builds a DLL/SO, then configures the Host to load and run that module.
No DDS APIs are needed in the Handler. An unimplemented generated Handler
rejects/throws, so it cannot silently report successful device behavior.

## Minimal strict host.yml (synthetic demo only)

~~~yaml
middleware:
  backend: "/absolute/path/to/umaa_mock_dds_backend.so"
  domain_id: 91
modules:
  - "/absolute/path/to/vehicle_component.so"
report_publishers:
  - service: Demo::EO::BatteryStatus
    topic: demo/BatteryStatus
    schema: demo:battery-v1
    qos: demo-reliable
    key: vehicle-a
    interval_ms: 100
~~~

Use platform-specific DLL/SO paths. An actual Cyclone/Fast DDS plugin can
replace the mock plugin only when it supports this *exact* explicit
synthetic TypeBinding; the mock backend itself has no network transport.
The existing RuntimeConfig also accepts explicit `qos_profiles:`,
but only actual backend-supported policies can be applied.

~~~sh
cmake -S sdk -B build/sdk -DUMAA_BUILD_TESTS=ON
cmake --build build/sdk --config Release
ctest --test-dir build/sdk --output-on-failure -R umaa_component_host_v1_smoke

# Indefinite, stoppable with SIGINT/SIGTERM
build/sdk/umaa_component_host --config /path/to/host.yml
# Deterministic short run
build/sdk/umaa_component_host --config /path/to/host.yml --duration-ms 500
~~~

The Host validates its configuration and admitted binding **before**
loading untrusted Component code. It opens one native plugin/domain,
validates Component ABI service/schema/Provider action, creates persistent
Report publishers, schedules device reads, relays through the backend,
checks asynchronous errors, and on orderly shutdown publishes Dispose,
waits for ACK via Runtime close, then unloads the module.
`UMAA_HOST_READY` and `UMAA_HOST_STOPPED` reflect those phases.
Failure returns code 2 and `UMAA_HOST_ERROR`.

This v1 Host deliberately **rejects official UMAA:: service names**,
unknown schemas and forged QoS. It only admits the known synthetic
`Demo::EO::BatteryStatus` / `demo/BatteryStatus` /
`demo:battery-v1` / `demo-reliable` binding. Thus the new executable
does not claim to have implemented official BatteryStatus or EngineControl
endpoints, real DDS QoS, arbitrary Operation dispatch or user-defined
Component Composition. The pre-existing SDK has separate, still-scoped
real vendor DDS integration tests.

### Next SDK milestone

Generate and verify native `ServiceBinding` metadata and C++ typed
codecs from *approved* ICD/IDL contracts. Use this binding to replace the
hard-coded synthetic Host allowlist, then bind one official Report service
to the Host and independently verify Cyclone and Fast real-wire lifecycle.
Subsequently generalize to Command/Ack/Status and Collection. A Component
DLL/SO is an **in-process** module; malicious or crashing C++ code can
terminate the Host, and this first executable does not provide crash
isolation, authentication or hardened production supervision.


## CLI-generated Host configuration (no hand-written Topic strings)

After the existing Component CLI generates and builds the module, create
the Host configuration from validated Component roles and explicit built
library paths:

~~~sh
python -m tools.umaa_component_cli host-config \
  --project /path/to/component \
  --module /absolute/path/to/built/component.so \
  --backend /absolute/path/to/dds-backend.so \
  --domain-id 91 --instance-key vehicle-a --interval-ms 100 \
  --output /path/to/host.yml

umaa_component_host --config /path/to/host.yml
~~~

Windows accepts built .dll paths; macOS development modules may use
.dylib. The CLI checks the Component Manifest has a **Demo Battery Provider**,
requires the generated CMake project and existing module/backend library
paths, enforces bounded Domain/interval/key, and uses exclusive file
creation. An existing user-owned host.yml is never overwritten.
No caller-provided schema or fake official QoS values are accepted.


## Generated Component → executable Host E2E

The `umaa_generated_component_host_v1_e2e` CTest now constructs a fresh
Component using the actual CLI, compiles its generated CMake DLL/SO against
the installed SDK, and loads that DLL/SO in the executable SDK Host.

First, the generated default `read()` Handler must fail closed. Then the
test edits *only* the user-owned business Handler to return a Report, verifies
that code regeneration preserves the edit, recompiles, and runs the identical
Host path successfully. No user-written DDS APIs, native TypeSupport,
CMake configuration or Module ABI plumbing is required for this synthetic
slice. The test backend is an **in-process mock**, not normative DDS wire.

## One-Host Provider → Reader → Consumer ABI v2 (development extension)

The **same** `umaa_component_host` executable now supports an explicit
`report_consumers:` configuration section. The SDK Host opens the Reader
using its single shared DDS Context, dispatches each accepted DATA update
and the **genuine DDS keyed Dispose** to the Consumer Component ABI-v2 role,
and refuses a rejected business callback or unresolved active source.
Publisher-only configurations remain supported.

~~~yaml
middleware:
  backend: "/absolute/path/to/umaa_mock_dds_backend.so"
  domain_id: 91
modules:
  - "/absolute/path/to/provider_component.so"
  - "/absolute/path/to/consumer_component.so"
report_publishers:
  - service: Demo::EO::BatteryStatus
    topic: demo/BatteryStatus
    schema: demo:battery-v1
    qos: demo-reliable
    key: source-a
    interval_ms: 25
report_consumers:
  - service: Demo::EO::BatteryStatus
    topic: demo/BatteryStatus
    schema: demo:battery-v1
    qos: demo-reliable
~~~

The consumer is a separate generated or hand-authored ABI-v2 module that
registers `component.consume<Service>(sink)` with both ReportAccept and
ReportDispose actions. The Host verifies service/schema/role before opening
any DDS Reader/Writer. Consumer-only subscriptions also work when another
process is responsible for the wire publisher; unresolved active sources
must fail the Host's clean-shutdown check instead of inventing disposal.

**Security and protocol boundaries:**
- Only the compiled `internal/host_report_bindings.hpp` **synthetic demo**
  `Demo::EO::BatteryStatus` contract can be executed in this Host.
  The larger reviewed literal Topic catalog does NOT approve official runtime
  TypeSupport, keys, Provider/Consumer applicability or mandatory QoS.
- The Consumer path calls `ReportSubscription::poll(1)` because its current
  last-event API is **not a bounded event queue** and batching multiple
  samples would silently coalesce callbacks. This is a development bridge,
  not an endorsement of its throughput/resource-budget guarantees.
- Source tokens are host-local, monotonically increasing, bounded to 64
  simultaneously active keys, and only retired after **actual** DDS DISPOSE.
  UNREGISTER, unknown disposal, callback failure, source overflow, or a
  bounded-drain overflow all fail nonzero.
- The test's mock DDS transport is in-process only; this exercise does
  **not** claim original UMAA native DDS, multi-Topic Collection, QoS,
  official ServiceBinding generation, real-device safety or conformance.

`umaa_host_report_binding_dispatch_e2e` builds independent ABI-v2
Provider/Consumer modules on Windows and Linux, runs the Host as its own
process, checks Report/Dispose counts and no remaining active sources,
proves Provider-only backward compatibility, and verifies fail-closed
business and binding cases. Real Cyclone/Fast DDS regressions stay separate.
