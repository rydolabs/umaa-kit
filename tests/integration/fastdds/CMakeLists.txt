cmake_minimum_required(VERSION 3.20)
project(UMAAOfficialBatteryFastDDS LANGUAGES CXX)
include(CTest)
find_package(fastrtps CONFIG REQUIRED)
find_package(fastcdr CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(UMAA_OFFICIAL_FASTDDS_GENERATED_ROOT "" CACHE PATH "Ephemeral original Fast DDS-Gen TypeSupport")
if(NOT UMAA_OFFICIAL_FASTDDS_GENERATED_ROOT OR
   NOT EXISTS "${UMAA_OFFICIAL_FASTDDS_GENERATED_ROOT}/BatteryReportType.h" OR
   NOT EXISTS "${UMAA_OFFICIAL_FASTDDS_GENERATED_ROOT}/BatteryReportTypePubSubTypes.h")
  message(FATAL_ERROR "Original official Fast DDS-Gen Battery TypeSupport missing")
endif()
set(GENERATED "${UMAA_OFFICIAL_FASTDDS_GENERATED_ROOT}")
file(GLOB_RECURSE ORIGINAL_SOURCES CONFIGURE_DEPENDS "${GENERATED}/*.cxx")
if(NOT ORIGINAL_SOURCES)
  message(FATAL_ERROR "No original official Fast DDS TypeSupport translation units")
endif()
add_library(umaa_battery_fastdds_official_type_support STATIC ${ORIGINAL_SOURCES})
target_compile_features(umaa_battery_fastdds_official_type_support PUBLIC cxx_std_17)
# Fast DDS-Gen preserves some source-relative includes (for example,
# IdentifierType.h -> Measurement/Measurements.h). Its backend plugin mirrors
# generated headers below the source paths; add those generated directories
# without ever adding or redistributing original IDL source directories.
file(GLOB_RECURSE ORIGINAL_GENERATED_HEADERS CONFIGURE_DEPENDS "${GENERATED}/*.h")
set(TYPE_SUPPORT_INCLUDE_DIRS "${GENERATED}")
foreach(header IN LISTS ORIGINAL_GENERATED_HEADERS)
  get_filename_component(header_dir "${header}" DIRECTORY)
  list(APPEND TYPE_SUPPORT_INCLUDE_DIRS "${header_dir}")
endforeach()
list(REMOVE_DUPLICATES TYPE_SUPPORT_INCLUDE_DIRS)
target_include_directories(umaa_battery_fastdds_official_type_support PUBLIC
  ${TYPE_SUPPORT_INCLUDE_DIRS})
target_link_libraries(umaa_battery_fastdds_official_type_support PUBLIC fastrtps fastcdr)

add_executable(umaa_fastdds_official_battery_publisher
  official_battery_payload_publisher.cpp)
target_include_directories(umaa_fastdds_official_battery_publisher PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/include"
  "${CMAKE_CURRENT_LIST_DIR}/../../../dds-backends/fastdds/include")
target_link_libraries(umaa_fastdds_official_battery_publisher PRIVATE
  umaa_battery_fastdds_official_type_support)

add_executable(umaa_fastdds_official_battery_oracle
  official_battery_payload_oracle.cpp)
target_link_libraries(umaa_fastdds_official_battery_oracle PRIVATE
  umaa_battery_fastdds_official_type_support)

add_test(NAME official_umaa_battery_fastdds_nested_wire
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../../../simulator/dds/run_battery_fastdds.py"
    "$<TARGET_FILE:umaa_fastdds_official_battery_publisher>"
    "$<TARGET_FILE:umaa_fastdds_official_battery_oracle>")
set_tests_properties(official_umaa_battery_fastdds_nested_wire PROPERTIES TIMEOUT 55)

add_test(NAME official_umaa_battery_fastdds_bad_checkpoint_rejected
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../../../simulator/dds/run_battery_fastdds.py"
    "$<TARGET_FILE:umaa_fastdds_official_battery_publisher>"
    "$<TARGET_FILE:umaa_fastdds_official_battery_oracle>"
    --bad-checkpoint)
set_tests_properties(official_umaa_battery_fastdds_bad_checkpoint_rejected PROPERTIES TIMEOUT 55)
