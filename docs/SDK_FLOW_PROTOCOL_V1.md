# SDK Flow Protocol v1 (phase 2) — implementation checkpoint

**Scope:** UMAA EO ICD 6.0 §§5.1, 5.2.1, 5.2.2; application-layer
protocol only. **NOT an official UMAA 6.0 conformance or DDS wire claim.**

## Protocols and source-backed semantics

| Flow | State | Ownership |
| --- | --- | --- |
| Command/Response | Reinforced | `command_engine.hpp`, `command_transport.hpp`, `remote_command_monitor.hpp` |
| Request/Reply without query data | Existing, regression-tested | `report_engine.hpp`, `report_transport.hpp`, typed `sdk_v1::ReportProvider/ReportConsumer` |
| Request/Reply with query data | **Not defined in EO ICD 6.0 §5.2.2** | No invented Requester/Replier wire protocol; unsupported service mapping fails closed |
| Optional Command Execution Status | Partial/not wired | Requires verified per-service types, topic, QoS and dedicated writer lifecycle |
| Genuine DDS Liveliness events | **Awaiting a new backend extension** | Protocol entrypoints exist; current DDS Backend C ABI V1 has no event callback for Liveliness |

### CommandStatus transition reasons

`command_transition_allowed(prior,next,reason)` implements EO ICD
Fig. 23, including initialization to ISSUED/SUCCEEDED, EXECUTING to
ISSUED/UPDATED, terminal-state prohibition, and state-specific FAILED
reason restrictions. It is used in the Provider FSM. The consumer monitor
has a backward-compatible, state-only overload and a **new reason-aware**
`status(session,provider,state,reason,timestamp)` that quarantines invalid
reason/state combinations. Official bindings **must** supply the reason-bearing
form; the older state-only form is strictly for synthetic/legacy callers.

### Optional CommandAck subscription

EO ICD 6.0 §5.1 explicitly allows a Consumer to subscribe to CommandAck
**optionally**. `RemoteCommandMonitor` now supports
`CommandAckPolicy::StatusOnly` (terminal CommandStatus can complete without
an Ack sample/Dispose), as well as the original
`CommandAckPolicy::Required` (legacy default). Typed
`sdk_v1::CommandConsumer` exposes the same explicit policy. The Provider
still publishes the Ack Topic as required; selecting StatusOnly changes
**only the Consumer's observation contract**, not Provider behavior.
A Status-only Consumer must never report terminal before receiving a
valid terminal CommandStatus and must still fence stale/reordered timestamps.

### DDS peer liveliness

For a trusted DDS Consumer-loss notification:
- `CommandEngine::consumer_liveliness_lost(source)` fences that source, tries
  safe device cancellation for its active sessions, emits CANCELED Status
  followed by response disposals only for confirmed cancellations, and leaves
  unrelated sessions untouched.
- When cancellation fails or hardware cannot confirm a stop, an explicit
  `LIVELINESS_CANCEL_NOT_SAFE` event is returned and the session stays tracked.
  A later actual completion or safe cancellation retires the response instances.
  This is **not full normative conformance** if the physical device cannot
  cancel as mandated; surface and reconcile the hardware failure.
- No Command from that Consumer is accepted while the source is fenced, even
  if a later DDS sample arrives. Only
  `consumer_liveliness_restored(source)` invoked by trusted DDS discovery may
  re-enable *new* sessions. Already orphaned sessions stay quarantined and
  reject Command Update to prevent duplicate device execution.
- `BoundCommandProvider`, `TimedCommandProvider`,
  `ComponentCommandProvider` and `sdk_v1::CommandProvider` expose these
  protocol hooks without changing existing C ABI structures.

For a trusted Provider-loss notification:
- `RemoteCommandMonitor::provider_liveliness_lost(provider)` returns
  in-progress sessions targeting that provider, marks each in need of
  reconciliation and prevents an apparent successful completion.
- `sdk_v1::CommandConsumer::provider_liveliness_lost()` returns full 128-bit
  session GUIDs. `RemoteCommandClient::on_provider_liveliness_lost(timeout)`
  actively Disposes initiating DDS Command instances for those sessions;
  a DDS acknowledgment is not a device cancellation acknowledgment.
  Ambiguous transport delivery never triggers automatic replay.
- The current V1 backend does not deliver the actual DDS liveliness events.
  Until an optional versioned backend event extension is implemented and
  independently verified, **callers must inject authenticated events** from
  their DDS adapter; this is not autonomous network-based liveliness handling.

### Report / Queryless Request/Reply

Provider startup recovers/replaces stale reports or explicitly disposes them.
Consumer subscribes without a query message. Shutdown disposes the report
and unregisters its writer; subscriber shutdown unsubscribes. Per-instance
timestamp watermarks and reader-epoch fencing reject stale samples and
Dispose. These are covered by existing `test_report_lifecycle.cpp` and
`test_report_transport.cpp`, and the new phase-2 regression.

## Verification

Run:

```sh
cmake -S sdk -B build/flow -G Ninja -DUMAA_BUILD_TESTS=ON
cmake --build build/flow --target umaa_flow_protocol_v1_test
ctest --test-dir build/flow --output-on-failure -R umaa_flow_protocol_v1_test
```

The `UMAA Flow Protocol v1` workflow runs the new test plus the existing
typed SDK v1, Command Engine, Command Monitor, deadline, Command Transport,
identity routing, Report lifecycle and Report transport regressions on
Linux GCC/Clang and Windows MSVC. Tests are C++17 in-process tests, not
official UMAA TypeSupport/DDS interoperability tests.

## Remaining blocking acceptance criteria

1. Connect a versioned backend Liveliness callback to the host Command
   Runtime and its trusted `consumer_liveliness_lost/restored` hooks.
   Do not modify the existing `UmaaDdsBackendV1` struct layout.
2. Preserve official 128-bit Command Source/Destination/Session ID in native
   DDS multi-key TypeBindings. Existing Module ABI v2 is `uint64_t`
   and the DDS Backend V1 opaque instance key is a string.
3. Obtain reviewed per-service Command Status/Ack/Execution Status types
   and QoS, encode exact ICD enums and reasons, and prove on-wire Update,
   Cancel, Timeout, recovery and response cleanup.
4. Bind Report lifecycle and source-aware keyed Dispose to approved
   official service Topic/TypeSupport and test late joiner/restart.
5. Add reviewed query-data request/reply only if a later UMAA ICD actually
   defines the corresponding interaction; EO ICD 6.0 §5.2.2 does not.
6. Bounded recovery/journal persistence and thread-safe scheduling around
   asynchronous DDS callbacks remain a separate production hardening task.

No special IDL source or restricted TypeSupport is checked into GitHub.
