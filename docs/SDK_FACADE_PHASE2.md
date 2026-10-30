# SDK Facade phase 2 — host-local typed consumer (incremental)

This checkpoint builds on module ABI v2 and leaves original UMAA protocol engines unchanged.

- `sdk/include/umaa/application.hpp` provides `Application::consume<Service>()`.
- `CommandHandle::wait()` reports *immediate* outcomes, and rejects Pending rather than fabricating a completed UMAA CommandStatus/Ack.
- `ReportConsumer::read_local()` explicitly calls a host-local device module. It is NOT a queryless DDS subscription.
- Synthetic EngineControl and BatteryStatus are used for unit tests. Official UMAA ServiceTraits, async CommandClient, vendor-independent Report subscriptions and generated TypeSupport remain open.
- The DDS backend plugin contract is still `sdk/include/umaa/internal/dds_backend.h`. The mock backend remains test-only.

Additional **local-only experimental files** for synthetic Cyclone plugin integration were prepared separately; they are not claimed as remote-run evidence until committed and validated on hosted runners.

Build: `cmake -S sdk -B build/sdk && cmake --build build/sdk && ctest --test-dir build/sdk --output-on-failure`.

No official UMAA conformance claim.


## Cyclone plugin vertical slice (after hosted Runner verification)

`dds/vendors/cyclone/adapter.cpp` implements only `demo:battery-v1`.
The separate-process smoke test sends a device module's synthetic Report
through the Cyclone plugin. The **subscriber application** uses typed
`Application::consume<BatteryStatus>().poll()/latest()`, observes a keyed
value and a Dispose invalidation. Production implementation still needs
approved official ServiceTraits/TypeSupport/QoS, asynchronous UMAA command
status correlation and automatic report lifecycle scheduling.


## Contract rejections and negative-path acceptance

The real Cyclone plugin is intentionally allow-listed to
`demo/BatteryStatus`, `demo:battery-v1`, and `demo-reliable`.
The integration CLI verifies that an unknown official-looking schema, a
required `COHERENT_GROUP` capability and malformed instance keys are rejected
rather than silently mapped to the synthetic type.

The same device component source is used in the hosted Linux plugin test.
Windows/MSVC compatibility of the general SDK core is verified separately.
Cross-vendor interoperability, official UMAA service behavior and complete
DDS QoS remain separate acceptance gates.
