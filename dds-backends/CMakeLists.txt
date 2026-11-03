cmake_minimum_required(VERSION 3.20)
project(UMAADDSBackends LANGUAGES C CXX)
# Build one backend per build tree; runtime switching uses UmaaDdsBackendV1.
set(UMAA_DDS_BACKEND "cyclone" CACHE STRING "DDS backend to build")
set_property(CACHE UMAA_DDS_BACKEND PROPERTY STRINGS cyclone fastdds)
if(NOT UMAA_DDS_BACKEND MATCHES "^(cyclone|fastdds)$")
  message(FATAL_ERROR "UMAA_DDS_BACKEND must be cyclone or fastdds (RTI not implemented)")
endif()
add_subdirectory("${UMAA_DDS_BACKEND}")
