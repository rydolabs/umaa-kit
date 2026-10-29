# Typed C++ SDK prototype (independent Project A)

**Status:** Verifiable core slice, not full UMAA 6.0 compliance.

## Public workflow

From the repository root:

```bash
python3 sdk/tools/umaa_codegen.py inspect --idl sdk/fixtures/demo.idl
python3 sdk/tools/umaa_codegen.py generate \
  --idl sdk/fixtures/demo.idl --catalog sdk/fixtures/services.json \
  --service Demo::EO::EngineControl --out build/generated
python3 sdk/tools/umaa_codegen.py generate \
  --idl sdk/fixtures/demo.idl --catalog sdk/fixtures/services.json \
  --service Demo::EO::BatteryStatus --out build/generated
```

The generator writes `/include/umaa/generated/*.hpp` (safe to regenerate) and `/user/My*.hpp` (written once, never overwritten). The example DTOs are **synthetic**, not wire-compatible UMAA IDL artifacts. A real DDS IDL compiler and generated TypeSupport are required when integrating official UMAA IDL.

## User-side contract

- For command Provider, implement `execute(typed_payload)` and `cancel(typed_payload)`; optionally override `validate`. The SDK owns session identity, transition sequencing and response event production.
- For report Provider, implement `read_report()`. The SDK provides source/timestamp bookkeeping, cache and dispose primitives.
- Use the SDK-generated API, not low-level Writer/Reader, in your device/business code.

## Current unsupported IDL syntax

The generator deliberately accepts **only** nested `module`, primitive `struct`, `@key`, strings, and simple signed/unsigned numeric fields. Actual UMAA 6.0 IDL uses other constructs such as imports, typedefs, enums, user-defined nested types, sequences, and optional members. The generator **fails closed** rather than silently corrupting types. It is NOT a substitute for official IDL code generation.

Service definitions are not inferable unambiguously from IDL names, so an explicit Service Catalog is mandatory. The catalog in `fixtures/` is deliberately marked "demo-only".

## CMake and tests

```bash
cmake -S sdk -B build/sdk-cmake -G Ninja
cmake --build build/sdk-cmake
ctest --test-dir build/sdk-cmake --output-on-failure
cmake --install build/sdk-cmake --prefix build/sdk-install
```

An external app may `find_package(UMAAPrototype CONFIG REQUIRED)` and `target_link_libraries(app PRIVATE UMAA::Core)`.

## Vendor integration contracts (transport-neutral subset implemented; DDS adapters mostly OPEN)

- Cyclone DDS-CXX: primary adapter candidate because upstream AUVSI Common uses it.
- RTI Connext 7.7: second independently testable adapter candidate, requires valid SDK/license.
- Keep generated vendor-specific DDS C++ types outside the public user Handler API.
- Reject a vendor that cannot meet the mandatory LargeCollection `PRESENTATION` QoS, rather than silently downgrading compliance.

## Command transport binding (2026-10-26)

`umaa/command_transport.hpp` defines `CommandTransport<Payload>` and
`BoundCommandProvider<Payload>`. The sink maps checked `publish_status`,
`publish_ack`, `dispose_status` and `dispose_ack` operations to the **reviewed**
vendor-specific DDS types, keys and required QoS. Its ACK callbacks receive the
entire accepted payload (including the latest Command Update), not merely an ID.
`BoundCommandProvider` sequences output and becomes permanently **Faulted** if
an operation throws; the host must rebuild DDS entities to resume. Caller
code owns deadlines and calls `timeout(session)` after establishing a deadline;
DEVICE cancellation must be confirmed before the provider emits TIMEOUT.
A disposed but non-cancellable command cannot be updated/restarted.

The stdio EngineControl fixture now exercises this binder and the existing
independent simulator remains green. This is **not** new official DDS wire proof.
The official DDS EngineControl host still has a separate example event emitter;
production consolidation and full ACK/status enum/QoS mapping remain OPEN.

For `CollectionAssembler`, use timestamped `dispose(id, timestamp)` when the
vendor supplies a valid disposal timestamp; without a timestamp, ordering cannot
be proven. Instances are scoped to one collection ID; call `clear()` or create
another assembler before processing a different ID. This is not a normative DDS
ordered-access or atomic-presentation implementation.

## Modular SDK architecture / component ABI v2 (2026-10-29 development slice)

The application writer implements **only typed equipment/business handlers**.
`UMAA_EXPORT_COMPONENT_V2()` generates the stable C entrypoint automatically;
`Component::provide<Service>(handler)` registers any number of services in
one DLL/SO. Host-side `UMAA::Runtime` is a shared library and the DDS vendor
boundary is defined **once** in `include/umaa/internal/dds_backend.h`.

- `include/umaa/component.hpp` — lightweight, vendor-agnostic application API.
- `include/umaa/module_api_v2.h` — versioned multi-service component C ABI.
- `include/umaa/component_module.hpp` — host-side component loader.
- `include/umaa/internal/dds_backend.h` — **single new-vendor contract**.
- `include/umaa/dds_backend_loader.hpp` — host-side vendor library loader.
- `include/umaa/internal/component_runtime.hpp` — common host runtime API.
- `src/component_runtime.cpp` — actual shared runtime library.
- `examples/modules/vehicle_component.cpp` — Engine + Battery handlers in one
  native module, with **no DDS imports**.
- `tests/plugins/mock_backend.cpp` — in-process mock for ABI conformance only,
  **not** genuine DDS network communication.

See [full architecture and vendor extension rules](../docs/SDK_MODULAR_ARCHITECTURE.md).
The old Module ABI v1 remains for legacy test compatibility. Synthetic
BatteryStatus TypeBindings run over separate Cyclone and Fast DDS plugins;
legacy official service hosts have not yet been fully migrated into the new
facade. RTI Adapter and normative service bindings remain outstanding. Never infer official UMAA
conformance from these development-only ABI tests.


## Phase 2 — application consumer + plugin vertical slice

Host-side users may call `umaa::Application::consume<Service>()`.
For a synthetic Report schema whose reviewed contract declares
`id`, `schema_id`, `topic_name`, `qos_profile`, `Report`, and
`decode_report`, the consumer's `poll(1)`, `latest(out,key)`, and
`disposed()` use the backend plugin through `ComponentRuntime`.
For a synthetic Command schema with `is_command=true` and
`encode_command`, `CommandHandle::wait()` handles immediate terminal results and refuses
`Pending`; `poll()`, `wait_for()` and `cancel()` handle host-local async
business sessions. Real DDS CommandStatus/Ack is not yet connected.

See `dds/vendors/cyclone/` for the **synthetic BatteryStatus-only** real-wire
adapter, and `docs/SDK_FACADE_PHASE2.md` for limitations and CI reproduction.
These development APIs are not official UMAA conformance evidence.

## Vendor-parity acceptance

The development API intentionally hides Reader/Writer/QoS from the user.
Two builds of `dds/vendors/cyclone` and `dds/vendors/fastdds` provide
separate vendor-native TypeSupport for one **synthetic** BatteryStatus fixture.
The parity workflow requires **the identical component .so and identical
consumer executable binaries** to run against each DDS backend plugin. It
does not establish official UMAA 6.0 compliance or multi-vendor interoperation
on a mixed DDS network.

`ReportPublisher` now maintains a writer across multiple updates and Dispose.
`ReportSubscription` caches by DDS instance key, and disposal of one source
does not remove another source. Both are subject to the explicit mock/real-wire
scope. See [backlog](../docs/SDK_IMPLEMENTATION_BACKLOG.md).

## Module safe-stop protection (2026-10-29)

`ComponentModule` now guards ABI calls against concurrent stop/unload.
A Pending device session is identified by (service ID, session ID), not the
session number alone. An ambiguous business invocation reserves its session
**before** hardware execution and is never auto-replayed. `stop()` rejects
an outstanding session; the caller may poll or cancel it and retry after it
reaches a safe terminal state. If the plugin's `stop()` fails, mapping
retention remains fail-closed; OS process isolation is still an open item.

`umaa_component_safe_shutdown_v2_test` exercises reject-while-pending,
duplicate replay fencing, poll-to-terminal, cancel and post-stop rejection.
This is a device-module ABI safety gate, not a claim that UMAA wire
CommandStatus/Ack integration is complete.

## Minimal Report Consumer callback API (development scope)

`auto report = app.consume<BatteryStatus>();` supports
`report.on_change(on_report, on_dispose)`. An internal Reader polling thread
delivers each keyed data/Dispose event to business callbacks without exposing
DDS APIs. `stop_watch()` drains the thread; `rethrow_watch_error()` reports
callback, DDS or decode failures. Manual `poll()` while watching is rejected,
so the Reader cannot be concurrently consumed from two threads. A Report
Consumer must not destroy itself on its own callback thread. This is currently
tested with the synthetic mock Backend; normative service bindings remain open.

## ReportSource automatic scheduling (development scope)

The host may start `ReportPublisher::start_periodic(interval)`. The SDK
invokes the user's `ReportSource::read()`, publishes each update through the
persistent backend Writer, and stops the thread with `stop_periodic()`.
`close()` joins the scheduler before final ACK/Writer shutdown;
`rethrow_periodic_error()` surfaces asynchronous failures. The device
module never sees a DDS Writer or emits Report lifecycle events itself.
This schedule is a generic development policy, **not** a reviewed UMAA
service-specific publication interval or automatic official QoS mapping.

## Typed Application Report publishing and component reload

The host application can now use `auto pub=app.publish<BatteryStatus>("17");`
and `pub.start_periodic(10ms)`, `pub.dispose()`, `pub.close()`. Topic,
schema and backend Writer come from the service descriptor, **not** explicit
DDS parameters or vendor calls in application code. A separate component
module supplies only `ReportSource::read()`. This is a synthetic demo
vertical slice; official UMAA bindings are not generated yet.

`app.unload_component("vehicle-component")` returns false if a module has
Pending commands or any retained ReportPublisher; it succeeds once those
resources drain. This supports *cooperative*, not crash-safe, replacement.

## Remote Report Consumer without local device module

A Subscriber that only uses `Application::consume<ReportService>()` now
loads **only** a DDS backend, not the remote Provider's Component DLL/SO.
The backend `open_reader` strictly rejects unknown schema/Topic/QoS, while
a locally registered Provider must additionally match its declared Report
role/schema. Actual Cyclone and Fast DDS cross-process test fixtures now
explicitly omit `load_component()` from Subscriber. This proves only the
synthetic `demo:battery-v1` binding; official generated role verification
remains open.

## Configuration-driven startup without DDS setup code

`auto app = umaa::Application::from_config("runtime.yml");` accepts a
**strict, documented YAML subset**, not general YAML aliases/evaluation:

```yaml
middleware:
  backend: ./libumaa_dds_cyclone_demo.so
  domain_id: 42
modules:
  - ./libvehicle_component.so
```

All paths are relative to the configuration file unless absolute.
To switch supported middleware, change `backend` only.
For a **remote-only Consumer**, use `modules:` with no entries; the
Subscriber never loads the remote Provider DLL. The parser rejects unknown
fields, invalid domain, duplicate settings and oversized inputs. The
ServiceTraits/Topic mapping remains synthetic and checked by the backend;
this is not normative UMAA configuration support.

### Fail-closed transport receivers (experimental)

`umaa/collection_transport.hpp` stages a bounded, sequence-checked
LargeCollection revision and only exposes an atomic snapshot after metadata
and all elements agree. A lost DDS coherent batch faults the receiver; an
explicit newer-generation full snapshot is required to recover. It does **not**
implement DDS coherent/ordered QoS or official type mapping.

`umaa/remote_command_monitor.hpp` correlates CommandAck and CommandStatus
for each provider/session. It never infers success from Status alone and treats
unknown delivery or invalid state transitions as requiring recovery. These
classes are transport-neutral components; the actual officially mapped
on-wire Command adapter is still unimplemented.

### Component Command → protocol → DDS backend (development-only integration)

`component_command_provider.hpp` adapts a registered Component ABI v2
Command handler to the vendor-independent `CommandEngine` transitions.
The engine now passes source/destination/session identity into optional
`Handler::execute_session`/`cancel_session` hooks (legacy hooks work).
`dds_command_event_transport.hpp` publishes schema-encoded Status and Ack
through the single Backend ABI, never retries ambiguous deliveries, and
requires explicit checked ACK on flush. `remote_command_monitor.hpp`
correlates the receive side and refuses synthesized terminal success.

The CTest `umaa_command_dds_event_transport_test` exercises the entire
host device→engine→writer→**mock** reader→client correlation pipeline;
`umaa_component_command_provider_test` exercises business session draining.
Neither test proves real DDS Command traffic or official UMAA conformance.
A reviewer-approved service codec and Cyclone/Fast DDS TypeBinding must
implement the Command, Status and Ack topics to clear that acceptance gate.

`ReportSubscription::latest()` now returns the newest *live* instance
instead of becoming empty whenever an unrelated key was disposed.

### Synthetic Command real-wire proof

The `dds/vendors/{cyclone,fastdds}/command_adapter.cpp` plugins implement
`demo:async-status-v1` and `demo:async-ack-v1` using the distinct
`DemoCommandEvent.idl` fixture. The host uses
`ComponentCommandProvider<Service>` and `DdsCommandEventTransport<Service>`;
an independent remote client correlates Status and Ack to session `17`,
checks keyed Dispose, and refuses fabricated terminal success. The
`sdk-dds-vendor-parity.yml` command leg reuses one client executable and
one user Component `.so` for both vendor plugins. Real vendor traffic
is tested, but the schema and QoS are explicitly synthetic. A complete
official Command consumer, Request writer, reviewed UMAA TypeBindings,
cancellation/update/recovery and production-grade DDS QoS remain open.
