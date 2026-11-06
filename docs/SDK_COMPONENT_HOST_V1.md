# Runnable UMAA SDK Component Host v1 — business module orchestration

The SDK now builds and installs an executable **umaa_component_host**. This
closes the *process orchestration* link for one development-only Report
Service using the existing Component ABI v2, UMAA::Runtime and DDS backend.

The developer selects a Service, generates a Component project and CMake
MODULE, implements only a typed \`ReportSource::read()\` business handler,
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
The existing RuntimeConfig also accepts explicit \`qos_profiles:\`,
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
\`UMAA_HOST_READY\` and \`UMAA_HOST_STOPPED\` reflect those phases.
Failure returns code 2 and \`UMAA_HOST_ERROR\`.

This v1 Host deliberately **rejects official UMAA:: service names**,
unknown schemas and forged QoS. It only admits the known synthetic
\`Demo::EO::BatteryStatus\` / \`demo/BatteryStatus\` /
\`demo:battery-v1\` / \`demo-reliable\` binding. Thus the new executable
does not claim to have implemented official BatteryStatus or EngineControl
endpoints, real DDS QoS, arbitrary Operation dispatch or user-defined
Component Composition. The pre-existing SDK has separate, still-scoped
real vendor DDS integration tests.

### Next SDK milestone

Generate and verify native \`ServiceBinding\` metadata and C++ typed
codecs from *approved* ICD/IDL contracts. Use this binding to replace the
hard-coded synthetic Host allowlist, then bind one official Report service
to the Host and independently verify Cyclone and Fast real-wire lifecycle.
Subsequently generalize to Command/Ack/Status and Collection. A Component
DLL/SO is an **in-process** module; malicious or crashing C++ code can
terminate the Host, and this first executable does not provide crash
isolation, authentication or hardened production supervision.
