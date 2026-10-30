# Implementing a new UMAA DDS backend without changing application code

The SDK has **one versioned vendor-neutral C ABI** in
`sdk/include/umaa/internal/dds_backend.h` (`UmaaDdsBackendV1`).
A new DLL/SO implements `umaa_get_dds_backend_v1(UMAA_DDS_ABI_VERSION)`.
It must never modify user Component modules, `CommandEngine`,
`CollectionAssembler` or `Application` API.

## Steps

1. Add `dds/vendors/<new-vendor>/` and keep SDK libraries, generated
   middleware headers, vendor QoS objects, DDS entities, and official
   TypeSupport in this adapter only. Keep `UMAA::Core` and business modules
   vendor-header-free.
2. Implement all ABI entries: `open_domain/close_domain`,
   `open_writer/open_reader`, `write`, `dispose`,
   `unregister_instance`, `poll`, `wait_for_acks`,
   `close_writer`, `close_reader`. Do not throw C++ exceptions across
   the C boundary. Drain callbacks before releasing Reader handles.
3. Declare every supported Service binding explicitly:
   `(topic_name,schema_id,qos_profile,required_features)` maps to an
   exact reviewed TypeSupport and key contract. Unknown contract => error.
   Never infer official service names, Key, QoS, or TypeSupport from a
   synthetic fixture or unchecked C++ type alias. The reviewed 53-service,
   113-topic catalog is a literal allow-list, **not** a QoS approval.
4. Advertise `RELIABLE`, `INSTANCE_LIFECYCLE`, `COHERENT_GROUP`,
   `ORDERED_ACCESS` only after vendor-specific genuine tests prove them.
   Reject mandatory unsupported capabilities before creating any Reader or
   Writer. The current Cyclone/Fast synthetic plugins do **not** advertise
   GROUP/ORDERED despite being able to transport one fully-framed batch.
5. Preserve the actual Source/Instance key on DATA, DISPOSE, UNREGISTER.
   Unknown lifecycle keys must fail closed; in particular, never map an
   unknown handle to the only cached key. An ambiguous write result must
   permanently fault the publisher instead of automatically retrying a
   physical Command.
6. Write independent-process middleware real-wire acceptance tests for
   Report changes/Dispose, Command completion/Cancel/Update, Collection
   snapshot/delta/Dispose, and final reliable ACK. Run *both* vendor
   substitution (same app binaries) and mixed-vendor **network**
   interoperability separately. Include Linux GCC/Clang and Windows MSVC
   when your vendor supplies SDK/compiler support.

## Development targets and evidence

- `dds/vendors/cyclone/CMakeLists.txt` and `fastdds/CMakeLists.txt`
  implement synthetic Report, Command and Set/List single-sample
  Collection plugins with real vendor TypeSupport.
- `sdk/tests/real_dds_typed_command_server_cli.cpp`,
  `real_dds_typed_command_client_cli.cpp`,
  `real_dds_collection_cli.cpp` are plain SDK-facing acceptance binaries.
- `.github/workflows/sdk-dds-vendor-parity.yml` checks identical
  executable and Component binary on either vendor.
- `.github/workflows/sdk-dds-mixed-collection.yml` is an explicitly
  opt-in **strict failing** mixed-network trial (see
  `docs/SDK_DDS_MIXED_NETWORK_LIMITS.md`).
- `tools/verify_official_service_types.py`,
  `generate_reviewed_type_support.py`, and
  `generate_reviewed_service_api.py` help build officially sourced type
  declarations from an explicitly reviewed service manifest. They do not
  produce unreviewed normative QoS policies.

**RTI Connext integration is BLOCKED:** supply a licensed SDK, usable
`rtiddsgen` compiler, independent Linux/Windows Runner access, reviewed
official TypeSupport+Key/Policy mapping and published legal deployment
requirements. There is no RTI pass or fabricated placeholder adapter.

## ROS 2 RMW-style adapter contract (incremental UMAA Kit design)

The **sole canonical runtime backend** location is
`dds/vendors/<vendor>/`. `dds/cyclone/` is a separate, historical
independent real-wire verification harness, not a parallel SDK adapter.
Shared synthetic IDL input is in `dds/fixtures/idl/`.

1. **Register identity/status** in `backend.json`; the architecture test
   refuses undeclared vendor plugins, unknown family/source paths, forged
   normative evidence, and unsupported capability claims. A BLOCKED vendor
   must not provide a dummy runtime DLL.
2. **Implement exactly ABI v1** and the C entrypoint from
   `sdk/include/umaa/internal/dds_backend.h`. No C++ exception crosses
   the ABI; every function pointer must be present. Source and binary tests
   both verify version/struct size/entrypoint.
3. **Use shared contract filtering:** initial demo fixtures must use
   `dds/vendors/common/demo_type_binding_rules.hpp` for exact
   Topic+Schema+QoS+required-feature validation; TypeSupport, native key
   handling and Reader/Writer QoS remain adapter-private. Official services
   need a **new, separately reviewed** generated binding registry from
   original signed-off ICD/IDL, not a synthetically extended demo allow-list.
4. **Use the same behavioral test contract** for both vendors: Report DATA,
   UPDATE, DISPOSE; Command/ACK/Status, UPDATE, CANCEL; Set/List snapshot and
   delta; unknown schema, unsupported QoS, invalid instance keys and ambiguous
   delivery. Native coherent/ordered collections must be independently
   proven before advertising their flags.
5. **Single logical plugin is the target**, not the present implementation:
   the existing prototype builds separate synthetic Report, Command and
   Collection modules *per vendor*. Native TypeSupport registration and
   multi-service lifecycle must be designed and tested before consolidating
   into one runtime-selectable backend module.

Test the source-tree contract with
`python -m unittest sdk.tests.test_dds_vendor_adapter_layout` and the
C++ ABI-independent allow-list with CTest
`umaa_vendor_type_binding_rules_test`. **Do not** substitute these static
tests for standalone middleware wire tests.

See [DDS root layout](../README.md).
