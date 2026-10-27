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
- Strict GCC CTest **13/13**, Clang ASan/UBSan **2/2**, and Python
  registry negative tests **6/6** pass locally. Real IDL-based Linux and
  Windows verified in the exact commit runs listed below.

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

Optional `--wire` variant additionally sends one **default-initialized** official
TypeSupport sample and checks it is deserialized by the opposite independent
DDS process. This proves serializer/transport connectivity only; zero-valued
sample contents are **not** validated as normative UMAA application commands
or fully-populated Reports. The check does not generate Command Ack/Status or
Report keyed Dispose, and is not a complete new service implementation.

## Exact-source CI evidence (final feature head before merge)

Validation source commit `66bd148b4e66403caca7efd54066eaba97ed3a47`
(see GitHub [PR #4](https://github.com/rydolabs/umaa-kit/pull/4)).

- [Linux official UMAA, real Cyclone, independent Simulator](https://github.com/rydolabs/umaa-kit/actions/runs/40426885356):
  official CTest **10/10**; both new Report and Command default-valued
  TypeSupport samples independently received (`REVIEWED_SERVICE_TYPED_WIRE_*_PASS`),
  Linux namespace/network impairment tests **15/15** including **7 impaired**
  and **3 blackout/recovery** cases.
- [Windows official UMAA, MSVC, real Cyclone](https://github.com/rydolabs/umaa-kit/actions/runs/40426879585):
  official CTest **9/9** including both new default-wire tests; additional
  109 reviewed TypeSupports generated; genuine exported Cyclone package tested
  with an independent CMake Consumer.
- [Linux/Windows SDK matrix](https://github.com/rydolabs/umaa-kit/actions/runs/40426885475):
  GCC, Clang and MSVC CTest **13/13**; generated registry checked against Catalog.

**Strictly limited acceptance:** these two *additional* services have only
Topic discovery and **default-initialized sample type serialization** tested.
Their semantic Report payloads and Command/Status/Ack/Dispose are not yet
implemented or accepted. The total count of partially operational services
is still **BatteryStatus + EngineControl**, not four complete services.
