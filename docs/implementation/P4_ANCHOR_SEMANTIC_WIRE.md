# P4 Anchor semantics: prototype independent real DDS verification

This checkpoint extends the existing official UMAA 6.0 **AnchorSpecs** and
**AnchorControl** TypeSupport smoke tests. Every implementation is a **limited
prototype**, not a normative UMAA DDS service conformance claim.

## Report lifecycle

The AnchorSpecs host publishes two reports with independently checked
non-zero `anchorHoldingPower` and `rodeLength` values and distinct timestamps,
then disposes the DDS report instance. The independent C oracle verifies exact
values, UUID, timestamps, order and Dispose. The values are engineering test
fixtures; unit semantics and mandatory Component QoS remain unverified.

## Command lifecycle

The AnchorControl host accepts valid LOWER and STOP inputs, updates the same source/session to
RAISE with a later timestamp, handles DDS Command Dispose as cancellation and
publishes keyed CommandStatus, CommandAck and terminal Dispose. The independent
C oracle checks issued/commanded/executing/canceled transitions, UPDATED and
CANCELED reasons, ACK contents and disposal. A wrong Destination UUID must be
rejected **without** Status/Ack. Actions are modeled by an in-process virtual
anchor; no actuator safety or physical state feedback is claimed.

## Validation requirements

Run `official_umaa_anchor_specs_meaningful_report` plus four
`official_umaa_anchor_control_{normal,update,stop,bad-destination}` CTests on real
Cyclone C TypeSupport built from the pinned original AUVSI 6.0 IDL, **both**
Linux and Windows. Existing discovery/default-wire scenarios remain separate.
The package must not bundle the original AUVSI IDL.

## Remaining acceptance gaps

Official Topic name, keyed instance mapping against Component Definitions,
QoS/Presentation policy, device control, concurrent sessions, stale updates,
restart recovery and cross-vendor interop remain open. Full 53-service runtime
is not achieved by these two targeted enhancements.


## P4.1 additional independent real DDS tests

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
