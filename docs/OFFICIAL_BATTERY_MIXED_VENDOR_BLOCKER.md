# UMAA 6.0 BatteryStatus Cyclone ↔ Fast DDS blocker

**Status: UNVERIFIED — DO NOT MERGE / DO NOT ENABLE OFFICIAL RUNTIME.**

## Source and environment

- AUVSI UMAA 6.0 original public with-EXP archive SHA-256:
  `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`
- Battery Report original IDL member SHA-256:
  `39a2c920cfaba9afc3c15c4c12f5d4470911ab8b7c197c9e27b08284e54b713c`
- Ubuntu 26.04 container with native Cyclone DDS `idlc` and modern
  Fast DDS 3 + Fast DDS-Gen 4. No original source files are committed.
- Independent same-vendor official Cyclone and Fast DDS 3 nested
  Report/Cell/Dispose negative-oracle jobs **PASS** on PR #50 and #49.

## Facts established from actual native interop CI

1. Original generated DDS Topic type names match byte-for-byte:
   `UMAA::EO::BatteryStatus::BatteryReportType` and
   `UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement`.
2. Initial failure: Fast DDS reader reported DataRepresentation
   incompatible QoS policy 23. XCDR2 was then explicitly offered and
   requested on both the Cyclone and Fast DDS test endpoints.
3. After XCDR2, independent Fast DDS Reader reported historical matched
   writers, but **no real Report/Cell DATA or Dispose** and eventually
   zero currently matched writers. Cyclone Writer reports
   `total_matches=0 current_matches=0 offered_incompatible=0`.
   This suggests asymmetric discovery/type matching, but does not prove
   the root cause.
4. For clarity, [run 41755501412](https://github.com/rydolabs/umaa-kit/actions/runs/41755501412)
   also fixed Fast DDS to **UDPv4 only** (rather than its default
   UDP + SHM) and executed **all four** original mixed-vendor tests
   independently. All four failed — neither vendor actually received
   the other's Battery data. Cyclone → Fast failed discovery on the
   Cyclone writer, Fast → Cyclone failed its reliable Cell ACK with the
   independent Cyclone reader reporting zero data.
5. A deliberately invalid checkpoint **MUST NOT PASS** simply because
   no messages were received or the writer timed out. The independent
   oracle requires verified genuine Report+Cell data, both keyed DDS
   Dispose events, and an explicit one-nanosecond mismatch rejection.

The related public Cyclone DDS [issue #2184](https://github.com/eclipse-cyclonedds/cyclonedds/issues/2184)
documents a similar *asymmetric endpoint discovery* symptom, but it is
**not evidence** that the internal root cause here is the same.

## Next independent, bounded investigation

- Inspect full Cyclone DDS **discovery + topic + parameter-list traces**
  for SEDP participant/Reader proxy creation, type assignability,
  type identifiers, matched subscriptions and locator addresses. Printing
  only the last shutdown lines is insufficient.
- Contrast original native Fast DDS-Gen **TypeObject** / XTypes IDs with
  Cyclone idlc's original TypeInformation; do **not** weaken
  `@optional`, Key identity, or type-consistency verification.
- Run a small non-normative *control IDL* over the same two DDSI
  transports to distinguish basic RTPS discovery from the Battery
  original XTypes schema. This cannot substitute for the Battery proof.
- If discovery remains asymmetric, run the two processes in isolated
  endpoints/network interfaces and verify SPDP, SEDP and bidirectional
  unicast locators rather than assuming Docker bridge multicast works.
- Repeat the full four-case original Battery test only after a
  source-backed change to the transport or TypeObject configuration.
  Do not rerun unchanged jobs until green by chance.

**No claimed interop, production QoS, conformant Component implementation,
or automatic official Runtime approval until the four native positive and
negative end-to-end cases prove actual wire samples.**
