# UMAA project documentation

This index groups the **current repository's** documents by responsibility. For
build/SDK/Simulator entry points, start with the repository [README](../README.md).

## Project state and release criteria

- [Delivery gates (G1–G12)](DELIVERY_GATES.md) — acceptance evidence and blockers; a passing unit test is not certification.
- [Repository layout](PROJECT_LAYOUT.md) — directory ownership, where generated files belong, and safe edit boundaries.
- [System architecture](ARCHITECTURE.md) — prototype service/transport/simulator boundaries.

## Specification and traceability

- [ICD catalog audit](specification/ICD_CATALOG_AUDIT.md) — reviewed service and Topic mappings, omissions and assumptions.
- [Traceability](specification/TRACEABILITY.md) — evidence links and limits.

## Implementation design

- [K-MOSA modules](architecture/K_MOSA_DYNAMIC_MODULES.md) — module ABI prototype boundaries.
- [Command transport](implementation/COMMAND_TRANSPORT_STATUS.md) — command integration checkpoint.
- [Cyclone command adapter](implementation/P1_CYCLONE_ADAPTER_STATUS.md) — historical P1 implementation checkpoint and limitations.
- [Cyclone Command input reader](implementation/P2_COMMAND_READER_STATUS.md) — shared sample lifetime, fail-closed epochs and official EngineControl binding.

## Validation records (stable paths retained)

- [Real DDS evidence](REAL_DDS_EVIDENCE.md)
- [EngineControl evidence](ENGINECONTROL_EVIDENCE.md)
- [Cross-platform validation](CROSS_PLATFORM_VALIDATION.md)
- [GitHub Actions](GITHUB_ACTIONS.md)

Historical implementation reports are snapshots from when they were written;
check the [current PR #3](https://github.com/nssang00/umaa-dev/pull/3) and
its exact commit/Actions results before concluding that a limitation remains
or has been resolved. Official AUVSI UMAA IDL is **not vendored** in this repo.
