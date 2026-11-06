# UMAA Kit System Model v1 — source-checked topology, not deployment

The SDK's third design layer joins the existing **partial reviewed ICD Catalog**
and **per-Component Manifest**: a read-only **System Model** for multiple
Component roles, DDS domains, exact directional Topic identities and explicit
QoS profile references. It is not a DDS deployment generator.

| Model layer | Canonical source | Validation |
|---|---|---|
| Standard Catalog | `catalog/umaa6-six-icd-reviewed.json` | 53 curated Service identities, named Operations and directional Topics; not full authoritative mapping |
| User Component | `umaa.component.json` | Explicit Provider/Consumer Service roles, synthetic codegen or official fail-closed gate |
| User System | `umaa.system.json` | SHA-pinned Component references, connection direction, same-domain DDS, explicit per-Topic QoS |
| Live Runtime | SDK Host and DDS plugins | **Not enabled by System Model v1** |

Schema: [system-assembly-v1.schema.json](../schemas/system-assembly-v1.schema.json).
CLI: `python -m tools.umaa_component_cli system validate --input path/to/umaa.system.json`.
Standalone: `python -m tools.umaa_system_model validate --input path/to/umaa.system.json`.

## Validation behavior

- Require separate existing Component Manifests by **relative paths** and
  their exact SHA-256. Reject symlinked input, absolute/traversing paths,
  modified bytes, duplicate Component IDs or manifests and inconsistent IDs.
- Reuse the current Component assembly validator. Official Service IDs must
  be present in the reviewed Catalog; a System with official Services must
  pin the exact same reviewed Catalog SHA-256.
- A connection requires two distinct declared Components, matching Provider
  and Consumer roles of the same Service, and exactly the same DDS domain.
  Backend choices per domain are standalone `cyclone` or `fastdds` only.
  Cross-domain/foreign-vendor links are rejected.
- For each official Service connection, require **every** reviewed Catalog
  Topic ID exactly once. The validator derives actual *logical* writer/reader
  direction from provider/consumer Topic roles. No guessed Operation
  links, DDS Topic names or native keys are permitted.
- Validate explicitly named QoS declarations, allowed values, depth and
  partition bounds. Check reader/writer reliability, durability,
  partition, ordered/coherent and presentation consistency conservatively.
  These user-supplied values are **not the officially mandated QoS**.
  Native DDS effective QoS, deadline, Key annotations, matching, GROUP
  coherence and liveliness remain unverified.
- Synthetic `Demo::` Services may be connected at Component/role level
  without fabricating official Catalog Topic identities; their `channels`
  must be empty.

Successful result: `VALID_SYSTEM_DESIGN_NOT_DDS_DEPLOYMENT`.
Output ``officialQosVerified``, ``ddsDeploymentReady``,
``officialRuntimeClaim`` and ``officialConformanceClaim`` are always false.
This validator does not create DDS entities, compile C++, or bypass official
native Runtime generation guards.

## AnchorControl mapping

The source-projected AnchorControl Contract currently identifies:

| Topic type (suffix) | Logical DDS direction |
|---|---|
| `AnchorCommandType` | Consumer writes → Provider reads |
| `AnchorCommandAckReportType` | Provider writes → Consumer reads |
| `AnchorCommandStatusType` | Provider writes → Consumer reads |

The source-projected `setAnchor` and `reportAnchorCommandStatus`
operations **still have `topicRef: null`**, and mandatory DDS Topic
names, original key fields and QoS references are unresolved. Modeling
the three Topic identities does **not** resolve or approve those fields.

### Example (repository root)

Create two real Component Manifests (metadata-only; no official DLL/SO):

```sh
python -m tools.umaa_component_cli new component anchor_provider --output anchor_model/anchor_provider
python -m tools.umaa_component_cli service add UMAA::EO::AnchorControl --role provider --project anchor_model/anchor_provider
python -m tools.umaa_component_cli new component anchor_consumer --output anchor_model/anchor_consumer
python -m tools.umaa_component_cli service add UMAA::EO::AnchorControl --role consumer --project anchor_model/anchor_consumer
```

Generate a digest-bound example using the **current** reviewed Catalog,
rather than hardcoding hashes that will become stale after a revision:

```python
import hashlib, json
from pathlib import Path
from tools import umaa_service_contracts as contracts

root = Path("anchor_model")
snapshot = contracts.create_snapshot()
service = next(s for s in snapshot["services"]
               if s["id"] == "UMAA::EO::AnchorControl")
def member(name):
    path = root / name / "umaa.component.json"
    return dict(id=name, manifest=f"{name}/umaa.component.json",
                sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                domainId=7)
demo_qos = dict(reliability="reliable", durability="volatile",
                history="keep_last", depth=16, presentation="instance",
                orderedAccess=False, coherentAccess=False, partition="")
model = {
    "schemaVersion": "1", "system": {"id": "anchor_system"},
    "catalogPin": {"kind": "reviewed-icd-catalog",
                   "sha256": snapshot["source"]["catalogSha256"]},
    "components": [member("anchor_provider"), member("anchor_consumer")],
    "dds": {"domains": [{"id": 7, "backend": "cyclone",
                        "qosProfiles": {"development_only": demo_qos}}]},
    "connections": [{
        "service": service["id"], "provider": "anchor_provider",
        "consumer": "anchor_consumer", "domainId": 7,
        "channels": [{"topicId": t["id"], "writerProfile": "development_only",
                      "readerProfile": "development_only"}
                     for t in service["topics"]]}]
}
(root / "umaa.system.json").write_text(json.dumps(model, indent=2) + "\n")
```

```sh
python -m tools.umaa_component_cli system validate --input anchor_model/umaa.system.json
```

This yields a design-level mapping with each exact IDL Type and writer/reader
Component, **not** a deployable DDS configuration. The official
AnchorControl native Component generator remains blocked.

## Verification

`python -m unittest discover -s sdk/tests -p 'test_system_model.py' -v`
checks original AnchorControl's three directional Topic identities, actual
pinned Component Manifests, CLI invocation and negative paths for unknown
services/roles, missing/foreign/duplicate Topics, divergent domains,
QoS contradictions, stale SHA pins, duplicate JSON fields and unsafe paths.
The existing `tools/ci_verify.py` runs the new suite in Linux and Windows
SDK CI.

**Remaining:** authoritative per-Operation Topic bindings and
source-verified Type/Key/QoS, system-level connection planning for
multiple production DDS domains, actual DataWriter/DataReader generation,
automatic service runtime deployment and independent official conformance.
