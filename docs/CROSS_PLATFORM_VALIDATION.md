# Cross-platform full prototype validation — 2026-10-23

**Authoritative run:** https://github.com/nssang00/umaa-dev/actions/runs/39962717654

| Job | GitHub-hosted environment | Result | CTest | Scenario count |
|---|---|---|---|---|
| 120444512642 | Ubuntu 24.04 / GCC | PASS | 4/4 | 4 |
| 120444511728 | Ubuntu 24.04 / Clang | PASS | 4/4 | 4 |
| 120444512122 | Windows Server 2022 / MSVC | PASS | 4/4 | 4 |

**Evidence from GitHub job logs:** `CI PASS` is emitted by `tools/ci_verify.py` after all steps. Each job logged:
- Python generators (synthetic + fail-closed official EngineControl subset) unit tests: 9 passing
- Python independent trace verifier tests: 6 passing
- C++ CTest: `umaa_command_engine_test`, `umaa_report_collection_test`, `umaa_dynamic_module_test`, `umaa_generated_report_smoke` — 4/4
- SDK CMake installation and external consumer compilation/execution
- Independent subprocess Simulator scenarios: invalid payload (5 events), normal (7), recovery/replay (9), update/cancel (11) — **32 events**

The sample messages are synthetic and exchange **stdio**, not DDS. This run does not establish official UMAA 6.0 compliance, K-MOSA certification, or DDS vendor interoperability.

**To reproduce:** `python tools/ci_verify.py --compiler default` (use `--compiler clang` on Ubuntu/Clang).

## Official UMAA 6.0 live DDS proof (separate from synthetic stdio tests)

Linux [official BatteryStatus](https://github.com/nssang00/umaa-dev/actions/runs/39952219497), Windows [official BatteryStatus](https://github.com/nssang00/umaa-dev/actions/runs/39953508112), and Linux [Engine Command/Status TypeSupport generation/compilation](https://github.com/nssang00/umaa-dev/actions/runs/39952552116) have separately passed. Details, precise scope and exclusions are in [REAL_DDS_EVIDENCE.md](REAL_DDS_EVIDENCE.md). The stdio-test CTest matrix above must not be conflated with these real DDS tests.

**Milestone 2 official DDS evidence:** Linux 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962602416) and Windows 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962717265); all 352 official IDL files generated and C-compiled on Ubuntu [run](https://github.com/nssang00/umaa-dev/actions/runs/39963083783). These are distinct from baseline synthetic stdio tests.
