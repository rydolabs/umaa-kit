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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_link_libraries(umaa_sdk_cyclone INTERFACE UMAA::Core CycloneDDS::ddsc)
  install(TARGETS umaa_sdk_cyclone EXPORT UMAASDKTargets)
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  set(UMAA_CYCLONE_FIND_DEPENDENCY
    "include(CMakeFindDependencyMacro)\nfind_dependency(CycloneDDS CONFIG REQUIRED)")
endif()
option(UMAA_BUILD_TESTS "Build standalone core tests" ON)
if(UMAA_BUILD_TESTS)
  enable_testing()
  add_executable(umaa_reviewed_service_registry_test tests/test_reviewed_runtime_registry.cpp)
  target_link_libraries(umaa_reviewed_service_registry_test PRIVATE UMAA::Core)
  add_test(NAME umaa_reviewed_service_registry_test COMMAND umaa_reviewed_service_registry_test)
  add_executable(umaa_cyclone_service_context_test tests/test_cyclone_service_context.cpp)
  target_link_libraries(umaa_cyclone_service_context_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_service_context_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include")
  add_test(NAME umaa_cyclone_service_context_test COMMAND umaa_cyclone_service_context_test)
  add_executable(umaa_command_engine_test tests/test_engine.cpp)
  target_link_libraries(umaa_command_engine_test PRIVATE UMAA::Core)
  add_test(NAME umaa_command_engine_test COMMAND umaa_command_engine_test)
  # Compile CycloneDDS-specific adapter against an explicit *test-only* ABI
  # shim so failure semantics are testable even where the vendor is absent.
  add_executable(umaa_cyclone_command_adapter_test tests/test_cyclone_command_adapter.cpp)
  target_link_libraries(umaa_cyclone_command_adapter_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_command_adapter_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include")
  add_test(NAME umaa_cyclone_command_adapter_test COMMAND umaa_cyclone_command_adapter_test)
  add_executable(umaa_cyclone_report_writer_test tests/test_cyclone_report_writer.cpp)
  target_link_libraries(umaa_cyclone_report_writer_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_report_writer_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include")
  add_test(NAME umaa_cyclone_report_writer_test COMMAND umaa_cyclone_report_writer_test)
  add_executable(umaa_cyclone_report_reader_test tests/test_cyclone_report_reader.cpp)
  target_link_libraries(umaa_cyclone_report_reader_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_report_reader_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include")
  add_test(NAME umaa_cyclone_report_reader_test COMMAND umaa_cyclone_report_reader_test)
  # Independent Command-topic reader fake-ABI tests. The official EngineControl
  # host separately exercises this adapter with real CycloneDDS/UMAA TypeSupport.
  add_executable(umaa_cyclone_command_reader_test tests/test_cyclone_command_reader.cpp)
  target_link_libraries(umaa_cyclone_command_reader_test PRIVATE UMAA::Core)
  target_include_directories(umaa_cyclone_command_reader_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake_dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/../dds/cyclone/include")
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
