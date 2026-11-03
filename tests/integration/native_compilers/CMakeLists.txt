cmake_minimum_required(VERSION 3.20)
project(UMAANativeIDLBackendRegistry LANGUAGES C CXX)
enable_testing()
set(UMAA_NATIVE_GENERATED_ROOT "" CACHE PATH "Native backend compiler output")
if(NOT UMAA_NATIVE_GENERATED_ROOT OR
   NOT EXISTS "${UMAA_NATIVE_GENERATED_ROOT}/native-generation.json" OR
   NOT EXISTS "${UMAA_NATIVE_GENERATED_ROOT}/native-targets.cmake")
  message(FATAL_ERROR "Backend-neutral native generation output missing")
endif()
# Registry-driven CMake: no hardcoded Cyclone, Fast DDS, or future vendor
# target names, source extensions, include paths or linker libraries here.
include("${UMAA_NATIVE_GENERATED_ROOT}/native-targets.cmake")
if(NOT UMAA_NATIVE_TARGETS)
  message(FATAL_ERROR "No native DDS compiler backend produced TypeSupport")
endif()
add_executable(umaa_native_idl_loader_smoke native_loader_smoke.cpp)
target_compile_features(umaa_native_idl_loader_smoke PRIVATE cxx_std_17)
target_link_libraries(umaa_native_idl_loader_smoke PRIVATE ${CMAKE_DL_LIBS})
set(UMAA_NATIVE_BINARY_PATHS "")
foreach(target IN LISTS UMAA_NATIVE_TARGETS)
  list(APPEND UMAA_NATIVE_BINARY_PATHS "$<TARGET_FILE:${target}>")
endforeach()
add_test(NAME umaa_native_idl_vendor_libraries_load
  COMMAND umaa_native_idl_loader_smoke ${UMAA_NATIVE_BINARY_PATHS})
