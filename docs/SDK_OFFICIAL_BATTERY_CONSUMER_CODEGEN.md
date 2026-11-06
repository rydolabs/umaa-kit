# Source-verified UMAA EO BatteryStatus Consumer DLL/SO generator

Status: **SDK business-only Host Preview MODULE**, **not official DDS Runtime
deployment, Component Definition approval, native Codec or UMAA conformance**.

## What now works

The SDK can create a **real native C++17 DLL/SO** with a typed
`BatteryStatusHostPreview` Consumer. The developer implements only:

- `on_report(const StableList&, uint64_t host_source_token)`
- `on_dispose(uint64_t host_source_token)`

The SDK generates and owns the Component entrypoint, C ABI v2 registration,
the ServiceBinding C++ header (Service ID, source-pinned Report and Element
Topic names, IDL types, Key fields, two reviewed Operation intentions and
the Topic-ordered Collection-element minimum), native CMake module and
ABI smoke CTest. User-owned `src/handlers/battery_status_consumer.hpp`
defaults to `Rejected` and survives regeneration without edits.

The generated module deliberately contains **no vendor DDS header** and
does **not** deserialize DDS CDR. A native Reader must first authenticate
the original TypeSupport and both keyed Report and Element topics,
assemble a completed, consistent `StableList`, and deliver that as a
versioned *host-local ABI payload*. An existing reviewed Cyclone path
`OriginalBatteryStatusReader` and `BatteryStatusComponentBridge` can
drive such a DLL/SO; it is not yet automatically wired to the standalone
`umaa_component_host` created for synthetic Report Providers.

## Generate and build

Keep the original files privately accessible on your build machine;
they are required **each time you regenerate** this preview project.

~~~sh
python -m tools.umaa_component_cli new component batteryclient --output batteryclient

python -m tools.umaa_component_cli service add UMAA::EO::BatteryStatus \
  --role consumer --project batteryclient

python -m tools.umaa_component_cli generate \
  --project batteryclient \
  --battery-host-preview \
  --idl-archive /private/umaa-6.0-idl-with-exp-services.zip \
  --original-icd /private/UMAA-SPEC-EOICD-v6.0.pdf

# Edit batteryclient/src/handlers/battery_status_consumer.hpp

python -m tools.umaa_component_cli build \
  --project batteryclient --sdk-prefix /path/to/installed-UMAA-SDK
python -m tools.umaa_component_cli test --project batteryclient
~~~

Expected output: `batteryclient` platform-specific CMake MODULE DLL/SO
plus a CTest that actually loads the module and sends a bounded versioned
Host ABI preview event and Dispose. Default behavior rejects both events
until the developer implements the business policy. An ABI smoke test
checks protocol validity, not whether user business policy rejects or
completes a valid event.

The generator refuses missing or mismatched original PDF/IDL hashes,
unknown Catalog revisions, extra Services or a non-consumer role,
tampered Topic/Key/Operation evidence, unsafe generation paths and
overwriting pre-existing user CMake/Handler files.

## Critical acceptance boundary

The **source hashes and declaration facts** were checked at generation
time against the original AUVSI EO ICD v6.0 and pinned 594-file IDL ZIP;
no original text is copied to source control or generated outputs.
The original PDF's *meaning* is a hand-reviewed assertion, not formal
machine-verifiable normative semantics.

The typed `StableList` contains a **completed Host projection**, not
native DDS CDR, and its `nativeDdsCoherenceProved` remains false.
The source-backed minimal element `TOPIC` + ordered Presentation QoS
rule is not a complete QoS profile. Actual Component Definition roles,
official effective Reader/Writer QoS, native endpoint creation,
Collection recovery, generic Command/Report Operation dispatch,
SDK standalone Host automatic official subscription, security and
formal compliance remain **unimplemented/unapproved**.

CI tests mock only original-file access to compile/test the generated
C++ module (the bytes cannot legally be republished into CI by default);
a manual generation with both exact originals is needed to establish
the source pin on a particular local generation run. Neither green
CTest nor matching SHA means native runtime conformance.
