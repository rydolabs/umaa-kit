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

# Two additional reviewed official services: generated from pinned original
# AUVSI IDL in CI, not redistributed. Tests prove independent DDS discovery
# for a Report Provider and Command Consumer, not payload semantics or QoS.
if(DEFINED UMAA_PROBE_GENERATED_ROOT)
  file(TO_CMAKE_PATH "${UMAA_PROBE_GENERATED_ROOT}" PROBE_ROOT)
  file(GLOB_RECURSE PROBE_TYPES CONFIGURE_DEPENDS "${PROBE_ROOT}/UMAA/*.c")
  if(NOT PROBE_TYPES)
    message(FATAL_ERROR "Official TypeSupport for reviewed service probes is missing")
  endif()
  add_library(umaa_probe_type_support STATIC ${PROBE_TYPES})
  target_include_directories(umaa_probe_type_support PUBLIC "${PROBE_ROOT}")
  target_link_libraries(umaa_probe_type_support PUBLIC CycloneDDS::ddsc)
  # Actual AnchorControl virtual-device Command lifecycle, separate from
  # the historical default-initialized discovery/type-only probe.
  add_executable(umaa_anchor_control_lifecycle_host src/official_anchor_control_host.cpp)
  target_compile_features(umaa_anchor_control_lifecycle_host PRIVATE cxx_std_17)
  target_include_directories(umaa_anchor_control_lifecycle_host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
  target_link_libraries(umaa_anchor_control_lifecycle_host PRIVATE umaa_probe_type_support)
  add_executable(umaa_anchor_control_lifecycle_oracle
    "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_anchor_control_oracle.c")
  target_link_libraries(umaa_anchor_control_lifecycle_oracle PRIVATE umaa_probe_type_support)
  foreach(anchor_case IN ITEMS normal update stop bad-destination)
    if(anchor_case STREQUAL "normal")
      set(anchor_flag "")
    else()
      set(anchor_flag "--${anchor_case}")
    endif()
    add_test(NAME official_umaa_anchor_control_${anchor_case}
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_anchor_control.py"
        "$<TARGET_FILE:umaa_anchor_control_lifecycle_host>"
        "$<TARGET_FILE:umaa_anchor_control_lifecycle_oracle>" ${anchor_flag})
    set_tests_properties(official_umaa_anchor_control_${anchor_case} PROPERTIES TIMEOUT 55)
  endforeach()
  foreach(probe IN ITEMS ANCHOR_SPECS_REPORT ANCHOR_CONTROL_COMMAND)
    string(TOLOWER "${probe}" probe_id)
    add_executable(umaa_${probe_id}_host src/official_reviewed_discovery_host.cpp)
    target_compile_definitions(umaa_${probe_id}_host PRIVATE UMAA_${probe})
    target_compile_features(umaa_${probe_id}_host PRIVATE cxx_std_17)
    target_include_directories(umaa_${probe_id}_host PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
    target_link_libraries(umaa_${probe_id}_host PRIVATE umaa_probe_type_support)
    add_executable(umaa_${probe_id}_oracle
      "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_reviewed_discovery_oracle.c")
    target_compile_definitions(umaa_${probe_id}_oracle PRIVATE UMAA_${probe})
    target_link_libraries(umaa_${probe_id}_oracle PRIVATE umaa_probe_type_support)
    add_test(NAME official_umaa_${probe_id}_discovery
      COMMAND "${Python3_EXECUTABLE}"
              "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_reviewed_discovery.py"
              "$<TARGET_FILE:umaa_${probe_id}_host>"
              "$<TARGET_FILE:umaa_${probe_id}_oracle>")
    set_tests_properties(official_umaa_${probe_id}_discovery PROPERTIES TIMEOUT 40)
    add_test(NAME official_umaa_${probe_id}_default_wire
      COMMAND "${Python3_EXECUTABLE}"
              "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_reviewed_discovery.py"
              "$<TARGET_FILE:umaa_${probe_id}_host>"
              "$<TARGET_FILE:umaa_${probe_id}_oracle>" --wire)
    set_tests_properties(official_umaa_${probe_id}_default_wire PROPERTIES TIMEOUT 40)
    if(probe STREQUAL "ANCHOR_SPECS_REPORT")
      add_test(NAME official_umaa_anchor_specs_meaningful_report
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_reviewed_discovery.py"
                "$<TARGET_FILE:umaa_${probe_id}_host>"
                "$<TARGET_FILE:umaa_${probe_id}_oracle>" --semantic)
      set_tests_properties(official_umaa_anchor_specs_meaningful_report PROPERTIES TIMEOUT 40)
    endif()
  endforeach()
endif()

# Official AnchorSpecs *semantic* Report (2 distinct payloads and keyed Dispose)
# and AnchorControl protocol (LOWER/STOP Update/Cancel/Ack/Status/Dispose).
# These tests use the separately generated official AUVSI 6.0 TypeSupport;
# reviewed per-service normative QoS and device hardware are not claimed.
if(DEFINED UMAA_PROBE_GENERATED_ROOT)
  add_executable(umaa_anchor_specs_payload_host src/official_anchor_specs_host.cpp)
  target_compile_features(umaa_anchor_specs_payload_host PRIVATE cxx_std_17)
  target_include_directories(umaa_anchor_specs_payload_host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
  target_link_libraries(umaa_anchor_specs_payload_host PRIVATE umaa_probe_type_support)
  add_executable(umaa_anchor_specs_payload_oracle
    "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_anchor_specs_oracle.c")
  target_link_libraries(umaa_anchor_specs_payload_oracle PRIVATE umaa_probe_type_support)
  add_test(NAME official_umaa_anchor_specs_verified_payload
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_anchor_specs.py"
      "$<TARGET_FILE:umaa_anchor_specs_payload_host>"
      "$<TARGET_FILE:umaa_anchor_specs_payload_oracle>")
  set_tests_properties(official_umaa_anchor_specs_verified_payload PROPERTIES TIMEOUT 45)

  add_executable(umaa_anchor_control_extended_host src/official_anchor_control_extended_host.cpp)
  target_compile_features(umaa_anchor_control_extended_host PRIVATE cxx_std_17)
  target_include_directories(umaa_anchor_control_extended_host PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
  target_link_libraries(umaa_anchor_control_extended_host PRIVATE umaa_probe_type_support)
  add_executable(umaa_anchor_control_extended_oracle
    "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/official_anchor_control_extended_oracle.c")
  target_link_libraries(umaa_anchor_control_extended_oracle PRIVATE umaa_probe_type_support)
  foreach(mode IN ITEMS normal update raise bad-destination complete fail timeout)
    if(mode STREQUAL "normal")
      set(extra_arg "")
    else()
      set(extra_arg "--${mode}")
    endif()
    add_test(NAME official_umaa_anchor_control_extended_${mode}
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../simulator/dds/run_anchor_control_extended.py"
        "$<TARGET_FILE:umaa_anchor_control_extended_host>"
        "$<TARGET_FILE:umaa_anchor_control_extended_oracle>" ${extra_arg})
    set_tests_properties(official_umaa_anchor_control_extended_${mode} PROPERTIES TIMEOUT 45)
  endforeach()
endif()
