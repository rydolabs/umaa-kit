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
