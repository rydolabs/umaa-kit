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
