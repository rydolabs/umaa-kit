# Independent UMAA Simulator / Verifier Prototype (Project B)

This project does **not** import SDK code, does **not** use SDK command state machines, and is designed to test arbitrary external System-Under-Test (SUT) executables. It has its own negative oracle tests and protocol trace validator.

**Current transport: development-only stdio fixture** (NOT DDS, NOT a production simulator, NOT the official UMAA wire protocol):

Requests to SUT (each on a separate line):

```text
COMMAND <source> <destination> <session> <monotonic_timestamp> <rpm>
FINISH <session> SUCCESS|FAILURE
DISPOSE <source> <session>
LOST_CONSUMER <source> <session>
RESTART
EXIT
```

Responses are `STATUS <session> <state> <reason>`, `ACK <session>`, `DISPOSE_STATUS <session>`, `DISPOSE_ACK <session>`, or `IGNORED <session> <reason>`.

The simulator compares exact scenario expectations to process stdout, and an **independent** oracle checks status transitions, reason codes, ACK ordering and terminal cleanup; these are not imported from SDK.

Run from repo root after building example target:

```bash
python3 simulator/src/run.py --sut "$(realpath build/mock_engine_host)" \
  --scenario simulator/scenarios/normal.json --report evidence/manual.json
```

The simulation currently does not model real DDS discovery, keyed samples, instance disposal QoS, liveliness events, late joining, or cross-vendor serialization. These MUST be exercised using real DDS participants before any UMAA conformance claim.

## Independent future direction

Simulator needs its own official IDL import/codegen and topic registry, DDS backend(s), service-emulation actors, virtual time, fault-injection controls, scenario execution, and a specification-derived oracle separate from SDK implementation. The actual device simulation is optional and should be a plugin to this external project, not the SDK itself.
