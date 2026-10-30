# UMAA Verifier — V1 scoped offline executable

An independent **local** test orchestrator over the existing `simulator/`
process and DDS oracle runners. Does **not** import SDK state machines, change
the simulator, certify UMAA compliance or install DDS middleware.

## What works now

- Six explicit simulator profiles, mapped to **development-unreviewed** contracts
  in `contracts/legacy-profiles.json`. No normative requirement claims.
- CLI single-case and validated batch-suite execution, subprocess time limits,
  selected scenario options only, no evidence overwrite.
- `PASS`, `FAIL`, `BLOCKED`, `ERROR`; `conformanceClaim: false` for every run.
- C++ binary, simulator runner, scenario and contract registry SHA-256 provenance.
- Per-case stdout/stderr and stdio oracle result; index and offline evidence
  digest consistency checker, including corrupted-file detection.
- Legacy Simulator/SDK test source remains independent and unchanged.

## Quick start (repo root)

```sh
python3 -m verifier list
python3 -m unittest discover -s verifier/tests -v
python3 -m unittest discover -s simulator/tests -v
python3 -m unittest discover -s simulator/dds -p 'test*.py' -v
cmake -S sdk -B build/verifier-fixture -DCMAKE_BUILD_TYPE=Release
cmake --build build/verifier-fixture --target umaa_mock_engine_host
python3 -m verifier run-suite \
  --suite verifier/suites/stdio-regression.json \
  --sut "$(realpath build/verifier-fixture/umaa_mock_engine_host)" \
  --output-dir evidence/ci/verifier-stdio-suite
python3 -m verifier verify-evidence --evidence-dir evidence/ci/verifier-stdio-suite
```

The four scenarios are **development-only stdio fixtures**, not DDS tests.
The `--output-dir` must not exist; use a new directory for subsequent runs.

## Single real DDS profile (requires prebuilt official-IDL test binaries)

```sh
python3 -m verifier run --profile dds-anchor-extended \
  --sut /trusted/anchor-host --oracle /trusted/independent-oracle \
  --option=--complete --output-dir evidence/ci/anchor-complete-001
```

This wraps the existing simulator's process supervisor. Success signifies
**only** its selected real-wire scenario. It requires actual Cyclone DDS and
UMAA official TypeSupport, not supplied or installed by this package.

Supported local command examples:

```sh
python3 -m verifier run --profile stdio-fixture \
  --sut /trusted/mock-host --scenario simulator/scenarios/normal.json \
  --output-dir evidence/ci/normal-001
python3 -m verifier verify-evidence --evidence-dir evidence/ci/normal-001
```

Exit codes: `0` PASS/integrity OK, `1` FAIL, `2` BLOCKED/setup error,
`3` ERROR/integrity mismatch. A suite returns ERROR > FAIL > BLOCKED > PASS.

## Interfaces and traceable data

- Suite schema: `suites/stdio-regression.json` as an example. Cases contain
  immutable `id`, allow-listed `profileId`, optional `option`, `timeoutSec`,
  and for stdio, a JSON scenario directly in `simulator/scenarios/`.
- `run-suite` accepts one trusted SUT and optional oracle/module per invocation;
  use a separate invocation when different service binaries are required.
- Contract registry is a **declaration of test harness coverage**, not an
  official or approved normative specification.
- Evidence files: `verdict.json`, `evidence-index.json`, `runner.stdout.log`,
  `runner.stderr.log`, plus `simulator-result.json` for successful stdio runs.
  A suite adds `suite-verdict.json`, `suite-index.json` and each case subfolder.

## Important limitations

This is a trusted **local operator** tool, not a protected multi-tenant job
service. It accepts executable paths and can interact with DDS networks; do
**not** expose through a browser/API or grant untrusted users arbitrary execution.
POSIX subprocess-group timeout and log limits are best-effort safety measures,
not container isolation. Windows recursive process-tree cleanup is not implemented.

SHA-256 indexes detect accidental changes or mismatched evidence, but someone
able to rewrite files and all indexes can forge them. Store externally signed
hashes/immutable audit records if legally meaningful provenance is required.
DDS tests require approved test networks, vendor dependencies, normative QoS
profiles and independent conformance contracts before broader acceptance.

See `docs/verifier/ACCEPTANCE_MATRIX.md`, `INTEGRATION_STATUS.md` and
`docs/DELIVERY_GATES.md` for scope, proof and blockers.

## Read-only offline result UI

```sh
python3 -m verifier render-report \
  --evidence-dir evidence/ci/verifier-stdio-suite \
  --output-file evidence/ci/verifier-stdio-report.html
```

Open the generated HTML locally. This is **not** a multi-user web UI, and it
refuses to render evidence that fails digest/semantic consistency checks.

## V2: DDS opt-in, scope assessment, Studio result exchange

Existing `simulator/` and SDK components remain unchanged. A real-wire DDS
profile must now be explicitly opted in by the trusted operator; otherwise it
records **BLOCKED** with evidence without launching a simulator or SUT process:

```sh
python3 -m verifier run --profile dds-anchor \
  --sut /trusted/official-anchor-host --oracle /trusted/official-anchor-oracle \
  --dds-network-approved --output-dir evidence/local/anchor-001
```

`--dds-network-approved` is an **operator declaration only**. It does not
create an isolated network, limit the DDS domain, establish authorization,
validate the binary provenance, or guarantee against unintended control
messages. Use only an already isolated, explicitly authorized test network.
Do not expose it through a multi-user HTTP endpoint without a hardened worker
and approval model. When the flag is omitted, DDS runs are blocked by default.

Inspect currently unapproved compliance coverage:

```sh
python3 -m verifier scope-report
```

For an independently digest-verified, versioned read-only result exchange with
Studio, run:

```sh
python3 -m verifier export-results \
  --evidence-dir evidence/ci/verifier-stdio-suite \
  --output-file evidence/ci/verifier-stdio-studio-result.json
```

Schema: `verifier/schemas/studio-result-v1.schema.json`. All exchanges explicitly
carry `conformanceClaim=false`, no normative requirement references, profile,
contract, run ID, result and input/output digests. The exported data is **not**
a test instruction nor an editable expected-result source. SHA-256 provides
consistency checking, not a signature or immutable audit trail.

`stdio-fixture` single runs now restrict scenarios to repository-local
`simulator/scenarios/*.json`; arbitrary external fixtures cannot silently
replace the baseline. V2 changes are additive and retain the existing V1 CLI.

To use the conformance scope as a *blocking* release gate (nonzero exit code
until separately approved), invoke `python3 -m verifier scope-report --require-ready`.
The current source catalog intentionally returns exit code `2` and JSON
`gateStatus=BLOCKED`. A normal `scope-report` without this option is a
successful diagnostic command and **must not** be treated as an approval.

## V3: stronger evidence checks and packaged DDS suites

New run envelopes store schemaVersion `1.1`. A successful stdio run includes a
hashed copy of its scenario fixture, and offline verification re-evaluates the
scenario ↔ oracle report relation, rejecting inconsistent PASS evidence even if
index hashes are regenerated. All development DDS profiles additionally require
expected supervisor/oracle markers in stdout before accepting exit code 0.
Legacy V2 `1.0` evidence can still be opened, without the new semantic checks.

Five service-specific DDS suite plans are packaged under `suites/dds-*-development.json`.
These are test definitions only, not evidence of real-wire execution. See
`docs/verifier/V3_INCREMENT.md` for setup and limitations.
