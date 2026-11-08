# SDK MVP integration plan — 2026-11-08

This is an implementation and review queue, **not** a release declaration or
UMAA conformance approval. Primary source of truth remains `PROJECT.md`,
`docs/CURRENT_PHASE_SCOPE.md` and `docs/DELIVERY_GATES.md`.

## One user-visible MVP acceptance path

1. Generate a Component project from an explicitly reviewed Service contract.
2. Write *only* business Handler(s), build a DLL/SO without DDS vendor headers.
3. A **single generic Host** admits the generated binding, checks Service
   identity, Provider/Consumer role, original Topic/Type/Key/QoS provenance,
   and creates the correct reviewed native DDS endpoints through the shared
   Runtime/Backend.
4. Run actual Command or Report on real standalone Cyclone DDS, then the
   same unmodified Component/client against standalone Fast DDS wherever that
   official TypeSupport/QoS mapping has been independently verified.
5. Verify external-process native wire behavior and negative
   TypeBinding/QoS/identity/Dispose conditions on Linux and Windows.
6. A previously approved ServiceBinding is reused by the next Service
   without a second service-specific Host implementation.

**This acceptance path is NOT yet complete.** Existing Component Host is
synthetic Report-only; the original-source Battery Consumer Host is a
special-case executable. The reviewed literal Topic catalog is not a
source-approved complete ServiceBinding registry.

## Work queue and boundaries

| Priority | Concrete deliverable | Observable acceptance |
|---|---|---|
| P0/A | Review and integrate passing shared DDS Context changes | PR #77 then stacked #78, exact-head CI and regression first; maintain one Backend/Domain and QoS policy owner |
| P0/B | Service-role dispatch within one Runtime (this stacked branch) | Two **independent DLL/SO modules** may provide/consume the same Service with matching schema and disjoint ABI actions; duplicate Provider/role, module ID or schema fail closed; safe unload removes only that module's role |
| P0/C | Explicit source-approved ServiceBinding admission and unified Host | No guessed DDS Topic/Type/Key/QoS; binding validation happens before native endpoints; one executable runs two selected real original-source Service contracts without service-specific host branches |
| P0/D | Original-source Battery/Command integration | PR #76, existing official Battery Host / EngineControl vertical tests reconcile into C and no duplicate host-specific runtime |
| P1 | Report queue/bounds and recovery, Role+Instance keys | Fail-closed bounded delivery and provider instance selection; no last-event coalescing for multi-event callbacks |
| P1 | Package the one recommended Application/CLI | Build/install/test external SDK consumers and user-created Component project |
| Future | RTI, mixed-vendor networking, full normative conformance | Separate specifically approved scope; not current-phase gates |

### PR triage without destroying unreviewed work

As checked on 2026-11-08, `main` is `f7334283320d` and
**33 open PRs are all Draft**. Do not equate CI success with merge approval.

- **Current integration spine:** #77 (Command/Report shared Context) →
  #78 (Collection/Split Context). Both passed exact-head CI; this role-dispatch
  patch is stacked on #78. Rebase/retest and integrate each dependency in order.
- **Independent original-source proof:** #76 (generated Battery Consumer
  DLL → original DDS), currently tested, needs separate review and
  compatibility reconciliation with the generic Host.
- **Other active evidence candidate:** #67 (negative netns Dispose stress),
  preserve strict failure evidence; review independently when relevant.
- **Earlier prototype/component/codegen stacks (#23–#54, with gaps):**
  retain **without mass merging or deleting**. Many point to other unmerged
  branch heads and may overlap already integrated code. Classify file diffs
  against current `main`, cherry-pick only missing source-gated contract or
  tested runtime capability, and close/mark superseded **only after explicit
  human approval**.
- No forced closure of Drafts, no weakening tests, no unreviewed production
  schema/QoS approvals, no RTI or mixed-vendor PASS claims.

## This incremental runtime change

**Files:** `sdk/src/component_runtime.cpp` (private service-role index and
action router), `sdk/tests/modules/battery_roles_v2.cpp` (separate consumer,
duplicate-provider and wrong-schema fixtures), `sdk/tests/test_runtime_multirole.cpp`,
`sdk/cmake/module_v2_tests.cmake`.

**Dependencies:** Component ABI v2, existing `ComponentModule`, shared
`DdsEndpointContext` from #77/#78. No new ABI, plugin, transport or public
registry layer. Existing Command safety/session handling and Report lifecycle
are reused unchanged.

**Acceptance:** loading matching independent Report Provider+Consumer modules
keeps a single logical Service entry; dispatch selects module by action,
rejects duplicate roles and mismatched schema without disturbing live routes,
supports Consumer-only remote Report subscription, and independently unloads
modules without unloading a retained active Provider publisher. The existing
Linux GCC/Clang, Windows MSVC and Cyclone/Fast standalone real-wire checks
must remain green. Demo-only test modules never imply original normative
ServiceBinding readiness.

**Next dependency, not claimed completed:** generated executable
`ServiceBinding` records must contain source-reviewed role, literal Topic,
TypeSupport binding, keyed identity and mandatory QoS requirements. The
generic Host must consume those records instead of inventing mappings.
