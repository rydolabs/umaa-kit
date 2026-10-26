# Real Cyclone DDS transport experiment (distinct from UMAA conformance)

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
