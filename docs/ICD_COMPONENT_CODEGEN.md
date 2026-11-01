# Contract IR and YAML+IDL Component generation (first vertical slice)

**Scope:** compile-tested, vendor-neutral C++17 Component v2 **scaffolds**, not
native official UMAA DDS endpoints. The generator is intentionally fail-closed.

## Inputs and ownership

1. **Original IDL source** declares exact types, scalar fields and `@key`
   annotations. The present parser accepts a strict **single-file subset**:
   nested `module`, `struct` with `long`/`string` fields and `@key`.
   Unsupported IDL syntax fails instead of being approximated.
2. **Authored Service YAML** validates against the canonical
   `schemas/umaa-icd-v1alpha1.schema.json` and supplies explicit Topic aliases,
   DDS Topic names, IDL type references, keys, QoS profile identifiers, one
   complete Report (queryless subscription) or Command (Command/Status/Ack)
   protocol, and resolved Operation references/clauses.
3. **Component manifest** (`umaa.component.json`,
   `schemas/component-assembly-v1.schema.json`) selects services and
   provider/consumer roles. An ICD does not imply physical Component membership.
4. **Native DDS TypeBindings** remain vendor/backend responsibilities. Codegen
   does not generate or validate these bindings.

The 59 reviewed YAML resources in `contracts/umaa6/` are **generated from the
JSON catalog** and must not be edited in place. A separately authored,
source-reviewed contract can be passed directly to the new tools. This is not
a second canonical catalog generator.

## Inspect source and generation blockers

```sh
# Outputs deterministic JSON with precise missing official evidence.
# Exit 3 means unresolved, even when the YAML itself is structurally valid.
python -m tools.umaa_contract_ir \
  --yaml contracts/umaa6/services/EO/BatteryStatus.yaml

python -m tools.umaa_contract_ir \
  --yaml contracts/umaa6/services/EO/EngineControl.yaml
```

For complete authored input supply `--idl <file.idl>` and the original
ICD document via `--original-icd <official-document>`. Only the actual
file digests are compared; flags and hashes **do not authenticate the identity
or redistribution rights of the claimed authoritative source**.

A missing/incorrect IDL SHA, unsupported IDL type, missing `@key`, unresolved
Topic or QoS, unreviewed official IDL/QoS, unresolved operation, source hash,
or unsupported Collection semantics prevents code generation. No guessed
fallbacks are permitted.

## Generate an independently compiled synthetic fixture

The dedicated regression test constructs synthetic `Demo::` Service YAML
from the reviewed mapping shapes, explicitly supplies Topic/Key/QoS and
pins its own synthetic IDL SHA. With those files and a normal Component
manifest:

```sh
python -m tools.umaa_component_cli generate \
  --project path/to/component \
  --yaml path/to/DemoBatteryStatus.yaml \
  --yaml path/to/DemoEngineControl.yaml \
  --idl path/to/synthetic.idl \
  --fixture

# Equivalent standalone generation entry point:
python -m tools.umaa_icd_component_codegen \
  --manifest path/to/component/umaa.component.json \
  --yaml path/to/DemoBatteryStatus.yaml \
  --yaml path/to/DemoEngineControl.yaml \
  --idl path/to/synthetic.idl \
  --out path/to/component \
  --fixture
```

`--fixture` is restricted to `Demo::` services and **cannot** bypass
the official `UMAA::` source-readiness gate.

Generated files include:

- `generated/types.hpp` and `generated/service_contracts.hpp`: typed
  Service traits with explicit identifiers, topic/QoS strings, and
  `encode/decode_command` or `encode/decode_report`.
- `generated/codec.hpp`: bounded, platform-neutral **host-only** payload
  encoding. It is **not DDS CDR**, RTPS or a native TypeSupport.
- `generated/component_entry.cpp`: Component ABI v2 Provider registration;
  user-owned `src/handlers/` files are created only if absent.
- `src/consumers/`: typed **callback skeletons only**. No DDS subscription,
  independent Consumer module or remote Command client is implemented by
  these files.
- `CMakeLists.txt`, `tests/codec_smoke.cpp`,
  `tests/module_smoke.cpp`, and valid JSON metadata in
  `generated/generation.json`, including full source hashes.

The `Component` currently requires at least one Provider in the manifest;
consumer-only runtime generation is not implemented. User-written business
logic is never regenerated, and unmarked generated files are not overwritten.

## Validation and explicit limitations

```sh
python -m pip install PyYAML==6.0.2 jsonschema==4.23.0
python -m unittest discover -s sdk/tests -p test_icd_contract_ir_codegen.py -v
python -m unittest discover -s sdk/tests -p test_icd_yaml_contract.py -v
```

The regression builds/installs the real C++ SDK, compiles the generated
Component using its installed CMake package, and runs generated
`component_abi_smoke` and `contract_codec_smoke` with CTest on Ubuntu and
Windows. No vendor SDK is required to compile the generated Component.

**Remaining work:** parse/compile the full official IDL bundle; produce a
source-audited Service/IDL field and Key graph; review source-derived Topic,
QoS, Operation rules and protocol state transitions; generate complete
Report and Command Provider/Consumer native bindings and runtime tests in
`dds-backends/` (PR #27), especially real independent-process and mixed
vendor cases. PR #26's catalog projection is not an official compliance proof.
