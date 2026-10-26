# UMAA prototype architecture

```text
User module (typed business logic)
            │
            ▼
Vendor-neutral SDK (sdk/include/umaa)
 Command state / Report lifecycle / Collection / Module ABI
            │
            ▼
Cyclone DDS adapter (dds/cyclone/include/umaa)
 DDS C participant, readers and writers; service-specific mappings
            │
            ▼
Official UMAA DDS TypeSupport and Topics (CI-acquired IDL)
            │
            ▼
Independent Simulator (simulator/dds)
```

**Independent verification boundary:** the Simulator is a separate process
that uses its own DDS C sample handlers. Its success does not imply every
official UMAA service, field, key, QoS, or vendor combination is implemented.

`catalog/` contains curated ICD mappings; `tools/` validates and generates
TypeSupport and compile-only service APIs. The SDK's example generator in
`sdk/tools/` accepts a restricted synthetic IDL subset, not arbitrary official
IDL. `sdk/tests/fake_dds/` exists exclusively for local adapter unit tests.

The current Cyclone binding has tested selected BatteryStatus and
EngineControl paths on Linux and Windows. Review [delivery gates](DELIVERY_GATES.md),
[real wire evidence](REAL_DDS_EVIDENCE.md), and the exact GitHub Actions runs
for limits. RTI Connext, mixed-vendor interop, full QoS correctness, and
UMAA/K-MOSA compliance remain **unverified**.
