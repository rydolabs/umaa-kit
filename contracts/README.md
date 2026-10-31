# UMAA 6.0 ICD → YAML catalog projection (v1alpha1)

**Status: 53/53 reviewed services converted, six partial ICD indexes created.**
This is NOT a complete transcription of the official UMAA 6.0 ICD documents.
The available source catalog contains 53 service headings, 113 explicit
Topic/type/role mappings and their operation names from six public secondary
transcriptions (EO, MM, MO, SA, SEM, SO). It does **not** contain full original
ICD paragraphs, all figures/tables/requirements, verified official TypeSupport
binding, Key fields, native DDS QoS or protocol FSM evidence.

- `contracts/services/{EO,MM,MO,SA,SEM,SO}/*.yaml`: one YAML per
  reviewed Service. All original source catalog records are losslessly
  preserved under `extensions.x-catalog.originalRecord`, including
  collection **type references** and Command groups. These collection types
  are **not silently reclassified as proven DDS Metadata/Element topics**.
- `contracts/icds/{EO,MM,MO,SA,SEM,SO}.yaml`: six **catalog-derived
  indexes**, containing all referenced clause IDs and service IDs. Empty
  paragraphs/tables/figures are intentional: they mean original document
  content has not been transcribed, NOT that the official ICD lacks them.
- `schemas/umaa-icd-v1alpha1.schema.json`: strict Service/ICD JSON Schema.
- `tools/test_icd_yaml_catalog_projection.py`: schema validation, duplicate
  YAML-key rejection and catalog-to-YAML exact record/Topic/Operation checks.

## What is not inferred

All generated `spec.operations.*.kind` are `unresolved` and
`topicRef: null` until operation-level source clauses and bindings have
been independently verified. The catalog's **pattern classification** is
preserved, but it does not imply complete behavior, timeout, session
correlation, Update/Cancel or Query handling. The YAML keeps
`ddsTopicName: null`, `keyFields: null`, `qosProfileRef: null`,
`originalSha256: null`, and `bundleSha256: null` where evidence is
unavailable. In particular, a nonempty Collection type list does not
prove the native Large Collection multi-Topic protocol.

**Do not generate an official Component or advertise compliance** from these
drafts. First review the original AUVSI ICD, original official IDL,
Component Definitions and compliance documents; then pin hashes, validate
exact Topic/Type/Key/QoS/Operation semantics, and run independent-process
real DDS tests per vendor.

## Run validation

```sh
python -m pip install 'PyYAML==6.0.2' 'jsonschema==4.23.0'
python -m unittest discover -s tools -p 'test_icd_yaml_catalog_projection.py' -v
```

The six document index files deliberately do not invent normative text.
The eight other IDL directory groups and the unreviewed CO-EXP ICD are
**not** counted as converted services. Source catalog:
`catalog/umaa6-six-icd-reviewed.json`.
