# Project status — 2026-10-23

**Milestone: entire existing SDK and independent Simulator prototype integrated to GitHub and passing native Windows/Linux CI — PASS.**

Evidence: https://github.com/nssang00/umaa-dev/actions/runs/39941190249

- GitHub Ubuntu/GCC: PASS, CTest 4/4, Python generator/Simulator tests 5+6, four subprocess scenarios
- GitHub Ubuntu/Clang: PASS, same suite
- GitHub Windows/MSVC: PASS, same suite
- SDK native module tests: two independent native modules; ABI/service/schema checks; reload
- SDK installation: external CMake consumer import compiled and executed
- Independent Simulator: 4 scenarios, 32 observed events total

The CI milestone is a **synthetic-interface prototype milestone only**. Actual official UMAA IDL compilation, DDS/RTI/Cyclone transport, full lifecycle correctness and independent on-wire conformance remain open (see docs/DELIVERY_GATES.md). K-MOSA compliance is not asserted.

The `smoke/` subproject is preserved as the original minimal native ABI regression fixture. The full suite now runs from `sdk/` and `simulator/`.
