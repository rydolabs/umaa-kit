# V3 local reproduction commands

Run from repository root in a trusted Linux development environment with
Python 3.13+, CMake, a C++ compiler, and `ctest`.

```bash
python3 -m unittest discover -s verifier/tests -v
python3 -m unittest discover -s studio/tests -v
python3 -m unittest discover -s simulator/tests -v
python3 -m unittest discover -s simulator/dds -p 'test*.py' -v
python3 tools/ci_verify.py

# The SDK CI creates build/ci-linux-default/umaa_mock_engine_host on Linux.
python3 -m verifier run-suite \
  --suite verifier/suites/stdio-regression.json \
  --sut "$(realpath build/ci-linux-default/umaa_mock_engine_host)" \
  --output-dir /tmp/verifier-v3-example-suite
python3 -m verifier verify-evidence --evidence-dir /tmp/verifier-v3-example-suite
python3 -m verifier export-results --evidence-dir /tmp/verifier-v3-example-suite \
  --output-file /tmp/verifier-v3-studio-result.json

# Expected gate exit code 2 until official scope/contracts have independent approval.
python3 -m verifier scope-report --require-ready
```

The stdio suite is synthetic development-only. A real DDS scenario requires a
correct trusted host, independent DDS oracle, official licensed TypeSupport,
Cyclone DDS runtime, and isolated authorized network. The five
`verifier/suites/dds-*-development.json` files are **not** evidence of such
runs. `--dds-network-approved` is an operator declaration only, not a network
sandbox.
