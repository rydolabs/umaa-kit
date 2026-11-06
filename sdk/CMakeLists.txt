cmake_minimum_required(VERSION 3.20)
project(UMAAPrototype VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
add_library(umaa_sdk_core INTERFACE)
add_library(UMAA::Core ALIAS umaa_sdk_core)
target_compile_features(umaa_sdk_core INTERFACE cxx_std_17)
target_include_directories(umaa_sdk_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(umaa_sdk_core PROPERTIES EXPORT_NAME Core)
# Optional vendor binding: normal Core consumers do not need CycloneDDS.
# Enables a separately exported UMAA::Cyclone target for SDK integrators.
option(UMAA_ENABLE_CYCLONE_ADAPTER "Export the CycloneDDS Command writer binding" OFF)
set(UMAA_CYCLONE_FIND_DEPENDENCY "")
if(UMAA_ENABLE_CYCLONE_ADAPTER)
  find_package(CycloneDDS CONFIG REQUIRED)
  add_library(umaa_sdk_cyclone INTERFACE)
  add_library(UMAA::Cyclone ALIAS umaa_sdk_cyclone)
  set_target_properties(umaa_sdk_cyclone PROPERTIES EXPORT_NAME Cyclone)
  target_compile_features(umaa_sdk_cyclone INTERFACE cxx_std_17)
  target_include_directories(umaa_sdk_cyclone INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_link_libraries(umaa_sdk_cyclone INTERFACE UMAA::Core CycloneDDS::ddsc)
  install(TARGETS umaa_sdk_cyclone EXPORT UMAASDKTargets)
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  set(UMAA_CYCLONE_FIND_DEPENDENCY
    "include(CMakeFindDependencyMacro)\nfind_dependency(CycloneDDS CONFIG REQUIRED)")
endif()
option(UMAA_BUILD_TESTS "Build standalone core tests" ON)
if(UMAA_BUILD_TESTS)
  enable_testing()
  # Vendor-free ABI-v2 Report Consumer, bidirectional service fusion,
  # malformed payload and user Handler fault guards (synthetic only).
  add_executable(umaa_component_report_sink_test tests/test_component_report_sink.cpp)
  target_link_libraries(umaa_component_report_sink_test PRIVATE UMAA::Core)
  add_test(NAME umaa_component_report_sink_test COMMAND umaa_component_report_sink_test)
  # Vendor-free host-local Battery ABI projection byte-level negative oracle.
  # Original TypeSupport and real DDS are covered in separate native CI.
  add_executable(umaa_official_battery_component_codec_test
    tests/test_official_battery_component_codec.cpp)
  target_link_libraries(umaa_official_battery_component_codec_test PRIVATE
    UMAA::Core ${CMAKE_DL_LIBS})
  add_test(NAME umaa_official_battery_component_codec_test
    COMMAND umaa_official_battery_component_codec_test)
  # Real ABI-v2 dynamically loaded business module: no DDS vendor APIs.
  # A fully disposed Reader epoch may be rotated explicitly, but incomplete
  # Host state and stale callbacks must fail closed.
  add_library(umaa_battery_epoch_component MODULE tests/modules/battery_component_epoch.cpp)
  target_link_libraries(umaa_battery_epoch_component PRIVATE UMAA::Core)
  add_executable(umaa_official_battery_component_epoch_test
    tests/test_official_battery_component_epoch.cpp)
  target_link_libraries(umaa_official_battery_component_epoch_test PRIVATE
    UMAA::Core ${CMAKE_DL_LIBS})
  add_test(NAME umaa_official_battery_component_epoch_test
    COMMAND umaa_official_battery_component_epoch_test
      "$<TARGET_FILE:umaa_battery_epoch_component>")
  add_executable(umaa_qos_policy_test tests/test_qos_policy.cpp)
  target_link_libraries(umaa_qos_policy_test PRIVATE UMAA::Core)
  add_test(NAME umaa_qos_policy_test COMMAND umaa_qos_policy_test)
  add_executable(umaa_reviewed_service_registry_test tests/test_reviewed_runtime_registry.cpp)
  target_link_libraries(umaa_reviewed_service_registry_test PRIVATE UMAA::Core)
  add_test(NAME umaa_reviewed_service_registry_test COMMAND umaa_reviewed_service_registry_test)
  add_executable(umaa_cyclone_service_context_test tests/test_cyclone_service_context.cpp)
  target_link_libraries(umaa_cyclone_service_context_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_service_context_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include")
  add_test(NAME umaa_cyclone_service_context_test COMMAND umaa_cyclone_service_context_test)
  add_executable(umaa_command_identity_routing_test tests/test_command_identity_routing.cpp)
  target_link_libraries(umaa_command_identity_routing_test PRIVATE UMAA::Core)
  add_test(NAME umaa_command_identity_routing_test COMMAND umaa_command_identity_routing_test)
  add_executable(umaa_command_engine_test tests/test_engine.cpp)
  target_link_libraries(umaa_command_engine_test PRIVATE UMAA::Core)
  add_test(NAME umaa_command_engine_test COMMAND umaa_command_engine_test)
  # Compile CycloneDDS-specific adapter against an explicit *test-only* ABI
  # shim so failure semantics are testable even where the vendor is absent.
  add_executable(umaa_cyclone_command_adapter_test tests/test_cyclone_command_adapter.cpp)
  target_link_libraries(umaa_cyclone_command_adapter_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_command_adapter_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include")
  add_test(NAME umaa_cyclone_command_adapter_test COMMAND umaa_cyclone_command_adapter_test)
  add_executable(umaa_cyclone_report_writer_test tests/test_cyclone_report_writer.cpp)
  target_link_libraries(umaa_cyclone_report_writer_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_report_writer_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include")
  add_test(NAME umaa_cyclone_report_writer_test COMMAND umaa_cyclone_report_writer_test)
  add_executable(umaa_cyclone_report_reader_test tests/test_cyclone_report_reader.cpp)
  target_link_libraries(umaa_cyclone_report_reader_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_report_reader_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include")
  add_test(NAME umaa_cyclone_report_reader_test COMMAND umaa_cyclone_report_reader_test)
  # Independent Command-topic reader fake-ABI tests. The official EngineControl
  # host separately exercises this adapter with real CycloneDDS/UMAA TypeSupport.
  add_executable(umaa_cyclone_command_reader_test tests/test_cyclone_command_reader.cpp)
  target_link_libraries(umaa_cyclone_command_reader_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_command_reader_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds-backends/cyclone/include")
  add_test(NAME umaa_cyclone_command_reader_test COMMAND umaa_cyclone_command_reader_test)
  add_executable(umaa_command_deadline_test tests/test_command_deadline.cpp)
  target_link_libraries(umaa_command_deadline_test PRIVATE UMAA::Core)
  add_test(NAME umaa_command_deadline_test COMMAND umaa_command_deadline_test)
  add_executable(umaa_remote_command_monitor_test tests/test_remote_command_monitor.cpp)
  target_link_libraries(umaa_remote_command_monitor_test PRIVATE UMAA::Core)
  add_test(NAME umaa_remote_command_monitor_test COMMAND umaa_remote_command_monitor_test)
  add_executable(umaa_command_transport_test tests/test_command_transport.cpp)
  target_link_libraries(umaa_command_transport_test PRIVATE UMAA::Core)
  add_test(NAME umaa_command_transport_test COMMAND umaa_command_transport_test)
  add_executable(umaa_coherent_collection_receiver_test tests/test_coherent_collection_receiver.cpp)
  target_link_libraries(umaa_coherent_collection_receiver_test PRIVATE UMAA::Core)
  add_test(NAME umaa_coherent_collection_receiver_test COMMAND umaa_coherent_collection_receiver_test)
  add_executable(umaa_report_collection_test tests/test_report_collection.cpp)
  target_link_libraries(umaa_report_collection_test PRIVATE UMAA::Core)
  add_test(NAME umaa_report_collection_test COMMAND umaa_report_collection_test)
  add_executable(umaa_report_lifecycle_test tests/test_report_lifecycle.cpp)
  target_link_libraries(umaa_report_lifecycle_test PRIVATE UMAA::Core)
  add_test(NAME umaa_report_lifecycle_test COMMAND umaa_report_lifecycle_test)
  add_executable(umaa_report_transport_test tests/test_report_transport.cpp)
  target_link_libraries(umaa_report_transport_test PRIVATE UMAA::Core)
  add_test(NAME umaa_report_transport_test COMMAND umaa_report_transport_test)
endif()
install(TARGETS umaa_sdk_core EXPORT UMAASDKTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT UMAASDKTargets FILE UMAASDKTargets.cmake NAMESPACE UMAA::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UMAAPrototype)
configure_package_config_file(cmake/UMAAPrototypeConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/UMAAPrototypeConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UMAAPrototype)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/UMAAPrototypeConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/UMAAPrototypeConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/UMAAPrototypeConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UMAAPrototype)

# Prototype native service modules: SDK keeps UMAA wire behavior outside modules.
option(UMAA_BUILD_MODULE_DEMO "Build cross-platform dynamic service module contract tests" ON)
if(UMAA_BUILD_TESTS AND UMAA_BUILD_MODULE_DEMO)
  add_library(umaa_demo_engine MODULE tests/modules/engine.cpp)
  add_library(umaa_demo_battery MODULE tests/modules/battery.cpp)
  add_library(umaa_demo_wrong_abi MODULE tests/modules/wrong_abi.cpp)
  target_link_libraries(umaa_demo_engine PRIVATE UMAA::Core)
  target_link_libraries(umaa_demo_battery PRIVATE UMAA::Core)
  target_link_libraries(umaa_demo_wrong_abi PRIVATE UMAA::Core)
  add_executable(umaa_dynamic_module_test tests/test_dynamic_module.cpp)
  target_link_libraries(umaa_dynamic_module_test PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
  add_test(NAME umaa_dynamic_module_test COMMAND umaa_dynamic_module_test
    "$<TARGET_FILE:umaa_demo_engine>" "$<TARGET_FILE:umaa_demo_battery>"
    "$<TARGET_FILE:umaa_demo_wrong_abi>")
endif()

# Fixture-only generated code; separate from official UMAA TypeSupport.
if(UMAA_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(UMAA_TEST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  add_custom_command(
    OUTPUT "${UMAA_TEST_GENERATED_DIR}/include/umaa/generated/EngineControl.hpp"
           "${UMAA_TEST_GENERATED_DIR}/include/umaa/generated/BatteryStatus.hpp"
           "${UMAA_TEST_GENERATED_DIR}/include/umaa/generated/types.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/umaa_codegen.py"
       generate --idl "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.idl"
       --catalog "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/services.json"
       --service Demo::EO::EngineControl --out "${UMAA_TEST_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/umaa_codegen.py"
       generate --idl "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.idl"
       --catalog "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/services.json"
       --service Demo::EO::BatteryStatus --out "${UMAA_TEST_GENERATED_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/umaa_codegen.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/demo.idl"
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/services.json"
    VERBATIM)
  add_custom_target(umaa_fixture_codegen DEPENDS
     "${UMAA_TEST_GENERATED_DIR}/include/umaa/generated/EngineControl.hpp"
     "${UMAA_TEST_GENERATED_DIR}/include/umaa/generated/BatteryStatus.hpp")
  add_executable(umaa_mock_engine_host examples/mock_engine_host.cpp)
  add_executable(umaa_mock_report_host examples/mock_report_host.cpp)
  foreach(t umaa_mock_engine_host umaa_mock_report_host)
    add_dependencies(${t} umaa_fixture_codegen)
    target_link_libraries(${t} PRIVATE UMAA::Core)
    target_include_directories(${t} PRIVATE "${UMAA_TEST_GENERATED_DIR}/include")
  endforeach()
  add_test(NAME umaa_generated_report_smoke COMMAND umaa_mock_report_host)
endif()

# Device component ABI v2 and DDS-vendor plugin ABI contract tests.
# Mock backend is deliberately NOT a real DDS middleware implementation.
if(UMAA_BUILD_TESTS)
  include(cmake/module_v2_tests.cmake)
endif()

# Shared, vendor-neutral host orchestrator: a single runtime loaded by host.
# The user module's C++ business code does not link any DDS vendor library.
add_library(umaa_sdk_runtime SHARED src/component_runtime.cpp)
add_library(UMAA::Runtime ALIAS umaa_sdk_runtime)
set_target_properties(umaa_sdk_runtime PROPERTIES EXPORT_NAME Runtime)
target_compile_definitions(umaa_sdk_runtime PRIVATE UMAA_RUNTIME_BUILD)
find_package(Threads REQUIRED)
target_link_libraries(umaa_sdk_runtime PUBLIC UMAA::Core PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
target_compile_features(umaa_sdk_runtime PUBLIC cxx_std_17)
# Installable SDK-owned Host process for the explicit synthetic Report
# runtime path. Official per-service DDS bindings are not generated here.
add_executable(umaa_component_host tools/umaa_component_host.cpp)
target_link_libraries(umaa_component_host PRIVATE UMAA::Runtime)
target_compile_features(umaa_component_host PRIVATE cxx_std_17)
install(TARGETS umaa_component_host RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS umaa_sdk_runtime EXPORT UMAASDKTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(UMAA_BUILD_TESTS)
  add_executable(umaa_runtime_v2_test tests/test_runtime_v2.cpp)
  target_link_libraries(umaa_runtime_v2_test PRIVATE UMAA::Runtime)
  add_test(NAME umaa_runtime_v2_test COMMAND umaa_runtime_v2_test
    "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")
endif()

# Build a separate deployable Worker without fault injection. This is an
# optional development process-isolation executable, not an authenticated
# hardened production RPC. Tests build a second fault-injection-only binary.
option(UMAA_BUILD_PROCESS_WORKER "Build and install isolated Component worker" ON)
if(UMAA_BUILD_PROCESS_WORKER)
  add_executable(umaa_component_worker tools/umaa_component_worker.cpp)
  target_link_libraries(umaa_component_worker PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
  target_compile_features(umaa_component_worker PRIVATE cxx_std_17)
  install(TARGETS umaa_component_worker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/umaa_process_supervisor.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/umaa_operation_journal.py"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/umaa/tools")
endif()

# Optional child-process Component isolation experiment. Device DLL/SO
# loads ONLY inside the subprocess; supervisor never auto-replays Commands.
if(UMAA_BUILD_TESTS AND UMAA_BUILD_MODULE_DEMO)
  add_executable(umaa_component_ipc_worker tools/umaa_component_worker.cpp)
  target_link_libraries(umaa_component_ipc_worker PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
  target_compile_definitions(umaa_component_ipc_worker PRIVATE UMAA_WORKER_TEST_FAULT_INJECTION)
  add_test(NAME umaa_component_process_supervisor_test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_process_supervisor.py"
      "$<TARGET_FILE:umaa_component_ipc_worker>"
      "$<TARGET_FILE:umaa_demo_vehicle_v2>")
  set_tests_properties(umaa_component_process_supervisor_test PROPERTIES TIMEOUT 35)
endif()

# SDK LargeCollection atomic batch sender+receiver unit validation.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_collection_batch_builder_test tests/test_collection_batch_builder.cpp)
  target_link_libraries(umaa_collection_batch_builder_test PRIVATE UMAA::Core)
  add_test(NAME umaa_collection_batch_builder_test COMMAND umaa_collection_batch_builder_test)
endif()

# Vendor-independent bounded binary Collection envelope and corruption tests.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_collection_frame_codec_test tests/test_collection_frame_codec.cpp)
  target_link_libraries(umaa_collection_frame_codec_test PRIVATE UMAA::Core)
  add_test(NAME umaa_collection_frame_codec_test COMMAND umaa_collection_frame_codec_test)
endif()

# SDK Core API v1: contract identity/correlation and adapters around existing engines.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_sdk_v1_core_test tests/test_sdk_v1_core.cpp)
  target_link_libraries(umaa_sdk_v1_core_test PRIVATE UMAA::Core)
  add_test(NAME umaa_sdk_v1_core_test COMMAND umaa_sdk_v1_core_test)
  add_executable(umaa_sdk_v1_bridge_test tests/test_sdk_v1_bridge.cpp)
  target_link_libraries(umaa_sdk_v1_bridge_test PRIVATE UMAA::Core)
  add_test(NAME umaa_sdk_v1_bridge_test COMMAND umaa_sdk_v1_bridge_test)
endif()

# UMAA EO Flow Control protocol tests: state/reason, peer liveliness and cleanup.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_flow_protocol_v1_test tests/test_flow_protocol_v1.cpp)
  target_link_libraries(umaa_flow_protocol_v1_test PRIVATE UMAA::Core)
  add_test(NAME umaa_flow_protocol_v1_test COMMAND umaa_flow_protocol_v1_test)
endif()

# SDK v1 Large Collection + Generalization/Specialization composition.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_sdk_v1_specialized_collection_test
    tests/test_sdk_v1_specialized_collection.cpp)
  target_link_libraries(umaa_sdk_v1_specialized_collection_test PRIVATE UMAA::Core)
  add_test(NAME umaa_sdk_v1_specialized_collection_test
    COMMAND umaa_sdk_v1_specialized_collection_test)
endif()

# Two-Topic Collection commit fence correctness and recovery.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_split_collection_joiner_test
    tests/test_split_collection_joiner.cpp)
  target_include_directories(umaa_split_collection_joiner_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(umaa_split_collection_joiner_test PRIVATE UMAA::Core)
  add_test(NAME umaa_split_collection_joiner_test
    COMMAND umaa_split_collection_joiner_test)
endif()

# 128-bit synthetic generalization/specialization binary contract.
if(UMAA_BUILD_TESTS)
  add_executable(umaa_specialization_packet_test
    tests/test_specialization_packet.cpp)
  target_include_directories(umaa_specialization_packet_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(umaa_specialization_packet_test PRIVATE UMAA::Core)
  add_test(NAME umaa_specialization_packet_test COMMAND umaa_specialization_packet_test)
endif()

# Executable Host process smoke with native dynamically-loaded Component and
# mock DDS Backend. Does not establish actual DDS wire interoperability.
if(UMAA_BUILD_TESTS AND UMAA_BUILD_MODULE_DEMO)
  add_test(NAME umaa_component_host_v1_smoke
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_component_host_cli.py"
      "$<TARGET_FILE:umaa_component_host>"
      "$<TARGET_FILE:umaa_demo_vehicle_v2>"
      "$<TARGET_FILE:umaa_mock_dds_backend>")
  set_tests_properties(umaa_component_host_v1_smoke PROPERTIES TIMEOUT 40)
endif()


# End-to-end generator contract: create Component, compile genuine CMake
# DLL/SO, reject unimplemented Handler, change only business read(), then
# load same generated module in the executable Host process.
if(UMAA_BUILD_TESTS AND UMAA_BUILD_MODULE_DEMO)
  add_test(NAME umaa_generated_component_host_v1_e2e
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generated_component_host.py"
      "$<TARGET_FILE:umaa_component_host>"
      "$<TARGET_FILE:umaa_mock_dds_backend>"
      "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(umaa_generated_component_host_v1_e2e PROPERTIES TIMEOUT 240)
endif()
