# GitHub Actions — verified cross-platform prototype

Workflow: [`.github/workflows/ci.yml`](../.github/workflows/ci.yml)

- Runs on every push, pull request, or manual dispatch.
- Matrix: `ubuntu-24.04` GCC, `ubuntu-24.04` Clang, `windows-2022` MSVC.
- Runs `python tools/ci_verify.py --compiler <default|clang>`, with Python 3.13, CMake and CTest.
- Verifies native service DLL/SO module loader, Command/Report/Collection unit behavior, subset-IDL generator, CMake install and external consumer, and **separate independent Simulator** subprocess scenarios.
- Uploads per-run JSON and detailed logs in Actions artifacts.

**Latest proof:** https://github.com/nssang00/umaa-dev/actions/runs/39962717654 — all three jobs SUCCESS, CTest 4/4 and Simulator 4 scenarios in each environment.

**This baseline workflow alone** is synthetic and does not cover DDS. Separate native DDS workflows now test official EngineControl and BatteryStatus; Cyclone↔RTI interop, full ICD compliance, K-MOSA certification and safe active-command hot swap remain unverified.

These gaps are documented in `docs/DELIVERY_GATES.md`. **Do not mark blocked acceptance gates as pass.**

## True DDS integration jobs

- [`.github/workflows/real-dds.yml`](../.github/workflows/real-dds.yml): Ubuntu native Cyclone DDS C TypeSupport with synthetic IDL. [Passing source run](https://github.com/nssang00/umaa-dev/actions/runs/39950750899).
- [`.github/workflows/official-idl-check.yml`](../.github/workflows/official-idl-check.yml): ephemeral AUVSI official 6.0 IDL archive, recursive TypeSupport generation, Engine Command/Status type compilation, **official BatteryReportType Linux wire CTest**. [Passing run](https://github.com/nssang00/umaa-dev/actions/runs/39952552116).
- [`.github/workflows/real-dds-windows.yml`](../.github/workflows/real-dds-windows.yml): vcpkg Cyclone DDS 0.10.5 + MSVC, both synthetic and official UMAA BatteryReportType DDS CTest with native DLL module. [Passing run](https://github.com/nssang00/umaa-dev/actions/runs/39953508112).

Both projects are distinct: the Simulator connects via DDS from an independent process and never links SDK Command state machine code. Official IDL is downloaded into CI runner temp, and must never be redistributed with the prototype. See [REAL_DDS_EVIDENCE.md](REAL_DDS_EVIDENCE.md) for limitations.

## Milestone 2 official live DDS and complete IDL coverage

- [Linux official-IDL integration](https://github.com/nssang00/umaa-dev/actions/runs/39962602416): BatteryStatus and EngineControl Command/Update CTest **3/3**, generated ignition-only Handler module loaded as native SO.
- [Windows official-IDL integration](https://github.com/nssang00/umaa-dev/actions/runs/39962717265): same official UMAA DDS **3/3** on MSVC, generated DLL.
- [Full official IDL TypeSupport coverage](https://github.com/nssang00/umaa-dev/actions/runs/39963083783): **352/352** official idlc generation and **352/352** generated C compilation on Ubuntu; this does not establish all service APIs or compliance.

See [ENGINECONTROL_EVIDENCE.md](ENGINECONTROL_EVIDENCE.md) for correct scope limits and links.
## Multi-node DDS verification with Linux network namespaces (2026-10-24)

- `.github/workflows/dds-netns.yml` runs on **GitHub Actions ubuntu-24.04**. It builds synthetic and official UMAA BatteryStatus/EngineControl DDS binaries, then runs ordinary CTest regression and namespace integration.
- `tools/dds_netns.sh` creates 3 **different network namespaces** using veth + bridge; IPv4 addresses are 10.233.77.11, .12, and .13. `simulator/dds/run_netns.py` checks distinct network namespace IDs and interface addresses, pins CycloneDDS to eth0 and uses explicit unicast discovery peers (no localhost fallback).
- Report checks: **1 publisher + 2 subscribers** in three namespaces; both readers must be matched before the publisher emits updates and dispose. EngineControl checks: **1 host + 1 independent simulator** across two namespaces for Command and Update/Cancel flows.
- Every namespace test failure fails the runner job; `if: always()` tears down the namespaces and uploads log evidence without redistributing the official IDL ZIP.
- `ci.yml` still runs GCC/Clang on ubuntu-24.04 and MSVC on **windows-2022**. `real-dds-windows.yml` runs native DDS/official IDL tests on Windows and now also triggers on pull requests. Linux network namespaces are not a Windows facility, so Windows checks use separate native DDS processes instead.

This test expands network isolation and multi-node fan-out coverage, but does **not** establish full UMAA 6.0 service, QoS, RTI interoperability, or conformance certification.

## Deterministic tc netem impairment gates (2026-10-24)

Linux only: the `dds-netns.yml` job now applies `tc qdisc replace dev eth0 root netem` **inside each participating network namespace**. This is bidirectional at the DDS flow level because every endpoint's egress is impaired. It does not emulate physically separate machines, and host hardware scheduling/CPU remain shared.

- **Degraded path:** 40 ms base latency with 10 ms jitter and 1% random egress packet loss; complete official BatteryStatus report (one publisher + two independent subscribers) and EngineControl Command/Status across independent namespaces. The test asserts the qdisc exists before running DDS; the configured loss rate is probabilistic, not proof that a packet was dropped in any single run.
- **Discovery blackout/recovery:** 100% egress packet loss at every participating endpoint, start independent DDS processes while traffic is blocked, verify Linux `tc -s qdisc` observed nonzero dropped packets, then remove the qdiscs **while the same processes remain alive**. Both official BatteryStatus and EngineControl flows must complete after restoration. This proves recovery of initial DDS discovery after a finite blackout, **not** live active-command resynchronization across a mid-session partition.
- Strict failures: missing `tc` or insufficient root/network capabilities, no actual packet drops during the blackout, participant exit before recovery, unsuccessful Report/Command validation, or failure to restore a clean qdisc fails the corresponding job. Runner evidence is uploaded as `linux-dds-namespace-integration` even after failure. Script cleanup and topology teardown remain explicit.
- Windows retains the native MSVC/Cyclone DDS testing path; `tc netem` and Linux network namespaces are not expected on the Windows runner.

Use the exact GitHub Actions run URL for each passing commit as acceptance evidence. `tc netem` configuration and successful selected smoke tests must not be mistaken for complete UMAA 6.0 QoS or service conformity.
