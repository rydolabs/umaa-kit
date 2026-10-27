# Progress and evidence

## Verified checkpoint 1: typed SDK skeleton, 2026-10-23
- Added strictly parsed synthetic IDL fixture and explicit service-catalog driven generator.
- Preserves user Handler code on regeneration; compilation proven.
- Added C++ CommandEngine common protocol skeleton with source/session/timestamp and terminal cleanup.
- Added independent simulator verifier and four subprocess scenarios.
- First pass: 4 SDK parser tests, 5 simulator oracle tests, 1 C++ engine test, 4 end-to-end scenarios.

## Verified checkpoint 2: coverage, corrected cancellation, 2026-10-23
- Found and corrected initial cancel-refused + later completion cleanup edge case (previous session resource leak).
- Added `ReportProvider`, `ReportCache`, `CollectionAssembler` with set/list and pending/committed snapshot separation.
- Added generated Report Handler, C++ tests and packaging.
- Verified 5 SDK parser tests, 6 simulator oracle tests, C++ engine and report/collection tests, generator-compiled report smoke, CMake tests and independent consumer.
- Ran UndefinedBehaviorSanitizer on C++ protocol and collection unit executables: no observed issue.
- Negative-control test: deliberately invalid external SUT trace was rejected by independent simulator (evidence/negative_control.txt).
- Exported and validated CMake installable target `UMAA::Core` with an external consumer project.

## Current verified envelope
- Four independently driven scenarios / 32 command-related events; trace artifacts under evidence/.
- No real DDS tests. See `evidence/vendor_environment.json`.
- External official UMAA 6.0 IDL, full compliance suite and active DDS vendors missing.
- Outcome: **PARTIAL working prototype**, NOT complete UMAA SDK / conformance / production.

## Next work (blocked/deferred, not represented as ongoing background work)
- Clear official input and DDS vendor availability gates in docs/DELIVERY_GATES.md; implement real TypeSupport, runtime adapters, broad protocols, independent DDS simulator and conformance corpus.

## Verified checkpoint 3: CI-ready platform boundary and two service modules, 2026-10-23
- Created `.github/workflows/ci.yml` with `ubuntu-24.04` GCC/Clang and `windows-2022` MSVC jobs, using a single portable `tools/ci_verify.py`; upload logs/artifacts on failures.
- Prototype loader: cross-platform DLL/SO C ABI (`module_api.h`, `module_loader.hpp`), service/schema validation, distinct EngineControl and BatteryStatus modules, incompatible ABI fixture. Host reserves UMAA protocol state; modules implement demo-only actions.
- Re-ran Linux GCC 14 and Clang 17: Python SDK 5/5, Python independent oracle 6/6, CTest 4/4, independent consumer linking, 4 process scenarios (32 events) each. See `evidence/ci/linux/{default,clang}/summary.json`.
- Found and corrected a dirty CMake cache issue when swapping the Clang external-consumer compiler; clean consumer build now passes. CI runners start from clean checkout.
- GitHub smoke run https://github.com/nssang00/umaa-dev/actions/runs/39930834209 confirms native DLL/SO service module prototype on Windows/MSVC and Ubuntu/GCC+Clang (three jobs succeeded). These are **not** full SDK/Simulator or real-DDS results. Full local prototype sources have not yet been imported.
- The K-MOSA ABI is a **proposal** inspired by MOSA principles, not a verified government interface. All official IDL/DDS/compliance gates remain blocked.
## 2026-10-23 — GitHub Windows/Linux full prototype validation

- Full SDK and independent simulator code (31 native source and tool files) integrated to `nssang00/umaa-dev`.
- Linux local GCC and Clang `python tools/ci_verify.py` passed.
- GitHub-hosted matrix Linux/GCC, Linux/Clang, Windows/MSVC passed **after source import and cleanup**: https://github.com/nssang00/umaa-dev/actions/runs/39941404598
- Each job: Python codegen 5 tests, simulator oracle 6 tests, C++ CTest 4/4, installed CMake external consumer, 4 subprocess simulator scenarios (32 observed events).
- Temporary source-transfer bundles and import workflow were removed from current repo tree.
- Open: official UMAA 6.0 IDL and Codegen, CycloneDDS-CXX/RTI transports, wire-level QoS and conformance testing, verified safe hot swap and official K-MOSA conformance.

## 2026-10-23 — Milestone 2 real Cyclone DDS verified (limited BatteryStatus scope)

- Official AUVSI 6.0 ZIP (492 files / 352 .idl), SHA-256 `9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411`, fetched in ephemeral GitHub Runner.
- BatteryReportType and 5 transitive dependencies: Cyclone C TypeSupport generation and compilation on Linux and Windows.
- Linux: SO BatteryStatus module invokes through project-owned host ABI, publishes official BatteryReportType `chargeRemaining` 20→80 over actual DDS. Separate Simulator validates both samples and DDS keyed Dispose; [Actions run](https://github.com/nssang00/umaa-dev/actions/runs/39952219497) PASS.
- Windows: DLL BatteryStatus module and MSVC official C TypeSupport same on-wire operation, [Actions run](https://github.com/nssang00/umaa-dev/actions/runs/39953508112) PASS.
- EngineCommandType and EngineCommandStatusType generated/compiled via official Cyclone `idlc` on Linux; [run](https://github.com/nssang00/umaa-dev/actions/runs/39952552116) PASS. Actual EngineControl DDS command state machine integration is **NOT** implemented.
- Open: official UMAA service-compliant Topic/Key/Source/time semantics, QoS full mapping, Handler codegen on official IDL, full EngineControl on-wire flow, RTI integration, official K-MOSA conformity and all-ICD conformance. This proof remains a **partial, non-certified prototype**.

## 2026-10-23 — Milestone 3: official EngineControl ignition subset + complete IDL compile coverage

- Official original ZIP SHA-256 pinned; on Ubuntu **all 352 AUVSI 6.0 IDL files generated with Cyclone idlc and 352 C TypeSupport sources compiled**, [coverage run](https://github.com/nssang00/umaa-dev/actions/runs/39963083783).
- Official EngineCommand, EngineCommandStatus, EngineCommandAckReport and dependencies generated in ephemeral CI. Fail-closed `tools/generate_official_engine_plugin.py` validates the required official members/enums and emits user Handler + plugin glue. The user-only demo implementation is unchanged and DDS-free.
- SDK Host loads the generated SO/DLL and invokes common `CommandEngine`; independent Simulator verifies RUN issuance, Ack, ISSUED/COMMANDED/EXECUTING, later-timestamp same-session OFF update with ISSUED(UPDATED), and DDS Command/Status/Ack Dispose. No natural auto-completion.
- Linux official DDS CTest 3/3 [PASS](https://github.com/nssang00/umaa-dev/actions/runs/39962602416). Windows official DDS CTest 3/3 [PASS](https://github.com/nssang00/umaa-dev/actions/runs/39962717265). Baseline cross-platform 9 Python generator tests, 6 Simulator tests, CTest 4/4, 4 scenarios [PASS](https://github.com/nssang00/umaa-dev/actions/runs/39962717654).
- Bugs corrected: generated module MSVC C++17 setting, DDS Dispose evidence carried on last valid sample's instance_state, safe terminal data instance key for Dispose. Regression/negative verification remains enforced.
- **Unfinished:** full per-service official Handler codegen, EngineControl plugState/propulsion binding, exact service QoS/liveliness/recovery requirements, RTI vendor/cross-vendor tests, safety, formal UMAA/K-MOSA certification.

## 2026-10-25 — G6 catalog and G1 official service API expansion (PR #1)

- Continued approved P1 work on `ci/linux-netns-windows-runners-20261024`. No changes to main, no operational actuation.
- The authoritative AUVSI 6.0 IDL ZIP (pinned SHA-256 `9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411`) was inventoried **352/352**. 61 domain/service directory *candidates*; those are not 61 formally specified services.
- Expanded manually reviewed EO service/topic slices from **2 / 4 Topic types to 10 / 15**. Added six report services, MastControl Command/Ack/Status, and UVPlatformSpecs two separate Report Topics. New codegen supports multi-topic report interface methods and preserves single-report API compatibility. Official C TypesSupport and typed C++ interfaces compile under Ubuntu/GCC and Windows/MSVC.
- Explicit 6.0 ICD section/Namespace/Topic/Data Type extraction from six pinned public Markdown transcriptions (EO, MO, MM, SA, SEM, SO) recorded **53 service sections / 113 operations and topics**, all checked against official IDL; cross-checked all 15 curated Topic mappings. No inferred flow roles or QoS for unreviewed records. CO and Component Definitions review remain OPEN.
- Added `tools/audit_official_idl_catalog.py`, `tools/extract_icd_topic_ledger.py`, `tools/generate_reviewed_type_support.py`, `tools/reviewed_api_smoke` and portable negative regression tests. JSON evidence is uploaded by Actions, official source text remains ephemeral.
- [Linux DDS+netns+netem + catalog audit run](https://github.com/nssang00/umaa-dev/actions/runs/40112010519): SUCCESS with DDS CTest 4/4, all 8 namespace wire scenarios and netem recovery. G6 strict incomplete gate is **expected FAIL code 3**.
- [Ubuntu GCC/Clang + Windows MSVC unit/regr run](https://github.com/nssang00/umaa-dev/actions/runs/40112010529): 3/3 jobs SUCCESS.
- [Windows native official DDS and typed API compile run](https://github.com/nssang00/umaa-dev/actions/runs/40112010626): **SUCCESS**; reviewed C++ API compile+CTest **1/1**, synthetic DDS CTest **1/1**, official BatteryStatus/EngineControl DDS CTest **3/3**.
- G1/G6 **PARTIAL/OPEN**. This is no full UMAA support: only BatteryStatus/EngineControl have real DDS protocol slices, service QoS/roles across all ICDs unverified, LargeSet/List incomplete; RTI Connext 7.7 and interop remain blocked without license. No compliance/certification or production claim.
- Detailed evidence and next audit steps: `docs/specification/ICD_CATALOG_AUDIT.md`, `docs/DELIVERY_GATES.md`.

### 2026-10-25 — Complete EO 6.1 *literal Topic mapping* (not G6 conformance)

- Reviewed EO 6.0 ICD Sections 6.1.1–6.1.14: all **14 service sections and 21 explicit Topic/IDL type mappings** now appear in the curated catalog. Added AnchorControl, AnchorSpecs, AnchorStatus and BatterySpecs to the previous ten.
- Added `--require-domain-topic-complete EO` to the pinned six-ICD extraction audit; it fails if even one EO operation Topic is missing from curated entries. It **does not** verify QoS, full Handler runtime, all service roles or Component Definitions.
- Generated C++ Handler interfaces now cover 14 reviewed EO services/21 official Topic types. Windows MSVC and Ubuntu native build and DDS tests are required evidence for this scope. CO and other ICDs remain an incomplete service catalog.
- **Delivery decision remains G1 PARTIAL, G6 OPEN, RTI Connext/cross-vendor unverified.**

### 2026-10-25 — Scoped CI result at EO 14/21 mapping checkpoint

- GitHub Actions [Linux official IDL/catalog/netns](https://github.com/nssang00/umaa-dev/actions/runs/40112466454): SUCCESS, EO complete Topic mapping check PASS, 14 reviewed service API contracts/21 official Topic types, DDS CTest 4/4, netns+netem wire 8/8.
- [Windows native reviewed API + official UMAA DDS](https://github.com/nssang00/umaa-dev/actions/runs/40112466468): SUCCESS, reviewed API smoke CTest 1/1, official DDS CTest 3/3 and synthetic 1/1.
- [GCC/Clang/MSVC SDK regression](https://github.com/nssang00/umaa-dev/actions/runs/40112466451): SUCCESS for all 3 jobs.
- Exact G6 result: EO literal service Topic mapping COMPLETE (14/21), **overall G6 still OPEN**; command/report runtime behavior only selected slice; QoS, component constraints, CO, RTI/interop and production safety remain unverified.

### 2026-10-25 — P1 six ICD operation / Topic expansion checkpoint

- Expanded the explicit-service catalog from EO-only 14 services / 21 Topic IDs to **53 service headings / 113 explicit Topic IDs** across EO, MO, MM, SA, SEM, SO with `catalog/umaa6-six-icd-reviewed.json`. These are documented entries in six pinned 6.0 public ICD transcriptions, NOT the normative total for all UMAA or an RTI/Component conformance certification.
- Added paired `command_groups` validation and C++ typed generated Handler interfaces for MM's 5 services with multiple Add/Delete/Clear Command types, plus MO's 5 ExecutionStatus messages. Preserved the single-Command and single-Report SDK APIs.
- Six-domain ICD ledger cross-check now rejects omission of any explicit Topic and verifies every referenced type exists in the official SHA-pinned IDL. The independent inventory script reports 352 IDL files, 61 possible service directories, 53 curated directories and 8 remaining unreviewed *directories* (not automatically 8 services).
- Added `sdk/tests/test_six_icd_contracts.py` for multi-group invalid contracts, type preservation and strict release rejection. Added this test to Ubuntu GCC/Clang and Windows MSVC CI.
- Linux [GitHub Actions #40153467543](https://github.com/nssang00/umaa-dev/actions/runs/40153467543): **PASS** official 53/113 ledger and generated API compilation, DDS CTest 4/4, netns 8/8 including packet-loss recovery. External official `idlc` headers produce warnings under `-Werror`; kept project `-Werror` and correctly isolated generated third-party headers as system includes instead of weakening project compiler checks.
- Cross-platform [GitHub Actions #40153467606](https://github.com/nssang00/umaa-dev/actions/runs/40153467606): **3/3 PASS** SDK regression on Ubuntu GCC/Clang and Windows MSVC.
- Windows official DDS [GitHub Actions #40153467601](https://github.com/nssang00/umaa-dev/actions/runs/40153467601) **SUCCESS**: 109 additional Topic TypeSupport generated, 53 reviewed service C++ APIs/113 official Topic types compiled with MSVC (CTest 1/1), BatteryStatus/EngineControl official DDS 3/3 PASS. Existing runtime DDS still only validates the BatteryStatus / EngineControl vertical slices, not 53 independent service implementations.
- **G1 remains PARTIAL; G6 remains OPEN.** CO-EXP ICD source, Component Definitions, QoS and independent per-service runtime acceptance not fully reviewed. RTI Connext 7.7 and interoperability remain blocked. Do not assert total UMAA 6.0 compliance.

### 2026-10-25 — Fail-closed Report DDS publisher and queryless reader binding

- Added `sdk/include/umaa/report_transport.hpp`: vendor-neutral, fail-closed Publisher intent executor (checked Register/Publish/Dispose/Unregister, no unsafe retries after unknown transport outcome) and subscription binding that fences callbacks by incremented Reader epochs on clean restart. This is not a normative QoS/vendor production adapter for all 53 services.
- Added deterministic `sdk/tests/test_report_transport.cpp`: handler exceptions, send/write/Dispose/unregister faults, subscription failures, reader teardown failure, timestamp ordering, cache cleanup, and stale callbacks after Reader restart; included in GCC/Clang/MSVC CTest. `Clang UBSan SDK and module regression` added to Actions.
- Official `BatteryStatus` example Publisher now creates/deletes the Cyclone Writer through the common transport binder and sends/disposes authentic `BatteryReportType`. Added distinct `umaa_official_battery_sdk_consumer` C++ process, reading authentic official DDS samples through `BoundReportConsumer`, checking Dispose/cache replay and Reader epochs; the independent C oracle remains unchanged.
- [Linux isolated DDS+netem job](https://github.com/nssang00/umaa-dev/actions/runs/40155793388): **SUCCESS**; CTest **5/5**, original 8 netns scenarios plus SDK Reader clean/degraded/outage-heal 3 scenarios = **11/11**. SDK Reader 100% outage produced **77** real kernel packet drops and recovered after qdisc removal.
- [Ubuntu GCC, Ubuntu Clang, Windows MSVC SDK CTest + Clang UBSan](https://github.com/nssang00/umaa-dev/actions/runs/40155793394): **3/3 PASS**, Clang UBSan step PASS.
- [Windows native DDS initial run](https://github.com/nssang00/umaa-dev/actions/runs/40155793392): SUCCESS for previous 3 official tests; new SDK Reader test was not included in that CTest regex. Revised Windows workflow **requires** the added reader test; its new run must be verified before claiming Windows new adapter wire proof.
- Compliance unchanged: G1 and G8 PARTIAL, G6/G7/G11 OPEN, G9/G10 RTI SDK/license BLOCKED, G12 only a bounded UBSan step PASS. CO/EXP and Component Definitions source evidence not fully mapped; do not invent mandatory QoS or declare all services running.

### 2026-10-25 — Fixed real 1%-loss Dispose race and proved cross-platform SDK Reader

- Linux `tc netem` revalidation detected a real intermittent defect: `OFFICIAL_SIM: incomplete 2/3` after both Report values but no Dispose, [failing Actions #40156075600](https://github.com/nssang00/umaa-dev/actions/runs/40156075600). **Do not treat the earlier 11/11 clean run as sufficient stability evidence.**
- Replaced the fixed 100ms Writer shutdown delay with `dds_wait_for_acks(writer, DDS_SECS(5))` before deleting Writer, checking all errors through `BoundReportProvider` fail-closed behavior. It is a DDS reliable middleware ACK barrier, not an official service QoS conformance proof.
- Repeated lossy official BatteryStatus tests **3 times for independent C oracle + 3 times for new SDK C++ Reader**, preserving EngineCommand and network recovery tests.
- [Final Linux runner #40156247032](https://github.com/nssang00/umaa-dev/actions/runs/40156247032): **SUCCESS**, official CTest 5/5, netns+veth+bridge+netem real-wire **15/15**, 100% blackout-heal 3/3, measured 108/72/72 dropped kernel packets.
- [Final Windows official DDS runner #40156246980](https://github.com/nssang00/umaa-dev/actions/runs/40156246980): **SUCCESS**, standalone SDK-side official C++ Battery Consumer CTest selected and PASS, 4/4 official DDS CTest, 53-service /113-official-Topic MSVC API compile smoke PASS, independent C wire proof PASS.
- [Final GCC/Clang/MSVC SDK + Clang UBSan #40156246985](https://github.com/nssang00/umaa-dev/actions/runs/40156246985): all 3 jobs SUCCESS, Clang UBSan step SUCCESS.
- G1/G8 remain PARTIAL: only BatteryStatus Report and EngineControl subset real-wire behaviors implemented. G6/G7/G11 OPEN (official CO/EXP + Component Definitions + full applicable QoS and protocol corpus), RTI vendor and interop BLOCKED, G12 incomplete despite UBSan. No UMAA/K-MOSA certification.

### 2026-10-26 — Follow-on autonomous implementation checkpoint (local only)

- Implemented `sdk/include/umaa/command_transport.hpp` as a vendor-neutral
  `CommandTransport<Payload>` / `BoundCommandProvider<Payload>` event binder.
  Strict ordering for Command Status/Ack/Dispose and last accepted payload
  retention (for full Ack Command reconstruction) are tested; any transport
  exception permanently faults the binder and prevents unreviewed retries.
- Fixed protocol race: a command disposed while device cancellation is unsafe
  can no longer accept a newer update. Added explicit `timeout(session)` that
  requires confirmed safe cancellation before transitioning to FAILED/TIMEOUT.
- Refactored synthetic stdio EngineControl host to use the new bound transport.
  Independent fixture scenarios preserve their expected traces.
- Hardened `CollectionAssembler`: single collection-instance identity, an
  optional timestamped Dispose with replay/reordering rejection, and bounded
  tombstone memory (fail closed on overflow). Untimed Dispose remains an
  explicitly weaker compatibility path.
- Local evidence: C++ CTest **7/7** (GCC strict & Clang UBSan), Python SDK
  tests **32/32**, simulator unit tests **6/6**, independent stdio scenarios
  **4/4**; native GCC and Clang CI-style pipelines each passed install,
  external consumer and scenario checks. See `docs/implementation/COMMAND_TRANSPORT_STATUS.md`.
- The new binder is NOT wired into the *official* Cyclone EngineControl host
  and does not supply vendor DDS entities, normative QoS/role mapping, or RTI.
  Thus G1/G8 remain PARTIAL, G6/G7/G11 OPEN, G9/G10 BLOCKED, G12 PARTIAL.
  Do not infer 53 real-wire service implementations or UMAA conformance.
- Official sources and DDS vendors unavailable in this local runtime; no
  Windows/RTI or new official on-wire tests rerun at this checkpoint.

## 2026-10-27 — Canonical `rydolabs/umaa-kit` P2 Reader & independent rejection proof

- The prior verified source snapshot from `nssang00/umaa-dev` was imported
  into `rydolabs/umaa-kit`; development/testing/distribution now use the same
  repository. The original repository still retains historical PRs. No full
  UMAA conformance or RTI license was transferred or implied.
- Added a reusable Cyclone DDS C `SampleReader` with ownership, stale epoch
  fencing, fail-closed error and callback semantics and RAII sample-content
  cleanup. The existing queryless `ReportReader` delegates to it; the genuine
  official EngineControl ignition-only host now uses `CommandReader` for
  incoming Command and keyed Dispose samples.
- The independent official EngineControl Simulator additionally sends a
  mismatched Destination Command and checks no Status/Ack response. The test
  supervisor independently requires the host's explicit rejection error.
  Windows real-wire CTest discovery now includes every `^official_umaa_` test
  rather than a hard-coded four-test allowlist.
- Feature PR [#1](https://github.com/rydolabs/umaa-kit/pull/1) is merged to
  main at `0541a6ca42fc0b0b8baa8bbade8e4315b533feec`. Exact verified source
  Git tree: `354102318ed618d4482242a49883dc7d4b55da42`.
- Local GCC strict 11/11 CTest and Clang ASan+UBSan 2/2 Reader unit tests passed.
  On the feature head, [Linux real DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40413028373)
  completed official CTest 6/6 including the negative Command; [Windows native real DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40413023193)
  completed official CTest 5/5, including the negative Command. Feature-head
  GitHub Actions 6/6 workflow runs SUCCESS.
- All other requirements remain gated: 51 additional service runtimes have
  not been independently implemented; complete QoS, roles, Collection,
  RTI cross-vendor interoperability, compliance and hardware safety are
  **OPEN/BLOCKED**. See [tracking issue](https://github.com/rydolabs/umaa-kit/issues/2)
  and `docs/DELIVERY_GATES.md`. The 53 reviewed typed API contracts compiling
  does **not** imply 53 real-wire service implementations.

## 2026-10-27 — P3 literal service registry / Cyclone context and new Topic proof

- Implemented C++ allow-list generated from 53 reviewed service names and
  113 explicit Topic/role/type definitions. Runtime rejects wrong type/role,
  unknown services and missing normative QoS before `dds_create_topic`.
- Added reusable `CycloneServiceContext`, integrated into existing real-wire
  BatteryStatus/EngineControl hosts. No hard-coded Catalog inference used.
- Added official AnchorSpecs Report Provider and AnchorControl Command Consumer
  opposite-role process tests with independent C Simulator, both discovery
  and *default-valued sample* DDS `write`/`take` transport only. This **does not**
  implement their full business semantics, Command Ack/Status/Dispose, keys or
  required Component QoS; total independently exercised service slices remain
  only two for protocol behavior.
- Linux official DDS CTest **10/10**:
  https://github.com/rydolabs/umaa-kit/actions/runs/40426885356
- Windows official DDS CTest **9/9**:
  https://github.com/rydolabs/umaa-kit/actions/runs/40426879585
- Local GCC CTest **13/13**, full GCC/Clang CI-style regression PASS,
  Clang ASan/UBSan registry+context **2/2**, Python negative registry **6/6**.
- **P3-1 remains PARTIAL** until actual reviewed new Report/Command payload
  fields and lifecycle are implemented and independently validated. G6 QoS,
  G7 Collection, RTI, broader 51 services and compliance are not closed.

## 2026-10-27 — P4 Anchor semantics merged and independently verified

- [PR #6](https://github.com/rydolabs/umaa-kit/pull/6) adds a new
  `AnchorSpecs` semantic real-wire Report update/Dispose path and
  `AnchorControl` virtual-device LOWER/RAISE/STOP, Command/Ack/Status,
  later-timestamp UPDATE, DDS Cancel/Dispose and negative Destination tests.
- Official AUVSI 6.0 TypeSupport and **independent C DDS oracles** used on
  Ubuntu and Windows; report physical fields are non-zero and checked twice.
  These are prototype-valued fixtures, not Component Definition-approved QoS.
- Exact validated feature commit `5ad1131033a0de7f4c78f3215c5d254195b5a66c`.
  [Linux real DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40450127689):
  official CTest **15/15 PASS**, Linux namespace test **15/15**, impaired
  **7**, blackout/recovery **3**.
  [Windows real DDS](https://github.com/rydolabs/umaa-kit/actions/runs/40450127737):
  official CTest **14/14 PASS**, installed real Cyclone CMake package external
  Consumer PASS.
  All **9/9** exact-source GitHub workflow runs succeeded.
- Merged into `main` as `ed4af11870fa8313c2d823a25ae1523e4661ec4d`
  with exact source tree `10a0cf40190a9636a6dd4a89fa87ad063dfc52d7`.
- Official service catalog QoS, full command/sensor/device semantics,
  remaining 49 reviewed service runtimes, RTI, conformance corpus and
  production/hardware acceptance remain **OPEN/BLOCKED**; do not release as
  a fully compliant SDK.

## 2026-10-27 M2 host-owned Command timeout

- PR [#12](https://github.com/rydolabs/umaa-kit/pull/12):
  clock-injected Command deadline scheduler, stale/replay fencing,
  safe-cancel retry, keyed disposal and DDS error fail-closed.
- Feature official DDS Linux **23/23** and Windows **22/22**, including
  independently checked `FAILED/TIMEOUT`, verified after official IDL
  generation. Feature Actions **9/9 SUCCESS**.
- Commit `dc4f5c15` merged; deeper normative QoS/recovery, RTI,
  entire 53-service runtime and conformance remain OPEN.
  [M2 test scope](docs/implementation/M2_COMMAND_DEADLINES.md).
