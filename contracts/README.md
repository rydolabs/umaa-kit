# UMAA 6.0 ICD YAML contracts (v1alpha1)

The **only generated reviewed-catalog contract tree** is
[`contracts/umaa6/`](umaa6/README.md): `icd/*.yaml` for six ICD indices
and `services/*/*.yaml` for 53 reviewed services. The former
`contracts/icds/` and `contracts/services/` trees were redundant
noncanonical projections and have been removed.

Source: `catalog/umaa6-six-icd-reviewed.json`.
Generator: `tools/generate_icd_yaml.py`.
Validator: `tools/validate_icd_yaml.py`.
Schema: `schemas/umaa-icd-v1alpha1.schema.json`.
Do not recreate a separate generator, schema, or parallel source of truth.

Current catalog coverage: **6 ICD indices / 53 services / 113 Topic/IDL
mappings / 131 operation names**. Exact operation-to-topic rules,
official DDS Topic names, keys, mandatory QoS, authoritative original
ICD prose/tables/figures/SHALL requirements, and native DDS conformance
remain unverified. The original reviewed mapping remains reproducible
from the pinned catalog. No experimental EXP/CO-EXP mapping is implied.

```sh
python -m pip install PyYAML==6.0.2 jsonschema==4.23.0
python -m tools.generate_icd_yaml --check
python -m tools.validate_icd_yaml
python -m unittest discover -s sdk/tests -p 'test_icd_yaml_contract.py' -v
```

Structural validation **does not** mean official UMAA or DDS wire
compliance. For future original-text ingestion, pin the authoritative
ICD and IDL hashes first; preserve paragraph/table/figure/requirement
provenance and verify coverage before enabling SDK runtime bindings.
