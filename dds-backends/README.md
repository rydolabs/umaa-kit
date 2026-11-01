# UMAA DDS Backends

This folder owns the vendor-specific implementations of
`sdk/include/umaa/internal/dds_backend.h` (`UmaaDdsBackendV1`).
The SDK and Component modules remain vendor-header-free.

- `cyclone/src/` and `fastdds/src/`: demonstration Report, Command, and
  Collection plugins using genuine vendor TypeSupport for *synthetic* IDL.
- `cyclone/include/umaa/`: reusable Cyclone-only bindings.
- `rti/`: Connext SDK/license requirements; **not implemented**.
- `tests/fixtures/idl/`: shared synthetic IDL (not official UMAA).
- `tests/integration/cyclone/`: standalone Cyclone DDS official-source
  integration hosts, simulators, and regression CMake project.

Build separately:
```sh
cmake -S dds-backends/cyclone -B build/cyclone-plugin
cmake -S dds-backends/fastdds -B build/fastdds-plugin
cmake -S tests/integration/cyclone -B build/cyclone-integration
```

Or use `cmake -S dds-backends -DUMAA_DDS_BACKEND=cyclone` / `fastdds`.
Each backend has a separate build tree and dynamic library. The
versioned DDS plugin ABI, target names, and SDK interfaces are preserved.
No official UMAA Topic/Key/QoS or cross-vendor interoperability is claimed.
