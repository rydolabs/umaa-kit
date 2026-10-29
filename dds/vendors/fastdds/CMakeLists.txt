cmake_minimum_required(VERSION 3.20)
project(UMAAFastDDSPluginSynthetic LANGUAGES CXX)
include(CTest)
find_package(fastrtps CONFIG REQUIRED)
find_package(fastcdr CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(FASTDDSGEN fastddsgen REQUIRED)
set(SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../sdk")
set(TYPE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${TYPE_ROOT}")
execute_process(COMMAND "${FASTDDSGEN}" -replace -d "${TYPE_ROOT}"
  "${CMAKE_CURRENT_LIST_DIR}/../../cyclone/idl/DemoBattery.idl"
  RESULT_VARIABLE GEN_RESULT OUTPUT_VARIABLE GEN_OUT ERROR_VARIABLE GEN_ERR)
if(NOT GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "fastddsgen failed: ${GEN_ERR} ${GEN_OUT}")
endif()
file(GLOB TYPE_SOURCES CONFIGURE_DEPENDS "${TYPE_ROOT}/*.cxx")
if(NOT TYPE_SOURCES)
  message(FATAL_ERROR "fastddsgen generated no C++ sources")
endif()
add_library(umaa_dds_fastdds_demo MODULE adapter.cpp ${TYPE_SOURCES})
target_compile_features(umaa_dds_fastdds_demo PRIVATE cxx_std_17)
target_include_directories(umaa_dds_fastdds_demo PRIVATE "${SDK_ROOT}/include" "${TYPE_ROOT}")
target_link_libraries(umaa_dds_fastdds_demo PRIVATE fastrtps fastcdr)
set(UMAA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDK_ROOT}" sdk-runtime)
add_library(umaa_fastdds_vehicle_demo MODULE "${SDK_ROOT}/examples/modules/vehicle_component.cpp")
target_include_directories(umaa_fastdds_vehicle_demo PRIVATE "${SDK_ROOT}/include")
add_executable(umaa_fastdds_plugin_wire_client "${SDK_ROOT}/tests/real_dds_backend_cli.cpp")
target_include_directories(umaa_fastdds_plugin_wire_client PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_plugin_wire_client PRIVATE UMAA::Runtime)
add_test(NAME umaa_fastdds_backend_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_backend.py"
 "$<TARGET_FILE:umaa_fastdds_plugin_wire_client>"
 "$<TARGET_FILE:umaa_dds_fastdds_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>")
set_tests_properties(umaa_fastdds_backend_real_wire PROPERTIES TIMEOUT 35)

# Synthetic CommandStatus/Ack TypeSupport, separate plugin using same Backend ABI.
set(COMMAND_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated-command")
file(MAKE_DIRECTORY "${COMMAND_ROOT}")
execute_process(COMMAND "${FASTDDSGEN}" -replace -d "${COMMAND_ROOT}"
  "${CMAKE_CURRENT_LIST_DIR}/../../cyclone/idl/DemoCommandEvent.idl"
  RESULT_VARIABLE COMMAND_GEN_RESULT OUTPUT_VARIABLE COMMAND_GEN_OUT
  ERROR_VARIABLE COMMAND_GEN_ERR)
if(NOT COMMAND_GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "Fast DDS Command generator failed: ${COMMAND_GEN_ERR} ${COMMAND_GEN_OUT}")
endif()
file(GLOB COMMAND_SOURCES CONFIGURE_DEPENDS "${COMMAND_ROOT}/*.cxx")
if(NOT COMMAND_SOURCES)
  message(FATAL_ERROR "Fast DDS Command generator produced no source")
endif()
add_library(umaa_dds_fastdds_command_demo MODULE command_adapter.cpp ${COMMAND_SOURCES})
target_compile_features(umaa_dds_fastdds_command_demo PRIVATE cxx_std_17)
target_include_directories(umaa_dds_fastdds_command_demo PRIVATE
  "${SDK_ROOT}/include" "${COMMAND_ROOT}")
target_link_libraries(umaa_dds_fastdds_command_demo PRIVATE fastrtps fastcdr)
add_executable(umaa_fastdds_command_cli "${SDK_ROOT}/tests/real_dds_command_cli.cpp")
target_compile_features(umaa_fastdds_command_cli PRIVATE cxx_std_17)
target_include_directories(umaa_fastdds_command_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_command_cli PRIVATE UMAA::Runtime)
add_test(NAME umaa_fastdds_command_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_backend.py"
 "$<TARGET_FILE:umaa_fastdds_command_cli>"
 "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>")
set_tests_properties(umaa_fastdds_command_real_wire PROPERTIES TIMEOUT 45)

# Independent remote Request -> Component -> Status/Ack -> Dispose test.
add_executable(umaa_fastdds_command_request_cli "${SDK_ROOT}/tests/real_dds_command_request_cli.cpp")
target_compile_features(umaa_fastdds_command_request_cli PRIVATE cxx_std_17)
target_include_directories(umaa_fastdds_command_request_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_command_request_cli PRIVATE UMAA::Runtime)
add_test(NAME umaa_fastdds_command_request_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_fastdds_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>")
set_tests_properties(umaa_fastdds_command_request_real_wire PROPERTIES TIMEOUT 55)

# Typed user-level Command client, no vendor APIs in application code.
add_executable(umaa_fastdds_typed_command_client_cli "${SDK_ROOT}/tests/real_dds_typed_command_client_cli.cpp")
target_compile_features(umaa_fastdds_typed_command_client_cli PRIVATE cxx_std_17)
target_include_directories(umaa_fastdds_typed_command_client_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_typed_command_client_cli PRIVATE UMAA::Runtime)
add_test(NAME umaa_fastdds_typed_command_client_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_fastdds_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>"
 "$<TARGET_FILE:umaa_fastdds_typed_command_client_cli>")
set_tests_properties(umaa_fastdds_typed_command_client_real_wire PROPERTIES TIMEOUT 55)

add_test(NAME umaa_fastdds_typed_command_cancel_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_fastdds_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>"
 "$<TARGET_FILE:umaa_fastdds_typed_command_client_cli>" cancel)
set_tests_properties(umaa_fastdds_typed_command_cancel_real_wire PROPERTIES TIMEOUT 55)

add_test(NAME umaa_fastdds_typed_command_update_real_wire
 COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
 "$<TARGET_FILE:umaa_fastdds_command_request_cli>"
 "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
 "$<TARGET_FILE:umaa_fastdds_vehicle_demo>"
 "$<TARGET_FILE:umaa_fastdds_typed_command_client_cli>" update)
set_tests_properties(umaa_fastdds_typed_command_update_real_wire PROPERTIES TIMEOUT 55)

add_executable(umaa_fastdds_typed_command_server_cli
  "${SDK_ROOT}/tests/real_dds_typed_command_server_cli.cpp")
target_compile_features(umaa_fastdds_typed_command_server_cli PRIVATE cxx_std_17)
target_include_directories(umaa_fastdds_typed_command_server_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_typed_command_server_cli PRIVATE UMAA::Runtime)
foreach(mode IN ITEMS normal cancel update)
  if(mode STREQUAL "normal")
    add_test(NAME umaa_fastdds_typed_server_real_wire
      COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
      "$<TARGET_FILE:umaa_fastdds_typed_command_server_cli>"
      "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
      "$<TARGET_FILE:umaa_fastdds_vehicle_demo>"
      "$<TARGET_FILE:umaa_fastdds_typed_command_client_cli>")
    set_tests_properties(umaa_fastdds_typed_server_real_wire PROPERTIES TIMEOUT 55)
  else()
    add_test(NAME "umaa_fastdds_typed_server_${mode}_real_wire"
      COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_command_request.py"
      "$<TARGET_FILE:umaa_fastdds_typed_command_server_cli>"
      "$<TARGET_FILE:umaa_dds_fastdds_command_demo>"
      "$<TARGET_FILE:umaa_fastdds_vehicle_demo>"
      "$<TARGET_FILE:umaa_fastdds_typed_command_client_cli>" "${mode}")
    set_tests_properties("umaa_fastdds_typed_server_${mode}_real_wire" PROPERTIES TIMEOUT 55)
  endif()
endforeach()

# Synthetic one-frame atomic LargeSet/LargeList transaction; NOT GROUP QoS.
set(COLLECTION_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated-collection")
file(MAKE_DIRECTORY "${COLLECTION_ROOT}")
execute_process(COMMAND "${FASTDDSGEN}" -replace -d "${COLLECTION_ROOT}"
  "${CMAKE_CURRENT_LIST_DIR}/../../cyclone/idl/DemoCollectionBatch.idl"
  RESULT_VARIABLE COLLECTION_GEN_RESULT OUTPUT_VARIABLE COLLECTION_GEN_OUT
  ERROR_VARIABLE COLLECTION_GEN_ERR)
if(NOT COLLECTION_GEN_RESULT EQUAL 0)
  message(FATAL_ERROR "Fast DDS Collection generation failed: ${COLLECTION_GEN_ERR} ${COLLECTION_GEN_OUT}")
endif()
file(GLOB COLLECTION_SOURCES CONFIGURE_DEPENDS "${COLLECTION_ROOT}/*.cxx")
if(NOT COLLECTION_SOURCES)
  message(FATAL_ERROR "Fast DDS Collection generated no source")
endif()
add_library(umaa_dds_fastdds_collection_demo MODULE collection_adapter.cpp ${COLLECTION_SOURCES})
target_compile_features(umaa_dds_fastdds_collection_demo PRIVATE cxx_std_17)
target_include_directories(umaa_dds_fastdds_collection_demo PRIVATE "${SDK_ROOT}/include" "${COLLECTION_ROOT}")
target_link_libraries(umaa_dds_fastdds_collection_demo PRIVATE fastrtps fastcdr)
add_executable(umaa_fastdds_collection_cli "${SDK_ROOT}/tests/real_dds_collection_cli.cpp")
target_compile_features(umaa_fastdds_collection_cli PRIVATE cxx_std_17)
target_include_directories(umaa_fastdds_collection_cli PRIVATE "${SDK_ROOT}/include")
target_link_libraries(umaa_fastdds_collection_cli PRIVATE UMAA::Runtime)
foreach(collection_kind IN ITEMS set list)
  add_test(NAME "umaa_fastdds_collection_${collection_kind}_real_wire"
    COMMAND "${Python3_EXECUTABLE}" "${SDK_ROOT}/tests/run_real_dds_collection.py"
    "$<TARGET_FILE:umaa_fastdds_collection_cli>"
    "$<TARGET_FILE:umaa_dds_fastdds_collection_demo>" "${collection_kind}")
  set_tests_properties("umaa_fastdds_collection_${collection_kind}_real_wire" PROPERTIES TIMEOUT 40)
endforeach()
