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
multiple concurrent sources/lists, full QoS/Component settings, and
independent official E2E for both Cyclone and Fast DDS. The existing
one-Cell Cyclone wire evidence from Draft #49 remains distinct.
