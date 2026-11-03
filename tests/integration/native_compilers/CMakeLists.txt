cmake_minimum_required(VERSION 3.20)
project(UMAARealVendorIDLGenerated LANGUAGES C CXX)
enable_testing()
set(UMAA_NATIVE_GENERATED_ROOT "" CACHE PATH "Path created by native_dds_codegen.py")
if(NOT UMAA_NATIVE_GENERATED_ROOT OR
   NOT EXISTS "\${UMAA_NATIVE_GENERATED_ROOT}/native-generation.json")
  message(FATAL_ERROR "Missing native generated source manifest; run real idlc and fastddsgen")
endif()
find_package(CycloneDDS CONFIG REQUIRED)
file(GLOB_RECURSE CYCLONE_NATIVE CONFIGURE_DEPENDS
  "\${UMAA_NATIVE_GENERATED_ROOT}/cyclone/*.c")
file(GLOB_RECURSE FAST_NATIVE CONFIGURE_DEPENDS
  "\${UMAA_NATIVE_GENERATED_ROOT}/fastdds/*.cxx")
if(NOT CYCLONE_NATIVE OR NOT FAST_NATIVE)
  message(FATAL_ERROR "Both native vendor compilers must produce C/C++ sources")
endif()
add_library(umaa_native_idlc_types SHARED \${CYCLONE_NATIVE})
target_include_directories(umaa_native_idlc_types PRIVATE
  "\${UMAA_NATIVE_GENERATED_ROOT}/cyclone")
target_link_libraries(umaa_native_idlc_types PRIVATE CycloneDDS::ddsc)
set_target_properties(umaa_native_idlc_types PROPERTIES C_STANDARD 11)
add_library(umaa_native_fastddsgen_types SHARED \${FAST_NATIVE})
target_compile_features(umaa_native_fastddsgen_types PRIVATE cxx_std_17)
target_include_directories(umaa_native_fastddsgen_types PRIVATE
  "\${UMAA_NATIVE_GENERATED_ROOT}/fastdds")
target_link_libraries(umaa_native_fastddsgen_types PRIVATE fastrtps fastcdr)
if(UNIX AND NOT APPLE)
  target_link_options(umaa_native_idlc_types PRIVATE "LINKER:--no-undefined")
  target_link_options(umaa_native_fastddsgen_types PRIVATE "LINKER:--no-undefined")
endif()
add_executable(umaa_native_idl_loader_smoke native_loader_smoke.cpp)
target_compile_features(umaa_native_idl_loader_smoke PRIVATE cxx_std_17)
target_link_libraries(umaa_native_idl_loader_smoke PRIVATE \${CMAKE_DL_LIBS})
add_test(NAME umaa_native_idl_vendor_libraries_load
  COMMAND umaa_native_idl_loader_smoke
    "$<TARGET_FILE:umaa_native_idlc_types>"
    "$<TARGET_FILE:umaa_native_fastddsgen_types>")
