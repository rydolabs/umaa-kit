# SDK Core API v1 — initial contract and compatibility slice

**Status:** development API; *not* UMAA 6.0 conformance and *not* a complete
Provider/Consumer network implementation. This branch preserves existing
Component C ABI v2, DDS Backend C ABI v1 and QoS extension v2 unchanged.
No licensed or restricted official IDL/ICD source is copied into this commit.

## Four independent axes

| Axis | Contract | Engine responsibility |
| --- | --- | --- |
| Flow control | `FlowKind::{CommandResponse,QuerylessRequestReply,QueryDataRequestReply}` | Command FSM + correlated Ack/Status, Report lifecycle; QueryData has **no** implementation yet |
| Service types | `ServiceKind::{Config,Control,Specs,Status,Report}` | Semantic metadata, not a forced flow mapping |
| Large collections | `CollectionShape::{None,LargeSet,LargeList}` | Existing `CollectionAssembler` validates metadata, ordered elements and lifecycle, separate real-wire topics remain outstanding |
| Generalization/specialization | `SpecializationRef`, `SpecializationJoin<G,S>` | Reject mismatched revisions and incomplete snapshots; generator must supply official IDs, timestamps and type bindings |

## Public typed contract

`sdk/include/umaa/sdk_v1_core.hpp` provides `Guid128`, `Identifier`,
`CommandIdentity`, `ServiceDescriptor`, `ServiceTraits<T>`, `ServiceCatalog`,
and `SpecializationJoin<G,S>`. `Guid128` uses exactly sixteen bytes; no
truncation to the existing component ABI v2 `uint64_t` session identifier is
allowed. The string form is canonical UUID notation and is only an internal
compatibility key until the DDS binding supplies the official multi-field key.

```cpp
struct BatteryStatus {};
struct BatteryReport { /* reviewed business representation */ };
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<BatteryStatus> {
    using Report = BatteryReport;
    inline static constexpr ServiceDescriptor descriptor{
        "UMAA::EO::BatteryStatus", "reviewed-schema-id",
        FlowKind::QuerylessRequestReply, ServiceKind::Status,
        CollectionShape::LargeList};
};
}
```

A generated service descriptor must use a reviewed ICD mapping, actual Topic
bindings, and official IDL TypeSupport before it can be used on a genuine UMAA
DDS network. Empty IDs are rejected; missing traits fail at compilation.
`ServiceCatalog` prevents duplicate service registration by ID. ServiceTraits
are a type contract, **not** a claim that the codec or QoS has been validated.

## Reuse of existing protocol engines

`sdk/include/umaa/sdk_v1.hpp` provides non-DDS wrappers:

- `ReportProvider<Service>` emits existing `ReportIntent` objects via
  `ReportProviderLifecycle`; `ReportConsumer<Service>` delegates to the keyed,
  epoch-fenced `ReportConsumerLifecycle`.
- `CommandProvider<Service>` delegates to `CommandEngine`; `CommandConsumer<Service>`
  delegates to `RemoteCommandMonitor`. The legacy transport-neutral command
  identity string is now derived from the **full** parent/source/destination
  128-bit GUIDs and 128-bit session ID rather than dropping high bits.
- `CollectionConsumer<Service>` delegates to `CollectionAssembler`; a valid
  snapshot is returned only when existing element+metadata checks succeed.

The wrappers do **not** silently open DDS readers/writers, issue DDS ACKs, or
run callbacks. They expose protocol engine operations for the next Application
and Component runtime integration. Device safety, actual cancellation,
publication pacing, deadline/liveliness reconciliation, DDS QoS, TypeSupport,
and resource limits must still be managed by the runtime and reviewed service
contracts. No automatic command replay is permitted after ambiguous delivery.

## Generalization / specialization pairing

`SpecializationJoin<General,Specialized>` handles independent arrival order:

1. `put_general(id, timestamp, value)` and
   `put_specialization(id, timestamp, value)` are independently accepted only
   when newer than that topic's sample and any deletion tombstone.
2. `resolve({id,timestamp})` succeeds only when *both* halves match the same
   requested identity and timestamp. A one-sided update hides the old pair.
3. `dispose(id, timestamp)` invalidates both halves and retains a bounded
   tombstone; stale traffic cannot resurrect the deleted pair.
4. `resolve_collection(assembler.snapshot(), projector)` returns no snapshot
   until every element's related pair resolves. It does not mutate the
   assembler's last committed state or claim DDS GROUP atomicity.

This pairing uses strict equal revision timestamps as an initial conservative
policy. The ICD-specific bindings must validate whether a given
Generalization/Specialization relationship requires equal revision timestamps
or some more detailed relation; unsupported mappings must fail closed.
The SDK has no eviction/persistence policy for tombstones yet. Once
`max_instances` is reached it rejects new identities; trusted resync may
create a new store.

## ABI policy

1. Existing `umaa_get_component_v2`, `umaa_get_dds_backend_v1`, and optional
   `umaa_get_dds_qos_backend_v2` symbols and structure layouts are unchanged.
2. Never add fields in-place to existing ABI structs. A future optional
   ABI extension will provide a versioned `UmaaGuid128`, multi-key identity,
   reception metadata, liveliness event and capability negotiation.
3. Boundary ownership remains borrowed for each call; no C++ STL, exceptions,
   or allocator ownership may cross the C ABI. Unknown QoS/schema/key types
   fail before network traffic; ambiguous delivery is not retried implicitly.
4. A business DLL/SO must not import vendor DDS headers. Generated TypeBindings
   remain in vendor adapters and are checked against reviewed service traits.

## Test and release gate

New CTest targets:

```sh
cmake -S sdk -B build/sdk -G Ninja
cmake --build build/sdk --target umaa_sdk_v1_core_test umaa_sdk_v1_bridge_test
ctest --test-dir build/sdk --output-on-failure -R 'umaa_sdk_v1_(core|bridge)_test'
```

Tests cover C++17 contract compilation, GUID roundtrip, 128-bit session
uniqueness, duplicate service IDs, mismatched/late specialization halves,
tombstone fencing, Report epoch/disposal, Command update/cleanup, and List
metadata/ordering. These are deterministic in-process engine tests, **not**
DDS network tests, TypeSupport tests, or full UMAA behavior certification.

## Remaining acceptance gates

- Wire official `IdentifierType` (parentID and id) / `NumericGUID` keys and
  official timestamps into the DDS Backend. The old `uint64_t` module session
  and single-string DDS key cannot transport normative multi-key identity as-is.
- Integrate this typed contract with `Application`, `Component`, and stable
  shared runtime; expose provider and consumer callbacks that contain business
  logic only. Harmonize existing `CommandHandler`/`Handler` APIs.
- Add liveliness/restart reconciliation and exact per-service Command reasons,
  CommandAck full-command correlation and optional Execution Status.
- Implement native large-collection Element + Metadata DDS topics, verified
  presentation QoS, read/writer interleaving, and nested specialization trees.
- Complete QueryData if a reviewed service requires it; do not generalize a
  synthetic RPC into an official UMAA Request/Reply by name alone.
- Re-run CI and official DDS tests on the actual source-reviewed mappings;
  verify real DDS backends independently. No formal UMAA conformance claim.
