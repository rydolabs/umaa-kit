# Official UMAA 6.0 BatteryStatus native Fast DDS 3 wire proof

Status: **selected-service original-TypeSupport prototype**, not a reviewed
Component Service Runtime, full QoS, Large List transaction or certification.

## Original source and generated-ABI safety

- Original external UMAA 6.0 with-EXP archive:
  SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`.
  Original `UMAA/EO/BatteryStatus/BatteryReportType.idl` member:
  SHA-256 `39a2c920cfaba9afc3c15c4c12f5d4470911ab8b7c197c9e27b08284e54b713c`.
- Do not reuse Ubuntu 24.04's Fast DDS-Gen 2.3 for official Battery:
  it compiles the source but **drops `@optional` presence** and exposes
  scalar/native fields instead of optionals. Such output is **NOT**
  lossless for the official payload.
- Use a matched modern Fast DDS 3.x, Fast CDR 2.x and Fast DDS-Gen 4.x
  toolchain. Dedicated CI uses an Ubuntu 26.04 container, with original
  source files and generated native C++ kept in runner temp.
- The actual binding in `dds-backends/fastdds/include/umaa/fastdds_official_battery_codec.hpp`
  statically checks that generated Charge, Cell-next-ID and Report
  checkpoint members retain optional presence semantics. Old/unsafe
  generated ABI fails **compilation** rather than silently assigning
  default values.

## Independently exercised two-process evidence

- `tests/integration/fastdds/official_battery_payload_publisher.cpp`
  populates the SDK host model (six present optional measurements, source
  IdentifierType, nontrivial DateTime, PowerPlantState, LargeListMetadata,
  separate Cell with two NumericGUID Keys) and converts it to Fast DDS-Gen
  C++ native values, verifying the host round trip.
- A separate executable
  `tests/integration/fastdds/official_battery_payload_oracle.cpp`
  links only original vendor-generated Fast DDS types, **not SDK
  headers/codecs**. It observes and validates Report + Cell samples,
  both keyed DDS Dispose events, and exact metadata checkpoint ID/time.
- Negative source mutation: Report updateElementTimestamp differs by one
  nanosecond; an independent oracle must reject the mismatch and exit
  with a specific nonzero code. A failure to discover or a timeout is
  **never accepted** as this expected rejection.
- `.github/workflows/official-battery-fastdds-wire.yml` downloads and
  authenticates the original SHA-256-pinned archive in a temporary workspace,
  checks original nested declarations, runs real Fast DDS-Gen and compiles
  the two native executables. It executes both CTests over real Fast DDS
  discovery and on-wire samples, not simulated events.

## Measured CI and incomplete work

The exact code checkpoint `ba811c8787c0e157bbffbc5c5432bd55cf5423f1`
passed **20/20** GitHub PR checks on Draft #50, including the dedicated
original source native Fast DDS positive + negative integration job
[run 41742497858](https://github.com/rydolabs/umaa-kit/actions/runs/41742497858).
Any later documentation or code SHA requires fresh check verification.

**Still OPEN:** cross-vendor Cyclone ↔ Fast DDS interoperability, missing
Component Specification and full domain/partition/reliability/history/
deadline/durability QoS requirements, production handler deployment, full
Large List multi-element mutation/recovery and source-vendor DDS key/Dispose
edge cases, and official UMAA conformance. The synthetic Fast DDS backend
remains explicitly synthetic; this proof does NOT enable the official
Runtime codegen gate from Draft #45.

Original external IDL and generated TypeSupport are NOT committed or
uploaded to GitHub.
