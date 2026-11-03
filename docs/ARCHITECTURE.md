# UMAA prototype architecture

```text
  User Component / typed Provider & Consumer SDK
                        |
                        v
           sdk/include/umaa (vendor-neutral)
      Command / Report / Collection / Component ABI
                        |
                        v
      sdk/include/umaa/internal/dds_backend.h
       UmaaDdsBackendV1: dynamic DLL/SO plugin ABI
                        |
             +----------+----------+
             |                     |
             v                     v
  dds-backends/cyclone    dds-backends/fastdds
   native TypeSupport       native TypeSupport
             |                     |
             +----------+----------+
                        |
             Real DDS wire transport
                        |
             Independent test processes
                simulator/dds/
```

Shared synthetic IDL fixtures live in `tests/fixtures/idl/` and are
used solely to validate vendor plugin behavior. They are **not** official
UMAA 6.0 contracts. Cyclone's official-source integration hosts live in
`tests/integration/cyclone/`; generated AUVSI IDL types are supplied
by pinned, externally acquired inputs. Neither plugin backend automatically
becomes an official UMAA binding because the synthetic real-wire tests pass.

The common DDS backend ABI and SDK headers remain stable across the
relocation. Platform-specific code stays under `dds-backends/<vendor>/`,
and CMake configures one vendor per plugin build tree. Vendor changes must
not require edits to user Component logic.

`catalog/` contains curated ICD mappings. `tools/` validates and generates
TypeSupport and compile-only service APIs. The fixture generator in
`sdk/tools/` accepts a restricted synthetic IDL subset, not arbitrary
official IDL. `sdk/tests/fake_dds/` exists only for local adapter tests.

See [delivery gates](DELIVERY_GATES.md),
[real wire evidence](REAL_DDS_EVIDENCE.md) and the exact GitHub Actions
runs for verification limits. Full Topic/Key/QoS accuracy, RTI Connext,
mixed-vendor network interoperability and UMAA conformance remain unverified.
