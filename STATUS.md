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

## Local V4 update — 2026-10-28 (not pushed)

- `studio/` now supports generating unapproved DOCX model inventories from QEA
  snapshots, a local trusted Word template subset, SHA-256 manifest consistency
  checks, and administrative Verifier exchange linking.
- Existing `sdk/`, `dds/`, `simulator/`, `catalog/`, `tools/`, and `verifier/`
  implementation source files were preserved. Separate V4 evidence includes
  Studio and Verifier regression runs, SDK local CI, a built C++ Mock Host suite,
  DOCX visual render, and scope-report BLOCKED gate.
- Real Sparx QEA acceptance, actual DDS, approved normative requirements,
  PostgreSQL and a multi-user web UI remain undelivered.
