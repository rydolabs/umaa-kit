# Official BatteryStatus: bounded Large List event joiner

Status: SDK host-side **event coherence candidate**, NOT a complete DDS
Collection Service, not a full QoS audit, and not a UMAA conformance claim.

Source: original public UMAA 6.0 EO ICD paragraphs 3.8.1–3.8.5 and
official `BatteryReportType` / `BatteryReportTypeCellsListElement`
IDL (pinned and verified in Draft PRs #47–#49). No original source bytes
are embedded here.

## What is implemented

`sdk/include/umaa/official_battery_collection.hpp` provides a bounded,
vendor-neutral `LargeListJoiner` consuming:

- `on_report(Report)`: original Report, IdentifierType and list metadata;
- `on_element(ListElement)`: separately keyed, timestamped Cell data
  samples that can be received before or after the Report;
- `on_dispose(listID, elementID)`: **only after a real DDS reader has
  observed NOT_ALIVE_DISPOSED and recovered the two original instance keys**;
- `latest_stable()`: a snapshot only when metadata, exact ID and
  timestamp checkpoint, full element count, and complete linked-list chain
  match. For delete-final updates whose metadata omits the timestamp,
  an actual matching Dispose event is required; otherwise remain pending.
- `reset_after_disconnect()`: invalidates earlier snapshots/buffers and
  requires a complete new stream of records to establish stability.

The joiner bounds records, rejects unknown source/list IDs, stale element
versions and non-monotonic Report timestamps, and detects missing/cyclic
links and mismatched sizes. It retains last committed snapshot while the
next batch is incomplete, and holds a bounded pending-Report queue rather
than inventing a cross-topic ordering guarantee.

## Bounded multi-list routing (candidate, Draft PR #58)

`sdk/include/umaa/official_battery_collection_router.hpp` implements
`LargeListRouter`, a vendor-neutral, bounded dispatch layer over independent
`LargeListJoiner` instances indexed by the **actual Collection listID**:

- Interleaved Report, Element and genuine keyed Dispose callbacks for distinct
  list IDs cannot mix their host-side buffered records or stable generations.
  Report-first and Element-first initial sequences are independently supported.
- `active_lists()` and a configurable maximum (default 16, cap 128) enforce
  bounded concurrent lists. The aggregate configuration caps
  `max_lists * max_elements_per_list` at 262144 records; the router never
  silently evicts an active list.
- `retire_list(listID)` explicitly discards a known-ended list's buffer.
  `reset_after_disconnect()` clears **all** buffered/stable lists; none is
  treated as valid until a complete new event stream is reconstructed.
- Invalid first events do not allocate a list slot. An oversize or invalid
  NIL Report is rejected **before** modifying a joiner's Report timestamp
  or pending queue, so an otherwise valid next Report can still be accepted.
- A second Report source using an already active listID is rejected by the
  underlying joiner. The original List Element does not carry a Source
  Identifier; the router **must not guess source/list ownership**. It does
  not resolve simultaneous reuse of a listID by multiple real sources.

This is a **host-only convenience** and does NOT establish that the real
DDS reader verified TOPIC ordered Presentation, has complete callback
coverage, correctly extracted Dispose keys, or implements reconnect replay.
No native DDS coherence proof or formal UMAA approval flag is set.

## Mandatory external precondition

The caller must select
`ElementOrdering::CallerVerifiedTopicOrdered` only after verifying the
Collection **ELEMENT** Writer and Reader `PRESENTATION` Topic scope and
`ordered_access=true` at real DDS creation time, per EO ICD §3.8.1.
The value is a **caller assertion**, not verified by this C++ class.
`ElementOrdering::NotVerified` rejects all events.

The returned `nativeDdsCoherenceProved` is always `false`. Host tests
do not prove that real DDS QoS matched, callbacks were complete, a source
never skipped a sequence, a network reconnection replayed old instances,
or independent vendors interoperate. Do not lift the official Runtime
generation refusal gate on these unit tests alone.

## Demonstrated / open behavior

New standalone C++17 suite:
`sdk/tests/official_battery_collection_joiner_test.cpp`.

- Metadata-first and element-first initial publication
- Three-element batch update with newest ID+DateTime checkpoint
- Deletion of an intermediate element, repaired `nextElementID` chain
- Delete-final-element signal via true Dispose event and absent timestamp
- Empty initial collection via NIL update UUID
- Rejection of replayed elements/Reports, foreign lists, invalid times,
  cycles, buffered overflow and unverified Topic ordering
- Explicit reset after disconnect before reconstructing a stable snapshot

The host tests run on **Ubuntu and Windows** via
`.github/workflows/official-battery-list-joiner.yml`.

Still required: vendor-specific real DDS callback adapters, native
Dispose-key extraction, actual network discovery/recovery replay,
source/list ownership and duplicate-ID collisions, full QoS/Component settings, and
independent official E2E for both Cyclone and Fast DDS. The existing
one-Cell Cyclone wire evidence from Draft #49 remains distinct.
