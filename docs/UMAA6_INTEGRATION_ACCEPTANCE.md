# UMAA 6.0 integrated component-generation status

This is an engineering verification ledger for Draft PR #31, based on the
mainline Contract IR/YAML generator and stacked Draft PRs #27 (vendor backend
isolation), #29 (official IDL evidence), and #30 (vendor-neutral QoS policy).
It is **not a UMAA compliance certificate**. None of the three source Draft
PRs is merged into main by this integration.

## Supported, source-checked pipeline

```text
AUVSI 6.0 original IDL ZIP (SHA-256 pinned, external)
           |
           +-- original 53-Service reviewed YAML catalog (read-only)
           |      |
           |      +-- strict IDL Topic and @key cross-check
           |             |
           |             +-- 53 source-backed YAML candidates (external temp)
           |                    |
           |                    +-- 114 Cyclone idlc native descriptor entries
           |                    |    (C compiled/linked; local Topic probe)
           |                    |
           |                    +-- 53 vendor-free C++ Service identity entries
           |                         114 source-verified Topic declarations
           |                         one reviewed EO Collection QoS minimum
           |                         full QoS/protocol/codec readiness = false
           |
           +-- EngineControl Command/ACK/Status IDL + YAML gate
                      |
                      +-- generated ignition-only user module
                           + existing Cyclone independent wire CTests
```

The supported source audit verifies 594 official IDL files, 1,705
`#include` references, 53 Service mappings, 113 direct Topics and one
embedded BatteryStatus large-collection element. In GitHub Actions, Cyclone
`idlc` and C compile all 594 files. The 114 exported native descriptors
link with their 241 transitive type objects. Distinct **non-normative local**
Topic names are used for local descriptor instantiation tests. The 53
vendor-free C++ Service identity contracts are compiled with C++17.

Reference Actions run that verified 114 native TypeSupport links:
https://github.com/rydolabs/umaa-kit/actions/runs/41354742723

GitHub Actions source-checked 53-Service C++ codegen and EngineControl gate:
See `.github/workflows/official-idl-runner-integration.yml`.

## Explicit runtime / standards blockers

1. **IDL Topic constant is not yet an approved physical DDS wire name.**
   A test passing the literal `UMAA::EO::AnchorControl::
   AnchorCommandAckReportType` as the Cyclone DDS Topic name to
   `dds_create_topic` returned `Bad Parameter`. The ICD references
   fully-qualified names. Silently shortening or rewriting names does NOT
   establish interoperability. The generated native registry records
   `originalWireTopicNameVerified=false` and names local test topics
   `UMAA_TYPE_PROBE_###`. Source IDL names remain unchanged.
2. **QoS remains incomplete.** Only EO ICD 6.0 §3.8.1 Large Collection
   element ordered TOPIC Presentation is source-reviewed. The reviewed
   minimum is in `contracts/qos/umaa6/eo-large-collections.yaml` and
   SDK QoS validation can enforce it, but no unverified per-Service profile
   may be auto-filled. The generated Service header distinguishes an
   established *minimum* from an approved *entire* QoS profile.
3. **Payload codec and official Component readiness remain false.**
   Cyclone TypeSupport linkage is not the same as DDS sample mapping
   to/from `UmaaDdsSampleV1`. The existing source-dependent
   `tools/umaa_icd_component_codegen.py` only generates executable
   C++ modules for the restricted synthetic `long`/`string` subset;
   it rejects incomplete official UMAA Service semantics.
4. **EngineControl is a proved ignition-only subset.** Source-verified
   Command/ACK/Status Topic and keys are checked before generating its
   user-owned Handler and compiling existing real-wire Command/Update
   tests. The full Operation/Cancel/Status flow requirements and official
   service QoS have **not** been certified by this source check.
5. **Cross-vendor official interoperability remains unproved.**
   Synthetic Cyclone/Fast DDS V2 QoS and C++ SDK tests pass, but official
   complex IDL TypeSupport and all 53 Service protocols are not connected
   to Fast DDS yet. RTI SDK is external.
6. **Impaired-network application-level Dispose delivery is a separate
   acceptance gate.** A flaky three-node 1% packet-loss run previously
   observed Report data but missed its final Dispose. A Writer-history DDS
   ACK alone does not prove an independent subscriber received the final
   lifecycle event. Do not disable the stress test to claim success.

## Concrete follow-up acceptance conditions

- Review normative ICD Operation, Collection, QoS and wire Topic naming
  rules with source page/section provenance; reject unknown values.
- Generate official on-wire CDR/TypeSupport bindings inside each
  `dds-backends/<vendor>` without vendor-specific code in SDK or user modules.
- Prove generated Component v2 **Provider and Consumer** over independent
  processes, first BatteryStatus Report/Dispose, then EngineControl Command,
  ACK, Status, Cancel, Update and session lifecycle.
- Re-run with Cyclone and Fast DDS independently, then mixed-vendor tests.
- Extend from two reviewed vertical slices to all 53 Services; report
  incomplete source requirements rather than claiming compliance.

## Private-source workflow

Never commit AUVSI original IDL/PDF/SDK sources or uploaded files. The official
ZIP is downloaded only into ephemeral Actions Runner storage, checked against
the independently pinned SHA-256, used to derive TypeSupport/contract
candidates there and not uploaded as an artifact. The audited C++ source
identity outputs are generated, not a redistributed copy of normative IDL.

Reproduction uses:
`python -m tools.official_icd_overlay`,
`python -m tools.generate_official_native_type_registry`,
`python -m tools.generate_qos_requirements --check`, and the workflow above.
