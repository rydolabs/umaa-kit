cmake_minimum_required(VERSION 3.20)
project(UMAAPluginCycloneSynthetic LANGUAGES C CXX)
include(CTest)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(CycloneDDS CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../sdk")
idlc_generate(TARGET umaa_cyclone_plugin_types
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../cyclone/idl/DemoBattery.idl")
add_library(umaa_dds_cyclone_demo MODULE adapter.cpp)
target_compile_features(umaa_dds_cyclone_demo PRIVATE cxx_std_17)
target_include_directories(umaa_dds_cyclone_demo PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_dds_cyclone_demo PRIVATE umaa_cyclone_plugin_types CycloneDDS::ddsc)
add_library(umaa_cyclone_vehicle_component MODULE "${SDK_ROOT}/examples/modules/vehicle_component.cpp")
target_compile_features(umaa_cyclone_vehicle_component PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_vehicle_component PRIVATE "${SDK_ROOT}/include")
set(UMAA_BUILD_TESTS OFF CACHE BOOL "Build SDK tests inside the plugin smoke" FORCE)
add_subdirectory("${SDK_ROOT}" sdk-runtime)
add_executable(umaa_cyclone_backend_cli "${SDK_ROOT}/tests/real_dds_backend_cli.cpp")
target_compile_features(umaa_cyclone_backend_cli PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_backend_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_cyclone_backend_cli PRIVATE UMAA::Runtime "${CMAKE_DL_LIBS}")
add_test(NAME umaa_backend_plugin_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_backend.py"
 "$<TARGET_FILE:umaa_cyclone_backend_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>")
set_tests_properties(umaa_backend_plugin_real_wire PROPERTIES TIMEOUT 35)

# Synthetic end-to-end CommandStatus/Ack over real Cyclone DDS.
idlc_generate(TARGET umaa_cyclone_command_types
  FILES "${CMAKE_CURRENT_LIST_DIR}/../../cyclone/idl/DemoCommandEvent.idl")
add_library(umaa_dds_cyclone_command_demo MODULE command_adapter.cpp)
target_compile_features(umaa_dds_cyclone_command_demo PRIVATE cxx_std_17)
target_include_directories(umaa_dds_cyclone_command_demo PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_dds_cyclone_command_demo PRIVATE
  umaa_cyclone_command_types CycloneDDS::ddsc)
add_executable(umaa_cyclone_command_cli "${SDK_ROOT}/tests/real_dds_command_cli.cpp")
target_compile_features(umaa_cyclone_command_cli PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_command_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_cyclone_command_cli PRIVATE UMAA::Runtime "${CMAKE_DL_LIBS}")
add_test(NAME umaa_command_backend_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_backend.py"
 "$<TARGET_FILE:umaa_cyclone_command_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>")
set_tests_properties(umaa_command_backend_real_wire PROPERTIES TIMEOUT 45)

# Independent remote Request -> Component -> Status/Ack -> Dispose test.
add_executable(umaa_cyclone_command_request_cli "${SDK_ROOT}/tests/real_dds_command_request_cli.cpp")
target_compile_features(umaa_cyclone_command_request_cli PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_command_request_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_cyclone_command_request_cli PRIVATE UMAA::Runtime "${CMAKE_DL_LIBS}")
add_test(NAME umaa_command_request_roundtrip_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_cyclone_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>")
set_tests_properties(umaa_command_request_roundtrip_real_wire PROPERTIES TIMEOUT 55)

# Typed user-level Command client, no vendor APIs in application code.
add_executable(umaa_cyclone_typed_command_client_cli "${SDK_ROOT}/tests/real_dds_typed_command_client_cli.cpp")
target_compile_features(umaa_cyclone_typed_command_client_cli PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_typed_command_client_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_cyclone_typed_command_client_cli PRIVATE UMAA::Runtime "${CMAKE_DL_LIBS}")
add_test(NAME umaa_cyclone_typed_command_client_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_cyclone_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>"
 "$<TARGET_FILE:umaa_cyclone_typed_command_client_cli>")
set_tests_properties(umaa_cyclone_typed_command_client_real_wire PROPERTIES TIMEOUT 55)

# Pending device command: early disposal must produce genuine CANCELED,
# not fabricated completion; replay remains forbidden.
add_test(NAME umaa_cyclone_typed_command_cancel_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_cyclone_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>"
 "$<TARGET_FILE:umaa_cyclone_typed_command_client_cli>" cancel)
set_tests_properties(umaa_cyclone_typed_command_cancel_real_wire PROPERTIES TIMEOUT 55)

add_test(NAME umaa_cyclone_typed_command_update_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_cyclone_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
 "$<TARGET_FILE:umaa_cyclone_vehicle_component>"
 "$<TARGET_FILE:umaa_cyclone_typed_command_client_cli>" update)
set_tests_properties(umaa_cyclone_typed_command_update_real_wire PROPERTIES TIMEOUT 55)

# Fully SDK-owned Request Reader + Handler FSM, using a typed service trait.
add_executable(umaa_cyclone_typed_command_server_cli
  "${SDK_ROOT}/tests/real_dds_typed_command_server_cli.cpp")
target_compile_features(umaa_cyclone_typed_command_server_cli PRIVATE cxx_std_17)
target_include_directories(umaa_cyclone_typed_command_server_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_cyclone_typed_command_server_cli PRIVATE UMAA::Runtime "${CMAKE_DL_LIBS}")
foreach(mode IN ITEMS normal cancel update)
  set(cmd_mode "client")
  if(NOT mode STREQUAL "normal")
    set(cmd_mode "${mode}")
  endif()
  if(mode STREQUAL "normal")
    add_test(NAME umaa_cyclone_typed_server_real_wire
      COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
      "$<TARGET_FILE:umaa_cyclone_typed_command_server_cli>"
      "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
      "$<TARGET_FILE:umaa_cyclone_vehicle_component>"
      "$<TARGET_FILE:umaa_cyclone_typed_command_client_cli>")
    set_tests_properties(umaa_cyclone_typed_server_real_wire PROPERTIES TIMEOUT 55)
  else()
    add_test(NAME "umaa_cyclone_typed_server_${mode}_real_wire"
      COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
      "$<TARGET_FILE:umaa_cyclone_typed_command_server_cli>"
      "$<TARGET_FILE:umaa_dds_cyclone_command_demo>"
      "$<TARGET_FILE:umaa_cyclone_vehicle_component>"
      "$<TARGET_FILE:umaa_cyclone_typed_command_client_cli>" "${mode}")
    set_tests_properties("umaa_cyclone_typed_server_${mode}_real_wire" PROPERTIES TIMEOUT 55)
  endif()
endforeach()
