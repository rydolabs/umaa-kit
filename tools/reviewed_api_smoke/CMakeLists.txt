cmake_minimum_required(VERSION 3.20)
project(UMAAReviewedServiceApiSmoke LANGUAGES CXX)
find_package(CycloneDDS REQUIRED)
foreach(required
    UMAA_REVIEWED_API_INCLUDE
    UMAA_BATTERY_TYPES_INCLUDE
    UMAA_ENGINE_TYPES_INCLUDE
    UMAA_EXTRA_TYPES_INCLUDE)
  if(NOT DEFINED ${required} OR NOT IS_DIRECTORY "${${required}}")
    message(FATAL_ERROR "Missing official generated API/type include path: ${required}")
  endif()
endforeach()
add_executable(umaa_reviewed_api_smoke smoke.cpp)
target_compile_features(umaa_reviewed_api_smoke PRIVATE cxx_std_17)
target_include_directories(umaa_reviewed_api_smoke PRIVATE
  "${UMAA_REVIEWED_API_INCLUDE}")
target_include_directories(umaa_reviewed_api_smoke SYSTEM PRIVATE
  "${UMAA_BATTERY_TYPES_INCLUDE}"
  "${UMAA_ENGINE_TYPES_INCLUDE}"
  "${UMAA_EXTRA_TYPES_INCLUDE}")
target_link_libraries(umaa_reviewed_api_smoke PRIVATE CycloneDDS::ddsc)
include(CTest)
add_test(NAME umaa_reviewed_api_smoke COMMAND umaa_reviewed_api_smoke)
