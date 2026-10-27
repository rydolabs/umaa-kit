# Delivery gates and blockers (Do not claim finished until passed)

| Gate | Acceptance test | Current state |
|---|---|---|
| G1 User typed API | Generate selected command/report handlers; user file stays intact | PARTIAL: typed interfaces generated and compiled for 53 reviewed literal-service contract shapes / 113 official Topic types across six ICDs on Linux and Windows (verified dedicated official TypeSupport build+smoke); a transport-bound official BatteryStatus Report writer now owns real DDS writer lifetime and faults shut; queryless reader adapter uses subscription epochs. Runtime adapters for most services remain OPEN; generated EngineControl user module still ignition-only |
| G2 C++ common protocol | Command status/ack/update/cancel/recovery policy, report, collection tests | PASS for tested prototype paths: added generic fail-closed CommandTransport (status/ack/dispose, payload retention, identity/update/deadline validation), deterministic DDS-operation failure injection, and timed Collection tombstone protections; full protocol still OPEN |
| G3 Independent simulator | Runs external process, rejects invalid traces, verifies expected scenarios | PASS for stdio fixture + independent official UMAA Cyclone DDS BatteryStatus/EngineControl Command/Update cross-process scenarios on Windows and Linux; full conformance still OPEN |
| G4 SDK distribution | Install CMake target and import from external consumer | PASS |
| G5 Official IDL 6.0 | Generate and C-compile all AUVSI 6.0 IDL types | **PASS for Cyclone DDS C on Ubuntu: 352/352 generated and 352/352 compiled**, official ZIP SHA-pinned; not equivalent to all UMAA service codegen
| G6 Service catalog | Every generated topic, role, required QoS validated against official Component Definitions/ICDs | OPEN: 352 official IDL files inventoried; 61 directory candidates (not normative service count); 53 curated service sections / 113 Topic types across EO/MO/MM/SA/SEM/SO; all six ICD literal Topic mappings PASS. 61 IDL folder candidates (not normative services), 8 unreviewed. CO/EXP scope and official Component Definitions Provider/Consumer roles, QoS and all-service runtime behavior OPEN. See docs/specification/ICD_CATALOG_AUDIT.md |
| G7 Full protocol | LargeSet/List full DDS atomic semantics, specializations, startup/recovery, liveliness, all reasons | OPEN: timestamped tombstone and single-collection-ID safety guards are locally tested, but DDS ordered/atomic QoS, specializations and network recovery not implemented |
| G8 Cyclone DDS | Real participant/reader/writer over official IDL incl. keyed/Dispose/liveliness/QoS | **PARTIAL:** official BatteryStatus/EngineControl real DDS PASS; transport-bound Battery writer and SDK Reader on Linux use official TypeSupport; independent C oracle retained. Linux 3-node/2-node netns+veth+bridge+tc netem clean, repeated impaired, and 100% blackout recovery PASS (15 scenarios). Reliable DDS ACK gate prevents losing Dispose during teardown. Windows official SDK Reader cross-process CTest PASS alongside independent C oracle; mid-session partition and full QoS/liveliness OPEN
| G9 RTI Connext 7.7 | Build/run same user handler on RTI + licensing | BLOCKED (missing RTI SDK/license) |
| G10 Cross vendor | Cyclone↔RTI both directions with specified QoS and XTypes | BLOCKED |
| G11 Independent conformance | Full ICD+Compliance Specification test corpus and evidence | OPEN (public Compliance Specification and Component Definitions are available from AUVSI, but entire corpus has not yet been traced and independently executed) |
| G12 Production | Fuzzing, race/leak/resource/timeout/security tests, hardware-level functional safety | **PARTIAL ONLY:** Clang UBSan core/module CTest PASS on GitHub Actions; fuzzing, race/leak/resource/timeouts, security and functional safety still NOT VERIFIED |

## Inputs and environment needed to clear blockers

1. Authoritative UMAA 6.0 IDL ZIP is downloaded at CI build time (not redistributed); review its license/TOU and thoroughly assess the publicly available Compliance Specification, Component Definitions, and all 6.0 ICDs.
2. Cyclone DDS C with idlc is installed in Linux and Windows Actions. Expand to CycloneDDS-CXX when reusing upstream SDK Common, pin versions, and capture upstream regression tests.
3. Install licensed RTI Connext 7.7 with `rtiddsgen` and verify supported Presentation QoS; set up two-way DDS interoperability test processes.
4. 53 six-ICD literal service contracts have generated C++ Handler interfaces using official CycloneDDS TypeSupport on Linux and Windows; bridge additional service handlers into real DDS protocol execution and independently test every required behavior.
5. Add the missing protocol rules, dynamic/distributed lifecycle and exhaustive negative tests; do not silently emulate unsupported QoS.

If these gates cannot be met, the correct delivery status is *partial* rather than success.
## 2026-10-27 P2 scoped evidence (does not change gate verdicts)

- The Cyclone Command input path uses a shared fail-closed `SampleReader`.
- Official Linux CTest **6/6** and Windows official CTest **5/5** include an
  independently constructed wrong-Destination Command and verify the host
  explicitly rejects it without publishing CommandStatus/CommandAck.
  See Linux [run 40413028373](https://github.com/rydolabs/umaa-kit/actions/runs/40413028373)
  and Windows [run 40413023193](https://github.com/rydolabs/umaa-kit/actions/runs/40413023193).
- G8 remains **PARTIAL**, G6/G7/G11 **OPEN**, G9/G10 **BLOCKED**, G12
  **PARTIAL**. No newly implemented 51-service runtime or license-backed RTI
  proof is claimed.

## P3 reviewed-topic service runtime progress (2026-10-27)

- 53 reviewed service names and 113 explicit Topic/type/role identities are now
  generated as a checked installable C++ allow-list. Unknown identities and
  unreviewed QoS are rejected **before creating a DDS Topic**.
- `CycloneServiceContext` owns an actual DDS participant and service-bound
  Topic creation, shared by BatteryStatus and EngineControl prototype hosts.
- Additional official `AnchorSpecs` Report and `AnchorControl` Command endpoints
  achieved independent real Cyclone DDS discovery and **default-valued TypeSupport
  sample transport** on Ubuntu ([10/10](https://github.com/rydolabs/umaa-kit/actions/runs/40426885356))
  and Windows ([9/9](https://github.com/rydolabs/umaa-kit/actions/runs/40426879585)).
- **G1/G8 remain PARTIAL; G6 remains OPEN.** Neither new service has a semantic
  Handler, required QoS/keys, CommandStatus/Ack/Dispose or normative payload
  oracle. `dds_qos_verified` is false for every service and an unsubstantiated
  flag cannot enable strict production mode. RTI and certification remain open.

## 2026-10-27 P4 Anchor semantic Report/Command wire evidence (scoped)

- Official `AnchorSpecsReportType` emits and independently verifies actual
  `anchorHoldingPower` 1500→1750 and `rodeLength` 35→42,
  UUID, increasing timestamp, and keyed DDS Report Dispose.
- Official `AnchorControl` CommandStatus/Ack types now execute virtual
  LOWER/RAISE/STOP actions; independent DDS oracle verifies Command, UPDATE,
  ACK, status transitions and reasons, cancel/Dispose and rejection of
  wrong-destination without any Status/Ack.
- [Linux P4 CI](https://github.com/rydolabs/umaa-kit/actions/runs/40450127689):
  CTest **15/15** and network namespace **15/15**.
  [Windows P4 CI](https://github.com/rydolabs/umaa-kit/actions/runs/40450127737):
  CTest **14/14**, genuine Cyclone CMake export Consumer PASS.
  Final feature commit `5ad1131`; merged `main` commit `ed4af11`.
- P4 advances **two additional service slices**, not two fully conformant
  services: field units, Component QoS, definitive key naming, physical
  feedback, concurrent operation, every required behavior and vendor
  interoperability are still unproven. G1/G8 remain **PARTIAL**,
  G6/G7/G11 **OPEN**, G9/G10 **BLOCKED**, G12 **PARTIAL**.

## 2026-10-27 P4.1 extended genuine DDS proof (scoped, merged)

- Canonical merged implementation commit: `d59612c333cd95599f3f69ba064d71551961c376`
  (source tree `c4dcb99cbd665dd2ab28bde19abaea8f95e36bdf`).
- Existing AnchorControl tests and their docs remain intact. The additional
  `official_umaa_anchor_control_extended_{normal,update,raise,bad-destination,complete,fail}`
  tests exercise a **separate** service host and independent C oracle using
  genuine AUVSI UMAA 6.0 TypeSupport compiled in ephemeral CI workspaces.
- Extended AnchorSpecs sends different non-default `anchorDescription`,
  `anchorSize`, `anchorHoldingPower`, `anchorHoldingPowerRatio`,
  `rodeLength`, `rodeSize`, `rodeWorkingLoadLimit`, source UUID and
  timestamp values; the oracle checks each value, update order and Dispose.
- AnchorControl checks LOWER/RAISE/STOP command values and exact Acks,
  ISSUED/COMMANDED/EXECUTING, later-timestamp Update, Cancel, terminal
  Status/Ack Dispose, and wrong Destination with no wire response. Two
  **test-only virtual actuator** signals additionally exercise:
  `COMPLETED/SUCCEEDED` and `FAILED/OBJECTIVE_FAILED`, followed by Dispose.
- [Merged-main Linux DDS run 40451709970](https://github.com/rydolabs/umaa-kit/actions/runs/40451709970):
  real official DDS CTest **22/22** and isolated netns/netem **15/15**, including
  impairments and recovery.
- [Merged-main Windows DDS run 40451709996](https://github.com/rydolabs/umaa-kit/actions/runs/40451709996):
  official DDS CTest **21/21**, external installed Cyclone CMake consumer and
  actual MSVC native tests PASS.
- [All five merged-main workflows](https://github.com/rydolabs/umaa-kit/actions)
  completed SUCCESS for `d59612c`.
- These results are **real-wire proof of narrow virtual service scenarios**,
  not measured hardware control, full AnchorControl ICD/Component QoS, all
  service runtime, all Collection semantics, RTI interoperability, official
  conformance or production safety. G1/G8 stay PARTIAL; G6/G7/G11 OPEN;
  G9/G10 BLOCKED; G12 PARTIAL. 
