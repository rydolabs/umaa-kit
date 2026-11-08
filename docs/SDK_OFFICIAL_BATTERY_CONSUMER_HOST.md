# Scoped BatteryStatus Native Consumer Host

**Stage:** Cyclone DDS source-generated native wire to ABI-v2 business MODULE.
This is a development Host, NOT all UMAA mandatory QoS, full Component
Specification approval, an arbitrary Service Runtime, or certification.

## Runtime integration

The existing official BatteryStatus Consumer generator builds a native C++17
DLL/SO containing a user-owned business Handler implementing on_report and
on_dispose. The business module links no DDS vendor libraries.

This optional executable, umaa_official_battery_consumer_host, loads that
DLL/SO, validates its exact UMAA::EO::BatteryStatus Service ID, non-normative
Host-only schema and ABI-v2 Consumer actions, then creates genuine native
Cyclone DDS Report and Collection Element DataReaders with original
source-generated TypeSupport.

The SDK reader authenticates the original Source, List and Element instance
keys and real Dispose callbacks. The existing BatteryStatusComponentBridge
forwards only complete Host StableList snapshots over the protected
Component ABI-v2 boundary; an incomplete update does not trigger a fake
Dispose. The Host polls, stops native DDS delivery before unloading the
Component, and fails when callbacks or active Source state cannot be drained.

## Build using private original IDL input

    python tools/official_idl_compile.py \
      --root /private/extracted-original-UMAA6-IDL \
      --source UMAA/EO/BatteryStatus/BatteryReportType.idl \
      --out /private/battery-typesupport

    cmake -S tests/integration/cyclone -B /tmp/battery-host-build \
      -DUMAA_OFFICIAL_GENERATED_ROOT=/private/battery-typesupport

    cmake --build /tmp/battery-host-build \
      --target umaa_official_battery_consumer_host

    cmake --install /tmp/battery-host-build \
      --prefix /tmp/battery-host-installed

The separate Component generator from PR #74 requires BOTH the original
EO ICD PDF and original IDL ZIP to source-verify each generated module.
Its CMake module then builds a platform-specific DLL/SO. Never commit
or redistribute the original source, source-generated proprietary
TypeSupport or PDFs without checking the applicable license.

## Run

    /tmp/battery-host-installed/bin/umaa_official_battery_consumer_host \
      --module /path/to/generated/batteryclient.so \
      --domain-id 0

The Host normally runs until SIGINT/SIGTERM. Shutdown with a still-active
source must fail; the ABI cannot represent an invented Dispose.

For deterministic supervised testing, use:

    umaa_official_battery_consumer_host \
      --module /path/to/component.so \
      --until-report-disposes 2       --until-element-disposes 4       --timeout-ms 18000

This requires a finite deadline, actual DDS Report AND Collection Element
Dispose events on their respective keyed Topics, and zero active lists/
sources. Report Dispose does not prove that later Element tombstones
were observed. Success prints READY and STOPPED with actual
accepted, disposed, nativeReportDisposes and nativeElementDisposes counts.
Invalid numeric options, duplicate flags, missing modules, missing
genuine Dispose or rejected business callbacks fail nonzero.

## Test and scope

The official-battery-multicell-native-wire GitHub workflow downloads the
original SHA-256-pinned AUVSI IDL ZIP into a temporary Runner directory,
compiles genuine Cyclone DDS TypeSupport and runs independent two-source
Report/Cell publisher and dynamically loaded business MODULE tests.
Positive wire case requires two accepted and two disposed business sources
with two native Report and four native Element disposes. The negative
poisoned-checkpoint case allows one accepted/one disposed business source
even though the original wire contains both source dispositions.
No invalid checkpoint can be promoted to a business snapshot.

The on-wire CI MODULE is a separately authored test fixture implementing
the same exact host-local ABI. The source-verified CLI-generated business
DLL/SO is tested separately by SDK Linux/Windows CMake regression: a
single combined generator-to-on-wire test is still pending.

Only the EO §3.8.1 ordered TOPIC Presentation minimum is source-backed.
The Reader's RELIABLE / KEEP_LAST(16) policies are explicit demo choices;
full per-Component mandatory QoS, arbitrary Service binding and
cross-vendor production behavior remain unverified. This executable
does not supersede the synthetic-only generic umaa_component_host.


## Opt-in original-source binding in the common SDK Host entrypoint (PR #81)

The existing generic `sdk/tools/umaa_component_host.cpp` source now
also exposes an **explicitly compiled-in**, source-pinned original Battery
Consumer factory, without using the demo DDS plugin as a surrogate for
native UMAA TypeSupport. The ordinary installed `umaa_component_host`
does **not** enable this factory and rejects `--native-binding` before
module or DDS access.

### Authenticate and compile the optional Native variant

Use the public original AUVSI 6.0 IDL ZIP **only in a private build or
ephemeral CI runner**, verify its SHA-256 equals
`15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08`,
and generate the matching native Cyclone TypeSupport with
`tools/official_idl_compile.py`. See the
`official-battery-multicell-native-wire` workflow for the executable
download, SHA verification, extraction, generation, CMake and CTest steps.

~~~sh
cmake -S tests/integration/cyclone -B /tmp/official-native-host \
  -DUMAA_OFFICIAL_GENERATED_ROOT=/private/generated-original-cyclone \
  -DUMAA_OFFICIAL_IDL_ARCHIVE=/private/verified-umaa6-original.zip
cmake --build /tmp/official-native-host --target umaa_component_host_source_pinned

/tmp/official-native-host/umaa_component_host_source_pinned \
  --native-binding UMAA::EO::BatteryStatus/cyclone/original-6.0-host-preview-v1 \
  --module /path/to/verified-business-component.so \
  --source-idl-sha256 15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08 \
  --source-icd-sha256 47e88ecec73e37562ad04f5cf1ee5aef993828f6dc8440d453 \
  --until-report-disposes 2 --until-element-disposes 4 --timeout-ms 18000
~~~

The archive is checked by CMake itself before exposing the native
target; a corrupt or wrong archive SHA **fails configure**. CI also tests
the forged-archive negative branch. The caller must supply TypeSupport
*generated from that verified source*; merely pointing at a correctly
hashed ZIP beside unrelated generated files is not a complete build
provenance attestation. The ICD SHA here is a pinned record from the
reviewed source mapping, **not** a runtime hash of the ICD PDF.

The original Battery module is still a host-local ABI-v2 **Consumer**
with service `UMAA::EO::BatteryStatus` and schema
`umaa-kit:host-only-battery-stable-v1:NOT-UMAA-CDR`. Its business
Handler sees a completed StableList value, not DDS CDR or an official
wire-format ABI. Runtime validates this exact schema and Consumer actions
before creating a Participant. Actual Report/Cell keyed Dispose, list
cleanup and business callback errors must agree before success.

The separate original native publisher → optional common Host →
independent business DLL/SO test requires normal delivery
**accepted/disposed 2/2** and **Report/Cell Dispose 2/4**; a poisoned
second list is isolated while genuine DDS Dispose counts stay **2/4**.
Neither branch invents a Dispose for an intermediate invalid update.

**Scope remains deliberately narrow:** a single original Battery
Cyclone Reader factory exists only in the opt-in build. It reuses the
previously tested `OriginalBatteryStatusReader` and
`BatteryStatusComponentBridge` and the same SDK Host main, but is
**not** arbitrary vendor/plugin switching, a universal dynamic official
ServiceBinding factory, native GROUP coherence, complete mandatory
QoS, or UMAA conformance. The prior dedicated Battery Host is retained
only as an independent regression oracle; no production deployment or
automatic PR merge was performed.
