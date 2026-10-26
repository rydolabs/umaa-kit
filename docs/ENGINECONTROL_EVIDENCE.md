# Official UMAA 6.0 EngineControl — real DDS integration evidence

**Status (2026-10-23):** partial EngineControl service coverage; end-to-end _Ignition State subset_ verified on Linux and Windows. Not full UMAA conformity or K-MOSA certification.

## Proven pipeline

1. CI downloads original AUVSI UMAA 6.0 ZIP to ephemeral storage and checks SHA-256:
   `9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411`.
2. Cyclone `idlc` generates TypeSupport from **actual** `EngineCommandType`, `EngineCommandStatusType`, `EngineCommandAckReportType` and their transitive dependencies.
3. `tools/generate_official_engine_plugin.py` verifies the official generated `state`, `source`, `destination`, `sessionID`, `timeStamp` layout and enum `OFF=0/RUN=1` before generating the typed `EngineControlHandler` interface and versioned C module glue. Unexpected schema expansions are hard failures.
4. The **only user-owned behavior file** in the demo is `sdk/examples/engine_control_user_handler.cpp`. The SDK generator does not overwrite it.
5. `dds/cyclone/src/official_engine_host.cpp` loads the generated SO/DLL module and owns the DDS entities, C++ common `CommandEngine`, Command→Ack/Status mapping, and session clean-up.
6. The independently compiled `simulator/dds/official_engine_simulator.c` shares **no SDK code**. It sends official DDS Command instances and verifies status reasons, Ack payload, Command Dispose, and Status/Ack keyed instance disposal.

## GitHub-hosted acceptance runs

| Gate | Native environment | Proof |
| --- | --- | --- |
| Official `BatteryReportType` + EngineControl Command/Update DDS CTests | Ubuntu 24.04, Cyclone DDS 0.10.4 | [Linux official-IDL job, CTest 3/3](https://github.com/nssang00/umaa-dev/actions/runs/39962602416) |
| Same **three** real DDS tests with native generated DLL | Windows 2022, MSVC, vcpkg Cyclone DDS 0.10.5 | [Windows official-IDL job, CTest 3/3](https://github.com/nssang00/umaa-dev/actions/runs/39962717265) |
| Code generators, core protocol, native plugin loader, independent stdio simulator regressions | Ubuntu/GCC, Ubuntu/Clang, Windows/MSVC | [Cross-platform prototype CTest 4/4](https://github.com/nssang00/umaa-dev/actions/runs/39962717654) |

### Mandatory independent DDS observations

Baseline command:
- `EngineCommandType` issued with RFC4122-shaped IDs, source and destination, and session.
- Ack reports the exact original command/session.
- Status topic: `ISSUED(SUCCEEDED) → COMMANDED(SUCCEEDED) → EXECUTING(SUCCEEDED)`.
- Because EO ICD §6.1.6.3 defines no determinate execution end, the Host **does not auto-complete**. The Consumer disposes its Command to cancel.
- Status topic: `CANCELED(CANCELED)`; then keyed Status and Ack disposed.
- DDS may communicate the disposed instance state on a **valid terminal Status data sample**, not necessarily as a distinct invalid-data sample; the independent verifier inspects `dds_sample_info.instance_state` in either representation.

Command Update:
- Same source and session; strictly newer `timeStamp`; ignition state changes `RUN → OFF`.
- Provider cancels previous virtual activity then emits `ISSUED(UPDATED) → COMMANDED(SUCCEEDED) → EXECUTING(SUCCEEDED)`, with an updated Ack.
- Consumer disposes Command; independent Simulator verifies CANCELED and both Status/Ack dispose events.

## Scope exclusions / remaining gates

- **Subset binding:** `plugState` and `propulsion` are not exposed by the generated demo Handler. The scalar C ABI is explicitly an **ignition-only interface**, not arbitrary UMAA command serialization or vendor-neutral full IDL service codegen.
- **Demo physical behavior:** Handler operates a virtual engine only. No hardware actuation, acceleration, actuator feedback, resource arbitration, authority checks or safe physical stop has been established.
- **Not complete ICD compliance:** Full service topic/QoS rules, late joiner, liveliness, persistence/restart recovery, all failure paths, collection ordering, and exact service Source/parent Identifier semantics are not covered by this narrow integration.
- **DDS vendor limitation:** Only Cyclone DDS C TypeSupport is exercised. RTI Connext and CycloneDDS-CXX SDK Common integration are not tested.
- **Dynamic module limitation:** The custom ABI is versioned and tested for load/stop/unload, but is NOT a normative K-MOSA API or safe active-command hot swap.

The evidence proves a reproducible real DDS command and report vertical slice. It is **not** an entire UMAA 6.0 SDK acceptance or formal conformance verdict.
