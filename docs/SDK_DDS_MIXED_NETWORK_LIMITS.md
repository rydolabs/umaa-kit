# Mixed Cyclone / Fast DDS network interoperability — NOT COMPLETE

**As of 2026-10-29**. This document separates two different goals:

1. **Same-binary adapter substitution (PASS, synthetic Report)**: one
   device component `.so` and one typed Consumer executable run against
   Cyclone or Fast DDS plugins **independently**, without recompilation.
2. **Mixed-vendor network communication (FAIL / not proven)**: publisher and
   subscriber use **different** DDS vendors in separate processes, including
   required reliable-writer acknowledgment and keyed Dispose.

## Actual GitHub-hosted experimental evidence

- [Run 40831127878](https://github.com/rydolabs/umaa-kit/actions/runs/40831127878):
  the normal same-binary independent tests passed.
- **Fast DDS publisher → Cyclone DDS subscriber:** Cyclone reported successful
  receipt of BatteryStatus data and keyed Dispose, but Fast DDS 2.11
  `wait_for_acknowledgments` reported `ERROR` on both intermediate and
  final checks. **No reliability ACK proof**; strict trial FAILED.
- **Cyclone DDS publisher → Fast DDS subscriber:** Cyclone publisher reported
  success; Fast DDS subscriber returned exit code 3 because it did not
  observe the full required data+Dispose condition. **Strict trial FAILED.**
  The initial run lacks separate data/dispose diagnostic fields; later code
  now emits both fields to narrow the fault.

These are genuine middleware processes exchanging **synthetic DemoBattery IDL**
only, not reviewed official UMAA 6.0 TypeSupport, QoS or keyed service mapping.
Do not infer compliant vendor interoperability.

## Reproduction

The opt-in `.github/workflows/sdk-dds-mixed-network-experimental.yml`
workflow runs both vendor directions and fails until both meet their
**strict** data + keyed Dispose + final reliable ACK checks. It is
`workflow_dispatch` only; its failure does not mask the already-verified
per-vendor, same-binary support. Commands can also be run on a Linux host
with Cyclone DDS, Fast DDS and their IDL compilers installed.

Future work: compare exact generated DDS type identifiers, XTypes/plain
representation, key serialization, reliable ACK/NACK interoperability,
reader lifecycle and matched endpoint QoS, then rerun both directions.
Only reviewed normative UMAA service bindings can clear official
interoperability acceptance gates.

## 2026-10-29 strict synthetic Collection cross-vendor retest

[Run 40884484884](https://github.com/rydolabs/umaa-kit/actions/runs/40884484884)
and [diagnostic run 40885117020](https://github.com/rydolabs/umaa-kit/actions/runs/40885117020)
repeated both Set/List combinations using identical CLIs and real
Cyclone/Fast DDS middleware:

- **Fast DDS Publisher → Cyclone Reader:** complete data, delta and keyed
  Dispose observed. Fast DDS 2.x `wait_for_acknowledgments` returned ERROR,
  so the *strict final ACK gate fails*.
- **Cyclone Publisher → Fast DDS Reader:** data and delta transferred,
  but the Fast DDS Reader's instance handle on a foreign Dispose was
  different from its cached data handle, with no recoverable key field
  (`key_field=0 mapped=1`). The adapter correctly fails closed instead
  of guessing a key.
- The reader was explicitly kept alive after Dispose to allow an RTPS ACK;
  the strict test still failed [40885883191](https://github.com/rydolabs/umaa-kit/actions/runs/40885883191).

This confirms a **specific real interoperability limitation**, not a
defect that may be hidden by mapping unknown lifecycle keys to the single
known instance or fabricating a DDS ACK. The opt-in mixed-network Workflow
remains available for testing a different supported vendor version.
