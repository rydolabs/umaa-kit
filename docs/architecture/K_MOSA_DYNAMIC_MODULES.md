# UMAA service-per-module architecture (K-MOSA-aligned **proposal**, not a certified K-MOSA interface)

K-MOSA denotes a Modular Open Systems Approach to modular/reusable/interoperable systems. No normative K-MOSA software plugin ABI was supplied or verified. Thus the ABI here is **a project-owned proposal**: a stable `extern "C"` entry point on `.dll` (Windows) or `.so` (Linux), rather than a claim about official K-MOSA requirements.

## Architectural contract

```
Official UMAA IDL + versioned Service Catalog
             |
          codegen (SDK)
             v
[UMAA Component Host / Protocol Runtime / DDS Adapter] <-- internal host-owned command state, GUID, QoS, sessions
             |
    stable internal C module ABI (this demo)
       |                   |
  EngineControl.dll    BatteryStatus.dll  (Windows)
  EngineControl.so     BatteryStatus.so   (Linux)
       |                   |
  User/device logic      User/data logic
```

- One swappable **service implementation** per module, independent of the DDS vendor. One physical shared library can later host several logical services if there is a real packaging requirement; one-service-per-module is the default.
- Module ABI is strictly local: never expose official UMAA DDS topic types, C++ objects, exceptions, STL containers, runtime-specific allocators or DDS vendor handles over it.
- The host is the sole owner of UMAA protocol state and DDS participants/readers/writers. No plugin can issue UMAA Ack, Status or Dispose directly.
- The host validates service identity, ABI version, schema ID/fingerprint, required entry points, configuration and dependencies **before activating** a module. Never silently load an incompatible module.
- SDK code generation owns the conversion from a typed user-facing Service Handler to this internal ABI; user should not need to implement the C ABI.
- Cancellation and module unloading require protocol-aware quiescence: stop accepting new commands; drain or reject/mark as failed outstanding commands according to the relevant ICD; only then call `stop`, destroy, unload. **The current demo proves clean load/unload only; it does not implement safe live hot swapping.**
- For in-process modules, crash and memory corruption are shared with the host. Use isolated OS processes when third-party modules are less trusted, require ABI/toolchain isolation, or must be restarted independently. Cross-process interface should still use official UMAA DDS topics or a separately specified control API.
- Dependence direction: modules depend only on public generated Handler interfaces and minimal module ABI; module implementations must not depend on the simulator. The independent simulator exchanges UMAA data over DDS once implemented, without linking to host internals.

## K-MOSA acceptance items for a later product

- Versioned interface and manifest: module ID, exact UMAA IDL revision/fingerprint, service interface, capabilities and dependencies, ABI revision, platform/architecture, publisher identity.
- Build each service independently into Windows DLL and Linux SO; have a host verify compatibility before load.
- Startup/shutdown/recovery and safe update contract; service substitution with explicit failure modes and rollback plan.
- Reproducible interface/contract tests and service-specific functionality tests; cross-vendor DDS tests are separate from dynamic loader tests.
- Code signing, provenance, module allowlist, resource limits and audit logging for deployable production software.

## Proof delivered in this iteration

`sdk/include/umaa/module_api.h`, `module_loader.hpp`, `sdk/tests/modules/{engine,battery}.cpp` and `sdk/tests/test_dynamic_module.cpp` demonstrate two independent shared modules and validation of load/invoke/unload, clean reload and rejection on schema/service mismatch. `ctest` runs the module test with platform-native library paths. **The numeric `invoke` contract is synthetic and does not implement real UMAA semantics.**