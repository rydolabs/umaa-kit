# DDS Endpoint Planner v1 — Studio-first design preview

The default Studio workflow is **Component → Service → Consumer/Provider**.
Users should not need to configure individual DDS Topic, DataWriter,
DataReader and QoS just to assemble their blocks.

This increment introduces a small JSON **System Composition Intent**,
separate from the explicit **System Model v1** advanced validator. It
contains only pinned Component references, Service links and a project-level
backend / DDS Domain decision. The SDK derives the logical endpoints and
returns unresolved official DDS fields instead of guessing them.

| Layer | User chooses | SDK planner infers |
|---|---|---|
| Standard Catalog | Source-pinned Service | Reviewed directional Topic Type IDs |
| Component | Provider / Consumer Service roles | Pinned Manifest and role matching |
| System | Explicit connections between Components | Every reviewed Writer / Reader direction |
| Deployment context | Standalone Cyclone or Fast DDS + Domain ID | Selected deployment context for preview |
| Advanced DDS | Later, separately reviewed overrides | **Not** fabricated official Topic names, keys or mandatory QoS |

Input schema: [System Composition Intent v1](../schemas/system-composition-intent-v1.schema.json).

## CLI and reusable API

```sh
python -m tools.umaa_component_cli system plan --input path/to/umaa.intent.json
# Equivalent standalone:
python -m tools.umaa_system_planner plan --input path/to/umaa.intent.json
```

Studio may call `tools.umaa_system_planner.plan(doc, model_dir)`
as a pure, deterministic JSON design preview. No code generation,
file creation, dynamic loader, network or DDS entities are involved.

The previous expert-mode Model validator is unchanged:

```sh
python -m tools.umaa_component_cli system validate --input path/to/umaa.system.json
```

## Minimal AnchorControl example

With two existing Component Manifests in separate directories, one Consumer
(`ship_control`), one Provider (`anchor_actuator`), this is the user intent:

```json
{
  "kind": "umaa-system-composition-intent",
  "schemaVersion": "1",
  "system": { "id": "vehicle" },
  "catalogPin": {
    "kind": "reviewed-icd-catalog",
    "sha256": "<current reviewed catalog SHA-256>"
  },
  "deployment": { "backend": "cyclone", "domainId": 7 },
  "components": [
    {
      "id": "ship_control",
      "manifest": "ship_control/umaa.component.json",
      "sha256": "<actual ship_control manifest SHA-256>"
    },
    {
      "id": "anchor_actuator",
      "manifest": "anchor_actuator/umaa.component.json",
      "sha256": "<actual anchor_actuator manifest SHA-256>"
    }
  ],
  "connections": [{
    "service": "UMAA::EO::AnchorControl",
    "consumer": "ship_control",
    "provider": "anchor_actuator"
  }]
}
```

The SHA-256 placeholders above are explanatory, not valid input. The
[test fixtures](../sdk/tests/test_system_planner.py) create real
pinned Manifests and build the model without hard-coded digest values.

The planner derives three Catalog-based endpoints:

| Logical IDL Type | Writer | Reader |
|---|---|---|
| `AnchorCommandType` | Consumer (`ship_control`) | Provider (`anchor_actuator`) |
| `AnchorCommandAckReportType` | Provider (`anchor_actuator`) | Consumer (`ship_control`) |
| `AnchorCommandStatusType` | Provider (`anchor_actuator`) | Consumer (`ship_control`) |

The actual `ddsTopicName`, `keyFields`, `writerQosProfile` and
`readerQosProfile` are explicitly **null**. The original AnchorControl
contract has unresolved Operation→Topic references and no independently
approved original DDS Topic/Key/QoS binding in the partial Catalog.
Consequently, ``status: DESIGN_PREVIEW_NOT_DEPLOYABLE`` and
``ddsDeploymentReady: false`` are not optional warnings.
The preview is **not** a runnable DDS deployment, official conformance
or a substitute for reviewed native TypeSupport.

An incomplete Studio canvas may use `connections: []` and gets an
`UNCONNECTED_CONSUMER_SERVICE` blocker; the SDK never silently connects
same-name Services across Components.

## Regression and remaining work

`python -m unittest discover -s sdk/tests -p test_system_planner.py -v`
checks exact AnchorControl Topic direction without per-Topic config,
CLI parity, deterministic output, explicit no-deployment flags,
pinned Component Catalog protection, role/domain/backend validation,
malformed input and unsafe path, duplicate links and injected Topic/QoS.

The SDK CI runs these tests on Linux GCC/Clang and Windows alongside
existing C++ Component CTests. The production DDS deployment planner still
requires verified Operation/Topic/Type/Key/QoS bindings, native
Endpoint construction, controlled override precedence, completeness
checks, deployment provenance, and actual vendor wire tests.

## Native deployment admission follow-up

A separate fail-closed [Deployment admission v1](DDS_DEPLOYMENT_ADMISSION_V1.md)
reports exact missing source obligations without converting this design
preview to runnable DDS. `system deployment-check --input` returns exit 3
when original native binding evidence is missing.
