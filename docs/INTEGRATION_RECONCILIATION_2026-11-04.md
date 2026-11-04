# PR #31 / PR #55 integration reconciliation (2026-11-04)

Status: **REVIEW RECORD, NOT MERGE APPROVAL**. Official native Runtime codegen remains fail-closed. No original AUVSI source is redistributed.

## Exact inspected refs

- `main`: `41073ef8395380228ac5a25eeee87148fbdc5330`
- Draft [#31](https://github.com/rydolabs/umaa-kit/pull/31), `integration/umaa6-idl-qos-components-20261102`: `0240e2a11d7cd1cf75985e971c0ca9337c452283`
- Draft [#55](https://github.com/rydolabs/umaa-kit/pull/55), `integration/official-battery-verified-candidate-20261104`: `707edc09a47c40110ba277a80aef1127540a732f`

GitHub commit comparisons show both candidates are ahead of the same `main` without being merged. The branches have **diverged**; neither should be blindly merged into the other. Against `main`, #31 has 61 commits and 89 changed paths; #55 has 246 commits and 167 changed paths. The changed-path overlap is 69; **20 paths are changed only in #31** and **98 paths only in #55**. Path-set equality does not establish equivalent file content or behavior.

## #31-only implementation paths (5)

| Path | What it contributes | Reconciliation decision |
| --- | --- | --- |
| `sdk/examples/official_battery_charge_user_source.cpp` | Narrow chargeRemaining fixture | Do not import by default. Compare with #55's complete nested Battery host model and SDK Provider. |
| `tests/integration/cyclone/src/official_battery_component_v2_host.cpp` | Separate official Battery Component ABI v2 host test | **Potential distinct coverage**. Inspect its native oracle and module lifecycle before deciding whether to adapt into #55. |
| `tools/generate_official_battery_component.py` | Explicitly chargeRemaining-only source-checked prototype generator | Do not silently enable official generic Runtime. Compare semantics to #55's restricted synthetic native generator. |
| `tools/generate_official_engine_plugin.py` | Additional legacy EngineControl generation changes | Inspect the 62 changed lines against existing EngineControl tests and preservation of original source checks. |
| `tools/generate_official_native_type_registry.py` | Source-pinned Cyclone descriptor registry and local TypeSupport smoke (not a payload codec) | Compare its source/topic evidence and descriptor lookup against #55's native compiler/type probes. Import only missing independently tested capabilities. |

The #31 registry's own manifest deliberately declares `payloadCodecGenerated=false`, `runtimeWireConformanceVerified=false`, and `qosApproved=false`. It cannot justify removing the official Runtime deny gate. Similarly, the narrow Battery prototype is not proof of the complete official service.

## #31-only documentation and historical paths (15)

- `PROJECT.md`, `sdk/README.md`
- `docs/ARCHITECTURE.md`, `docs/ENGINECONTROL_EVIDENCE.md`, `docs/PROJECT_LAYOUT.md`
- `docs/README_IMPLEMENTATION_HISTORY_2026-10-30.md`, `docs/REAL_DDS_EVIDENCE.md`
- `docs/SDK_FACADE_PHASE2.md`, `docs/SDK_IMPLEMENTATION_BACKLOG.md`, `docs/SDK_MODULAR_ARCHITECTURE.md`
- `docs/UMAA6_INTEGRATION_ACCEPTANCE.md`
- `docs/implementation/P1_CYCLONE_ADAPTER_STATUS.md`, `docs/implementation/P2_COMMAND_READER_STATUS.md`, `docs/implementation/P3_SERVICE_CONTEXT_STATUS.md`
- `docs/specification/ICD_CATALOG_AUDIT.md`

These were compared by path, **not** individually adjudicated for authoritative wording. Avoid carrying forward historical statuses that predate #55.

## Verified integration baseline and still-open acceptance

On exact #55 head `707edc09`, GitHub reported 26/26 workflow runs successful (the PR records 51/51 job checks). That validates only the tested candidate tree. It does **not** prove #31-specific behavior or authorize merging. The #55 code includes independent official Battery Cyclone and Fast DDS payload tests, a scoped SDK Provider path, and 3-cell Large List proof. Draft [#53](https://github.com/rydolabs/umaa-kit/pull/53) mixed-vendor tests are intentionally excluded and still fail.

Before any `main` integration:

1. Review the **69 overlapping paths for content differences**, especially native IDL runner, QoS ABI, source evidence and CMake paths.
2. Decide explicitly whether each of the five #31-only implementation paths needs adaptation, a regression test, or archival; never assume identical behavior because files share a feature name.
3. Inspect changed files for confidential/original IDL/PDF/ZIP/generated TypeSupport redistribution and review AUVSI license conditions.
4. Keep the official non-fixture Runtime generation refusal in `tools/native_component_codegen.py` and its negative test intact.
5. Require human code review and fresh `main`-target exact-head Linux/Windows/native DDS/negative CI after any content change.
6. Keep full Component/QoS/Collection/liveliness/official conformance gates open. Mixed Cyclone/Fast interoperability and RTI remain separately deferred.

**Disposition:** #55 remains the preferred **integration staging candidate**, not an approved merge. #31 is a divergent source of targeted review items. No `main` merge, release, device operation or official conformance claim was performed as part of this record.
