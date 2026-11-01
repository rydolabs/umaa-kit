# Repository layout and ownership

The DDS implementation has one vendor-specific root, `dds-backends/`.
Standalone Cyclone integration programs and cross-backend IDL fixtures are
kept in `tests/`; the independent oracle remains in `simulator/`.
This layout preserves the existing dynamic `UmaaDdsBackendV1` ABI.

```text
umaa-kit/
├── .github/workflows/       Continuous integration and real DDS evidence
├── catalog/                 Reviewed Topic and service mappings
├── sdk/                     Vendor-neutral C++17 SDK and Component runtime
│   ├── include/umaa/        Stable SDK / DDS backend ABI headers
│   ├── src/                 Runtime implementation
│   ├── tests/               SDK unit and fake-DDS tests
│   └── cmake/               Installed package configuration
├── dds-backends/
│   ├── CMakeLists.txt        One-vendor-per-build dispatcher
│   ├── cyclone/
│   │   ├── CMakeLists.txt    Cyclone plugin / real-wire synthetic tests
│   │   ├── src/             Native Cyclone backend plugin implementations
│   │   └── include/umaa/    Reusable Cyclone-only transport bindings
│   ├── fastdds/
│   │   ├── CMakeLists.txt    Fast DDS plugin / real-wire synthetic tests
│   │   └── src/             Native Fast DDS backend plugin implementations
│   └── rti/                 Documented external SDK prerequisite only
├── tests/
│   ├── fixtures/idl/        Shared synthetic DDS IDL; NOT official UMAA IDL
│   └── integration/cyclone/ Independent Cyclone official-archive / wire
│       ├── CMakeLists.txt    Standalone, source-gated integration project
│       └── src/             Test hosts and publishers
├── simulator/
│   └── dds/                Independent oracle processes and test drivers
├── verifier/               Scoped verifier and evidence handling
├── studio/                 EA QEA and platform prototype
├── tools/                  Official IDL, catalog, CI and auditing utilities
├── docs/                   Architecture and implementation documentation
├── smoke/                  Separate ABI smoke checks
└── evidence/               Evidence manifest (scratch output is ignored)
```

The public SDK does not depend on a DDS vendor. The backend plugins compile
the vendor API and generate fixture TypeSupport in a private build tree.
Official AUVSI IDL/TypeSupport is acquired separately for source-gated
integration tests, not committed to this repository.

## File management rules

1. Place **public, vendor-neutral headers** in `sdk/include/umaa/` and
   Cyclone-specific headers in `dds-backends/cyclone/include/umaa/`.
2. Put actual component implementations with their matching SDK or DDS build;
   keep the **independent** oracle under `simulator/`, not in SDK code.
3. Do not commit official AUVSI IDL archives, unpacked official IDL, build
   directories, generated TypeSupport, downloaded vendor SDKs, credentials,
   or CI scratch logs. GitHub Actions acquires the pinned official archive
   temporarily subject to the publisher's terms.
4. Keep reviewed Topic contracts in `catalog/`; do not infer UMAA normative
   topics from folder names.
5. Put release evidence links and reproducible commands in `docs/`. CI output
   may appear in ignored `evidence/ci/`; published workflow artifacts remain
   accessible from the corresponding GitHub Actions runs.
6. Treat `docs/DELIVERY_GATES.md` as the gate ledger; archived progress notes
   are not automatic proof of runtime conformance.

## Local verification

From repository root:

```sh
python tools/ci_verify.py --compiler default
python tools/ci_verify.py --compiler clang  # when Clang is installed
```

For official UMAA TypeSupport and real DDS integration use the pinned
`.github/workflows/` definitions on Linux/Windows runners; do not replace
those checks with fixture-only unit tests.
