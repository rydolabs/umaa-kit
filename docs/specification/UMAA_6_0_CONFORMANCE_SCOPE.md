# UMAA 6.0 official requirements and component applicability — controlled trace

> **G6/G11 OPEN — NOT A CONFORMANCE CERTIFICATE.** Review date 2026-10-27.
> Machine-readable preflight: [`catalog/umaa6-conformance-scope.json`](../../catalog/umaa6-conformance-scope.json).
> Canonical baseline: `be4a1d6e29e21a4da459d58845f491b99c31e0db`.

## The three different scopes

1. **SDK feature surface:** `umaa-kit` is a reusable SDK and independent simulator, not a declared UMAA government autonomy Component. Its 53 previously curated six-ICD service headings are *candidate API inventory*, **not** an exhaustive official 6.0 inventory or a component compliance certificate.
2. **Government Component definition:** public Component Definitions v1.0 lists published component names, functions and provided/consumed services. Its section 1.3 explicitly says the generated Component Definitions document **is not a requirements document**. No Component has been selected for acceptance and no entire service membership list has been reviewed against its authoritative pages.
3. **Applicable compliance:** only authoritative UMAA Compliance Specification Rev 1, the selected acquisition Component/Platform Specification and applicable 6.0 ICDs can establish normative requirements, exceptions and applicability. Full authenticated clause extraction and signed scope acceptance are still required.

## Authority ledger and source status

| Authority | Source link | Status in this repository |
|---|---|---|
| AUVSI Compliance Specification T0300-BE-IDS-010 Rev 1 | [official PDF](https://www.auvsi.org/wp-content/uploads/2025/03/T0300-BE-IDS-010-Rev.-1.pdf) | **IN PROGRESS**: original 24-page PDF fetched and SHA-pinned on CI, line candidates found, but exact requirement clauses and applicability not reviewed |
| AUVSI Component Definitions v1.0 | [official PDF](https://www.auvsi.org/wp-content/uploads/2025/03/Distro_A_UMAA_Component_Definitions_v1.0.pdf) | **PARTIAL**: original 12-page PDF fetched and SHA-pinned on CI; component names identified, provided/consumed service rows still require verified extraction |
| UMAA 6.0 ICDs EO / MO / MM / SA / SEM / SO | [official publication table](https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/) | **PARTIAL**: 53 literal service headings from secondary public Markdown, 114 candidate Topic rows in current JSON; original normative clauses and DDS roles/QoS not fully reconciled |
| UMAA 6.0 CO ICD | [official PDF](https://www.auvsi.org/wp-content/uploads/2025/03/UMAA-SPEC-CO-EXP-ICD.pdf) | **PARTIAL**: original 76-page PDF fetched and SHA-pinned on CI; service clauses not yet ingested into the six-ICD Catalog |
| UMAA 6.0 EXP ICD | [official PDF](https://www.auvsi.org/wp-content/uploads/2025/03/UMAA-SPEC-EXP-ICD.pdf) | Experimental; exclude from baseline compliance, keep conditional contract overrides separate |
| Official UMAA IDL | [official publication table](https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/) | 352 TypeSupports generated/compiled in CI; this verifies type compilation, **not** normative service behavior or compliance |

Sources are linked, not copied into the repository. The public official source PDFs were not fully accessible from this review environment. Original source clauses must not be invented from secondary text. Download hashes/page ranges should be captured in independent audit artifacts when available.

## Candidate component set: **names only**, not a completed service matrix

1. Autopilot
2. COLREGS And Hazard Avoidance
3. Logging
4. Mission Executor
5. USV Navigation
6. UUV Hazard Avoidance
7. UUV Navigation
8. Water Environment
9. Weather

This is the nine-entry **public v1.0 table of contents**, not a claim that only nine Component types exist in the government model. Do not assign any provider/consumer services from guesses.

## Existing Catalog audit discrepancy

- Existing six-ICD audit documentation says **53 service sections and 113 Topic types**.
- The six-ICD Catalog has **113 typed DDS Topic contract objects** plus **one BatteryStatus Collection element reference string** (`BatteryReportTypeCellsListElement`): 114 total entries in its three arrays, but **not** 114 DDS Topics.
- Initial `TOPIC_COUNT_001` count discrepancy is **resolved as a classification error**. Keep the reconciliation evidence and ensure CI rejects another silent change. The 113 Topic records still require authoritative ICD verification; the count reconciliation itself is **not** G6 compliance.
- CO services are absent. EXP services are separately excluded by default. The existing 53 service candidates cannot be labeled all UMAA 6.0 requirements.

## Requirement-level trace data contract

The `normative_requirements` array is **empty on purpose** until clauses can be read and verified. Every future binding must include:

`requirement_id, authoritative_document, document_version, clause, source_sha256, exact_scope, component, applicability, applicability_justification, implementation_reference, independent_test, evidence_commit, evidence_run_url, verdict, reviewer`.

- `applicable`: an exact clause applies to the selected Component/SDK claim; proof required.
- `conditional`: applicability depends on a documented operating requirement and evaluated condition.
- `not_applicable`: reviewer-approved rationale and authoritative citation required; never a blank or automatic exclusion.
- `undetermined`: default, blocks final compliance.
- An implementation CTest passing does **not** change a normative result to PASS without the independent test-to-clause link.

## Prioritized manual/automated closure queue

1. Obtain and SHA-pin the full official Compliance Specification Rev 1; extract every numbered normative requirement and applicability condition, including tables/notes. Reviewer cross-check must reconcile number of clauses extracted to the source. This is **not done**.
2. Download and SHA-pin full Component Definitions v1.0; for each published public Component, extract **each** provider/consumer service row and service document reference. Cross-map to the Compliance Specification and actual intended government Component/Platform Specification. This is **not done**.
3. Review authoritative EO/MO/MM/SA/SEM/SO **and CO** ICD text, including Section 3 DDS behavior and Section 6 operation-level contracts. Confirm the 53 six-ICD services / 113 literal Topic contracts **plus separate Collection-element metadata** and any scope additions. EXP optional-only.
4. Select the exact Component/Platform compliance profile to be claimed. A reusable SDK by itself has no automatically implied single UMAA Component role.
5. For every applicable requirement, link exact service/Topic/IDL type, required DDS key and runtime policy, implementation path, independent Simulator oracle, evidence run, and reviewer result.
6. Re-run source/IDL/type/API checks and full service-specific Linux/Windows DDS tests. Verify QoS, Presentation, Liveliness, concurrency, recovery, vendor scope and applicable security/safety obligations.
7. An external approval claim needs the contractually authorized acceptance process; do not equate GitHub CI success with an official certificate.

## Fail-closed verification

`python tools/verify_compliance_scope.py` checks that the candidates have not drifted from the actual Catalog and do not contain invented conformance claims. `--strict` deliberately returns exit **3** while authoritative clause extraction, component choice or applicability are missing. Normal validation PASS is **data integrity only**, never official compliance. Open umbrella backlog: [issue #2](https://github.com/rydolabs/umaa-kit/issues/2).

## Public original PDF provenance — independently fetched on GitHub runner

| Document | Pages | SHA-256 | Clause/service acceptance |
|---|---:|---|---|
| COMPLIANCE_REV1 | 24 | `6570067f3d2f03320afaeb721fe03344cca333d81ede17922d7756dda82661a9` | **NOT REVIEWED** |
| COMPONENT_DEFINITIONS_1_0 | 12 | `872fd164b9a07b963d0d02d4ae026b2c4e34621654677aa2dbceec2a6ead0e75` | **NOT REVIEWED** |
| ICD_CO_6_0 | 76 | `d019f406290b0d9ad2d2eb98bbd6ab76437b0d391b15c40f973abe18dd596ed6` | **NOT REVIEWED** |

Evidence: [source provenance workflow](https://github.com/rydolabs/umaa-kit/actions/runs/40473732828).
For Compliance Rev 1, the PDF text pass located **31 lines containing "shall"**, including administrative front matter. **26 post-front-matter line candidates** are stored in `normative_clause_candidates` with stable extracted-text line locators. These are not parsed or reviewed requirements; one requirement can span several lines and some lines are only context. Do not use them as a requirement count.

The machine-readable `normative_requirements` remains empty until a reviewer reads each complete clause, checks conditions/exceptions and attaches authoritative page and selected Component applicability. CO and Component service assignments remain unverified. Correct interpretation is **source acquisition PASS, clause-level G6/G11 BLOCKED**.
