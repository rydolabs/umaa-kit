# UMAA EO ICD v6.0 requirements traceability (prototype coverage)

Basis: user-provided `UMAA-SPEC-EOICD-1.pdf`, Version 6.0, June 6, 2024. Page numbers are the **printed/PDF** numbers (both identical here). `SDK Common` refers to user-supplied archive, but upstream full build is not available in this environment.

| Spec | Required observable property | Working prototype evidence | Status |
|---|---|---|---|
| §3.1 pp.11-12 | MDE/IDL data model; programmatically generated type support | Fail-closed subset IDL→C++ typed Handler generated, synthetic fixture | PARTIAL; full official IDL not verified |
| §3.4 pp.12-13 | Some query/cancel are DDS operations, not standalone topics | Service Catalog explicit; no fake query RPC generated | PARTIAL |
| §3.7 p.14 | UUID conforming to RFC 4122 variant | Not implemented in prototype | OPEN |
| §3.8 pp.14-22 | LargeSet/List element metadata and atomic synchronization | C++ CollectionAssembler tests element-before/after-metadata, list cycles, delete, memory cap | PARTIAL; actual QoS, identity, full IDL not tested |
| §3.8.1 p.14 | Publisher + subscriber ordered access, topic scope | Upstream `CYCLONE_QOS_PROFILES.xml` publisher ordered_access=false, subscriber=true; flagged for correction | OPEN; must test real DDS semantics |
| §3.9 pp.22-25 | Specializations separate topic, paired ID/timestamp, disposal | Not implemented in prototype | OPEN |
| §5.1 pp.32-33 | Legal states, reasons and transitions | C++ unit tests + independently authored simulator verifier; explicit allowed failure reasons | PARTIAL; no real DDS delivery |
| §5.1.2 pp.35-36 | Recover or reject persisted commands at startup | `fail_unrecoverable` policy demonstrated, not discovery of persisted DDS samples | PARTIAL |
| §5.1.4.2 pp.39-40 | Source/session/timestamp matching; update as ISSUED/UPDATED | C++ engine checks route/source/timestamp; update and replay scenario verified | PARTIAL |
| §5.1.4.5 p.42 | Dispose-driven cancellation; liveliness loss | Test host emulates consumer lost; actual DDS liveliness not integrated | PARTIAL |
| §5.1.5 p.43 | Dispose responses only after command disposal | Normal and cancel traces verified; cancel refusal still permits terminal completion & late cleanup | PARTIAL |
| §5.2.1 pp.46-48 | Query-less Request/Reply by Report subscription; provider lifecycle | Typed ReportHandler, ReportProvider, source cache, timestamp and dispose unit tests | PARTIAL; no late-joiner durability DDS test |
| §5.2.2 p.49 | Query-data Request/Reply not yet specified | Deliberately NOT generated | NOT APPLICABLE in EO ICD v6.0 |
| §6 pp.50 onward | Service-by-service official named interfaces | Synthetic explicit service catalog only | OPEN; official catalog/IDL required |

**Critical distinction:** Running generated code against a synthetic fixture and checking status traces is not evidence of conformance to the UMAA wire format. Phrases such as "UMAA-compliant" and "certified" are not used for this prototype.

## Upstream AUVSI Common code review spot checks

- `config/CYCLONE_QOS_PROFILES.xml` around lines 96–118: publisher `ordered_access=false` and subscriber `true` for LargeCollections profile. Mismatch against §3.8.1; do not assume this file is necessarily the final active effective QoS until runtime verified.
- `include/umaa/services/base/CommandProviderSession.h` around lines 131–137: update checks session ID and state-machine update, so source ID / timestamp guarding is a review priority.
- `include/umaa/services/base/CommandProviderBase.h` around lines 482–498: `COMPLETE_EXISTING` sends a completed status via a special path. Needs explicit state-transition scrutiny.

These are static source observations, not executed upstream SDK regression tests.