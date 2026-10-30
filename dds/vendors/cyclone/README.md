# Cyclone DDS — canonical UMAA Kit runtime plugin adapter

**Runtime implementation lives here.** `dds/cyclone/` is the older
**independent Cyclone wire verification harness**, not a second plugin API.
Both Cyclone and Fast DDS consume the shared synthetic IDL in
`dds/fixtures/idl/` and the identical SDK Topic/Schema/QoS filter from
`dds/vendors/common/demo_type_binding_rules.hpp`.

Current separate example DLL/SO modules:

- `adapter.cpp` — BatteryStatus Report data, per-instance Dispose.
- `command_adapter.cpp` — synthetic Command, ACK, Status, Update/Cancel.
- `collection_adapter.cpp` — synthetic bounded single-sample Collection.

All export `umaa_get_dds_backend_v1` and implement
`sdk/include/umaa/internal/dds_backend.h`. The host selects a module by
its DLL/SO path. The SDK Application/Component and Command/Report protocol
cores must not import native DDS headers.

Build and independent tests:

```sh
cmake -S dds/vendors/cyclone -B build/cyclone-sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build/cyclone-sdk
ctest --test-dir build/cyclone-sdk --output-on-failure
```

**Scope:** synthetic development bindings only. No official UMAA 6.0
normative per-service TypeSupport/Key/QoS or full conformance is asserted.
Unknown official schemas/unsupported coherent or ordered QoS must fail
closed. Same-binary standalone Cyclone/Fast parity is distinct from mixed
vendor network interoperability (historically failing/deferred).

See [backend.json](backend.json) and [backend rules](../ADDING_A_BACKEND.md).
