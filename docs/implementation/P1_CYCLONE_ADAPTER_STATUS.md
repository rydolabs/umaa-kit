# P1 Cyclone Command Adapter checkpoint (2026-10-26)

## What changed

Based on the local checkpoint `3d4b351` (which itself incorporates the handoff
at upstream `89155aa4163f04d69585c4e1054dbbbd8c944c2d`):

- `dds/cyclone/include/umaa/cyclone_command_transport.hpp` supplies a reusable,
  fail-closed `CommandWriter<Payload,StatusSample,AckSample,Mapper>` implemented
  with actual `dds_write` and `dds_dispose` C API calls. The per-service mapper
  is required to supply exact reviewed instance keys, payload serialization
  and any deep-copy ownership. The class intentionally does not guess UMAA QoS,
  roles, or Vendor SDK settings.
- The existing **official EngineControl example host** now calls
  `BoundCommandProvider<EnginePayload>` through this writer, rather than
  duplicating the state-event mapping in its local `emit()` function. The
  fixed-layout EngineCommand sample is retained for generating an Ack reflecting
  the latest accepted update, and latest keyed status is held per session for
  safe Dispose. Explicitly unsupported `plugState`/`propulsion` pointer-bearing
  samples are rejected before retention.
- The installable `UMAA::Cyclone` target is exported only when
  `-DUMAA_ENABLE_CYCLONE_ADAPTER=ON` and `find_package(CycloneDDS CONFIG)`
  succeeds. Core-only installations remain independent of CycloneDDS. The
  exported target includes the vendor's C library and the header-only SDK
  core; installed downstream `find_package(UMAAPrototype)` resolves the
  Cyclone package dependency automatically. This export was tested with a
  **fake** CycloneDDS CMake package, not a real vendor installation.
- `sdk/tests/test_cyclone_command_adapter.cpp` covers interleaved sessions,
  accepted updates, latest-payload Ack Dispose, checked negative DDS writes,
  faulted-provider fencing, missing status instance, and failing Dispose of
  either output topic. Test-only DDS ABI shims are **not used in production**.
- All old official integration CTest scenarios and the independent simulator
  remain in source for a vendor-enabled runner, unmodified.

## Reproduced local validation

Run in Linux container without vendor DDS libraries or official AUVSI IDL:

| Check | Actual result | Evidence and boundaries |
|---|---|---|
| GCC `-Wall -Wextra -Wpedantic -Werror` | CTest **8/8 PASS** | DDS adapter test uses test-only stub, not wire |
| Clang UBSan, same warnings | CTest **8/8 PASS** | Local bounded sanitizer checks |
| Clang ASan, same warnings | CTest **8/8 PASS** | Local bounded sanitizer checks |
| `python tools/ci_verify.py --compiler default` | **PASS** | SDK Python 32; Simulator Python 6; 8 CTest; 4 stdio scenarios; Core & optional Cyclone mock install/external consumers |
| `python tools/ci_verify.py --compiler clang` | **PASS** | Same CI-shaped prototype tests |
| Official EngineControl C++ host syntax | **PASS only against fake structural declarations** | No authentic IDL, Cyclone headers or linkage; strictly weaker than actual compile |
| Actual official IDL + Cyclone wire tests | **NOT RUN** | Official IDL archive and CycloneDDS unavailable in the local runtime; external download DNS blocked |
| Windows MSVC / RTI 7.7 | **NOT RUN** | No usable native toolchain / RTI SDK or license |

The original handoff has independent *prior* official BatteryStatus/EngineControl
wire evidence on GitHub for the **old host code**. Those runs do not validate
this new code. New official Cyclone acceptance must be rerun after the change
reaches a runner with pinned official input and vendor libraries.

## Acceptance still open

- **P1-1** Cyclone Adapter: writer method integration is implemented, but official
  wire build and subscriber integration not yet revalidated.
- **P1-2** EngineControl: host refactored, independent real-wire tests pending;
  physical engine and the non-ignition Command fields still absent.
- **P1-3** BatteryStatus: existing handoff Report publisher/reader are preserved;
  no new DDS rerun this checkpoint.
- **P1-4** 53-service generated bindings: **not completed**; each service still
  needs reviewed type and key mapping, QoS and lifecycle definitions.
- **P1-5** Loss/recovery: existing handoff tests are preserved, but not rerun
  against modified Engine host with real DDS.
- **P1-6** CI: SDK CTest includes the adapter stub regression across runners;
  real-DDS netns/Windows workflows already match `dds/cyclone/**` edits but
  new real-wire runs have **not** completed for this checkpoint.

Thus **P1 remains PARTIAL**, not accepted, and UMAA/RTI/K-MOSA conformance is
not claimed. Do not interpret local fake ABI tests as Cyclone or official
schema acceptance.

## Reproduction commands

```bash
cmake -S sdk -B build/p1 -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build build/p1 --parallel 4
ctest --test-dir build/p1 --output-on-failure
python tools/ci_verify.py --compiler default
python tools/ci_verify.py --compiler clang
# Production integration requires installed CycloneDDS:
# cmake -S sdk -B build/p1-cyclone -DUMAA_ENABLE_CYCLONE_ADAPTER=ON
# cmake --install build/p1-cyclone --prefix /your/install/prefix
# External CMake project: find_package(UMAAPrototype CONFIG REQUIRED)
#                         target_link_libraries(app PRIVATE UMAA::Cyclone)
```

For official vendor-enabled CI, use the existing `.github/workflows/dds-netns.yml`
and `.github/workflows/real-dds-windows.yml` which fetch the pinned official
IDL temporarily, generate TypeSupport and run independently compiled C
simulators. The upstream ZIP is not redistributable as part of this package.
