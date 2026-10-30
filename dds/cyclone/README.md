# Real Cyclone DDS verification harness (NOT the SDK plugin)

This directory is a **historical, independent real-wire verification suite**;
its programs are NOT loaded by `UmaaDdsBackendV1`. The supported SDK
plugin implementation is exclusively at `dds/vendors/cyclone/`. Shared
development IDL fixtures now live at `dds/fixtures/idl/`, alongside
vendor-neutral tests. Avoid introducing new adapter implementations here.



The two C binaries are in **separate OS processes** and use the Cyclone DDS
wire transport and compiler-generated DDS TypeSupport from a **synthetic**
BatteryReportType fixture.  The SDK-like publisher and independent Simulator
subscriber verify a keyed Report's two updates and DDS instance Dispose.

Requires Cyclone DDS C including idlc, CMake and Python 3:

```sh
cmake -S dds/cyclone -B build/dds-real
cmake --build build/dds-real
ctest --test-dir build/dds-real --output-on-failure
```

Do not infer official UMAA 6.0 support from this experiment: the IDL is a
non-standard test fixture and does not implement any UMAA Command protocol.
Only the official UMAA IDL in a separately provided input archive can be used
to validate official type compatibility.
