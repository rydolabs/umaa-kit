## M2 — Monotonic Command deadline proof (2026-10-27)

**Status:** bounded experimental implementation. NOT full official UMAA 6.0 QoS/conformance.

- Header `sdk/include/umaa/command_deadline.hpp` exposes an injectable
  `TimedCommandProvider<Payload, Clock>`. The DDS host drives the monotonic
  clock; no timer thread and no unreviewed per-service Deadline QoS is inferred.
- Valid new command and timestamp-newer accepted Update establish/reset a
  positive local deadline. Replays/source conflicts never move it; completed or
  disposed instances do not time out again.
- Safe cancellation failure retains the session without inventing FAILED.
  Transport failure remains fail-closed with no duplicate uncertain DDS write.
- An actual official `AnchorControl` generated TypeSupport test injects a **CI
  only** 250ms host-owned timeout, then independently checks wire
  `ISSUED→COMMANDED→EXECUTING→FAILED(TIMEOUT)`, correlated Command Ack,
  initiating Command Dispose and keyed final Ack/Status Dispose.
- Feature branch `30c311326a4f96e675a53749a6e043cf82555f39`, merged via
  [PR #12](https://github.com/rydolabs/umaa-kit/pull/12) into
  `dc4f5c15b93b238be83673e347cd4d0c5c94c2c3`, exact tree
  `1c73d6345200ee9a02f39aec1d2e026478997018`.
- [Linux actual DDS + netns](https://github.com/rydolabs/umaa-kit/actions/runs/40487440082)
  official CTest **23/23**, netns/netem **15/15**.
  [Windows actual DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40487439998)
  official CTest **22/22**, external Cyclone Consumer PASS.
  9/9 feature-head workflows passed; local GCC strict, ASan/UBSan test passed.
- Remaining: normative per-Component Deadline/History/Presentation QoS,
  unrelated service runtimes, cross-session wire concurrency, late joiner,
  partition+reconnect, durable startup recovery, Collection LargeSet/List,
  RTI 7.7 interop, official Compliance review and hardware safety.
  G6/G7/G8/G9/G10/G11/G12 are NOT closed by this work.

See [the gate ledger](../DELIVERY_GATES.md) and
[issue #2](https://github.com/rydolabs/umaa-kit/issues/2).
