# EO BatteryStatus: real original Cyclone TypeSupport to SDK host value proof

Status: **prototype, scoped evidence**. This increment builds on Draft #48
and does not authorize official UMAA Runtime generation in the Component
codegen, release, or conformance claims.

## Actual implementation

- `sdk/include/umaa/official_battery_host.hpp` defines the
  DDS-vendor-neutral value model: source IdentifierType/16-octet GUID,
  timestamp, six optional electrical measurements, PowerPlantState,
  LargeListMetadata and a separate cell ListElement. No vendor headers.
- `dds-backends/cyclone/include/umaa/cyclone_official_battery_codec.hpp`
  converts these values into **actual compiler-generated C TypeSupport**
  `UMAA_EO_BatteryStatus_BatteryReportType` and
  `UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement` and
  decodes them into host values. Optional pointer backing storage is held
  inside noncopyable owner objects until synchronous writer calls return.
  No manual CDR serialization or source-vendor type assumptions other than
  explicitly reviewed original field shapes.
- `tests/integration/cyclone/src/official_battery_payload_publisher.cpp`
  creates two DDS writers and publishes a nested Report and Cell List element,
  then two keyed Dispose events, using ephemeral official AUVSI TypeSupport.
  It locally round-trips the host values through the native mapping before
  transport.
- `simulator/dds/official_battery_payload_oracle.c` is a
  **separate C process** and contains no SDK headers/codecs. It validates
  both Topic samples, original Key fields, all present Report optional
  measurement values, cell contents, distinct List/Element IDs, metadata,
  timestamps and both DDS Dispose notifications.
- Negative oracle: with `--bad-checkpoint` the publisher
  emits an inconsistent Report metadata update timestamp. The independently
  executed C oracle must reject the mismatch with an exact error status,
  not merely time out or accept the publisher's own output.

## Source, QoS and reproducibility

The original public UMAA 6.0 with-EXP ZIP has SHA-256:
`15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`.
The source archive is downloaded at CI time to Runner temp, checked by
SHA-256, and processed by the existing source-pinned Battery IDL structural
validator. Original IDL and generated TypeSupport are never committed or
uploaded as artifacts.

The new `.github/workflows/official-battery-native-wire.yml`
uses a small original-source compile closure rather than recompiling 594
types and runs two named CTest scenarios on Ubuntu/Cyclone DDS.
The historical official Battery, Engine and native **synthetic** parity
regressions remain unchanged and run separately.

The code explicitly sets TOPIC+ordered PRESENTATION on publisher and
subscriber (the collection-element minimum from EO ICD §3.8.1).
Reliability/History are **prototype settings, not fully reviewed mandatory
QoS**. The same participant carries the two distinct topics, but DDS does
not guarantee their relative ordering. The independent oracle buffers
observed report/element fields and only compares the checkpoint after both
arrive.

## Formal limitations / next steps

This proof does not integrate a production Device Component Handler into
the official Battery service, verify full Queryless Report provider restart
semantics, deliver full Large List mutation/delete/tombstone replay or
recover from network partitions. It uses one cell, one Report update,
and one dispose per Topic, plus an invalid-checkpoint scenario.
It does not prove DDS ordered access across multiple collection updates
or GROUP cross-topic ordering.

Fast DDS has a separate native TypeSupport generator/backend and still
requires the **same official** nested host-field mapping, a reviewed QoS
configuration, and independently run Fast DDS/possibly mixed-vendor wire
tests. Do not describe the existing synthetic Fast DDS CI as proof of
this official payload.

The official Runtime fail-closed gate from Draft #45 remains in effect.
Reviewer acceptance of the full source mapping, complete QoS/Component
requirements, lossless native codec coverage, and independent vendor
negative tests are needed before that gate can change.


## 2026-11-05 — SDK-owned native Cyclone DataReader (scoped candidate)

`dds-backends/cyclone/include/umaa/cyclone_official_battery_reader.hpp`
implements `OriginalBatteryStatusReader` for the original verified EO
BatteryStatus two-Topic subset. The SDK-owned Cyclone adapter now creates
and deletes the real Participant, original TypeSupport-backed Report and
Cell Topics, Subscriber, and DataReaders. It performs an **effective
Subscriber** TOPIC+ordered-access QoS query, rather than trusting only
the user-requested value. Reliability and History(16) are **demo choices**,
not declared official per-Component defaults.

Native `dds_take` samples are owned by scoped guards invoking the
correct original generated TypeSupport `dds_sample_free` even after
decoder/Handler exceptions. Actual `instance_handle` → original
Source Identifier (including ParentID) and (listID, elementID) Keys
are tracked independently for Report and Cell Topics; only a true
`NOT_ALIVE_DISPOSED` event with a previously observed native handle
can retire a source/list. Unknown instance states fail closed.

The application in
`tests/integration/cyclone/src/official_battery_sdk_native_reader_consumer.cpp`
only implements typed `BatteryStatusHandler`, polls the adapter,
verifies two separate source snapshots, and explicitly restarts
and stops the reader. Normal and intentionally mismatched second
List checkpoint oracles use a separate sender process with the actual
original, SHA-pinned AUVSI IDL-generated wire support.

**Remaining:** this is a limited Cyclone-only source-backed native
adapter, not a plugin-neutral or automatically generated official
Runtime for every UMAA Service. Native Publisher QoS and cross-Topic
GROUP coherence are not proven from local Subscriber QoS, real
network-partition continuation/replay is not implemented, Fast DDS
ordered TOPIC behavior is still unsupported, and complete original
Component/Platform requirements remain unresolved. The generic
official Runtime generation guard remains active.
