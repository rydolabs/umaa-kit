# Native DDS IDL TypeSupport pipeline

## Real vendor compiler path

The supported TypeSupport generation path is tools/native_dds_codegen.py.
It invokes Cyclone DDS idlc and Fast DDS fastddsgen directly, with
transitive include dependencies, original SHA-256 checks and bounded
external compiler invocations. It does NOT invoke the regex-subset parser
in sdk/tools/umaa_codegen.py.

Command from repo root, with both toolchains installed:

    python3 -m tools.native_dds_codegen \
      --root tests/fixtures/idl/native \
      --source NativeDevice.idl --vendor both --out build/native-types
    cmake -S tests/integration/native_compilers -B build/native-test \
      -DUMAA_NATIVE_GENERATED_ROOT="$PWD/build/native-types"
    cmake --build build/native-test --parallel 2
    ctest --test-dir build/native-test --output-on-failure

The synthetic fixture includes preprocessor include, bounded sequence,
union and a keyed struct. The native generated sources are compiled into
separate Cyclone and Fast DDS shared libraries and loaded in CTest.

For authorized official input, substitute a secure external UMAA IDL root
and the relative source path UMAA/EO/BatteryStatus/BatteryReportType.idl.
The GitHub Actions workflow downloads only a SHA-256 pinned official ZIP,
checks it, and extracts under the ephemeral Runner temp directory. Original
official IDL and generated TypeSupport must not be committed or published
without verifying all applicable redistribution conditions.

## Responsibilities and limitations

Native compilers: actual IDL grammar, generated vendor C/C++ types, DDS CDR,
and TypeSupport. ICD YAML Contract IR: source-reviewed service, Topic, Key,
QoS and operations. SDK generator: future reviewed bindings from verified
native TypeSupport to C++17 ServiceTraits, Component Runtime and DDS backend.

The legacy Python parser remains fixture-only for basic host-neutral
Component scaffolds and is NOT the authoritative native DDS generation path.
This PR verifies generated native vendor libraries, not a complete generated
official UMAA Service Adapter, nor official QoS/Key semantics, nor official
UMAA 6.0 conformance.

Later work: audited YAML-to-vendor TypeBinding adapter generation and
separate-process official keyed writer/reader, Collection and Specialization
tests across both DDS vendors. A successful TypeSupport build alone does not
prove real DDS interoperability or service protocol correctness.
