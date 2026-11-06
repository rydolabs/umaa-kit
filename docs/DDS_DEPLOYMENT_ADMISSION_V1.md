# DDS Deployment admission v1 — source-evidence gate (not a generator)

This phase is a separate, **fail-closed native DDS deployment admission**
layer on top of the existing System Composition Intent v1 Planner. It
does not emit compiled code, vendor configuration, a Participant, DataWriter,
DataReader, or any other executable DDS entity.

## Commands

- Studio/backend API: call tools.umaa_dds_deployment.assess(model, root).
- File API: tools.umaa_dds_deployment.assess_file(path).
- CLI: python -m tools.umaa_component_cli system deployment-check --input path/to/umaa.intent.json
- Standalone: python -m tools.umaa_dds_deployment check --input path/to/umaa.intent.json

Both CLI commands print deterministic JSON. **Exit code 3 means deployment
admission blocked** (the expected outcome on the current Catalog); exit code 2
is malformed input or a failed source pin. Automation must not treat a
JSON report as permission to deploy.

## What is checked

The same Planner validates every pinned Component Manifest, original reviewed
Catalog SHA-256, explicit Consumer/Provider Service link, and standalone
backend/domain context. This layer adds per-service source-evidence obligations:

1. Original Operation-to-Topic and Flow assignments (not name-based guesses).
2. Approved native IDL TypeSupport and instance Key annotations.
3. Exact DDS Topic names (IDL type identities do not prove wire names).
4. Required Writer/Reader QoS and vendor effective QoS matching.
5. Native Command/Report/Dispose and Collection lifecycle behavior.
6. Independent exact-build real-wire verification and, for Collections,
   coherent Element/Metadata endpoint ownership.

For each selected Service, the report preserves the reviewed logical Topic
writer/reader components and lists individual Operations with topicRef and
flow **null**. It preserves all native Topic name, Key, QoS fields as null.
A nonempty, validated design is therefore NOT converted to a deployable
plan. The current 53-Service Catalog marks every service's original IDL,
QoS and behavior verification gates false. Synthetic Demo services likewise
cannot authorize official native deployment.

## Why this gate is separate

The normal System Planner stays a side-effect-free design preview and returns
success when topology is valid. Native deployment CI must instead use
deployment-check, which returns nonzero until independently reviewed source
bindings and native vendor wire tests exist. User-provided topic/QoS fields,
self-reported approval flags and guessed fallback values are not accepted.

The next implementation should add trusted source-side Operation/Topic/Key/QoS
review artifacts and their independent provenance verifier, then bridge only
accepted evidence into vendor-specific DDS endpoint construction. This
**preflight does not claim to perform that later bridge**. Mixed Cyclone/Fast
network interoperability and RTI Connext remain future-only phase work.

Regression: python -m unittest discover -s sdk/tests -p test_dds_deployment_admission.py -v


## Exact-source AnchorControl milestone (2026-11-06)

One original-source review record was derived from AUVSI EO ICD v6.0,
pages 50–51 (service §6.1.1), and original UMAA 6.0 IDL ZIP. The record
contains ONLY derived Topic names, key member names, operation/transport
intentions, source page numbers and SHA-256 digests; no original PDF or
IDL bytes are published in this repository.

Record: catalog/umaa6-eo-anchorcontrol-source-bindings-v1.json.
Verifier: tools.umaa_original_evidence.verify_originals(idl_archive, eo_icd_pdf).

Example:

    python -m tools.umaa_component_cli system deployment-check \
      --input path/to/umaa.intent.json \
      --original-idl-zip /private/umaa-6.0-idl-with-exp-services.zip \
      --original-eo-icd /private/UMAA-SPEC-EOICD-v6.0.pdf

The two source paths MUST be supplied together. The exact original
ZIP/PDF SHA-256 values are required. The verifier reads only the three
pinned AnchorControl IDL files (one per Writer/Reader Topic), validates
their individual digests, extracts their literal Topic constant and
@key fields, and compares them with the reviewed Catalog and the source
record. It also checks the exact original EO PDF SHA; Operation/Topic
assignments and the "query -> subscribe, cancel -> dispose" mapping are
manually reviewed assertions anchored to EO ICD §6.1, pages 50–51,
NOT results of generic PDF semantic parsing.

If the originals are not supplied, all fields retain the original
unresolved/null outcome and deployment remains blocked. When they are
supplied and verified, the report exposes exactly the 3 Topic names and
key arrays plus 5 Operation/topic/transport intentions. This removes
ONLY the corresponding per-Service source identity/mapping gaps.
Mandatory effective QoS, complete Command/Dispose lifecycle, runtime
bindings, independent wire tests, production Studio integration and UMAA
conformance are still unverified and must remain BLOCKED. A previous
synthetic DDS test is not evidence for all official requirements.

Compliance note: respect the AUVSI IDL Terms of Use. Keep source PDF,
ZIP, and full IDL file bodies external to public Git history. Never
copy them into the project to make CI green. In existing CI without
the private source files, only schema/review/deny regressions run;
no CI test can claim to have reverified the original bytes.


## Source-pinned BatteryStatus and EngineControl (2026-11-06)

The same exact original EO ICD v6.0 (SHA-256
\`47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453\`)
and AUVSI IDL archive (SHA-256
\`15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08\`)
are now reviewed for **two additional** EO Services.

- **BatteryStatus** — EO §6.1.5 pp. 54–55. \`reportBattery\`
  writes \`BatteryReportType\` (IDL Key: \`source\`), and
  \`queryBattery\` is DDS subscribe to that Topic, *not* a query
  DataWriter. The Report contains a Large List with a separate DDS
  \`BatteryReportTypeCellsListElement\` Topic defined in the
  \`BatteryReportType.idl\` file; element keys: \`listID\`, \`elementID\`.
  EO §3.8.1 p. 14 mandates Topic-scope PRESENTATION with ordered
  access for Collection element publishers/subscribers.
  This *partial source rule* is now recorded, but does **not** constitute
  a full effective native DDS QoS profile or prove atomic cross-topic
  Collection handling. The Collection element is reported separately;
  it is **not** silently added to executable or logical endpoint counts.
- **EngineControl** — EO §6.1.6 pp. 55–56. Command/Ack/Status
  Topics have exact original literal names and Key annotations:
  Command \`source\`, \`sessionID\`, \`destination\`;
  Ack/Status \`source\`, \`sessionID\`. \`setEngine\` writes,
  \`queryEngineCommandAck\` subscribes, \`cancelEngineCommand\`
  disposes, and both reports write. EO §6.1.6.3 further requires
  explicit cancel to end ongoing Engine command execution.

Bindings are kept as **reviewed derived metadata** only in:
\`catalog/umaa6-eo-batterystatus-source-bindings-v1.json\` and
\`catalog/umaa6-eo-enginecontrol-source-bindings-v1.json\`. They are
independently checked against reviewed Catalog service identities and
exact source IDL member digests, literal Topic constants and \`@key\`
annotations by \`tools.umaa_eo_original_evidence\`.

With **both original source files supplied**, the admission CLI now
populates reviewed Topic/Key/Operation values for selected
AnchorControl, BatteryStatus and/or EngineControl. Every unselected or
unreviewed Service remains unresolved. The partial Collection QoS
rule is surfaced separately and **never** converted to invented
writer/reader QoS profiles or DDS entity creation.

\`COLLECTION_ENDPOINT_OWNERSHIP\`,
\`COLLECTION_TOPIC_COVERAGE_INCOMPLETE\`, effective mandatory QoS,
full lifecycle, vendor wire, and production deployment gates stay
BLOCKED. No source document or original IDL text is committed.
