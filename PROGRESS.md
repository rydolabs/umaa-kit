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

## 2026-10-28 — Verifier V1 offline / local delivery (no repository push)

- New `verifier/` is an independently managed orchestration CLI; the original
  `simulator/`, `sdk/`, `dds/`, and `catalog/` paths are unchanged.
- Added development-only contract registry, four-scenario suite, scoped
  execution evidence, input/runner/contract file hashes, consistency audit,
  negative result semantics and a portable offline HTML evidence viewer.
- New Verifier unittest **28/28 PASS**; existing Simulator Python tests **6/6**
  plus DDS configuration tests **5/5**; original `tools/ci_verify.py` **PASS**
  (SDK Python **38/38**, CTest **14/14**, process fixtures **4/4**).
- Actual built `umaa_mock_engine_host` + independent existing simulator oracle
  via Verifier: four development-only stdio scenarios **4/4 PASS**.
- Added `.github/workflows/verifier.yml` but it has **not** been executed in
  GitHub Actions. This is a local working-tree overlay derived from handoff,
  NOT a new upstream commit. G6/G11, vendor, security and production UI remain
  OPEN/BLOCKED as before. See `verifier/INTEGRATION_STATUS.md`.

## 2026-10-28 — Verifier V2 scoped offline increment (local only)

- Independently added explicit operator DDS test-network acknowledgement gate;
  omitted acknowledgement blocks before subprocess launch. This is procedural,
  not enforced domain/network isolation or production authorization.
- Added SHA-256 checked, non-normative Studio JSON result exchange with schema;
  shared case IDs, contract IDs, run IDs, verdicts and input/artifact hashes.
- Added catalog-backed `scope-report` and release blocking
  `scope-report --require-ready`; zero approved normative requirements, 53
  candidate services and 26 unreviewed clause locators: status BLOCKED.
- Restricted single stdio scenario paths to repository simulator fixtures.
- Local tests: verifier **38/38**, Simulator Oracle **6/6**, Simulator network
  config **5/5**, original full SDK CI **PASS** (38 Python, 14 CTest and four
  stdio process fixtures); new Verifier actual C++ fixture suite **4/4 PASS**.
- V2 Studio exchange and offline HTML summary generated and validated.
- No real DDS, RTI cross-vendor, official conformance, hardened multiuser
  server, PostgreSQL or Web UI claimed. No remote push or GitHub Actions run.
- All existing `sdk/`, `dds/`, `simulator/`, `catalog/` source files preserved.

## 2026-10-28 — Studio V4 offline documents and Verifier integration (local only)

- DOCX model inventory generator and trusted Word-template subset with
  cross-run placeholder replacement, optional model tables, no overwrite and
  SHA-256 manifest verification added to `studio/`. Draft status is mandatory.
- Administrative-only Studio↔Verifier result association records digests and
  explicitly disclaims SUT/model identity equivalence and official conformance.
- Fixed EA SQLite importer to deterministically close read-only connections.
- New Studio tests include successful generation, tampering, malformed model,
  unknown template tokens, truncated record fields, export-schema rejection and
  model/result digest mismatch.
- Studio **15/15** unit tests, prior Verifier **44/44**, Simulator **6/6** and
  network-configuration **5/5**, existing SDK local CI **PASS** (38 Python,
  14 CTest, 4 stdio scenarios). Built C++ Mock Host through Verifier suite:
  **4/4 PASS**. Draft example DOCX rendered and visually inspected.
- Official conformance scope remains BLOCKED (0 approved normative requirements);
  no fresh real DDS / RTI test or real Sparx QEA acceptance was possible.
- See `docs/UMAA_ROADMAP_STATUS_V4.md`. No GitHub remote push or Actions run.

## 2026-10-30 — Repository-derived handoff + durable supervisor admission/recovery hardening

- Previous-session handoff was unavailable. Reconstructed the actual working
  ancestry from canonical GitHub: `main` `e6ba98d`; Draft Studio PR #14
  `9c1a779`; Draft modular SDK PR #15 `041a0f6`. Current continuation
  branch `fix/supervisor-journal-recovery-20261030` starts at the exact
  PR #15 source head, without any merge or deployment.
- **Exact tested implementation commit:**
  `3746c12d20a463ae07d97876cc01540c39d17269`. Recovery obtains the
  journal's exclusive owner marker during the entire read-modify-fsync
  transaction, closing a reconciliation-versus-new-owner TOCTOU race. A
  quarantined/absent/exited Worker is rejected before a fresh durable
  Command intent is persisted. Journal schema boolean version and
  non-integer action/session identities are rejected.
- Seven additional independent Python regression cases cover owner races,
  corrupt journal recovery, durable settled-session dedup, 24 bounded
  close/reopen cycles and dead-worker admission. These are synthetic /
  development safety checks, **not** power-loss or physical device tests.
- [Cross-platform SDK/Simulator run 40976805937](https://github.com/rydolabs/umaa-kit/actions/runs/40976805937):
  **SUCCESS** on Ubuntu 24.04 GCC, Ubuntu Clang + UBSan, and Windows MSVC.
  SDK discovery ran **51 Python tests** on each platform; the pipeline also
  built and ran CTest, installed SDK, and checked external consumers.
- [Cyclone plugin synthetic real-wire run 40976805660](https://github.com/rydolabs/umaa-kit/actions/runs/40976805660):
  **SUCCESS**; standalone plugin/process scenario, not mixed-network.
- [Independent scoped Verifier run 40976805791](https://github.com/rydolabs/umaa-kit/actions/runs/40976805791):
  **SUCCESS**; development fixtures only. Results are for the exact
  `3746c12` implementation snapshot; documentation-only follow-up commits
  require their own CI check and are not silently treated as the same SHA.
- [Current handoff](HANDOFF/CURRENT_2026-10-30.md) names exact ancestry,
  code/test entry points, unfinished approval gates, next actions and
  cross-session recovery limitations. `STATUS.md` now marks its V4 summary
  historical. All official TypeSupport/Key/QoS, native GROUP, mixed
  Cyclone/Fast network, RTI and independent conformance gates remain
  **OPEN/BLOCKED/DEFERRED**, never PASS.

## 2026-10-30 — approved current milestone scope reduction

The current development milestone is **standalone Cyclone DDS** and
**standalone Fast DDS** operation (tested separately), one compiled
business Component/client binary reusable with either backend, and
the scoped SDK/simulator/service-binding/supervisor safety work.
Mixed Cyclone↔Fast DDS network operation and RTI Connext DDS adapter /
licensed independent RTI tests are reclassified as **FUTURE / DEFERRED**.
They are excluded from this phase's required work and pass/fail gates.
The known strict mixed-network failures remain FAIL in historical evidence;
RTI remains unverified; full UMAA normative conformance is not claimed.

This is an explicit scheduling/scope decision, not deletion or weakening
of future technical requirements. Source of truth:
[CURRENT_PHASE_SCOPE.md](docs/CURRENT_PHASE_SCOPE.md).

## 2026-10-30 — P0 official service catalog fail-closed Topic ownership validation

**Branch:** `feature/official-contract-guards-20261030`, an additive
Draft PR #17 against unmerged scoped PR #16. **Source implementation SHA:**
`f1afcc5f5052ab57e6fa90974d8cfcab2c5fd363`. Changes do not touch
official UMAA on-wire topics or vendor backend/IPC behavior.

- One shared validator now gates BOTH the official C++ service-binding
  generator and the catalog-derived C++ runtime Topic allow-list.
  It rejects a Topic/IDL/Collection belonging to another service,
  Provider/Consumer direction inversion, forged/unsupported service
  identity, malformed roles/groups/clauses, and non-object/list shapes.
  This is a necessary G6 mapping integrity guard, **not** proof that
  normative Component roles, Keys or QoS have been independently approved.
- Regression mutations cover **all 113 currently catalogued Topic
  records** for foreign-service ownership rejection. The official
  C++ binding generator must refuse the foreign Topic **without writing
  any generated header**. Invalid documents are rejected, not repaired
  by guessing a DDS role or default QoS.
- On the implementation SHA, [Ubuntu SDK/Simulator CI](https://github.com/rydolabs/umaa-kit/actions/runs/40983077165)
  reports `REVIEWED_SERVICE_REGISTRY_PASS services=53 topics=113
  normative_qos_claim=none` and `Ran 61 tests` in the SDK Python
  discovery, with SDK build/CTest/simulator run SUCCESS. Other standalone
  vendor and OS checks must be inspected at the exact final PR HEAD
  before declaring full cross-platform regression PASS.
- [PR #17](https://github.com/rydolabs/umaa-kit/pull/17) remains Draft,
  unmerged. Mixed Cyclone↔Fast network and RTI independent verification
  remain **future-only** and are excluded from this phase. The current
  acceptance scope is [CURRENT_PHASE_SCOPE.md](docs/CURRENT_PHASE_SCOPE.md).

**Remaining G6/implementation work:** reviewed official per-service key and
DDS QoS evidence, Component Definition Provider/Consumer assignments,
real TypeSupport mapping and independent physical/single-vendor runtime tests
for every applicable service. Runtime identity validation does not satisfy
these normative review gates by itself.


## 2026-10-30 — first common Component Manifest/CLI + CMake DLL/SO project slice

- [Draft PR #19](https://github.com/rydolabs/umaa-kit/pull/19) on
  `feature/component-manifest-cli-generator-20261030` (based on Draft PR #17),
  **not merged**. No production deployment.
- Added `tools/umaa_component_cli.py` with `new component`, `service add/list`,
  `generate`, `build` and `test` using a shared versioned JSON assembly
  manifest. This is the future CLI/UI common input, but the React Flow
  designer itself is NOT implemented.
- Generator emits CMake MODULE, ABI v2 entry/registration, demo-only
  EngineControl/BatteryStatus service DTO codecs, per-service user Handler
  skeletons and native module loader CTest. Existing user handler files
  and README are never overwritten; unmarked CMake files are protected.
  Default user Command implementation rejects; Report throws rather than
  claiming successful device operation.
- Official reviewed catalog identities may be selected but **cannot be
  generated** without vetted TypeSupport, key, normative DDS QoS, role
  and protocol bindings. Module-side Consumer binding, custom IDL and
  official fully conformant runtime remain OPEN. Only **synthetic demo
  Provider** module generation is supported.
- New SDK Python tests include negative/duplicate role paths, injection
  guard, protected regeneration, CLI subprocess, and real installed-SDK
  CMake build + generated ABI-v2 module loader CTest.
- [Exact implementation source CI 41006782102](https://github.com/rydolabs/umaa-kit/actions/runs/41006782102):
  Linux GCC, Linux Clang and Windows MSVC SUCCESS, **69 SDK Python tests**
  (1 skipped) and **36/36 CTest** in inspected Linux and Windows job logs.
  [Cyclone plugin CI 41006782035](https://github.com/rydolabs/umaa-kit/actions/runs/41006782035)
  SUCCESS and [scoped Verifier CI 41006782045](https://github.com/rydolabs/umaa-kit/actions/runs/41006782045)
  SUCCESS at that same implementation source SHA.
- Root README now highlights the platform through Mermaid architecture
  diagrams, a copyable CLI demo and a factual current-vs-planned capability
  table. The former detailed README is retained at
  `docs/README_IMPLEMENTATION_HISTORY_2026-10-30.md`, and the
  common assembly/React Flow roadmap is documented in
  `docs/COMPONENT_PROJECT_GENERATION.md`.
- **Next acceptance gates:** formal Service Contract + Manifest schema,
  pinned EA/IDL/ICD provenance, vetted official SA GlobalPoseStatus
  Report Provider/Consumer vertical slice, later MO GlobalVectorControl
  Command/Status/Dispose, then supported React + React Flow Studio
  using exactly the same validator/CLI generator.
- Standalone Cyclone and standalone Fast DDS with same built modules remain
  current; mixed-vendor network interoperability and licensed RTI
  Connext validation remain later-only. G6 full normative conformance OPEN.

## 2026-10-30 — reviewed Service Contract snapshot and assembly catalog pin

- **Next isolated development branch:** `feature/component-contract-provenance-20261030`
  on top of the unmerged Draft PR #19; no merge or deployment.
- Added `tools/umaa_service_contracts.py`, a deterministic, SHA-256
  source-pinned projection of the existing independently reviewed 53-service
  / 113-Topic partial catalog. Snapshot records original Operation lists,
  Topic ID/type/provider/consumer directions, Collection type names,
  explicit Command families, ICD clause references, ledger revision and
  per-service `officialIdlVerified` / `ddsQosVerified` / `ddsBehaviorVerified`.
- Added standalone JSON Schemas for Component Assembly v1 and reviewed
  Service Contract Snapshot v1. CLI `contract export/verify` independently
  recreates the snapshot and rejects forged role/QoS claims, Topic changes
  and stale catalog revision. CLI `validate` checks assembly identity,
  role syntax, catalog source pin and actual *synthetic-only* generation readiness.
- New official entries added to an assembly get an explicit catalog SHA
  lock. Unpinned/stale/unknown official service entries are rejected;
  old Demo-only v1 manifests remain valid. Official-service code generation
  still remains blocked by unreviewed TypeSupport/key/QoS/runtime gates.
- Review found **two existing catalog rows without a per-service `source`
  string**: EO BatteryStatus and EngineControl. Exporting those now records
  `reference: null` explicitly rather than inventing a canonical source;
  source completeness remains OPEN. Tests assert this.
- New negative tests include mutated source digest, forged normative QoS
  approval, inverted Topic direction, invented Operation, stale source,
  unpinned manual assembly, role duplicates and CLI export/verify paths.
- The generator/ABI v2 and existing standalone Cyclone/Fast DDS milestones
  are otherwise unchanged. No official K-UMAA/UMAA conformance or real QEA
  acceptance is claimed.


## 2026-10-30 — Studio UI/CLI contract parity API (bounded read-only slice)

- Branch `feature/studio-component-contract-api-20261030` stacks on
  unmerged Draft PR #20, then Draft PR #19. No merges or deployments.
- Refactored CLI `check_assembly(doc)` into a **pure validator**; the CLI
  still uses `check_project(path)`, and Studio API now reuses the exact same
  contract/role/source-hash gate without filesystem or code generation.
- New project-authenticated GET `/api/v1/projects/{id}/service-contracts`
  returns the deterministic 53-service/113-Topic reviewed mapping with the
  same source SHA and explicitly unknown source fields where missing.
  POST `/api/v1/projects/{id}/assemblies/validate` checks typed JSON only.
  Unauthorized calls fail 401/404; malformed/unknown/stale contracts 422;
  >128 KiB payloads 413; no DB writes, job enqueue or native build execution.
- Added Studio API tests for project RBAC, viewer access to non-mutating
  contract inspection, absent source preservation, official provider+consumer
  roles in one Assembly, forged/unpinned contracts, oversized bodies,
  no queued jobs and CLI/Studio readiness parity. Studio V6 still uses
  legacy React; **React Flow Component Designer is NOT implemented**.
- Verified scope is **API/UI foundation only**, not full EA service
  extraction, production authentication, native official DDS bindings or
  formal UMAA compliance. Standalone Cyclone/Fast current milestone unchanged.

---

> **Historical checkpoint:** The following PR #18 record was authored before its integration into the mainline. Earlier Draft/unmerged statements below are historical, not the current project status.

## 2026-10-30 — independent supervisor post-close owner fence

**Separate Draft [PR #18](https://github.com/rydolabs/umaa-kit/pull/18)**,
branch `fix/supervisor-closed-owner-guard-20261030` based on unmerged PR #16,
implementation commit `1ec7f457a8789fd56f3988247bf60cac0250f487`.
This does not depend on the unrelated PR #17 official catalog guard.

- `OperationJournal.begin/resolved/resolved_all_after_reconciliation`
  require its exclusive owner handle to remain held; a stale journal
  object cannot write/settle intent after releasing the owner marker.
- `ComponentProcessSupervisor.restart(hardware_reconciled=True)`
  cannot resuscitate a closed supervisor after the journal marker was
  released; lifecycle checks are serialized with `close()`.
- `stop()` cannot return a clean success for an already faulted/closed
  worker simply because the process handle was removed.
- Two synthetic, hardware-free regressions cover released-object writes
  and post-close restart. Exact implementation SHA's
  [Linux SDK run 40983705423](https://github.com/rydolabs/umaa-kit/actions/runs/40983705423)
  reports `Ran 53 tests` and a successful SDK/CTest/Simulator job.
  Independent vendor CI and Windows results must be inspected for final
  HEAD before claiming full regression pass.

**Residual risks and blockers:** single trusted local filesystem, untrusted
physical device state, power-loss resilience, authenticated IPC,
long-duration crash/soak, and full official UMAA conformance remain OPEN;
the code does NOT perform hardware reconciliation. Mixed Cyclone/Fast
network and RTI remain explicitly future-only.

### Follow-up: real Worker process integration for post-close policy

Commit `fe4422f43b08b27433092e11a47b51eedf5a75f3` adds
**two real worker-process CTest regression scenarios** to
`sdk/tests/test_process_supervisor.py`:

1. `close()` releases the journal owner; restart of the closed instance
   refuses even with an explicit hardware attestation, while a new owner
   with no pending Command can start safely.
2. Closing an asynchronous pending Command does not falsely indicate a
   clean stop, allow a stale-owner restart, or let a fresh process run
   against the unresolved durable journal.

[Linux GCC implementation CI](https://github.com/rydolabs/umaa-kit/actions/runs/40983969876):
**PASS** on exact test commit, 53 Python SDK tests and 36/36 CTest
(including isolated Worker scenarios). Check Windows/Clang + vendor plugin
results and the final documentation SHA independently before declaring
the complete PR verified.

**Integration resolution (2026-10-30):** PR #18 Supervisor owner-release and post-close worker tests were reconciled with the SDK, catalog, CLI and Studio changes on main. This does not establish physical hardware reconciliation, production IPC security or formal UMAA conformance.


## 2026-11-03 — official Report source-candidate handoff (Draft PR #46)

- **Ancestry:** Draft PR #46 `feat/official-report-source-handoff-20261103` stacks on Draft PR #45 `safety/official-runtime-evidence-gate-20261103`, then #44. Do not merge, deploy or reinterpret source verification as release approval.
- Added `tools/official_report_source_handoff.py`: require an external exact original ICD byte digest, pinned external official IDL ZIP, authored official-reference Service YAML, and independent ZIP/YAML pins. Reuse existing audited IDL declarations to verify a single queryless provider Report Topic and Key fields without copying source files or inferring normative QoS.
- Output is deliberately `UNAPPROVED_OFFICIAL_REPORT_SOURCE_CANDIDATE` with source member digest, provenance and unresolved requirements; `readyForOfficialNativeRuntime=false` and `officialConformanceClaim=false` unconditionally. No official native DDS/SDK runtime adapter was generated.
- Added synthetic-only negative regression against incorrect original/IDL SHA, forgery, missing Key, wrong direction/protocol, unauthorized secondary transcription, overwrite attempts. Existing synthetic Report E2E path remains untouched.
- **Observed CI for code+documentation snapshot `1d28ce2b5e79bfa8c79bfbaadbde6d5a3a0cfd0d`: Ubuntu and Windows `source-audit` jobs both SUCCESS in [run 41656536440](https://github.com/rydolabs/umaa-kit/actions/runs/41656536440). Other applicable jobs were still running when this note was written; their final results and this note's later HEAD need independent verification.
- **Blocking evidence:** original authoritative ICD clause interpretation, Component Definition roles/applicability, mandatory DDS QoS, complete keyed semantics, reviewed nested payload codec, full lifecycle/Collection semantics, independent official vendor E2E/negative oracles, source reviewer acceptance and conformance gates remain OPEN.


## 2026-11-04 — actual EO BatteryStatus source and Collection QoS evidence (Draft PR #47)

- **Stack:** Draft #47 `feat/official-battery-source-qos-20261104` targets Draft #46, then #45/#44. No merges, deploys, or official Runtime enablement.
- Source files were read **outside Git**. Independently measured public original EO ICD PDF SHA-256 `47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453` (see canonical evidence manifest for the exact digest); public UMAA 6.0 **with-EXP** ZIP SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`, **594 IDLs**, distinct from prior 352-IDL bundle. RTI QoS XML SHA-256 `84d0b7d84dda6ffd8c4a9ece18d11df8ff865a67b6471a98391ec032485f628b`.
- Implemented pinned manifest (only hashes, source IDs and reviewed clause positions), `tools/official_battery_status_evidence.py` for offline authenticated source/IDL key/Topic/import checks, scoped EO ICD 3.8.1 element Topic ordering against the pre-existing QoS YAML, and **reference-only RTI XML** inspection. The RTI example has RTI Connext-only license restrictions and must not be copied into Cyclone/Fast DDS.
- Extended the existing Report candidate CLI to optionally attach the authenticated Battery Report + separate Collection evidence. The canonical reviewed YAML, generation plugins, native SDK ABI and all official Runtime safeguards remain unchanged.
- New synthetic positive/negative regression in `sdk/tests/test_official_icd_evidence.py` for forged Topic/Key, source SHA, QoS scope, RTI example drift, and unapproved Runtime flags; Linux/Windows source-audit workflow tracks all changed files. **Exact PR #47 CI pending** at this checkpoint; do not infer it passed from #46.
- **Still OPEN:** full normative QoS, Component Specification role/applicability, original PDF semantic machine-review, official nested payload codecs, Large List semantics across Topics, independent official DDS E2E, mixed-vendor networking, RTI runtime and UMAA conformance.


## 2026-11-04 — official BatteryStatus nested host structure (Draft PR #48)

- **Base:** Draft #48 `feat/official-battery-payload-structure-20261104` stacks on Draft #47 then #46/#45/#44. It leaves official Runtime generation fail-closed.
- Inspected original externally supplied **594-IDL** UMAA 6.0 bundle SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`. Targeted **six** structs and **nine** scalar typedef declarations, including Battery Report, Cell List element, BatteryCellDataType, IdentifierType, LargeListMetadata and DateTime. Report has six `@optional` measurement fields; element has optional nextElementID; types/keys/field order are preserved in `catalog/official-evidence/eo-battery-payload-v1.json`. No original IDL or PDFs added to Git.
- `tools/official_battery_payload_structure.py` authenticates archive+selected member digests, exact nested IDL structure/annotation and typedef bounds, and the existing PR #47 Topic/Key ledger. An explicit **host JSON structural validator** checks required versus absent optional fields, nested data, 16-octet GUID represented as 32 lowercase hex digits, signed timestamp/nanoseconds, enum and measured IDL range limits. It is NOT a CDR codec, vendor TypeSupport ABI, or actual DDS transport.
- Important non-inferred ranges: `EnergyPercent` original IDL 0..1000 (not 0..100), `DateTimeSeconds` signed 64-bit with decimal-string bounds in JSON manifest to preserve exact integer precision, `DateTimeNanoseconds` 0..999999999.
- Added synthetic-only positive/tamper/negative suite `sdk/tests/test_official_battery_payload_structure.py`; it runs in the existing Linux/Windows source audit CI. The actual original IDL is checked locally, not uploaded as runner test material.
- Added `validate_completed_list_snapshot` for an externally assembled Battery List: exact listID/size, linked chain, timestamped update checkpoint, and NIL empty-list validation (EO ICD §3.8.2–3.8.5). Missing Dispose/tombstone evidence deliberately rejects nonempty timestamp-less updates. This is post-hoc host-only validation, **not** live DDS buffering/coherence/Dispose proof.
- **PENDING** exact-head PR #48 check completion, including older backend regressions. Preserve Draft/unmerged status.
- **BLOCKED:** official native DDS TypeSupport↔host codec, full QoS and Component roles, separate Report/ListElement lifecycle/Dispose/coherence, independent actual official Cyclone and Fast DDS E2E; no formal UMAA conformity claim.


## 2026-11-04 — official BatteryStatus Cyclone native host codec + independent two-topic DDS wire (Draft #49)

- Based on Draft #48 exact previously verified head `c12ecb0fc8017dfb47459e91245f41e3f6881f43` (**15/15 GitHub checks SUCCESS**). Draft #49 stacks on #48; do not merge or deploy.
- Added a DDS-header-free C++ SDK host model for original nested BatteryStatus Report, IdentifierType, DateTime, LargeListMetadata, Cell and ListElement; a **Cyclone-only native adapter** maps it into actual compiler-generated AUVSI 6.0 C TypeSupport and decodes it back. Six optional measurement values have bounded, per-sample stable native pointer backing; Key GUIDs are 16 bytes, and timestamp/enum/scalar bounds fail closed. No handwritten DDS CDR.
- Added a real two-Topic Cyclone DDS publisher and **independent standalone C oracle** verifying all selected Report fields, separate Cell fields, source/list/element IDs, metadata checkpoint and both keyed Dispose events over separate OS processes. An intentional mismatched metadata checkpoint must be detected and rejected by the oracle (exact exit code), not silently accepted.
- Added source-pinned dedicated `.github/workflows/official-battery-native-wire.yml` to download the original **594-IDL** ZIP (SHA-256 `15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`) into temporary CI storage; validate the selected original payload declaration closure, generate *only* transitive Battery TypeSupport with idlc, and execute two CTests (positive and deliberate negative). Original source/RTI example/generation outputs are not committed or uploaded. Other existing synthetic and Windows regression workflows remain independent.
- **PR #49 new head native CI PENDING** at the time of this checkpoint; exact CI run/job, errors and outcomes must be checked and repaired. A green original-scope test does NOT constitute a full official UMAA service, Fast DDS official Runtime, reviewed per-Component QoS or conformance.
- Still OPEN: Fast DDS native nested Battery mapping and independent E2E; complete cross-topic collection lifecycle including deletion/tombstones/recovery; report Provider business module binding, source GUID and actual Component Definition/Specification; full QoS & deployment; independent negative protocol cases and certification.



### 2026-11-04 — isolated DDS netem regression investigation

- On PR #49 snapshot `db21cb0931dd091a19ac4da3035031227f93edfa`, the dedicated **official nested Battery TypeSupport / independent C oracle job PASSED**, including positive data/Dispose and deliberate bad checkpoint rejection (run 41729227092). Most legacy jobs passed.
- One existing `isolated-dds-wire` run (41729227031, job 126241899689) failed only on the **third** 1% packet-loss iteration of the older SDK scalar Battery Report reader: `Battery Report subscriber incomplete`. The preceding two impaired repetitions, independent real DDS tests and blackout recovery steps passed. It is NOT a failure of the new Battery codec job.
- The original failing job was rerun individually to assess reproducibility; results require exact run-attempt checking.
- Increased the old scalar official Report **test-only** impaired Writer final-Dispose retention from 1s to 3s and reader impairment poll budget from ~6.25s to ~15s, while still requiring two ordered data events, a keyed Dispose, cache removal and epoch safety. Clean/unimpaired settings unchanged; the receiver now reports the incomplete phase. This is a bounded test robustness adjustment, NOT an invented DDS or UMAA QoS requirement and NOT evidence that application-level Dispose ACK exists.
- **Current HEAD regressions must be rerun at the new SHA before declaring this fix verified.** If errors persist, capture the exact phase, source and DDS reader info and quarantine the flaky netem case rather than weakening the oracle.


## 2026-11-04 — bounded EO Battery multi-list host routing (Draft PR #58)

- Stack: Draft PR #58 is based on verified Draft #57, then #56, then #55. `main` remains unchanged; no Runtime unlock or cross-vendor claim.
- Added `sdk/include/umaa/official_battery_collection_router.hpp`: bounded per-listID routing over existing LargeListJoiner, interleaved independent reports/elements/Dispose, explicit retirement and disconnect invalidation; aggregate record cap prevents silent unlimited buffering.
- Hardened `LargeListJoiner::on_report`: reject over-limit and invalid initial NIL metadata before advancing source/timestamp/pending state; later valid Reports can proceed without a forced reset from the rejected input.
- Expanded the original host C++ regression for two interleaved list IDs and distinct sources, independent generations, reused-listID source collision, capacity, retirement, unverified QoS, and reset/replay. Linux/Windows Battery host test workflow tracks the added header.
- **Verified on exact PR #58 SHA `3004e8499ece55477d0f87145c5e059d298e9c10`: 10/10 GitHub Actions workflows SUCCESS**, including both Linux and Windows host router tests, existing native Cyclone 3-cell and one-list Topic E2E, cross-platform SDK, independent verifier, Cyclone/Fast DDS plugin regressions. [host router run 41822483427](https://github.com/rydolabs/umaa-kit/actions/runs/41822483427), [original three-cell DDS run 41822483459](https://github.com/rydolabs/umaa-kit/actions/runs/41822483459). Native source/list ownership, official two-list E2E, cross-topic atomicity, reconnect replay, full Component QoS, vendor interoperability and conformance remained OPEN at that SHA.


## 2026-11-04 — actual two-source EO Battery original DDS List isolation (Draft PR #59)

- Base: exact verified Draft PR #58 SHA `3004e8499ece55477d0f87145c5e059d298e9c10` (10/10 workflows successful); #58 stacks on Draft #57 → #56 → #55. `main` unchanged.
- Extended original Cyclone TypeSupport multicell publisher with opt-in two-source/two-list mode. Interleave 2 Report source GUIDs, 4 Cell key pairs across 2 listIDs, 2 original Report Dispose and 4 original Element Dispose events. Retain existing single-list options and tests unchanged.
- Added independent two-list consumer using actual original TypeSupport, real DDS instance handles and the host `LargeListRouter`. Require both independently stable payload-checked snapshots and all six genuine keyed Dispose events. Negative mode corrupts only second list's checkpoint: all data/disposals must still arrive, first list stays stable and second must never be stable.
- Added separate Python process supervisor, two CTests, and the original-source-pinned multi-cell Cyclone workflow now invokes **four** positive/negative tests, subject to exact-head CI. No copyrighted source is committed.
- **Verified on exact code/documentation snapshot `c9d5320383ff9ee889c05d304dfa19eba08c3677`: 12/12 triggered GitHub Actions workflows SUCCESS; 0 failures/pending.** The original source-pinned Cyclone DDS [native two-list run 41823468936](https://github.com/rydolabs/umaa-kit/actions/runs/41823468936) compiled all original types and ran **4/4 CTests PASS**, including both new independent two-source List positive/negative cases and existing three-Cell checks. [Windows official DDS run 41823468940](https://github.com/rydolabs/umaa-kit/actions/runs/41823468940) and [Linux isolated netns recovery run 41823468939](https://github.com/rydolabs/umaa-kit/actions/runs/41823468939) also succeeded; cross-platform SDK, both DDS plugin regressions and same-binary vendor parity were successful. This later progress-only text commit is NOT claimed to have independent new exact-head Actions. Linux/Windows host tests ran at verified #58 code; the native two-list oracle runs on Ubuntu. This scoped real-wire proof does not establish full native QoS, Component roles, source/list ownership for arbitrary inputs, Fast DDS two-list parity, network reconnect or official conformance.


## 2026-11-04 — source-reviewed UMAA Component role projection and native QoS checks

- Original AUVSI 6.0 Component Definitions v1.0 §3 reviewed: a tilde
  marks Consumer, absence marks Provider. Existing 91 source-extracted
  service mention rows now record 51 Provider/40 Consumer source roles,
  with original 12-page PDF SHA-256 pin and a mandatory ephemeral
  PDF-to-row CI cross-check. No original PDF or licensed IDL committed.
- `tools/verify_compliance_scope.py` preserves all 53/113 candidate Topic
  counts, checks exact role symbol evidence, requires undetermined
  applicability and refuses any unsupported Component membership/QoS or
  conformance PASS; strict gate remains intentionally code 3.
- SDK Component assembly `validate --reference-component '<published name>'`
  rejects conflicting source roles and ambiguous service identities without
  changing Runtime generation, and accepts one Component exposing the same
  service as both Provider and Consumer when §3 lists both roles.
- Cyclone original Battery two-list source-pinned DDS producer and
  independent consumer check the **actual** Publisher/Subscriber
  Presentation QoS; a separate default-QoS negative CTest rejects missing
  EO §3.8.1 mandatory Topic ordered access.
- No selected platform Component Specification, complete per-service QoS,
  nonexperimental CO trace, native official all-service Runtime, production
  Device safety or formal UMAA conformance has been claimed.
- **Pending exact-head CI verification:** original PDF role audit,
  synthetic SDK CLI negative tests, original Cyclone DDS 5 native CTests,
  and triggered cross-platform/vendor regressions. Do not merge on docs
  or previous green CI alone.


## 2026-11-05 — typed BatteryStatus Handler subscriber (scoped, CI pending)

- New SDK header `sdk/include/umaa/official_battery_subscription.hpp`
  owns the **host-side** BatteryStatus List state and dispatches accepted
  original `Report`+`Element` snapshots to a business-only Handler.
  Explicit Source/Parent ID → List ID binding, per-reader epoch, true
  keyed Dispose retirement and stale callback rejection are implemented.
- Partial Report/Cell updates invalidate the current snapshot until
  a complete checkpoint is observed. Source collision, wrong Parent,
  replay of retired IDs and business Handler exceptions fault the
  subscription and require an explicit reset. No stale automatic replay.
- The original Cyclone DDS independent two-source/two-list CTest now
  calls the SDK subscription; business Handler only receives callbacks.
  The original TypeSupport, native Publisher/Subscriber QoS inspection
  and genuine DDS instance-handle-derived Dispose oracle remain.
- Added C++17 Linux/Windows host regressions for two sources, independent
  update generations, source Dispose, late Cell tombstones, no resurrected
  data, reader epoch rotation, invalid input and Handler failure/recovery.
- **Verified exact code snapshot:** commit
  `683fbbd16ad9bf19a234605ec6d5321db6d9f5f8`, GitHub Actions
  **12/12 workflows SUCCESS, 0 failures/pending** (11 PR events plus the
  branch push event). [Linux/Windows Handler host run 41931272014](https://github.com/rydolabs/umaa-kit/actions/runs/41931272014):
  both platforms compiled -Werror/WX and **2/2 CTests PASS** per platform.
  [Authentic original Cyclone two-List run 41931271957](https://github.com/rydolabs/umaa-kit/actions/runs/41931271957):
  **5/5 native CTests PASS**, including two distinct Source business
  Handler snapshots, scoped bad-checkpoint isolation, genuine 6
  keyed DDS Disposes and default-QoS negative.
  [Full native source-pinned integration run 41931271922](https://github.com/rydolabs/umaa-kit/actions/runs/41931271922),
  [Windows official DDS run 41931271967](https://github.com/rydolabs/umaa-kit/actions/runs/41931271967),
  [Linux netns fault/recovery run 41931272030](https://github.com/rydolabs/umaa-kit/actions/runs/41931272030),
  and independent SDK/Fast DDS plugin and same-binary substitution checks
  also succeeded. This later evidence-only documentation commit requires
  a separate HEAD-status check; do not misattribute the earlier exact
  code SHA's CI to a later commit.
- This is NOT automatically generated official native DDS Runtime, complete
  Service QoS/Component requirements or full UMAA conformance. The
  official native Runtime generation gate remains unchanged.


## 2026-11-05 — native Cyclone BatteryStatus SDK Reader ownership candidate

- Parent: merged #61, `main` SHA
  `019678442fd016afee88bf4bc42e1788475e695b`.
- Added `OriginalBatteryStatusReader` under the existing Cyclone vendor
  include tree. It owns actual original TypeSupport-backed DDS
  Participant/Subscriber/Report and Collection Element Readers, queries
  effective TOPIC ordered Presentation, decodes original Report/Cell
  samples and resolves two original instance Keys per Cell from genuine
  native DDS handles. DDS resources and native sample allocations have
  explicit RAII cleanup on normal, fault and restart paths.
- Added a separate business-only Handler application: no DDS vendor calls
  in user application logic, two original Source/List Report snapshots,
  full Cell payload checks, native keyed Dispose counts, independent
  negative mismatched List checkpoint and new Reader restart.
- Native original source-pinned CTest workflow requires **7 CTests**
  (prior 5 + 2 SDK-owned native Reader cases). Existing source-pinned
  eight-case official Cyclone workflow, standalone SDK and vendor
  synthetic regressions remain enabled.
- **Verification pending for this PR exact HEAD.** Do NOT claim
  automatically generated/approved official Runtime, full Component QoS,
  GROUP coherence, network-partition reconnect recovery, Fast DDS
  official two-list parity, or formal UMAA conformance.


## 2026-11-05 — real DDS delayed-Dispose observer proof (netns candidate)

- Investigated merged-main [Linux namespace run 41934679212](https://github.com/rydolabs/umaa-kit/actions/runs/41934679212):
  during the third 1% packet-loss/delay repetition the independent
  BatteryStatus C reader observed **both** ordered Report values but
  **no real DDS keyed Dispose** (`OFFICIAL_SIM: incomplete 2/3`, rc=6).
  Existing native `dds_wait_for_acks` and fixed three-second
  Writer-retirement grace had completed; middleware ACK alone is not
  evidence that the separate reader application observed Dispose.
- Added a **strictly opt-in** bounded original Cyclone `ReportWriter`
  retirement-observation callback. When enabled it retains the real DDS
  Writer until a higher-level independently verified observation is
  confirmed or throws on deadline; default output and lifecycle remain
  unchanged, and no fake DDS/UMAA on-wire ACK is introduced.
- Linux `run_netns.py` now creates a new temporary private receipt
  directory **per impaired run**, shares only its paths with its isolated
  process actors, and requires one receipt for each actual independent
  reader before returning PASS. Original independent C Battery reader
  writes the marker **only after exact 20/80/real keyed DDS Dispose**
  and clean teardown. The C++ SDK reader writes it only after the same
  on-wire evidence, cache disposal and stale-epoch rejection. Publisher
  waits for the exact marker for all configured readers (max 15s) before
  retiring the DDS Writer. Malformed, missing, stale or wrong-node
  receipts are negative-tested, not treated as success.
- These private marker files are **test-harness-only out-of-band
  coordination**; they are not additional UMAA Topics, official protocol
  acknowledgments, durable evidence or production hardware success.
  A missing real DDS event still fails the independent oracle, publisher
  gate and namespace supervisor. Normal/non-impaired deployments retain
  the existing default fail-closed behavior.
- **Exact-head netns & cross-vendor CI remains to be run for this branch.**
  No official QoS profile, GROUP coherence, Fast DDS ordered TOPIC,
  mixed-vendor operation, automatic reconnect replay or formal
  conformance is claimed.


### Netns negative CI correction (same 2026-11-05 branch)

The first [netns run 41942073180](https://github.com/rydolabs/umaa-kit/actions/runs/41942073180)
confirmed **three separate real 1%-loss Battery tests**, each with actual
20/80/Dispose at BOTH original C readers and exact receipt markers:
`DDS_NETEM_INDEPENDENT_DISPOSE_RECEIPTS_PASS` appeared three times.
Its next EngineControl degradation test then failed **in the harness**
because the receipt gate was initially enabled for all impaired services;
the original Engine Command/Ack/Status/Dispose oracle itself had passed.
The fixed harness now enables the observation check only with explicit
`--require-observed-dispose`, applied to the four native Battery impaired
workflow commands (two C reader scenarios and two SDK reader scenarios),
and rejects this flag on clean or Engine UPDATE scenarios. No EngineControl
sender/receiver, normative role or QoS is changed. Exact new-head CI must
pass before merging; the first failed workflow is retained as negative
diagnostic evidence.

## 2026-11-05 — netns subprocess cleanup must not mask missing native DDS Dispose

- Latest merged-main network namespace workflow [41944916113](https://github.com/rydolabs/umaa-kit/actions/runs/41944916113) **FAILED** after two successful impaired original Battery runs. The third run reported `official module/transport error: independent application Dispose observation not confirmed` and independent `OFFICIAL_SIM: incomplete 2/3`. This is an **unresolved real DDS delivery/lifecycle issue**, not an acceptable PASS.
- The harness then raised a *second*, unrelated `ValueError: I/O operation on closed file` in `stop()` while re-calling `communicate()` on an already-closed stream, obscuring the original failure. Fix cleanup to use bounded `wait()` for closed streams and bounded `communicate()` otherwise; add real child-process negative regressions for both paths.
- This change is **diagnostic hardening only**. It does not alter Cyclone DDS QoS, lengthen timeouts, synthesize Dispose, suppress the independent oracle, or establish reconnection correctness. The impaired test must remain red if a genuine DDS Dispose is absent.

## 2026-11-05 — Component v2 mixed Provider + synthetic Report Consumer

- Based on README visual-architecture merge `main`
  `6a1a5c96dbc472a3275c65a9bc3ee4cc0c74b4ad`.
  This increment implements a missing **development** assembly lane:
  the same generated native DLL/SO can both provide and consume
  synthetic `Demo::EO` Services and can bind the same synthetic
  Report Service in both roles. This is a scoped prototype, NOT
  official native UMAA 6.0 Runtime generation.
- C ABI v2 structure/layout/version unchanged. New explicitly
  host-local `UMAA_COMPONENT_REPORT_ACCEPT` (5; bit 8) and
  `UMAA_COMPONENT_REPORT_DISPOSE` (6; bit 16) actions carry a
  nonzero opaque host-local source token via the existing
  `session_id` field; the caller must validate genuine external
  DDS Keys, Topic/QoS and disposal. The default generated
  `ReportSink` user callbacks always reject, no silent success.
- The C++ `Component` registration merges disjoint Consumer and
  Provider actions for the same Service ID/schema under one
  ABI-v2 descriptor, but rejects duplicate overlapping roles or
  differing schema IDs. Host loader checks new bits and still
  drains in-flight callbacks before module unload.
- CLI generates a protected business Consumer Handler file,
  adds the synthetic Report decoder and validates a mixed-role
  manifest; official Service and Command Consumer generation
  remain **fail closed**. The generated native smoke additionally
  rejects malformed Report payloads, invalid source tokens and
  fake Dispose data, and verifies module action bits.
- New portable C++ CTest validates actual Consumer Handler
  callbacks, keyed opaque source token, Dispose, exception
  fencing and same-Service bi-directional dispatch. Existing
  Python CLI suite builds and runs synthetic Provider, mixed,
  and bidirectional DLL/SO projects against installed SDK.
- **Original feature HEAD CI 19/19 success; integrated branch HEAD CI pending**. No official
  TypeSupport, native DDS Consumer reader ownership, full
  per-Component QoS, source GUID attestation or UMAA conformance
  claim; no original AUVSI source files committed.

## 2026-11-06 — native DDS Deployment admission: structured source-gap gate (candidate)

- Based on merged main 6330b997f94e97e3321f978641a1230cc641265b (PR #73).
- Added a deterministic read-only Deployment admission report derived strictly
  from the pinned minimal System Intent and existing Planner. It reports
  individual original Operation-to-Topic/Flow, native Key/TypeSupport,
  DDS Topic name, mandatory writer/reader QoS, lifecycle and independent
  vendor wire-evidence obligations. Nothing is inferred from service naming.
- Both CLI entrypoints return JSON plus **exit code 3** for blocked native
  deployment, rather than making a valid topology appear executable.
  Symlinks, stale Catalog/Manifest SHAs, invalid links, and injected
  user QoS/endpoint fields retain the existing strict validator failures.
- New SDK unit and subprocess CLI regressions. No real DDS entities, official
  runtime code generation, production Studio wiring or certification.
  CI and native regression results must be checked on the exact branch HEAD;
  this entry alone is NOT a successful test claim.
- Next: source-approved explicit Operation/Topic/Type/Key/QoS bindings with
  exact original provenance plus native vendor construction/independent tests.

## 2026-11-06 — EO AnchorControl original source-recheck bindings (PR #74 follow-up)

- Source review: AUVSI EO ICD v6.0 SHA-256
  47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453,
  §6.1 pp. 50–51. Original UMAA 6.0 IDL ZIP SHA-256
  15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08.
- Reviewed the three literal AnchorControl DDS Topic constant values and
  exact @key annotations. Command has source/sessionID/destination; Ack
  and Status have source/sessionID. Five operations map to three Topics:
  reports / set write; query subscribes to Ack; cancel disposes Command.
- Added independently callable exact-source verifier with no original source
  redistribution; optional dual source paths on both deployment check CLIs.
- Even after source recheck, QoS/vendor profiles, native lifecycle completeness,
  actual official runtime, independent exact-build wire evidence and UMAA
  conformance remain BLOCKED. Catalog flags unchanged; PR remains Draft.
- CI without supplied ZIP/PDF validates schema, negative gates, projections
  and backward-compatible no-evidence output only. Exact original files are
  manually byte-checked for this scoped evidence and must NOT be committed.


## 2026-11-06 — EO BatteryStatus and EngineControl original evidence expansion

- Continued PR #74 from AnchorControl-only source admission. Verified
  original EO ICD 6.0 §6.1.5 pp54–55 and §6.1.6 pp55–56, and original
  UMAA IDL ZIP source digests for BatteryReport and the three Engine
  Command/Ack/Status IDL types.
- BatteryStatus has a **second Collection Element DDS Topic** in the same
  BatteryReportType.idl, with listID/elementID keys, separate from the
  Report's source key. EO §3.8.1 p14 requires Topic-scope ordered
  PRESENTATION for Collection element publishers/subscribers; this
  partial source rule is not a whole approved effective QoS profile.
- Added exact reviewed source records and a separate source verifier for
  these two Services; optional source-gated admission now projects
  selected Service topics and operations without changing existing
  Planner, native component lifecycle, or compilation.
- No original PDF, ZIP, IDL bodies or vendor source are committed. Real
  DDS runtime/effective QoS, Collection coherence/ownership and
  complete official conformance remain BLOCKED and require
  independently checked native tests.
- Added targeted tests and CI checkpoints. Exact-head workflow results
  must be examined before changing Draft/merge status.


## 2026-11-06 — runnable Component Host process, synthetic Report integration

- Re-centered development priority on SDK product end-to-end:
  Component Manifest/Service → generated user Handler + CMake DLL/SO →
  Host module load → DDS backend and lifecycle, not evidence reports alone.
- Introduced installable \`umaa_component_host\` executable on top of the
  existing ABI v2 Component Loader and shared \`UMAA::Runtime\`.
- Strict \`report_publishers:\` configuration gives explicit Service,
  Topic, Schema, QoS name, instance key and sampling interval. Host opens
  persistent Report Writer, invokes business Handler periodically, propagates
  async errors and shuts down via Dispose/ACK/close, keeping unsafe
  completion statuses visible.
- Generated business Handler defaults remain fail-safe; host currently
  admits only known synthetic Demo Battery binding and fails closed for
  \`UMAA::\` identifiers and guessed wire mappings.
- New CTest executes the real process with a real DLL/SO and mock backend,
  including negative cases. Mock backend is in-process only; official
  per-Service runtime/native QoS and independent vendor proof remain OPEN.
- Check exact-head Linux, Windows and DDS vendor CI before merge.

## 2026-11-06 — generated Component Host configuration CLI

- Added \`host-config\` subcommand on Component CLI, consuming checked
  Manifest roles and actual built DLL/SO and backend plugin file paths.
  Produces complete strict Host YAML, with validated Domain, instance key
  and publish interval, only for the approved synthetic Battery Provider.
- Config is user-owned: cannot be silently overwritten, missing/bogus
  module paths or official Service identities are rejected.
- Added Python negative and regeneration tests.
- Source-backed native UMAA ServiceBinding and complete Host operations
  remain OPEN; verify exact-head SDK and vendor CI before merging.


## 2026-11-06 — generated business DLL/SO to executable Host E2E

- Added CTest `umaa_generated_component_host_v1_e2e` to build the
  **actual Component CLI output** against an installed SDK and launch
  it through the standalone Host process using an SDK test backend.
- The default unimplemented Report Handler must fail nonzero.
  Changing only user-owned `read()` then rebuilding must yield Host
  READY→STOPPED; regeneration must not overwrite the business edit.
- Mock DDS endpoint is not real wire. Official Service TypeSupport,
  effective per-Component QoS, full Command/Collection Host dispatcher
  and release conformance are still OPEN.

## 2026-11-06 — generated Host E2E Windows build-config recovery

- Windows CI was Debug but the new generated Host E2E installed Release,
  causing a missing `Release/umaa_component_host.exe` file. Fixed
  CTest to pass `$<CONFIG>` to the test and install the actual SDK
  build configuration. No runtime fallback or assertion weakening.
- Re-run exact-head Linux/Windows E2E and required vendor CI.


## 2026-11-07 — official BatteryStatus Consumer SDK module generation

- Added an explicit `generate --battery-host-preview` path requiring
  BOTH exact EO ICD v6.0 PDF and original 594-file IDL ZIP. Source
  bytes are checked first by the existing independently pinned verifier.
- User-selected Component Manifest must contain exactly one
  `UMAA::EO::BatteryStatus` Consumer with a current reviewed Catalog
  SHA; non-consumer roles and unreviewed extra Services are blocked.
- Generator writes C++17 source-pinned ServiceBinding (Report and
  Collection Element Topic names/keys, two Operation intentions, minimum
  element ordered TOPIC Presentation), ABI-v2 `Component::consume`
  entrypoint, user-owned `on_report/on_dispose` Handler and CMake DLL/SO
  plus negative ABI smoke CTest.
- Host payload type explicitly remains `BatteryStatusHostPreview`,
  NOT original DDS CDR and NOT standalone Host official auto-binding.
  Full normative QoS/Component roles/native production runtime remain
  OPEN and compiler metadata holds `nativeDdsRuntimeApproved=false`.
- Added unit tamper/role/source/overwrite tests and Linux/Windows
  installed-SDK CMake build/smoke coverage under a **mocked original-byte
  verifier boundary**. Independent original bytes are not bundled in CI.

