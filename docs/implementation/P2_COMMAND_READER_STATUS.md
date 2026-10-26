# P2 Cyclone Command input reader / shared sample lifecycle

## Implemented (SDK subset)

- `dds/cyclone/include/umaa/cyclone_sample_reader.hpp` owns a Cyclone C DDS
  Reader with explicit externally reviewed Topic, TypeSupport descriptor and
  QoS; it enforces subscription generations and fail-closed behavior.
- `dds/cyclone/include/umaa/cyclone_command_reader.hpp` exposes the same
  reader for Command topics. Service hosts remain responsible for the actual
  on-wire keys, source/destination checks, QoS, and Command/Dispose mapping.
- `cyclone_report_reader.hpp` delegates to the common sample Reader without
  changing the existing Report subscription interface.
- The official `EngineControl` host now polls its actual Command TypeSupport
  through `CommandReader`, not an ad-hoc `dds_take` loop. DDS dynamic sample
  contents are always reclaimed on both successful processing and exception.
- `test_cyclone_command_reader.cpp` covers stale epochs, invalid entities,
  valid/Dispose samples, DDS create/take/delete failures, callback faults,
  cleanup after faults and re-subscription fences using a test-only DDS ABI.

## Boundary / acceptance

The Engine host still implements only the ignition `RUN`/`OFF` slice. The
reader itself does not infer UMAA service QoS, identity keys, or roles.
Compiled official TypeSupport + independent real-wire Command/Update/Dispose
scenarios must pass on Linux and Windows runners after this change; local fake
DDS tests alone do **not** establish real-wire or full UMAA conformance.

RTI, broad service runtime generation, hardware behavior and the normative
UMAA compliance suite remain open and must not be labeled complete.
