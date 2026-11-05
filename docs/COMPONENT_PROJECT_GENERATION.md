# UMAA Component Project Generation — first end-to-end slice (2026-10-30)

> **Status:** development-only, synthetic ABI-v2 Provider demonstration.
> This is **not** formal UMAA 6.0 conformance, an official service binding,
> or a working React Flow designer.

## Target product model

~~~mermaid
flowchart LR
    E["EA / official ICD + IDL"] --> C["Versioned Service Contract"]
    C --> M["Component Manifest"]
    U["React Flow UI (planned)"] --> M
    L["UMAA CLI (first slice)"] --> M
    M --> V["Shared validator & generator"]
    V --> P["CMake + generated ABI v2 wrappers"]
    P --> B["User business logic"]
    B --> D["DLL / SO Component"]
    D --> H["UMAA Host Runtime"]
    H --> X["Cyclone or Fast DDS standalone"]
~~~

The authoritative UMAA 6.0 ICD/IDL/Component Definitions are **not**
replaced by an internal YAML/JSON representation. Sparx EA is the proposed
K-UMAA model-of-record for project-managed contract modeling, while normative
UMAA claims still require source-document and wire evidence.

## Contract tiers

| Input | Current state | Code-generation decision |
|---|---|---|
| Demo::EO::EngineControl | Verified ABI v2 synthetic int32 fixture | **Allowed Provider module** |
| Demo::EO::BatteryStatus | Verified ABI v2 synthetic int32 fixture | **Allowed Provider module** |
| UMAA::… reviewed catalog identities | Reviewed literal service/Topic mappings; official QoS/runtime not yet proven | **Reject until reviewed native binding exists** |
| Unknown service | Not in catalog | **Reject** |
| Synthetic Demo::EO::BatteryStatus Consumer in DLL/SO | ABI-v2 inbound Report/Dispose Handler + protected user files; native Linux/Windows generated module CTest | **Allowed development fixture only** |
| Command Consumer and official Service Consumer | Native DDS Source/Key/QoS/Reader binding or Command client protocol unreviewed | **Reject** |
| User extension IDL | Studio has limited extension-only draft generator | **Not yet accepted** |

This distinction prevents a generated CMake project from pretending that a
literal service name implies a complete official wire implementation.

## CLI (from repository root)

~~~sh
python -m tools.umaa_component_cli new component engine_demo
python -m tools.umaa_component_cli service add Demo::EO::EngineControl --role provider --project engine_demo
python -m tools.umaa_component_cli service add Demo::EO::BatteryStatus --role provider --project engine_demo
python -m tools.umaa_component_cli generate --project engine_demo

cmake -S sdk -B build/sdk-cli -DUMAA_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/sdk-cli-install"
cmake --build build/sdk-cli --config Release
cmake --install build/sdk-cli --config Release

python -m tools.umaa_component_cli build --project engine_demo --sdk-prefix build/sdk-cli-install
python -m tools.umaa_component_cli test --project engine_demo
~~~

This creates a platform-native CMake **MODULE** library (SO on Linux, DLL on
Windows), not an independent process. The host still owns DDS backend
selection. The generated CTest opens the module via the actual Component ABI
v2 loader, checks every service ID and schema ID, and requires a clean close.

## Generated project ownership

~~~text
engine_demo/
├── umaa.component.json       # common future UI/CLI source of truth
├── CMakeLists.txt            # generated, optional cmake/user.cmake extension
├── generated/
│   ├── service_contracts.hpp # synthetic development fixture only
│   └── component_entry.cpp   # ABI registration and export
├── src/handlers/             # user owned; never overwritten
│   ├── engine_control.hpp
│   └── battery_status.hpp
└── tests/module_smoke.cpp    # native ABI loader smoke
~~~

User Handler defaults are fail-safe: Command execution **rejects**, and
Report sampling **throws** until explicitly implemented. A generated ABI
module does not imply a safe physical device implementation.

Generate can be repeated without modifying user Handler files or the project
README. Only files marked generator-owned may be replaced. An unmarked
CMakeLists.txt is protected from overwrite.

## Next acceptance gates

1. **Schema and provenance:** formal Service Contract + Assembly schema;
   canonical manifest serializer, pinned EA GUID/model snapshot and official
   ICD+IDL hashes, validated Operation-to-Topic-to-Type identity and role.
2. **Official Report vertical slice:** SA/GlobalPoseStatus Provider and Consumer,
   verified official IDL TypeSupport and normative DDS Topic/key/QoS evidence;
   separate Cyclone and Fast DDS tests.
3. **Command vertical slice:** MO/GlobalVectorControl with Request/Ack/Status,
   ExecutionStatus, UPDATE, Dispose and actual device safety semantics.
4. **React Flow Studio:** supported React build and graphical Service role
   assembly edit the **same** Manifest and call the **same** generator. UI
   coordinates/layout are stored separately.
5. **System Composer:** validate Provider/Consumer availability, DDS domains,
   versions, instance keys, QoS and deployment independently of Component code.

The current DDS milestone continues to target **standalone Cyclone DDS**
and **standalone Fast DDS**, with the same built module/client binaries in
separate setups. Mixed-vendor same-network interoperability and licensed
RTI Connext independent validation are **explicitly later objectives**.

## Tests and safety

- Python negative tests cover malformed/duplicate manifests, unknown services,
  unsupported official/Consumer generation and user-owned file protection.
- Native integration tests install the SDK, build a generated MODULE and use
  a generated CTest to open it through Component ABI v2.
- A passing generated-module smoke is **not** DDS wire or formal service
  conformance evidence.
- Untrusted user CMake/IDL must not run on a public Studio Worker before
  isolation, quotas, audit and deployment security controls are in place.

See [SDK modular architecture](SDK_MODULAR_ARCHITECTURE.md),
[current phase scope](CURRENT_PHASE_SCOPE.md),
[official ICD catalog audit](specification/ICD_CATALOG_AUDIT.md)
and [delivery gates](DELIVERY_GATES.md).

## Contract snapshot and pinned assembly gate — added 2026-10-30

The **Service Contract snapshot is an export artifact**, not another
authoritative source to edit. It projects the already validated reviewed
catalog, retaining every recorded explicit Operation, directional Topic,
TypeSupport identity, Command group, source clause and current evidence flag.
All 53 reviewed services / 113 Topic records remain a *partial mapping*.
The original catalog bytes are SHA-256 pinned; `contract verify` independently
recreates and compares the entire projection, rejecting altered roles, forged
QoS claims or a changed catalog.

~~~sh
# From repository root: deterministic reviewed mapping export
python -m tools.umaa_component_cli contract export --output build/reviewed-contracts.json
python -m tools.umaa_component_cli contract verify --input build/reviewed-contracts.json

# Same assembly gate used by future Studio; not a runtime conformance verdict
python -m tools.umaa_component_cli validate --project engine_demo
~~~

- Schemas: [Component Assembly v1](../schemas/component-assembly-v1.schema.json)
  and [reviewed Service Contract snapshot v1](../schemas/reviewed-service-contract-snapshot-v1.schema.json).
- `umaa.component.json` remains the source of truth for **user assembly**.
  Standard services added through the CLI now include a `catalogPin` with
  SHA-256 of the validated source file. Stale or missing pins are rejected
  by the assembly gate. This does not silently migrate a user project to a
  changed ICD/catalog; future explicit upgrade/approval UX remains OPEN.
- **Provider/Consumer at assembly level is not the Topic writer direction.**
  The snapshot records per-Topic directions from the service ICD and the
  assembly chooses which whole-service role the Component will play.
- The existing partial catalogue lacks a per-service `source` field for
  EO `BatteryStatus` and `EngineControl`. Their exported source reference is
  explicitly **null**, rather than invented. The top-level reviewed basis
  and pinned ledger revision are preserved, but detailed per-service
  provenance still needs authoritative confirmation.
- `codegenReady=true` means **synthetic Demo Provider MODULE generator only**.
  The gate always returns `officialRuntimeClaim=false` and
  `normativeQosClaim=false`; official service selection is allowable as an
  assembly draft but will not produce an official DLL/SO binding.
- The source repository catalog is pinned, **not** a validated Sparx EA
  model revision. EA GUID/MDG profile + service contract extraction from
  real EA model and formal conformance proof remain separate next steps.

The current CLI/JSON Schema layer has no extra runtime dependency; if a
third-party editor uses the JSON Schema independently, it still must
invoke the Python contract and assembly gate for catalog provenance,
cross-record Topic ownership and normative safety restrictions.


## 2026-11-05 — bidirectional synthetic Report Consumer in one DLL/SO

The development CLI can now compose a **single** C++ ABI-v2 native
Component from different synthetic Provider/Consumer Service roles,
including the *same* `Demo::EO::BatteryStatus` in both roles. One
bidirectional Service produces **one** ABI descriptor with the union
of its Report Provider + Consumer action bits, not duplicate
registrations. The two business Handler classes remain independent
and user-owned.

```sh
python -m tools.umaa_component_cli new component mixed_demo
python -m tools.umaa_component_cli service add Demo::EO::EngineControl --role provider --project mixed_demo
python -m tools.umaa_component_cli service add Demo::EO::BatteryStatus --role consumer --project mixed_demo
python -m tools.umaa_component_cli generate --project mixed_demo
# Build/test using the installed SDK prefix from the earlier example.
python -m tools.umaa_component_cli build --project mixed_demo --sdk-prefix build/sdk-cli-install
python -m tools.umaa_component_cli test --project mixed_demo
```

The module exports additional **host-local ABI-v2** calls
`UMAA_COMPONENT_REPORT_ACCEPT` (action 5, kind bit 8) and
`UMAA_COMPONENT_REPORT_DISPOSE` (action 6, kind bit 16).
Both require an **explicit nonzero opaque source token** in the existing
`session_id` field. Report input must decode into the selected synthetic
type; Dispose accepts no data bytes. These are **not DDS Topics, UMAA
Report Ack protocol, normative Source GUIDs, or a complete vendor
Consumer Reader**. The external Host remains responsible for genuine
DDS source/key/ordered QoS validation and converting native Dispose
before it can forward any callback. Unknown/invalid payloads fail
closed. Generated Consumer Handler defaults return `Rejected`
on Report and Dispose; no hardware, data or component success is
manufactured.

No C ABI structs or `UMAA_COMPONENT_ABI_VERSION` were changed:
existing v2 Provider modules keep working. The new registration
merges Provider and Consumer action sets only for an identical
Service ID **and identical schema**; conflicting schemas or repeated
roles are fatal. The same generated module ABI loader/stop gate
drains both roles before unloading. The generated CTest exercises
real DLL/SO loading and malformed/rejected inbound callbacks; the
new standalone C++17 CTest also exercises an accepting user Handler
and exception fencing.

**Still OPEN:** genuine DDS-backed synthetic/official Consumer
subscription wiring into the module from a reviewed Service Contract,
exact per-Source GUID + Timestamp semantics, all official QoS, native
GROUP coherence, Command consumer bindings, consistent full-service
code generation and official UMAA conformity.
