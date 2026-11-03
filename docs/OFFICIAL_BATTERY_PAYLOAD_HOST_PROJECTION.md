# UMAA 6.0 EO BatteryStatus — source-checked nested host projection

**Status: SOURCE-VERIFIED STRUCTURE ONLY, not a DDS codec, official Service Runtime, or UMAA conformance.**

This increment stacks on Draft PR #47. The only authoritative source body
used during development is the externally held, public UMAA 6.0 594-IDL ZIP
(SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`).
No source IDL/PDF/RTI XML bytes are committed.

## Pinned payload closure and value model

- `BatteryReportType`: exactly 10 fields. Required: `state`,
  `timeStamp`, `source`, `cellsListMetadata`. Six optional fields:
  `chargeRemaining`, `current`, `energyUsageRate`, `hours`, `temp`,
  `voltage`.
- `BatteryReportTypeCellsListElement`: exactly five fields; required
  `element`, `listID`, `elementID`, `elementTimestamp`; optional
  `nextElementID`. It is a **separate keyed DDS topic**.
- `BatteryCellDataType`: three required fields
  `current`, `temperature`, `voltage`.
- `IdentifierType`: required `id` and `parentID`, both NumericGUID.
- `LargeListMetadata`: required `listID`, `updateElementID`,
  `startingElementID`, `size`; optional `updateElementTimestamp`.
  `size` is an IDL `long` with reviewed 0..2147483647 range.
- `DateTime`: required `seconds` and `nanoseconds`.
- `PowerPlantStateEnumType`: `FAULT`, `OFF`, `ON` — the declared
  enum members, not invented SDK states.
- Scalar typedefs and reviewed source limits are exact: `NumericGUID`
  is `octet[16]`, `DateTimeSeconds` is a signed `long long`,
  `DateTimeNanoseconds` 0..999999999, `EnergyPercent` 0..1000,
  `PowerBusCurrent` -100000..100000, `ElectricalPower` 0..100000000,
  `DurationHours` 0..10505, `Temperature` -273..1000,
  `PowerBusVoltage` -100000..100000.
  These are original *IDL declarations*, not empirical system accuracy.

The pinned metadata are in
`catalog/official-evidence/eo-battery-payload-v1.json`; per-source
digests protect all six IDL source members. The existing original Battery
Topic/Key source ledger is independently cross-checked.

## Host-side projection API and its strict limitations

`tools/official_battery_payload_structure.py` authenticates the ZIP and
source member hashes, checks nested struct field type/name/order/optional/@key,
the source scalar typedefs and numeric limits, and the exact state enumeration.
The scanner supports **only these reviewed IDL declaration shapes**.
Production TypeSupport compilation, CDR serialization, optional ABI layout
and DDS Key semantics still belong to the actual vendor compiler.

`HostProjection` validates a JSON-compatible host value:

- 16-byte NumericGUIDs are represented as 32 lowercase hex digits.
  This is a **project-side interchange representation**, not a mandated
  UMAA wire representation. Parent NIL GUID remains representable.
- Optional fields must be **omitted** to denote absence; JSON null
  is rejected instead of guessing at DDS optional-presence semantics.
- Required fields, nested fields and enum values must match the pinned
  declarations exactly; unknown fields are rejected.
- DateTime has explicit signed 64-bit seconds and bounded nanoseconds.
  IEEE floating-point scalar inputs must be finite and within the exact
  declared IDL ranges. No implicit scaling/rounding of physical units.
- This model validates a *single sample*; it does not claim complete
  metadata/list-element snapshots, linked-list traversal, topic ordering,
  transient-disconnect recovery, DDS Dispose, or a usable native Runtime.

Example (supply exact original ZIP **outside Git**):

    python3 -m tools.official_battery_payload_structure \
      --idl-zip "/private/umaa-6.0-idl-with-exp-services(2).zip" \
      --json "/private/sample-battery-report-host.json" \
      --type report \
      --out "/tmp/battery-host-structure-check.json"

CLI without --json verifies structure only; --type element is for
`BatteryReportTypeCellsListElement`. It **never** generates C++/DDS code
or alters original Service YAML.

## Tests and outstanding gates

`sdk/tests/test_official_battery_payload_structure.py` uses **synthetic**
IDl ZIP contents generated from the reviewed metadata (no original source
redistribution). It checks valid nested Report/Element values, absence versus
null, unknown fields, unsafe GUID, invalid enum, integer overflow, out-of-range
float, malformed nested metadata, and manifest/source/annotation/typedef drift.

Run:

    python -m unittest discover -s sdk/tests -p test_official_battery_payload_structure.py -v

The Linux/Windows `official-icd-source-audit.yml` workflow runs both old
source/QoS audits and the new structural tests. Do not claim success for a new
SHA until its checks finish. Only development-time external original-archive
inspection binds the metadata to authentic official source bytes.

**OPEN:** native vendor TypeSupport-to-SDK value transform, accurate numeric
representation/round-trip across languages, complete DDS QoS, two-topic Large
List atomic update/lifecycle, independently tested official Cyclone/Fast DDS
E2E, Component role/specification review, and formal conformity. Synthetic
Runtime adapters from Draft #44 and the official Runtime fail-closed gate from
Draft #45 remain unchanged.
