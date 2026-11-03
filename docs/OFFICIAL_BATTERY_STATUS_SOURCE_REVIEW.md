# UMAA 6.0 EO BatteryStatus — source-pinned evidence review

**Status: source-checked candidate; NOT UMAA Runtime/complete QoS/conformance approval.**

This continuation uses original public Distribution A UMAA 6.0 EO ICD and the
**with-experimental IDL ZIP (594 .idl files, 139 Markdown files)**. This is
different from the older 352-IDL source bundle used in prior CI milestones.

## Exact original input pins (no raw originals redistributed)

| Input | SHA-256 |
|---|---|
| UMAA-SPEC-EOICD-1(3).pdf (83 pages) | 47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453 |
| umaa-6.0-idl-with-exp-services(2).zip | 15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08 |
| ZIP member UMAA/EO/BatteryStatus/BatteryReportType.idl | 39a2c920cfaba9afc3c15c4c12f5d4470911ab8b7c197c9e27b08284e54b713c |
| RTI example umaa_qos_lib.xml | 84d0b7d84dda6ffd8c4a9ece18d11df8ff865a67b6471a98391ec032485f628b |

The RTI XML explicitly permits use of its software **solely with RTI Connext DDS**
and calls its QoS settings examples, not official required UMAA profiles.
Do not copy this file into Cyclone or Fast DDS, or imply it establishes normative QoS.

## Source-anchored facts (original PDF locators)

- EO ICD §6.1.5, printed PDF page 54, Table 18: queryBattery input,
  reportBattery output.
- EO ICD §6.1.5.1, printed PDF page 55, Table 19: the Report uses
  UMAA::EO::BatteryStatus::BatteryReportType, with a separate
  BatteryReportTypeCellsListElement Large List element type/topic.
- The official IDL Report declares Key member **source**, type
  UMAA::Common::IdentifierType. The element declares two Keys,
  **listID** and **elementID**, each type
  UMAA::Common::Measurement::NumericGUID. They are separate DDS Topics.
- EO ICD §3.8.1, printed PDF page 14: publishers and subscribers of
  Large Collection **element topics** use PRESENTATION Topic scope with
  ordered_access enabled. Metadata and element messages use distinct DDS
  Topics: DDS does NOT guarantee ordering across them.
- This minimum is already scoped in contracts/qos/umaa6/eo-large-collections.yaml;
  it does not define complete Report, Element or Component QoS.

The printed pages were independently inspected against the original PDF,
but the automated script only authenticates the **PDF bytes by digest**.
It does not machine-parse normative PDF clauses or certify all semantics.

## Reproducible source checks

The executable tools/official_battery_status_evidence.py:

1. Authenticates external original EO ICD and official ZIP against pinned SHA-256.
2. Uses the existing safe IDL source auditor to check ZIP imports, exact
   Report/Element Topics, member hashes and declared Key names/types.
3. Cross-checks the narrow element QoS minimum in the existing reviewed
   repository ledger without broadening it to the Report.
4. Optionally authenticates the RTI example XML and checks its
   participant-level Topic/ordered Presentation illustration, explicitly
   marked RTI-only and non-normative.
5. Returns an evidence-only JSON with all runtime/conformance readiness FALSE.

Command example (provide the original files outside Git):

    python3 -m tools.official_battery_status_evidence \
      --idl-zip "/private/umaa-6.0-idl-with-exp-services(2).zip" \
      --original-icd "/private/UMAA-SPEC-EOICD-1(3).pdf" \
      --rti-reference-xml "/private/umaa_qos_lib.xml" \
      --out "/tmp/battery-source-only.json"

GitHub CI on Linux and Windows runs SYNTHETIC positive and tamper-negative
tests; it does not include official raw source due to source/TOU constraints.
Fail-closed official Runtime generation from PR #45 remains unchanged.

## Still open

- Reviewed original Component Definition/Specification role for an actual
  BatteryStatus Provider. The uploaded Autopilot, Mission Executor and
  USV Navigation specs are references, not a BatteryStatus provider spec.
- Complete DDS QoS (reliability, history, durability, deadline, liveliness,
  Domain, Partition, GUID) and deployment interoperability requirements.
- Genuine official nested payload and optional-field TypeSupport/SDK codec,
  LargeListMetadata and separate element samples, keyed Dispose semantics.
- Verified Collection ordering/recovery, standalone vendor E2E and negative
  oracles, lifecycle and compliance traceability.
