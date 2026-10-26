# Command transport / Collection safety checkpoint (2026-10-26)

## Scope

Source basis: UMAA handoff 2026-10-26, repository commit
`89155aa4163f04d69585c4e1054dbbbd8c944c2d`.
The handoff's original 53 reviewed service contracts, 113 Topic types and
selected official Battery/Engine wire examples remain intact.

This checkpoint introduces a **vendor-neutral** Command transport binding,
not a complete CycloneDDS/RTI adapter and not an official conformance claim.
See `sdk/include/umaa/command_transport.hpp`. `CommandTransport<Payload>`
is responsible for checked Status/Ack/Dispose operations. Every status is
associated with the accepted source, destination, session and latest timestamp.
Every Ack or Ack Dispose also has access to the last accepted `Payload` (an
actual official Command type in a real DDS adapter), so an update cannot
silently send the original payload. `Ignored` state machine events never
trigger a transport write.

The binder stops output after any exception, including an unknown vendor
write result; reconstruct DDS entities and persisted state before resuming.
This is intentionally conservative and is not automatic durable-state recovery.
`timeout(session)` requires host-supplied deadline detection and confirmed
handler cancellation. A command whose initiating DDS instance was already
disposed cannot be updated while device cancellation is still unsafe.

The collection assembler now rejects a second collection ID without explicit
reset, and supports `dispose(element_id, timestamp)` to reject out-of-order
resurrection. When no reliable transport timestamp is available, the legacy
`dispose(element_id)` method is still available but **cannot prove ordering**.
Neither path implements mandatory DDS PRESENTATION ordered/atomic QoS.

## Local evidence (Linux container, 2026-10-26)

| Check | Actual result | Scope |
|---|---|---|
| Strict GCC CMake/CTest | 7/7 pass | Header-only core/fixture, `-Wall -Wextra -Werror -pedantic` |
| Clang UBSan CTest | 7/7 pass | Undefined behavior sanitizer, bounded tests |
| SDK Python unittests | 32/32 pass | Generator/catalog fixtures |
| Simulator Python unittests | 6/6 pass | Oracle checks |
| Independent stdio scenarios | 4/4 pass | Synthetic generated EngineControl fixture |
| Portable verification pipeline | PASS GCC, PASS Clang | Includes package install and external CMake consumer |
| New official DDS/Cyclone tests | **NOT RUN** | Local CycloneDDS/idlc and official IDL absent |
| RTI Connext 7.7 | **BLOCKED** | No RTI SDK/license |
| Windows MSVC | **NOT RUN in this session** | Prior handoff CI evidence does not validate new changes |

Local logs are provided as a separate evidence attachment, not interpreted
as DDS wire, multi-vendor, production or certification evidence.

## Remaining acceptance blockers

- Bind official service-specific Command struct/Status/Ack to Cyclone C
  Reader/Writer/Dispose, with all mandatory QoS and instance keys reviewed.
- Trace every applicable service role, Topic, QoS, Component Definitions and
  Compliance requirement; complete CO/EXP scope.
- Cover multi-Command families, execution-status variations, full Collection
  synchronization, specialization and startup/liveliness recovery.
- Independently execute real DDS per-service tests under network impairment,
  Windows, RTI↔Cyclone and appropriate security/robustness stress testing.
- Keep G1/G8 PARTIAL and G6/G7/G11 OPEN until evidence satisfies each gate.
