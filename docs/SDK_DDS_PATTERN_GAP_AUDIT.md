# SDK pattern-first DDS and official ICD/IDL generation audit

**Date:** 2026-10-30 · **Scope:** current UMAA Kit main + bounded next SDK slice.
This is an engineering audit, NOT UMAA 6.0 certification or official TypeSupport approval.

## Required outcome

A generated Provider Component DLL/SO and a *different* generated Consumer
Component DLL/SO, running in independent host processes, must communicate
through **real DDS Topics** with the **original UMAA 6.0 IDL-generated
TypeSupport**, reviewed ICD semantics, Keys and mandatory QoS. The user
implements only business logic. A passing in-process CTest or synthetic
topic flow cannot substitute for this end-to-end requirement.

### Current evidence matrix

| UMAA pattern | Implemented / tested *development* substrate | Missing official wire/generation proof |
|---|---|---|
| Report / Status | Typed ReportPublisher/ReportConsumer, per-key cache, Dispose/watch callbacks; independent-process synthetic real DDS Cyclone and Fast DDS tests | Real official per-service Report IDL TypeSupport, keys, entity QoS, source-to-host generation and two separately generated Component processes. Command Status is a distinct Command/Response response topic. |
| Command / Response | CommandEngine, typed DdsCommandRequestServer / RemoteCommandClient; separate synthetic DDS processes; Command/ACK/Status + update/cancel, fault fences | Reviewed original command type and destination/source/session keys, Ack/Status/ExecutionStatus full field and enumeration mapping, startup/retirement/liveliness, per-service QoS and end-to-end generation for multiple independent Components |
| Request / Reply without Query Data | Report subscription exists and catalog explicitly maps 35 current reviewed services to query-less pattern | Generate per-service Provider Report Writer and Consumer Reader from actual ICD, **do not invent a Query RPC**. Verify actual DDS TypeSupport and startup/Dispose and QoS. |
| Large Set / Large List | Bounded Collection receiver, batch builder, one-DDS-sample atomic-frame fallback and independent-process synthetic DDS tests | Original two-Topic Metadata + element types, ID/timestamp synchronization, Topic PRESENTATION ordered_access, complete Set/List, keyed Dispose, consistency/resync and independently verified native DDS behavior. Single sample fallback is *not* equivalent. |
| Generalization / Specialization | **New** header-only GeneralizationSynchronizer + CTest: separate-topic identity/timestamp join, out-of-order buffering, stale/replay safety, two-topic Dispose barrier and bounded per-ID state | Reviewed real parent/child TypeSupport, specializationTopic/ID/Timestamp DDS field mapping, real DDS readers/writers, official keys and QoS, multi-process tests including specialization inside a Large Collection |

UMAA 6.0 EO ICD sections **3.8 and 3.9** describe separately published
Metadata/Elements and Generalization/Specialization message pairs,
respectively. Generalization is **not merely C++ class inheritance**.
The specialization pair shares an ID and timestamp, and removal disposes
both topics. Topic presentation ordering does not imply order across
two independent Metadata/Element Topics.
Mirrored document: https://github.com/rticommunity/rticonnextdds-usecases-umaa/blob/main/docs/UMAA-SPEC-EOICD.md
Treat the original authoritative UMAA 6.0 source and project-supplied
official IDL as final required evidence; a mirrored Markdown file is
reference context, not a substitute for a pinned official source.

## Actual current architecture gap

1. `tools/umaa_component_cli.py` generates **only synthetic Provider**
   modules. Official service identities are selected/pinned but fail closed
   in code generation. Module-side Consumer service binding is unimplemented.
2. `tools/generate_reviewed_service_api.py` generates a reviewed **type
   signature** mapping from official C headers, not complete DDS transport,
   Key/QoS profiles, native Reader/Writer or workflow.
3. `tools/generate_reviewed_type_support.py` invokes actual `idlc` on
   the separately supplied original UMAA IDL tree, but does not generate
   service protocol behavior or portable Fast DDS native TypeSupport.
4. `sdk/include/umaa/module_api_v2.h` currently advertises provided
   COMMAND/CANCEL/REPORT calls. It does not register Consumer dependencies,
   asynchronous delivery into a Component, dynamic topology or a shared
   lifecycle for multiple independent service instances.
5. `sdk/include/umaa/internal/dds_backend.h` v1 supports generic Topic,
   Writer, Reader, Write, Dispose, Unregister, ACK with capability flags,
   but has no approved official TypeBinding registry or native cross-topic
   transaction API. Expanding it requires explicit ABI versioning and
   compatibility strategy, never vendor-specific changes in user Handlers.
6. Existing catalog contains **53 reviewed service identities / 113 Topic
   identities from six ICDs**. It does not represent all official Services
   and does not fully encode per-instance keys/QoS, generalized subtypes
   or normative native wire behavior. Contract snapshots record
   `ddsQosVerified=false` and `ddsBehaviorVerified=false`.
7. The existing end-to-end real DDS tests are **synthetic**. Both Cyclone
   and Fast DDS standalone parity are verified; mixed-vendor same-network
   interoperability is explicitly a later objective and historically failed.

## Implementation priority and hard evidence gates

### P0 — approve one complete official Report service

Pick **UMAA::SA::GlobalPoseStatus** *only after validating* full original
SA ICD, original IDL and key/type/topic/QoS instructions. Produce a
machine-readable ServiceBinding **with source-file SHA and normative clause
references** for each Topic and Role. The current reviewed service catalog
does not by itself authorize production native bindings.

Gate: static IDL→TypeSupport check, Key/QoS source approval, checked
Provider/Consumer topic/instance mapping; fail closed on missing fields.

### P1 — reusable runtime binding and generation

Build reviewed `ServiceTraits` and vendor-specific native
`TypeBinding` for **both** standalone Cyclone and Fast DDS.
Implement generated Provider and Consumer registration into independently
loadable Component DLL/SO modules; a host orchestrator owns DDS entities,
lifecycle, subscriptions and periodic updates. Consumers must receive
typed events in their own Component business logic without DDS imports.
One Assembly Manifest and Generator serves CLI and future Studio.

Gate: two independent generated Component processes, separate binaries,
same DDS domain/vendor; actual DDS discovery, data and keyed Dispose;
rebuild modules under Windows and Linux from clean installed SDK.

### P2 — Command protocol and multi-service wiring

Validate the `UMAA::EO::EngineControl` or selected MO Command service
against original ICD/IDL, including all Command/ACK/Status and
ExecutionStatus as applicable, unique source+session identity,
destination filtering, UPDATE, cancellation and dispose, status ordering,
startup/recovery, liveliness and missing-ack fault. A synthetic FSM PASS is
not an official conformant protocol verdict.

Gate: independent generated Provider/Consumer processes on real DDS;
valid response and failure, timeout, update/cancel, lost provider,
restart and multi-component/multi-service routing, both vendors separately.

### P3 — Large Collections and Generalization/Specialization native DDS

Generate Metadata+Element and General+Specialization topic pairs from
fully reviewed IDs and types. Each pair must be buffered/joined safely
across out-of-order streams, and relevant vendor QoS/presentation features
must be proven rather than guessed. Newly added
`generalization_sync.hpp` is **only the transport-neutral
synchronization protocol core**; no native DDS adapter is yet connected.

Gate: real DDS independent-process tests for metadata-first,
element-first, revisions, missing element/time mismatches, duplicate
replay, keyed Dispose, bounded memory and recovery; then combined
generalized elements inside Set/List.

## Delivery criteria

A service can be labeled **OFFICIAL_WIRE_READY** only when all of these
are evidenced at an exact commit for both standalone vendors:

- Original official ICD, IDL and Component applicability verified and
  cryptographically pinned; no guessed Topic/Key/Type/QoS.
- Original native TypeSupport generated and compiled.
- Provider and Consumer Component projects generated by the same generator,
  with user-owned business logic protected from regeneration.
- Real DDS from **independent Component host processes**, including
  positive and negative semantic cases, verified by an independent observer.
- Linux GCC/Clang and Windows MSVC build/CTest/packaging and strict error
  handling; no fabricated device success or fallback to synthetic TypeSupport.

A schema-only reviewed mapping, this new GeneralizationSynchronizer CTest,
or a demo DDS message **MUST NOT** set `OFFICIAL_WIRE_READY`.
