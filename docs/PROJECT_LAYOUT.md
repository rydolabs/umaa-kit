# Repository layout and ownership

This structure is deliberately **not** an all-in-one `src` migration: `sdk/`,
`dds/cyclone/`, `simulator/`, and `tools/` are consumed by existing CMake,
Python, and GitHub Actions paths. Moving them without changing all those
callers would make already verified tests non-reproducible.

```text
umaa-dev/
├── .github/workflows/   GitHub Actions CI and real-DDS validation
├── catalog/             Reviewed service and Topic JSON contracts
├── dds/cyclone/         CycloneDDS C binding, hosts, wire integration
│   ├── include/umaa/    Vendor-specific transport headers
│   ├── src/             Official-type and synthetic sample programs
│   └── idl/             Synthetic test IDL (not official UMAA IDL)
├── sdk/                 Installable C++17 core library and user API
│   ├── include/umaa/    Vendor-neutral public headers
│   ├── cmake/           Package configuration
│   ├── tools/           Restricted fixture codegen
│   ├── examples/        Sample handlers/hosts
│   ├── fixtures/        Synthetic test fixtures
│   └── tests/           Unit tests, fake DDS, downstream consumers
├── simulator/           Independent oracle and cross-process tests
│   ├── dds/             Independent real-DDS simulator processes
│   ├── scenarios/       stdio scenario fixtures
│   ├── src/             Scenario runner/verifier
│   └── tests/           Simulator tests
├── tools/               Official IDL, catalog, CI and auditing tools
├── smoke/               Separate native shared-module ABI smoke proof
├── docs/
│   ├── README.md        Documentation index
│   ├── ARCHITECTURE.md  Architecture boundaries
│   ├── PROJECT_LAYOUT.md Repository map and conventions
│   ├── DELIVERY_GATES.md  Release gate status
│   ├── architecture/    Native module design details
│   ├── implementation/  Transport/adapter implementation notes
│   └── specification/   ICD catalog and traceability notes
├── evidence/            Committed evidence manifest; CI output ignored
├── AGENTS.md            Development rules
├── PROJECT.md           Intended technical scope
├── PROGRESS.md          Chronological handoff log
├── STATUS.md            Historical project snapshot
└── README.md            Primary entry point
```

## File management rules

1. Place **public, vendor-neutral headers** in `sdk/include/umaa/` and
   Cyclone-specific headers in `dds/cyclone/include/umaa/`.
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
