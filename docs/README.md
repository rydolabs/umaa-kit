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
- [Official UMAA 6.0 conformance scope](specification/UMAA_6_0_CONFORMANCE_SCOPE.md) — original authority sources, 9 Component tables, trace candidates and fail-closed G6/G11 evidence.

## Implementation design

- [AnchorSpecs/AnchorControl P4 semantic real DDS checkpoint](implementation/P4_ANCHOR_SEMANTIC_WIRE.md) — non-default measurements, three virtual actions and independent Command/Report lifecycle oracles.


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
check the [current project](https://github.com/rydolabs/umaa-kit), its
[ongoing gate tracker](https://github.com/rydolabs/umaa-kit/issues/2)
and the exact commit/Actions results. [Legacy PR #3](https://github.com/nssang00/umaa-dev/pull/3)
is historical P1 evidence, not the canonical repository. Official AUVSI UMAA IDL is **not vendored** in this repo.

- [P3 service context checkpoint](implementation/P3_SERVICE_CONTEXT_STATUS.md) — 53-service literal Topic guard, fail-closed QoS, runtime entity creation.
