# UMAA Kit — C++ SDK and Independent Simulator

**Canonical repository:** https://github.com/rydolabs/umaa-kit  
**Status:** experimental UMAA 6.0 implementation subset; **not** official
UMAA/K-MOSA conformance or a complete 53-service runtime. Prior
`nssang00/umaa-dev` links below are historical evidence, not the canonical
repository. See [release gates](docs/DELIVERY_GATES.md), [the SDK quick start](docs/QUICKSTART.md)
and the [open implementation roadmap](https://github.com/rydolabs/umaa-kit/issues/2).


Two **separate projects** for development and independent verification of UMAA services:

- [sdk/](sdk/): strict synthetic-subset generator and **official-EngineControl-derived ignition Handler generator**, Command/Report/Collection prototype, versioned native service module loader (DLL/SO), installable CMake package and tests.
- [simulator/](simulator/): independent scenario runner and protocol trace verifier. Baseline synthetic fixture exchanges JSON over stdio. Additional native DDS simulator programs in `simulator/dds/` independently receive samples and Dispose via **real Cyclone DDS**.

## Where things live

The stable implementation directories are `sdk/` (vendor-neutral core),
`dds/cyclone/` (real DDS transport), `simulator/` (independent verification),
`catalog/` (reviewed contracts), and `tools/` (build and audit utilities).
Start at the [documentation index](docs/README.md) and the
[complete repository map](docs/PROJECT_LAYOUT.md) for file ownership,
verification evidence, and generated-data rules.

## Cross-platform GitHub CI — executed

| GitHub-hosted runner | Result |
| --- | --- |
| Ubuntu 24.04 / GCC | PASS |
| Ubuntu 24.04 / Clang | PASS |
| Windows Server 2022 / MSVC | PASS |

**[Full SDK + Simulator Actions run](https://github.com/nssang00/umaa-dev/actions/runs/39962717654)**: all three matrix jobs passed, verified from GitHub-hosted job logs. Each job performed 9 Python codegen unit tests, 6 independent simulator oracle unit tests, a CMake build, CTest **4/4**, CMake install and external consumer build, and four cross-process simulator scenarios (**32 observed events total**). A separate earlier smoke workflow also verified native DLL/SO modules on the same three environments.

## Local reproduction

Requires Python 3.10+, CMake 3.20+, a C++17 compiler:

```sh
python tools/ci_verify.py --compiler default
# Linux/Clang only:
python tools/ci_verify.py --compiler clang
```

The verifier writes command logs and JSON summaries under `evidence/ci/<platform>/<compiler>/`, while the GitHub workflow uploads job artifacts.

## CycloneDDS real-wire proof (official and synthetic)

- [Linux official AUVSI UMAA 6.0 BatteryReportType + SO module → Cyclone DDS → separate Simulator test](https://github.com/nssang00/umaa-dev/actions/runs/39952219497) — PASS.
- [Windows vcpkg Cyclone DDS 0.10.5 + DLL plugin + synthetic test IDL → separate Simulator](https://github.com/nssang00/umaa-dev/actions/runs/39952395220) — PASS.
- [Windows official UMAA 6.0 BatteryReportType + DLL module + live DDS + separate Simulator](https://github.com/nssang00/umaa-dev/actions/runs/39953508112) — PASS.
- **[Official EngineControl Command/Ack/Status, Cancel/Dispose, later-Timestamp UPDATE](https://github.com/nssang00/umaa-dev/blob/main/docs/ENGINECONTROL_EVIDENCE.md)** — Linux CTest 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962602416); Windows CTest 3/3 [run](https://github.com/nssang00/umaa-dev/actions/runs/39962717265).
- **[All 352 original AUVSI 6.0 IDLs idlc-generated and C TypeSupport compiled](https://github.com/nssang00/umaa-dev/actions/runs/39963083783)** — 352/352; this is NOT 352 generated service APIs or compliance certification.
- [Official AUVSI IDL archive verification and Engine Command/Status TypeSupport compilation](https://github.com/nssang00/umaa-dev/actions/runs/39952552116) — PASS.

The official IDL ZIP is downloaded to ephemeral CI space for each applicable run; it is **not committed or redistributed**. UMAA IDL TOU must be reviewed for any downstream licensing/distribution.

**Conformance boundaries:** the BatteryStatus adapter currently uses manual C TypeSupport glue and a shortened DDS topic string for testing, with some metadata fields unset. It is a valid on-wire **type and lifecycle proof**, *not an UMAA ICD service conformance verdict*. LargeCollection QoS, startup/recovery, timestamps, service addressing, official Topic naming, CycloneDDS-CXX parity, and EngineControl **all-field/full-conformance** coverage remain open.

## Scope / limits

This repository contains a **technology prototype**, not a complete UMAA 6.0 implementation or official K-MOSA/UMAA certification. `sdk/fixtures/demo.idl` is synthetic, the generator implements a deliberately restricted IDL subset, and the independent simulator uses a test-fixture stdio protocol. **Real Cyclone DDS tests are now available:** Linux and Windows synthetic-IDL module-backed cross-process samples; **both Linux and Windows** official AUVSI UMAA 6.0 `BatteryReportType` compiled from downloaded IDL and verified over DDS with an independent simulator (20→80→Dispose). Official `EngineCommandType`, `EngineCommandStatusType` and `EngineCommandAckReportType` are now used in live real DDS integration tests for command, cancellation and update on **both** operating systems. The generated user module API is limited to the ignition-state field. RTI Connext DDS and mixed-vendor tests have not been executed. The generic IDL generator is still restricted to synthetic types. A separate fail-closed official `EngineControl` subset generator now creates a DDS-free user Handler and C ABI plugin glue. The native module ABI is project-defined, not an official K-MOSA ABI, and does not implement live hot swap of active commands.

Read [delivery gates](docs/DELIVERY_GATES.md), [architecture](docs/ARCHITECTURE.md), [real DDS evidence](docs/REAL_DDS_EVIDENCE.md) and [validation evidence](docs/CROSS_PLATFORM_VALIDATION.md) before treating any subset as production-ready.


## 2026-10-26 P1 Cyclone writer checkpoint (local only)

A reusable CycloneDDS C CommandStatus/Ack writer adapter and an EngineControl
example-host integration are now in the tree. CMake can optionally export the
`UMAA::Cyclone` target (`-DUMAA_ENABLE_CYCLONE_ADAPTER=ON`) when CycloneDDS
is installed. Adapter behavior was checked
using a test-only DDS ABI mock. **No official DDS or Windows CI run has
validated these new changes yet.** For precise evidence, scope, and remaining
acceptance criteria see `docs/implementation/P1_CYCLONE_ADAPTER_STATUS.md`.

## Latest structure baseline

The repository layout is documented at [docs/PROJECT_LAYOUT.md](docs/PROJECT_LAYOUT.md).
The independently verified P1 Reader/Writer checkpoint is [PR #3](https://github.com/nssang00/umaa-dev/pull/3)
(commit `cb9a87f`); older sections above remain as dated historical notes.
Structure maintenance does not expand the current UMAA service-conformance scope.

## P2 Cyclone Command input Reader (2026-10-27)

`dds/cyclone/include/umaa/cyclone_sample_reader.hpp` provides a reusable
RAII sample Reader shared by Command and Report; the official EngineControl
host consumes its Command input through `CommandReader`. A rejected command,
DDS failure or exception faults the epoch and releases sample-owned data.
Refer to [implementation notes](docs/implementation/P2_COMMAND_READER_STATUS.md).
This remains an EngineControl ignition-only and BatteryStatus Report subset,
not complete UMAA service coverage.
