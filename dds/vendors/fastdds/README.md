# Fast DDS — canonical UMAA Kit runtime plugin adapter

This is the **single canonical Fast DDS adapter source directory**:
`dds/vendors/fastdds/`. It implements the same vendor-neutral
`UmaaDdsBackendV1` ABI as `dds/vendors/cyclone/`.

The implementation currently builds **three separate synthetic example
DLL/SO plugins**:

- `adapter.cpp` — BatteryStatus Report/Data/Dispose;
- `command_adapter.cpp` — synthetic Command/ACK/Status/Update/Cancel;
- `collection_adapter.cpp` — bounded single-sample Set/List Collection.

The exact allowed Topic/Schema/QoS/capability combinations are shared with
Cyclone through `dds/vendors/common/demo_type_binding_rules.hpp`. The
synthetic IDL is in `dds/fixtures/idl/`, not under the Cyclone-specific
verification directory.

Build: `cmake -S dds/vendors/fastdds -B build/fastdds` (requires
`libfastrtps-dev`, `libfastcdr-dev` and `fastddsgen`). The C++ SDK
application and Component modules do not include Fast DDS headers.

All three plugins are **synthetic development proofs**, not native official
UMAA 6.0 per-service Key/QoS/TypeSupport implementations. In particular,
single-frame Collection support does not establish coherent/ordered native
GROUP Presentation. Unknown or unreviewed bindings and required features
must fail closed. Mixed Cyclone↔Fast DDS same-network interoperability,
formal conformance and RTI validation are not established.

See [backend.json](backend.json) and [vendor onboarding rules](../ADDING_A_BACKEND.md).
