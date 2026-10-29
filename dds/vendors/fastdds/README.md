# Fast DDS vendor adapter — NOT YET IMPLEMENTED

Implement the single vendor contract
`sdk/include/umaa/internal/dds_backend.h` inside this directory.
Keep all eProsima headers, Fast DDS library calls, generated `fastddsgen`
TypeSupport and QoS mapping inside `dds/vendors/fastdds/`.

Acceptance requires official reviewed type/key/topic/QoS contracts, compile/link
with a pinned supported Fast DDS version, real typed data and Dispose tests,
UNKNOWN_DELIVERY handling, discovery/reliability, source time and mandatory
coherent/ordered Presentation capability checks. A missing capability or
unreviewed type must fail closed. Pass the same independently executed
publisher/subscriber contract tests as Cyclone with the unchanged user module.

**Status: OPEN — no implementation or real-wire Fast DDS evidence yet.**
