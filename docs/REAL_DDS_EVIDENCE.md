# Real Cyclone DDS proof — SDK + independent Simulator (2026-10-23)

## Verified observations (GitHub-hosted runners)

| Test case | Runner / Cyclone | Verified outcome | Primary run |
| --- | --- | --- | --- |
| Synthetic Battery test IDL + SO plugin | Ubuntu 24.04 / Cyclone C 0.10.4 | 20→80 and keyed Dispose from DDS, independent process CTest 1/1 | [39950750899](https://github.com/nssang00/umaa-dev/actions/runs/39950750899) |
| Synthetic Battery test IDL + DLL plugin | Windows 2022 / vcpkg 0.10.5 | 20→80 and Dispose, independent CTest 1/1 | [39952395220](https://github.com/nssang00/umaa-dev/actions/runs/39952395220) |
| **Official AUVSI UMAA 6.0 BatteryReportType + SO plugin** | Ubuntu 24.04 / Cyclone C 0.10.4 | Generated from official IDL, 20→80 `chargeRemaining`, Dispose, CTest 1/1 | [39952219497](https://github.com/nssang00/umaa-dev/actions/runs/39952219497) |
| **Official AUVSI UMAA 6.0 BatteryReportType + DLL plugin** | Windows 2022 / vcpkg 0.10.5 | Generated from official IDL, 20→80 `chargeRemaining`, Dispose, CTest 1/1 | [39953508112](https://github.com/nssang00/umaa-dev/actions/runs/39953508112) |
| Official EngineCommandType + EngineCommandStatusType TypeSupport | Ubuntu 24.04 | Cyclone `idlc` dependency closure generated and C code compiled; **no actual command DDS process test** | [39952552116](https://github.com/nssang00/umaa-dev/actions/runs/39952552116) |

**Original AUVSI ZIP**: `https://www.auvsi.org/wp-content/uploads/2025/03/umaa-6.0-idl.zip` ; observed archive SHA-256 `9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411` ; 492 entries, 352 IDL files. CI verifies the pinned hash before use. Source IDL is **never committed or uploaded in artifacts**. Review AUVSI IDL Terms of Use before any distribution.

## Independence boundary

`sdk/tests/modules/battery.cpp` is a **user-logic prototype module**; `dds/cyclone/src/official_battery_publisher.cpp` is **handwritten experimental host glue** using generated official TypeSupport. It loads the plugin, creates the actual DDS writer and publishes an optional `chargeRemaining` value. `simulator/dds/official_battery_subscriber.c` is an independently compiled executable that does **not** import SDK-internal state machines. It validates two received data samples and an instance Dispose event.

## Exclusions (NOT proven)

- UMAA 6.0 entire-ICD or Compliance Specification certification.
- Official DDS fully qualified Topic name mapping (currently uses `BatteryReportType` as on-wire test Topic), proper Source GUID, Timestamp, realistic LargeList metadata, mandatory optional fields and full service QoS.
- Official `EngineControl` Command, Status, Ack and Update/Cancel flow **over the wire**. Only static TypeSupport compilation has been tested.
- Full UMAA IDL to user Handler code generation: the current general generator implements a synthetic subset; the official binding uses handwritten host glue.
- DDS vendors other than Cyclone; RTI cross-vendor interoperability.
- Safe active-command Hot Swap; security/signing or K-MOSA formal conformance.

The result supports an **official UMAA type serialization and transport proof**, not a normative service compliance claim. The broader SDK+Simulator milestone is **partial**.

## EngineControl and all official IDL Types (additional evidence)

- [Linux EngineControl Command/Ack/Status/Cancel/Dispose and later-Timestamp Update](https://github.com/nssang00/umaa-dev/actions/runs/39962602416): official TypeSupport, auto-generated ignition Handler/SO, independent DDS Simulator, CTest 3/3 (includes BatteryStatus).
- [Windows equivalent EngineControl + BatteryStatus proof](https://github.com/nssang00/umaa-dev/actions/runs/39962717265): official TypeSupport, generated Handler/DLL, independent DDS Simulator, CTest 3/3.
- [Full official 352 IDL TypeSupport compile coverage](https://github.com/nssang00/umaa-dev/actions/runs/39963083783): 352 generated and 352 C files compiled on Ubuntu; no SDK end-user codegen conclusion from this tally.

Detailed trace and exclusions: [ENGINECONTROL_EVIDENCE.md](ENGINECONTROL_EVIDENCE.md).
