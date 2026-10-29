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
