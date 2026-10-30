# V3 Increment — evidence PASS guards and DDS development runbooks

This increment does not change `simulator/`, `sdk/`, `dds/`, `catalog/` or
claim any official UMAA conformance result.

1. **Return code 0 is no longer sufficient for PASS.** `verifier/assertions.py`
   enforces independent simulator/host success markers for five real DDS
   profiles (including negative destination and typed-wire/semantic modes).
   Tests that deliberately return exit code 0 without markers now produce
   `FAIL` and saved evidence. These assertions are defense-in-depth on top of
   the independent simulator supervisor; they are *not* a formal oracle.
2. **Stdio success evidence is replay-checkable offline.** A V3 run stores a
   `scenario.snapshot.json`, records its digest, and re-evaluates the JSON
   oracle report against the snapshot when checking evidence. A forged trace
   or changed snapshot is rejected even if all local index checksums are
   recalculated. V3 run envelopes use schemaVersion `1.1`; V2 `1.0` read
   compatibility is retained and has weaker assurance.
3. **Scoped DDS test suites** in `verifier/suites/dds-*-development.json`
   provide ready-to-run lists for EngineControl, AnchorControl, extended
   AnchorControl, AnchorSpecs and reviewed service discovery. A separate suite
   command is required for each service because binaries may differ.

## Required external dependencies / blocked gates

Actual DDS execution remains unavailable in the local environment without
installed Cyclone DDS, official licensed UMAA IDL TypeSupport and the correct
service-specific SUT/Oracle binaries. Existing gate
`--dds-network-approved` is a declaration, **not technical network
isolation**; only use in a genuinely isolated authorized test network.
Normative release gate remains `BLOCKED` until applicable primary requirements
and independently reviewed verification contracts exist.

Example once service binaries and an isolated lab are independently verified:

```sh
python3 -m verifier run-suite \
  --suite verifier/suites/dds-anchor-extended-development.json \
  --sut /trusted/anchor-host --oracle /trusted/independent-anchor-oracle \
  --dds-network-approved --output-dir /new/evidence/anchor-suite-001
python3 -m verifier verify-evidence --evidence-dir /new/evidence/anchor-suite-001
```

Do not use `--dds-network-approved` on operational equipment or networks.
Formal third-party and cross-vendor testing is not covered by this command.
