# UMAA SDK DDS QoS: reviewed contract → runtime values → vendor mapper

**Scope:** Explicit, fail-closed QoS on the existing V1 plugin architecture
using an optional V2 extension. No claim of full UMAA 6.0 conformance.

## Normative source and policy boundaries

- UMAA Component Definitions v1.0 §3: Participant, endpoint and Topic QoS
  must be configurable without editing or recompiling source code. Domain,
  partitions and GUID configuration are also identified. This prototype
  implements **domain, a single Publisher/Subscriber partition and selected
  endpoint QoS**, NOT all of these options.
- UMAA Compliance Specification T0300-BE-IDS-010 Rev. 1 §1-2(d):
  documentation of the QoS requirements necessary for each Service
  Participant is mandatory. We do not infer a missing per-service profile.
- UMAA EO ICD v6 §3.8.1 (p.14): **Large Collection element Topic**
  requires DDS PRESENTATION access_scope TOPIC and ordered_access=true
  on publishers and subscribers. This is the **only source-reviewed
  normative rule encoded here**; see
  `contracts/umaa6/qos/eo-large-collections.yaml`.
  Neither coherence nor Reliable/Durable/History values are inferred.
  Metadata and element Topics have no cross-topic ordering guarantee,
  and the service-level assembler must still handle out-of-order arrival.

## Architecture and API compatibility

```
Reviewed ICD rule YAML -----+
                            v
User runtime configuration -> SDK QoS requirements/validation
                                   |
                                   v
                  UmaaDdsQosV2 (optional C ABI extension)
                         /                     \
                 Cyclone DDS                 Fast DDS
               DDS qos API                DataReaderQos,
             + Publisher/Subscriber       DataWriterQos,
                                            PartitionQos
```

- Existing `UMAA_DDS_ABI_VERSION=1` is **unchanged**; `DdsBackendPlugin`
  loads the optional `umaa_get_dds_qos_backend_v2` symbol.
- Explicit QoS **never silently falls back** to the V1 vendor defaults.
  Old configurations continue to use V1 for compatibility.
- SDK `umaa/qos_policy.hpp` defines Policy, Requirements,
  `validate()`, `required_capabilities()`, and
  `validate_compatible()`. There are no vendor headers in SDK.
- `sdk/include/umaa/generated/umaa6_qos_requirements.hpp` comes from
  the reviewed YAML; run `python -m tools.generate_qos_requirements --check`.
  Unknown contract requirements are **not** represented as known defaults.
- The Vendor Adapter remains responsible for applying every selected
  QoS to the correct DDS entity and returning failure on unsupported values.
  In addition to implementing V1, new vendors implement the optional V2
  function table in their `dds-backends/<vendor>` directory.

## Minimal runtime configuration

```yaml
middleware:
  backend: "./libumaa_dds_cyclone_demo.so"
  domain_id: 10
qos_profiles:
  demo-reliable:
    reliability: reliable
    durability: volatile
    history: keep_last
    depth: 8
    presentation_scope: instance
    ordered_access: false
    coherent_access: false
    partition: fleet-alpha
modules:
  - "./libumaa_cyclone_vehicle_component.so"
```

**This is a synthetic demo configuration, not an official BatteryStatus
requirement.** Relative module paths are resolved from the config file.
The RuntimeConfig parser accepts exact two-space profile indentation,
four-space policy fields, strict policy keys, and bounded values.
Missing profiles, invalid enumerations, unsupported ordered/group/coherent
policy and rejected IDL TypeBindings fail before DDS traffic begins.
A QoS profile is selected by `Service::qos_profile` where available;
the original synthetic default remains for older Service traits.

A new configuration takes effect on **next startup/endpoint creation**;
the SDK does not promise live QoS mutations on existing DDS endpoints.

## Verified vendor support (synthetic BatteryStatus)

| Policy | Cyclone DDS | Fast DDS |
| --- | --- | --- |
| Reliability reliable/best_effort | mapped | mapped |
| Durability volatile/transient_local | mapped | mapped |
| History keep_last / keep_all | mapped | mapped |
| History depth | mapped | mapped |
| Single Publisher/Subscriber partition | mapped | mapped |
| Presentation TOPIC + ordered_access | mapped via Publisher/Subscriber | **unsupported, fail closed** |
| Presentation GROUP or coherent_access | **unsupported** | **unsupported** |
| Security / full Participant QoS / multiple partitions | **not implemented** | **not implemented** |

Fast DDS Presentation QoS is not available as a verified behavior in the
supported environment. Merely exposing an API field would not justify a
capability claim, so the backend explicitly refuses it. No assumption is
made that a different vendor automatically meets the official EO requirement.

## Test evidence

The two synthetic DDS plugin suites compile and execute a common
`sdk/tests/real_dds_qos_cli.cpp` binary in **separate processes**, for
direct C ABI QoS and Application `from_config` QoS scenarios.
Additional tests exercise transient_local and Cyclone ordered Topic
presentation. Unsupported QoS requests are expected to fail.
Portable SDK policy and runtime YAML tests run on Linux/Windows; generated
contract is checked on both systems in
`.github/workflows/umaa-qos-contract.yml`.

**Remaining explicit work:** Official TypeBindings and YAML-defined full
Command/Report/Collection protocols; mapping all source-reviewed per-service
QoS requirements (rather than only the EO Collection minimum); richer
participant-level QoS, security/Domain partitions and GUIDs; validating
mixed-vendor interoperability; native official Component generators.
The official AUVSI source archive is managed separately in PR #29.
