# P3 service-bound DDS entity creation — checkpoint (2026-10-27)

## What is implemented

- `tools/generate_reviewed_runtime_registry.py` generates the **exact 53-service,
  113-Topic** `sdk/include/umaa/reviewed_service_registry.hpp` using the reviewed
  six-ICD Catalog. The script verifies role, naming, duplicate identities, and
  *explicit* QoS review flags. `--check` compares byte-exact output in CI.
- `dds/cyclone/include/umaa/cyclone_service_context.hpp` owns the Cyclone C
  participant, checks the service/Topic/type/direction **before** creating DDS
  topics, and deletes the participant when closed or destroyed. Default mode
  rejects any Topic with `dds_qos_verified=false`. Only an **explicitly requested
  prototype mode** permits those demos; it does *not* claim compliance.
- Existing official BatteryStatus Report publisher and EngineControl Command
  host now use the common `ServiceContext` and exact Catalog Topic/Role mapping
  instead of bypassing the runtime guard.
- Strict GCC CTest **13/13** and Python registry negative tests **5/5** pass
  locally. Real IDL-based Linux/Windows Actions still required for G8 proof.

## What this does not do

- The Catalog validates ICD-derived logical Topic/type/role identities **only**.
  It does not prove the caller's TypeSupport descriptor contents, DDS Topic
  name string, instance-key mapping, mandatory QoS, Presentation QoS, or
  Component Definitions. It currently contains **zero normative-reviewed
  per-service QoS policies**. A production mode service therefore fails closed.
- No new service is fully implemented by compiling a Topic registry.
- BatteryStatus and EngineControl still use selected, manually mapped payloads.
  Do not count them as full-service conformance or claim that the 51 remaining
  reviewed service runtimes are implemented. RTI and hardware remain open.

## Acceptance to build next

1. Validate Component Definitions and exact DDS names/keys/QoS for the first
   **new** Report and Command services (not just these two existing slices).
2. Add genuine DDS type-aware mappers and independent C/other-language oracle
   processes for their actual message fields, ack/status/dispose and negative
   scenarios. Run both Linux and Windows. Until this passes, **P3-1 PARTIAL**.
3. Extend generated service runtime and per-service compliance matrix; never
   substitute synthetic IDL or default QoS for normative acceptance.

## New reviewed official service endpoints — discovery-only milestone

The optional `UMAA_PROBE_GENERATED_ROOT` generates real original TypeSupport for
`UMAA::EO::AnchorSpecs::AnchorSpecsReportType` (Report Provider) and
`UMAA::EO::AnchorControl::AnchorCommandType` (Command Consumer). A new
`ServiceContext` creates the host's actual DDS endpoint, while an independent
C simulator creates the **opposite** endpoint. Each requires a genuine Cyclone
DDS discovery-match status on its own participant before passing. Both are
compile- and real-discovery tests on Linux/Windows once verified in CI.

**Explicit limitation:** neither probe sends a user payload. Their presence
must **not** be counted as a third/fourth fully running UMAA service or
normative Report/Command lifecycle validation. Payload fields, keys, Status/Ack,
Dispose and QoS still need independent on-wire and Component review.
