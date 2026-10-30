# UMAA 6.0 C++ SDK and independent DDS verification simulator

## Approved end-state goal

Preserve **every applicable official UMAA 6.0 IDL wire contract** and implement
every service interface, role, Topic, key, DDS QoS, pattern and required
protocol lifecycle **as defined by its normative ICD and Component Definitions**.
An application developer writes only typed business/device Handler code.
The SDK owns DDS/vendor-adapter entities, Command/Response state, Report
Publish/Subscribe, **UMAA queryless Request/Reply by Report subscription**,
Collections, Dispose, discovery/recovery, failure semantics, and module lifecycle.

Do **not** invent a separate on-wire RPC or Query-Data Request/Reply where the
UMAA 6.0 ICD does not define one. Service contracts may have multiple Report
Topics and cannot be safely inferred from type-name conventions alone.

The **long-term** intended delivery includes native C++17+ Linux (GCC/Clang) and Windows
(MSVC), independent Simulator processes, Cyclone DDS and RTI Connext 7.7
adapters, cross-vendor interoperability and K-MOSA-aligned independently
loadable service modules. RTI and mixed-vendor interoperation are **later
objectives**, explicitly excluded from the current milestone. The custom
demo module ABI is **not** claimed to be an official K-MOSA ABI.

## Active milestone scope (approved 2026-10-30)

The **current phase** targets independent standalone Cyclone DDS and Fast DDS
backends, same compiled Component/client binaries against either plugin in
separate vendor-only deployments, and the SDK safety/official-service work
described in [CURRENT_PHASE_SCOPE.md](docs/CURRENT_PHASE_SCOPE.md).
**Cyclone↔Fast DDS mixed-network functionality and RTI Connext DDS adapter /
independent verification are future milestones only**, excluded from current
phase implementation, CI acceptance and release-blocker calculations.
Historical mixed-vendor failures remain FAIL; RTI remains unverified.
This is a phased development target, **not** a reduction of normative UMAA
conformance obligations or a claim of finished/production-grade software.

## Current evidence and limits (2026-10-25)

This repository remains an **incomplete, non-certified SDK prototype**.

- Original AUVSI 6.0 IDL archive is SHA-256 pinned and acquired only in
  ephemeral GitHub Actions runners. The existing Cyclone C TypeSupport
  coverage proof compiles **352/352 official IDL files** on Ubuntu.
- `catalog/umaa6-eo-reviewed-subset.json` is now a manually checked **EO Section 6.1 Topic mapping** of 14 services/21 Topics.
  The larger `catalog/umaa6-six-icd-reviewed.json` now records 53 service
  sections and 113 explicit Topic definitions across EO/MO/MM/SA/SEM/SO, with
  paired multi-Command and optional ExecutionStatus contracts.
  The corresponding typed C++ API contracts use actual Cyclone-generated
  official TypeSupport and compile under Linux and Windows. This is **not**
  complete service implementation or all-field execution behavior.
- `tools/audit_official_idl_catalog.py` inventories all 352 files,
  `tools/extract_icd_topic_ledger.py` preserves explicit operations/topics
  from six pinned ICD Markdown transcriptions; their proof and unresolved
  scope are in `docs/specification/ICD_CATALOG_AUDIT.md`.
- Working **real DDS vertical slices** remain BatteryStatus Report and
  EngineControl ignition Command/Ack/Status/Update/Cancel/Dispose.
  Tests run as independent DDS processes in Linux and Windows Actions.
- Linux network impairment uses three Linux namespaces, veth, bridge and
  bidirectional `tc netem` egress, including measured packet drops and
  discovery restoration with the same processes. This does NOT prove
  active-command mid-flight partition recovery or all UMAA QoS requirements.
- Full ICD/Component/Compliance trace, collection semantics, vendor-neutral
  QoS and RTI/Cyclone interoperability, comprehensive negative tests,
  production security and certification remain **OPEN/BLOCKED**.

## Implementation and responsibilities

- `sdk/`: typed generated Handler contract, service/catalog toolchain,
  common Command and Report lifecycle cores, installable CMake library and
  versioned shared library loader.
- `dds/cyclone/`: real Cyclone DDS examples and selected official service
  wire integration. DDS entity ownership belongs to SDK/host, not user Handler.
- `simulator/`: independent process and independent DDS sample oracle.
- `tools/`: official IDL TypeSupport generation, catalog/ICD audits and
  fail-closed C++ API generation.
- `catalog/`: reviewed, **not inferred**, service contracts.
- `docs/DELIVERY_GATES.md`: required proof and blocking criteria (G1–G12).
  Do **not** declare release, full conformance or hardware safety unless
  all appropriate gates pass.

## Authoritative inputs and reproducibility

AUVSI UMAA 6.0 ICD, official IDL, Component Definitions and
Compliance Specification form the normative acceptance basis. Original
IDL is not redistributed by this repo. Public upstream ICD Markdown
transcriptions can assist traceability but are not themselves a replacement
for authoritative AUVSI normative documents.

Canonical GitHub repository: `rydolabs/umaa-kit` (source snapshot imported from
`nssang00/umaa-dev` on 2026-10-26, which retains earlier PR history). CI uses hosted `ubuntu-24.04`
(GCC/Clang) and `windows-2022` (MSVC). Real Linux DDS,
network-namespace/netem and native Windows DDS workflows upload logs.
Use the **exact Actions run and Git commit** as evidence for each assertion.
`AGENTS.md` defines development/approval boundaries; do not overwrite
developer-authored Handler files or silently weaken any verification gate.
