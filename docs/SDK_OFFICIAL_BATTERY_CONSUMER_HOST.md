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


## Source-gated CLI-generated Component to original DDS E2E (candidate, 2026-11-07)

The independent on-wire module above was a separately authored business fixture.
The new integration workflow `.github/workflows/generated-battery-component-native-e2e.yml`
adds a different end-to-end proof with an actual Component created by
`umaa_component_cli new/service add/generate --battery-host-preview`.

- Fetch and independently SHA-256-verify **both** the original EO ICD 6.0
  PDF and the original 594-file-with-EXP IDL archive in the ephemeral CI Runner.
  No source PDF, ZIP, extracted TypeSupport or generated business DLL/SO is
  committed or uploaded.
- Generate a pinned BatteryStatus Consumer `service_binding.hpp`,
  user-owned C++ Handler and ABI-v2 MODULE/CMake from the real inputs; install
  the SDK and compile that generated MODULE as a real native Linux `.so`.
- First run the **unchanged default rejecting Handler** against genuine native
  DDS, and require the Host to fail with a *specific business-event refusal*.
  Neither a publisher timeout nor a Host crash alone is an acceptable pass.
- Replace only the **temporary generated project's user-owned Handler** with
  the reviewed two-source business test fixture. Regenerate and prove that
  the generator cannot overwrite it; rebuild and run its ABI smoke test.
- Execute genuine two-source Report/Cell DDS **positive and poisoned-checkpoint
  negative** runs through the new CLI-generated MODULE using the existing
  strict independent publisher/Host/supervisor. Both require genuine Report
  and Element Dispose counts, correct stable snapshots and no fake business
  Dispose for rejected/uncommitted checkpoints.

**CI status must be checked on the final feature commit before calling this a
PASS.** Even after green CI, this is source-verified **BatteryStatus-only**
host-preview integration, not approved effective Component QoS, a generic
service Runtime, cross-vendor native parity or normative UMAA conformance.
