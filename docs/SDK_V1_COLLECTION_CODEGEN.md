# SDK v1 Collection + Specialization and ICD YAML code generation

## Delivered (draft PR #37)

- `RuntimeApplication::publish_collection<Service>` and
  `consume_collection<Service>` connect the existing typed ServiceTraits
  contract to the genuine Cyclone DDS / Fast DDS Collection frame adapter.
  Element type, Set/List shape, schema, Topic and QoS identifier must agree
  with the explicitly supplied `WireService`. No inferred binding.
- `SpecializedCollectionConsumer<Service,Detail>` composes the existing
  bounded `CollectionAssembler` and `SpecializationJoin`. A snapshot is
  visible only when every referenced Generalization and Specialization has
  the exact requested GUID and revision; missing halves, tombstones and
  one-sided updates fail closed. Supports Set and linked List.
- CTest exercises Set/List ordering, revision skew, tombstone and missing
  specialization. Real DDS tests use two independent processes with the
  **same** typed SDK executable on Cyclone DDS and Fast DDS.

**Transport limit:** these tests use synthetic single-frame transactions.
They do not establish native UMAA Element/Metadata separate DDS Topic
delivery, GROUP coherence, official TypeSupport or actual separate
Generalization/Specialization DDS Topic synchronization. Those need a
reviewed multi-topic TypeBinding and versioned backend capability.

## Delivered (draft PR #38)

The existing `tools.umaa_contract_ir` + `tools.umaa_icd_component_codegen`
pipeline now emits `generated/sdk_v1_traits.hpp` for source-pinned,
fully resolved Command and Queryless Report IR, with:
- `ServiceTraits<generated::Service>` using the same host-only generated
  `WireService`, Command/Report type, source-pinned schema identifier,
  flow kind, and explicit `CollectionShape::None`.
- A compiled `sdk_v1_traits_smoke` CTest inside the generated Component
  project, plus Python contract and metadata regressions.
- No changes to Component ABI v2 or DDS Backend ABI v1.

The codegen cannot be used as native DDS TypeSupport: the current codec is
host-only, and official IDL types (including sequences, arrays, unions,
nested annotations) are not supported by the restricted parser. Unsupported
collection/specialization IR, missing official ICD SHA, key annotations,
QoS, operations, original ICD digest or vendor binding continue to fail
closed. Never claim official UMAA 6.0 conformance from this generated output.

## Acceptance gates

- PR #37: `SDK v1 Collection and Specialization integration` plus
  existing Cyclone/Fast DDS synthetic real-wire workflows.
- PR #38: `ICD YAML catalog projection` on Linux and Windows, which builds
  and runs the generated Component and `sdk_v1_traits_smoke`.
- No PR merges until reviewed. Full native multi-topic collections and
  official TypeSupport generation are explicit **remaining work**.
