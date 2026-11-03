# SDK native IDL code generation — extensible backend protocol

Status: Native vendor compilation and ICD YAML-driven native TypeSupport
build generation are implemented and validated. Official UMAA Provider/
Consumer runtime bindings are NOT inferred from compilation.

## Unified architecture

1. Existing Component CLI and tools.umaa_icd_component_codegen select native
   ICD generation using explicit --idl-root and --idl-binding arguments.
2. tools.native_dds_codegen validates original source paths and hashes,
   gathers the transitive include closure, and discovers installed native
   compiler plugins. **No vendor-specific branches occur in the core.**
3. Each provider runs its own real IDL compiler and declares generated
   source files, include paths, CMake packages/libraries, compiler features
   and language requirements.
4. Common native CMake generation consumes only those declarations.
5. tools.native_component_codegen checks ICD YAML, pinned original source,
   explicit type-to-file mappings, and emits native TypeSupport targets,
   generated C++ Topic/Key/QoS identity contracts, compiler-owned native
   type-and-member probes, shared-library load tests, and a source-backed
   native Contract IR with unresolved Runtime binding blockers.
6. The old Python scalar IDL parser is used ONLY by the explicit legacy
   Demo:: --fixture --idl compatibility path, not official ICD generation.

## Third-party DDS compiler plugin

A trusted installed plugin supplies three functions:

    available(*, executable=None) -> bool
    generate(root, target, closure, output, *, executable=None) -> dict
    type_probe(type_name, source, keys) -> dict

The generate method returns exactly:

    {
      "sources": ["type_source.cpp"],
      "includeDirs": ["."],
      "language": "CXX",
      "cmakePackages": ["VendorDDS CONFIG REQUIRED"],
      "cmakeLibraries": ["VendorDDS::vendor"],
      "cmakeFeatures": ["cxx_std_17"],
      "cmakeProperties": {}
    }

The type_probe method returns a dictionary with language, extension, code.
It must generate a native C/C++ translation unit checking the exact type
and named Key **member existence**, not unverified @key semantics.

Install a trusted, version-pinned Python package with an entry point:

    [project.entry-points."umaa.native_dds_backends"]
    thirdparty = "trusted_vendor.dds_compiler_plugin:backend"

Alternatively add a plugin module under tools/native_dds_backends/.
No changes to native_dds_codegen, Component CLI, ICD generator, the
CMake consumer, or tests are required for a new provider. Installed
Python plugins execute trusted code; untrusted YAML/IDL paths must never
be used to import plugin implementations.

## Native ICD workflow

Given a matching Component manifest and authored Service YAML:

    python3 -m tools.umaa_component_cli generate \
      --project /path/component \
      --yaml /path/Service.yaml \
      --idl-root /private/pinned/idl-tree \
      --idl-binding 'UMAA::Some::ReportType=UMAA/Some/ReportType.idl' \
      --original-icd /private/original-icd.pdf \
      --idl-archive /private/pinned-official-idl.zip \
      --backend cyclone --backend fastdds

For Demo tests only, add --fixture and pin YAML to the exact source
IDL SHA. Omitting --backend selects all trusted, installed plugins.
The low-level legacy --vendor both is an alias for all installed plugins.

Build independently of backend names:

    cmake -S /path/component -B /path/component-build
    cmake --build /path/component-build
    ctest --test-dir /path/component-build --output-on-failure

IDL source parsing and serialization belong to the vendor compiler.
The Python Contract IR retains reviewed Topic/Key/QoS/Operation metadata.
Compiler-generated type/member probes do NOT prove official Key
annotation, normative QoS, transaction state semantics or wire codec
identity. A full official SDK Provider/Consumer Runtime binder still
needs independently verified native TypeSupport ↔ host-wire mapping.
Until then, nativeDdsBinding=false and componentRuntimeBound=false
are deliberate fail-closed statuses: a buildable native library is NOT
a certified UMAA Component DLL/SO with live business handlers.

Original official source and generated TypeSupport must not be
redistributed without confirming all applicable rights and recipients;
CI uses ephemeral Runner temp and never publishes official artifacts.
