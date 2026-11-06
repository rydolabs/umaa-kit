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
