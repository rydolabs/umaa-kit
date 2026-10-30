# Fast DDS plugin (synthetic BatteryStatus)

This plugin implements the same native DDS backend facade contract as Cyclone DDS
(`sdk/include/umaa/internal/dds_backend.h`). The *same* user module and
`Application::consume<BatteryStatus>()` can be used without Fast DDS includes.

Requires `libfastrtps-dev`, `libfastcdr-dev`, `fastddsgen`.
Build with `cmake -S dds/vendors/fastdds -B build/fastdds`.

Only the synthetic `demo:battery-v1` binding and `demo-reliable` QoS profile
are supported. Unknown official UMAA types and unimplemented required QoS
are rejected. Formal UMAA compatibility, Command/Ack/Status, collections,
RTI and actual DDS interoperability are not claimed.
