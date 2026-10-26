cmake_minimum_required(VERSION 3.20)
project(UMAARealDDSProof LANGUAGES C CXX)
find_package(CycloneDDS REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)
idlc_generate(TARGET demo_battery_types FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/DemoBattery.idl" WARNINGS no-implicit-extensibility)
add_library(umaa_dds_battery_module MODULE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/tests/modules/battery.cpp")
target_include_directories(umaa_dds_battery_module PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
add_executable(umaa_demo_dds_publisher src/publisher.cpp)
target_include_directories(umaa_demo_dds_publisher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
target_link_libraries(umaa_demo_dds_publisher PRIVATE demo_battery_types CycloneDDS::ddsc ${CMAKE_DL_LIBS})
add_executable(umaa_demo_dds_simulator ${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/subscriber.c)
target_link_libraries(umaa_demo_dds_simulator PRIVATE demo_battery_types CycloneDDS::ddsc)
add_test(NAME real_dds_battery_report_dispose
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
          "$<TARGET_FILE:umaa_demo_dds_publisher>" "$<TARGET_FILE:umaa_demo_dds_simulator>"
          "$<TARGET_FILE:umaa_dds_battery_module>")
set_tests_properties(real_dds_battery_report_dispose PROPERTIES TIMEOUT 35)


# Generate official UMAA TypeSupport from AUVSI input only in CI or a user-supplied build tree.
if(DEFINED UMAA_OFFICIAL_GENERATED_ROOT)
  file(TO_CMAKE_PATH "${UMAA_OFFICIAL_GENERATED_ROOT}" OFFICIAL_ROOT)
  file(GLOB_RECURSE OFFICIAL_TYPES CONFIGURE_DEPENDS "${OFFICIAL_ROOT}/UMAA/*.c")
  if(NOT OFFICIAL_TYPES)
    message(FATAL_ERROR "Official TypeSupport missing; invoke tools/official_idl_compile.py first")
  endif()
  add_library(umaa_official_type_support STATIC ${OFFICIAL_TYPES})
  target_include_directories(umaa_official_type_support PUBLIC "${OFFICIAL_ROOT}")
  target_link_libraries(umaa_official_type_support PUBLIC CycloneDDS::ddsc)
  add_executable(umaa_official_battery_publisher src/official_battery_publisher.cpp)
  target_compile_features(umaa_official_battery_publisher PRIVATE cxx_std_17)
  target_include_directories(umaa_official_battery_publisher PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(umaa_official_battery_publisher PRIVATE umaa_official_type_support ${CMAKE_DL_LIBS})
  add_executable(umaa_official_battery_simulator
      "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_battery_subscriber.c")
  target_link_libraries(umaa_official_battery_simulator PRIVATE umaa_official_type_support)
  add_test(NAME official_umaa_battery_dds_wire
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
            "$<TARGET_FILE:umaa_official_battery_publisher>"
            "$<TARGET_FILE:umaa_official_battery_simulator>"
            "$<TARGET_FILE:umaa_dds_battery_module>")
  set_tests_properties(official_umaa_battery_dds_wire PROPERTIES TIMEOUT 35)
  # SDK-side C++ subscriber bound to the same official TypeSupport. This is
  # an additional adapter smoke, NOT the independent C simulator above.
  add_executable(umaa_official_battery_sdk_consumer
      src/official_battery_sdk_consumer.cpp)
  target_compile_features(umaa_official_battery_sdk_consumer PRIVATE cxx_std_17)
  target_include_directories(umaa_official_battery_sdk_consumer PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(umaa_official_battery_sdk_consumer PRIVATE umaa_official_type_support)
  add_test(NAME official_umaa_battery_sdk_reader_wire
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
            "$<TARGET_FILE:umaa_official_battery_publisher>"
            "$<TARGET_FILE:umaa_official_battery_sdk_consumer>"
            "$<TARGET_FILE:umaa_dds_battery_module>")
  set_tests_properties(official_umaa_battery_sdk_reader_wire PROPERTIES TIMEOUT 35)

endif()


# EngineControl official AUVSI 6.0 Command/Status/Ack TypeSupport from external IDL.
# SDK Host owns protocol state management and user module dispatch.
if(DEFINED UMAA_ENGINE_GENERATED_ROOT)
  file(TO_CMAKE_PATH "${UMAA_ENGINE_GENERATED_ROOT}" ENGINE_ROOT)
  file(GLOB_RECURSE ENGINE_TYPES CONFIGURE_DEPENDS "${ENGINE_ROOT}/UMAA/*.c")
  if(NOT ENGINE_TYPES)
    message(FATAL_ERROR "Official EngineControl TypeSupport missing")
  endif()
  add_library(umaa_engine_type_support STATIC ${ENGINE_TYPES})
  target_include_directories(umaa_engine_type_support PUBLIC "${ENGINE_ROOT}")
  target_link_libraries(umaa_engine_type_support PUBLIC CycloneDDS::ddsc)
  set(ENGINE_PLUGIN_OUT "${CMAKE_CURRENT_BINARY_DIR}/generated-engine-plugin")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/generate_official_engine_plugin.py"
            --type-root "${ENGINE_ROOT}" --output "${ENGINE_PLUGIN_OUT}"
    RESULT_VARIABLE ENGINE_PLUGIN_GEN_RESULT)
  if(NOT ENGINE_PLUGIN_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "Official EngineControl Handler/plugin code generation failed")
  endif()
  add_library(umaa_dds_engine_module MODULE
      "${ENGINE_PLUGIN_OUT}/engine_control_module.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/examples/engine_control_user_handler.cpp")
  target_include_directories(umaa_dds_engine_module PRIVATE
      "${ENGINE_PLUGIN_OUT}" "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
  target_compile_features(umaa_dds_engine_module PRIVATE cxx_std_17)
  add_executable(umaa_official_engine_host src/official_engine_host.cpp)
  target_compile_features(umaa_official_engine_host PRIVATE cxx_std_17)
  target_include_directories(umaa_official_engine_host PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(umaa_official_engine_host PRIVATE
      umaa_engine_type_support ${CMAKE_DL_LIBS})
  add_executable(umaa_official_engine_simulator
      "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_engine_simulator.c")
  target_link_libraries(umaa_official_engine_simulator PRIVATE umaa_engine_type_support)
  add_test(NAME official_umaa_engine_command_flow
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
            "$<TARGET_FILE:umaa_official_engine_host>"
            "$<TARGET_FILE:umaa_official_engine_simulator>"
            "$<TARGET_FILE:umaa_dds_engine_module>")
  set_tests_properties(official_umaa_engine_command_flow PROPERTIES TIMEOUT 40)
  add_test(NAME official_umaa_engine_update_flow
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
            "$<TARGET_FILE:umaa_official_engine_host>"
            "$<TARGET_FILE:umaa_official_engine_simulator>"
            "$<TARGET_FILE:umaa_dds_engine_module>" --update)
  set_tests_properties(official_umaa_engine_update_flow PROPERTIES TIMEOUT 40)
  # Independent negative on-wire input: wrong provider destination must
  # fail closed and yield no CommandStatus/Ack publications.
  add_test(NAME official_umaa_engine_bad_destination_rejected
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run.py"
            "$<TARGET_FILE:umaa_official_engine_host>"
            "$<TARGET_FILE:umaa_official_engine_simulator>"
            "$<TARGET_FILE:umaa_dds_engine_module>" --bad-destination)
  set_tests_properties(official_umaa_engine_bad_destination_rejected PROPERTIES TIMEOUT 40)
endif()
