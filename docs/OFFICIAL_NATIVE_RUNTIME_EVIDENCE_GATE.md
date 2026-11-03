# Official UMAA native Runtime evidence gate — continuation checkpoint (2026-11-03)

## Baseline and verified status

- Parent: Draft PR #44, `abedb85b11c22b5c767706fc02c15659d8a1d937`. Its applicable GitHub check runs finished successfully; they prove the scoped *synthetic* DDS Report Runtime only.
- This continuation isolates a pre-generation denial for official Runtime requests. It does **not** enable official services or alter vendor plugins, DDS ABI, authorized source pins, or user-owned business logic.
- Both standalone vendor backends and the SDK/Component architecture remain as in PR #44; mixed-network and RTI adapter work are future-only.

## Required evidence before enabling even one official native service

For an explicitly selected Component and exact service, review and record all of:

1. Original authoritative UMAA 6.0 ICD identity, digest, page/section and operation clauses; Component Definition provider/consumer role and applicability, with reviewer.
2. Official original IDL archive digest and exact source/include closure; native compiler TypeSupport, type identity, and actual DDS Key annotations (a member-existence probe alone is insufficient).
3. Every Report/Command/Status/Collection Topic's exact name, direction, instance identity, mandatory Reliability/History/Deadline/Presentation and any GROUP coherence/order requirements. Record evidence for unsupported or conditional fields, not assumed defaults.
4. Reviewed bidirectional mapping between each official nested/bounded payload field and the SDK host representation, including bounds, discriminated unions, exceptions, unknown values, key handling, Dispose and no-loss round trips.
5. Independently run vendor-specific cross-process positive **and negative** wire tests, exact source SHA, CI run/job, logs and source/reviewer link; verify failure paths such as Key/QoS mismatch, stale data, incomplete Collection, and lifecycle events.
6. Reconcile applicable conformance gates in `docs/DELIVERY_GATES.md`; do not claim official conformance or production/device safety on generated TypeSupport or successful synthetic wire tests.

## Implementation increment in this branch

- `tools/native_component_codegen.py`: when an official YAML Runtime Report is requested without fixture mode, reject it **before** native TypeSupport generation or output, even if a caller claims `officialIdlVerified` and `ddsQosVerified`. This is a deliberate guard until the normative mapping review and a separate official Runtime implementation are delivered.
- `sdk/tests/test_native_icd_component.py`: a regression with fake official identity and self-asserted verification flags must reject and leave the output directory absent.
- Existing synthetic `Demo::EO::BatteryStatus` Report generation continues to be the only approved native Runtime path.

## Acceptance and remaining gates

- Run `python3 -m unittest discover -s sdk/tests -p test_native_icd_component.py -v` with required native toolchains in CI. Local environments without DDS compilers may skip the existing end-to-end vendor tests; a local unit PASS is not an E2E PASS.
- Re-run relevant Linux/Windows SDK and standalone Cyclone/Fast regressions at this branch's exact HEAD, and cite that HEAD and checks before stating they passed.
- **OPEN:** authoritative Key/QoS/Topic and per-Component roles, nested payload codecs, native official lifecycle, official Large Collection and Generalization/Specialization TypeSupport, independent normative tests and production safety.
- The next implementation should start from a fully reviewed, source-anchored *single* official Report contract and extend the existing backend interface, not remove the guard based on guessed metadata.

No PR merge, release, or official conformance claim is authorized by this checkpoint.

## Next increment — source-checked Report candidate handoff (2026-11-03)

`tools/official_report_source_handoff.py` adds a **read-only source-evidence handoff** for one official queryless Report service. This does not replace `tools/official_icd_evidence.py` or permit the official native Runtime path. It reuses that source auditor rather than introducing a parallel IDL parser.

- Requires a separately authored Service YAML in official-reference mode, an external original ICD file, an external official IDL ZIP, and an independently supplied expected archive SHA-256.
- Checks the original ICD **byte hash** against the YAML source pin, and the ZIP bytes against both the YAML and CLI expected pin. It does not parse or semantically approve the original ICD.
- Validates one provider-to-consumer Report Topic, the queryless subscription protocol, explicitly populated DDS Topic and Key fields, and their exact match to the archive's source declarations (including ZIP integrity/import checks).
- Emits a source/provenance-only JSON candidate with the selected IDL member and digest, Key declarations, exact unresolved Operation list and remaining blockers. Original official files and generated TypeSupport are never included in the output.
- Explicitly keeps `readyForOfficialNativeRuntime=false`, `nativeDdsBinding=false`, `componentRuntimeBound=false`, and `officialConformanceClaim=false` even if source declaration checks succeed. The IDL declaration scanner is **not** a full OMG IDL semantic/Key annotation compiler check. QoS or payload codecs cannot be approved by an archive hash alone.

Example with **external, lawfully obtained** input files (do not add originals to git):

```sh
python3 -m tools.official_report_source_handoff \
  --zip /private/official-idl.zip \
  --yaml /private/authored/UMAA-Report.yaml \
  --original-icd /private/original-ICD.pdf \
  --expect-sha256 "$OFFICIAL_IDL_SHA256" \
  --out /tmp/umaa-report-source-candidate.json
```

The candidate is an *inspection artifact*, not a Runtime binding approval. Source mismatches and malformed inputs are fatal before output. Existing generated baseline Service YAML files are intentionally not rewritten and remain unverified.

Regression suite: `python3 -m unittest discover -s sdk/tests -p test_official_icd_evidence.py -v` runs on Ubuntu and Windows under the source-audit workflow. After its new exact-head checks complete, update `PROGRESS.md` with the observed SHA and job links; do not infer this branch's CI from PR #45.
