# UMAA Battery SDK integrated candidate — review and merge gates

**Status: DRAFT INTEGRATION CANDIDATE. NOT MERGED, NOT CERTIFIED, OFFICIAL RUNTIME FAIL-CLOSED.**

## Provenance and contents

This integration candidate branches from validated Draft #54
(sha `218b758ef16449da9237d849bbedb85f8055da94`), which
includes the older Draft #51 bounded Large List joiner, #49 original Cyclone
nested Battery TypeSupport, and their parent chain back to #32.

To this branch we added the **exact approved code contents** from:

- Draft #50 / `bc40c510827f4c6e9295e11cc3a8953ee83d706e`:
  seven added files for real Fast DDS 3 original Battery nested TypeSupport,
  strict native optional-presence ABI, independent oracle and source-pinned
  original wire CI (**23/23 checks previously green on its separate HEAD**).
- Draft #52 / `ba001f1c75a3cc11f024f506b5e1fcf0353292a7`:
  dedicated SDK Handler/BoundReportProvider original Cyclone DDS writer,
  independent C oracle job, and the two scoped CMake test cases
  (**13/13 checks previously green on its separate HEAD**).

The shared original Cyclone CMake file was combined without dropping
existing source-pin, single-Cell, live-joiner, or 3-Cell targets.
The inherited `official-battery-native-wire.yml` was found malformed
(missing closing CTest quote and duplicated YAML lines). This candidate
**repairs** it and executes **eight** actual source-generated Cyclone CTests:
two original payload, two live joiner, two 3-Cell, and two SDK Provider.
The Fast DDS original wire job remains independent, plus the Linux/Windows
host/synthetic regressions.

## Branch/merge hazards

1. The stack is not based directly on main. Review #32, then the ordered
   chain #33 → #34 → #35 → #36 → #37 → #38 → #39 → #40 → #41 →
   #42 → #43 → #44 → #45 → #46 → #47 → #48 → #49 before integrating this
   branch. Earlier Draft #31 is a **divergent main-rooted proposal**, not an
   ancestor. Explicitly reconcile overlapping files; do not merge #31
   into this branch blindly.
2. The child PRs #50, #51, #52 and #54 are partly parallel. This candidate
   is a **non-merge artifact** that runs a main-base integration CI; do not
   independently squash every source branch into main afterward or
   duplicate its changes.
3. The test success on separate heads does not prove this combined tree.
   Require exact-head main-target integration CI, source evidence checks,
   native wire jobs, Linux/Windows, and a reviewed changed-file/security
   diff before any merge.
4. The official native Runtime generator's fail-closed conditions MUST stay
   in force. Generic official Battery Component definition/production QoS,
   complete Large List recovery/deletion, full vendor parity/conformance,
   and a real provider deployment are **not** claimed.
5. Draft #53 **not included**: cross-vendor Cyclone ↔ Fast DDS still
   has incomplete E2E and must pass its four independent positive/negative
   cases before a separately reviewed integration. Prototype XCDR2
   configuration from #53 is not a default shipping QoS policy.

## Acceptance

- Validate this Draft PR is mergeable and all **current-head** jobs pass.
- Obtain reviewer approval; validate source TOU and no raw original IDL/PDF
  are committed.
- Confirm production official Runtime is explicitly blocked, not
  activated by this merge.
- Merge only a reviewed, tested integration plan into main; don't
  mark this candidate or its original branch Drafts as merged prematurely.
