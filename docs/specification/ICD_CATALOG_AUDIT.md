# UMAA 6.0 G6 service contract inventory — incremental proof (2026-10-25)

**Decision: G6 remains OPEN.** Do not infer UMAA service compliance from IDL
directory names, matching Topic type names, or a compiling Handler contract.

## Two independent sources, two distinct kinds of evidence

1. **AUVSI official UMAA 6.0 IDL:** ephemeral ZIP (SHA-256
   `9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411`)
   fetched on the Linux GitHub runner. `tools/audit_official_idl_catalog.py`
   enumerates every file, computes per-file hashes and highlights candidate
   directory groups. Only explicitly reviewed mappings in
   `catalog/umaa6-six-icd-reviewed.json` enter the scoped service Handler generator.
2. **Six public 6.0 ICD Markdown transcriptions** from RTI Community commit
   [ecb923646e7f3fe569b773c3946444e793d8a8be](https://github.com/rticommunity/rticonnextdds-usecases-umaa/commit/ecb923646e7f3fe569b773c3946444e793d8a8be),
   consumed ephemerally. `tools/extract_icd_topic_ledger.py` reads explicit
   Section 5.1 / 6.1 operation headers plus literal Namespace / Topic / Data Type
   declarations; every extracted Topic is matched against the official IDL ZIP.
   The resulting `official-icd-topic-ledger.json` keeps ICD clause references
   and SHA-256 of each transcription. An independent six-domain literal-topic check requires 53 service sections and 113 Topic IDs to match the curated catalog. These transcriptions are secondary sources;
   the AUVSI ICD, Component Definitions and Compliance Specification are the
   authoritative acceptance inputs.

## Observed quantitative results (GitHub Actions Linux)

| Independent measure | Verified value |
|---|---:|
| UMAA official IDL files enumerated | 352 |
| IDL directory groups that resemble service folders | 61 (not normative service count) |
| EO ICD explicitly reviewed Topic service sections | 14 |
| EO explicit official Topic types | 21 |
| Not-yet-reviewed IDL directory groups | 8 |
| Public ICD texts processed | 6: EO, MO, MM, SA, SEM, SO |
| Explicit service sections found in those ICDs | 53 |
| Explicit operation/Topic/IDL triples in those ICDs | 113 |
| Six-ICD curated service sections | 53 |
| Six-ICD Topic types checked against official IDL | 113 |
| Full G6 service/QoS/Component acceptance | **FAIL / OPEN** |

The complete 6-ICD literal Topic/role-shape mapping is stored in `catalog/umaa6-six-icd-reviewed.json`; it covers 53 service headings and 113 Topic IDs, with 5 services using multiple Command groups and 5 MO execution-status side topics. This mapping is not a conformance or QoS claim.

The 14 reviewed EO services are AnchorControl, AnchorSpecs, AnchorStatus,
BatterySpecs, BatteryStatus, EngineControl, EngineStatus, FuelTankSpecs,
FuelTankStatus, GeneratorSpecs, GeneratorStatus, MastControl, MastStatus, and
UVPlatformSpecs. All 14 explicit EO 6.1 service sections and all 21 of their
Topic IDs now have curated Type/role mappings. This is a **literal Topic mapping**
milestone, not QoS or runtime-complete certification. Both single- and **multi-Report** service shapes are
supported by the **typed interface generator**; this is not runtime proof of
every service protocol. Official TypeSupport for all 113 scoped Topics generated and typed C++ service contracts compiled on Ubuntu and Windows (native MSVC).

## Proof and fail-closed controls

- [Linux official IDL, compiled typed APIs, independent netns+netem DDS tests](https://github.com/nssang00/umaa-dev/actions/runs/40112466454).
  Evidence artifact `linux-dds-namespace-integration` includes
  `official-idl-catalog-inventory.json`,
  `official-idl-catalog-strict-evidence.json`,
  `official-icd-topic-ledger.json` and DDS process logs.
- `--strict-g6` must return **3** while Component Definitions, full service
  catalog, all QoS and runtime behavior remain unverified. CI asserts this exact
  nonzero status instead of silently marking compliance.
- Schema drift (352 file count), mismatched ICD namespace/Topic/data type,
  nonexistent official IDL, incomplete source documents, missing manually
  reviewed Topic, and unsafe duplicate topic contracts fail the audit.
- Linux/Windows baseline SDK and native DDS proof are separate from full G6.

## Next audit tasks / release blockers

- **CO ICD** is not present in the pinned six-document transcription batch, although AUVSI publicly lists the CO-EXP ICD 6.0 PDF. Its normative content has not yet been ingested, and the 8 unreviewed IDL directory groups must not be declared to be 8 services.
  Experimental (EXP) services are explicitly out of this nonexperimental
  catalog scope. Complete the authoritative public CO ICD trace separately.
- Read the complete [AUVSI UMAA release](https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/)
  **Component Definitions v1.0**, all applicable 6.0 ICDs and
  **Compliance Specification**; resolve provider/consumer direction,
  query/cancel operations, real DDS Topic strings, QoS, entity keys,
  LargeSet/List, startup, dispose, liveliness and session recovery.
- Expand the reviewed catalog one service at a time without assuming one
  service equals one Topic or every report-shaped IDL is a DDS Report Topic.
  Use `official-icd-topic-ledger.json` as a *review queue* only.
- Tie codegen, generated DLL/SO adapters, native cross-process DDS verification,
  negative cases and vendor interop to every reviewed service.
- RTI Connext 7.7 SDK/license and vendor interoperability remain unverified.

**No official IDL ZIP, transcribed full ICD text, or restricted files are
committed to this repository.** Only code, manually curated facts and runtime
audit metadata are retained.

## Latest verified Windows and Linux service API gates

- [Ubuntu network/IDL/ICD audit](https://github.com/nssang00/umaa-dev/actions/runs/40112466454): explicit EO 14/21 Topic mapping PASS, generated reviewed service C++ contracts 14/21, 352/352 file inventory, DDS CTest 4/4 and 8 isolated process scenarios PASS.
- [Windows native official DDS + reviewed Handler APIs](https://github.com/nssang00/umaa-dev/actions/runs/40112466468): official reviewed C++ TypeSupport generation 17 additional Topics, 14-service/21-Topic API compilation and CTest 1/1, official DDS Battery/Engine CTest 3/3 PASS.
- [Linux GCC/Clang and Windows MSVC baseline tests](https://github.com/nssang00/umaa-dev/actions/runs/40112466451): all three jobs PASS. **No release/compliance claim** is implied by this scoped CI success.

## Six-ICD enlarged catalog evidence (2026-10-25)

- [Linux six-ICD 53 service / 113 Topic contract source audit, compiled SDK API and netns/netem real DDS regression](https://github.com/nssang00/umaa-dev/actions/runs/40153467543): SUCCESS. 352/352 original IDL inventory, six-domain completeness check, official TypeSupport header proof, C++ header smoke and 8 network scenarios.
- [Ubuntu GCC+Clang and Windows MSVC SDK regression](https://github.com/nssang00/umaa-dev/actions/runs/40153467606): 3/3 PASS. This is SDK simulator/compiler regression, not a proof of Windows official 113-Topic TypeSupport until the dedicated DDS workflow completes.
- Full service QoS, Component Definitions Provider/Consumer interface and wire semantics, and all-service DDS runtime integrations are still OPEN.

## Cross-platform full six-ICD API source proof

- [Windows official 113-topic TypeSupport / 53-service MSVC smoke and 3 official DDS tests](https://github.com/nssang00/umaa-dev/actions/runs/40153467601): SUCCESS, 109 additional official Topic TypeSupports generated, 53 reviewed C++ contracts using 113 official Topic types compiled with MSVC, smoke CTest 1/1, official BatteryStatus/EngineControl DDS CTest 3/3.
- [Linux 53/113 audit, C++ compile, DDS netns+netem regression](https://github.com/nssang00/umaa-dev/actions/runs/40153467543): SUCCESS, 4/4 CTest, 8/8 netns DDS scenarios and six-domain 113 Topic completeness proof.
- [Linux GCC/Clang and Windows MSVC prototype regressions](https://github.com/nssang00/umaa-dev/actions/runs/40153467606): 3/3 jobs SUCCESS.
- These runs demonstrate compilation of typed API contracts, **not** runtime behavior for 53 services. Real official DDS still covers BatteryStatus and EngineControl subsets.

## DDS runtime binder and fault-injection checkpoint (2026-10-25)

- The SDK now includes `sdk/include/umaa/report_transport.hpp`: a **vendor-neutral, fail-closed** Report Publisher transport with checked Register / Publish / Dispose / Unregister operations. Any unknown DDS vendor operation outcome poisons the binding; it never silently retries and cannot emit a stale/reordered follow-up publication. Reconstruct DDS entities via the owning Host; the binding does not auto-replay uncertain transactions.
- A separate queryless Report Consumer subscription binding enforces checked Subscribe/Unsubscribe and a new Reader epoch at every clean resubscription; old Reader callbacks are rejected after restart.
- `dds/cyclone/src/official_battery_publisher.cpp` now uses the common Report transport binding to create/delete the real DDS Writer and write/dispose **official BatteryReportType** data. The current value mapping and QoS are explicitly **demo settings**, not an audited official service QoS/Component specification.
- The new `umaa_report_transport_test` injects deterministic failures on all four publisher operations, device handler exceptions, subscription, unsubscribe and prior-generation callbacks. It fails closed and does not claim unknown writes were undone.
- [Linux real-wire / multi-node netem tests](https://github.com/nssang00/umaa-dev/actions/runs/40155133646) verify the initial transport binding; use the final branch commit's Actions for post-restart-epoch and UBSan evidence.
- Official AUVSI has also released downloadable [UMAA SDK Source Code](https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/), a candidate for future verified source/API reuse **after** provenance, licenses and binary/supply-chain inspection. No assumption of equivalence or reuse is made yet.

The overall G1/G6/G7/G8/G11/G12 delivery gates are **not complete**.

## Verified Report publisher/consumer DDS bindings and netem (2026-10-25)

- [Ubuntu CTest + independent DDS + three additional SDK Reader namespace scenarios](https://github.com/nssang00/umaa-dev/actions/runs/40155793388): **SUCCESS**. Official DDS CTest **5/5** (new SDK-owned official Battery C++ reader test + existing independent C oracle); namespace real wire scenarios **11/11** (8 existing plus three new SDK Reader cases for clean, 40ms/10ms/1% loss, and 100% blackout/heal). The new SDK Reader outage test measured **77 kernel-dropped packets** and verified continued wire communication from the same process after restoring network.
- [Linux GCC/Clang and Windows MSVC core SDK regression + Clang UBSan](https://github.com/nssang00/umaa-dev/actions/runs/40155793394): **3/3 SUCCESS**; Clang UndefinedBehaviorSanitizer core/module test step SUCCESS.
- [Windows official TypeSupport and DDS job](https://github.com/nssang00/umaa-dev/actions/runs/40155793392): **SUCCESS**, but this run explicitly selected the *old* 3 official tests; the new SDK Reader was not exercised on Windows. The later Windows workflow revision adds `official_umaa_battery_sdk_reader_wire` to the mandatory regex. Its Actions result must be checked separately before cross-platform reader proof is claimed.
- Existing independent C Simulator is not replaced by the SDK-side test. A sequence count in the SDK Consumer is a *test-only cache watermark*, not an actual official DDS timestamp field. The current Beta/Battery demo Reliability/History QoS is a test profile and MUST NOT be treated as verified UMAA QoS.

## Reliable Dispose and repeated packet loss verification (2026-10-25)

- A repeated Actions regression exposed an **intermittent real bug**: after both official Battery Report data samples arrived under 1% netem loss, one subscriber missed the DDS Dispose event because the demo Writer was deleted after a fixed 100 ms sleep. This is preserved as **failing evidence**, [Linux run #40156075600](https://github.com/nssang00/umaa-dev/actions/runs/40156075600) (`OFFICIAL_SIM: incomplete 2/3`). A previous success alone was insufficient.
- Fixed the Publisher's `unregister_source()` transport path to call `dds_wait_for_acks(writer, DDS_SECS(5))` *before* `dds_delete(writer)`, failing closed on error. This waits for middleware-level ACK from matched reliable readers; it does not claim application-level processing confirmation. The independent C subscriber still independently checks Report 20, Report 80, and final Dispose in order.
- Increased loss-case repetitions to three for each **independent C Battery consumer** and **SDK C++ Battery consumer**. [Final Linux run #40156247032](https://github.com/nssang00/umaa-dev/actions/runs/40156247032) **SUCCESS**: DDS CTest **5/5**, namespace clean/degraded/blackout wire trials **15/15**, blackout/heal **3/3** (actual drop counts **108, 72, 72**), no incomplete-Dispose findings in that run.
- [Windows final official DDS + both independent and SDK-side Battery Reader CTest](https://github.com/nssang00/umaa-dev/actions/runs/40156246980) **SUCCESS**: official DDS **4/4** (new SDK Reader test reports `SDK_SIM: PASS`), standalone reviewed 53-service/113-type C++ API smoke **1/1**, synthetic DDS **1/1**.
- [Cross-platform SDK tests](https://github.com/nssang00/umaa-dev/actions/runs/40156246985): Ubuntu GCC, Ubuntu Clang, Windows MSVC **3/3 jobs SUCCESS**, Clang UBSan step PASS.
- Scope limitations remain exactly as before: these are tested BatteryStatus and EngineControl *slices*. They do not establish 53-service runtime behavior, official component-level QoS, full compliance, RTI or hardware safety.

