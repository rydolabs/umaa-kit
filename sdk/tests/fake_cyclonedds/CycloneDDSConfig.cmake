# Compile-test fixture ONLY; no DDS entities, discovery or wire transport.
if(NOT TARGET CycloneDDS::ddsc)
  add_library(CycloneDDS::ddsc INTERFACE IMPORTED)
  get_filename_component(_stub_include "${CMAKE_CURRENT_LIST_DIR}/../fake_dds" ABSOLUTE)
  set_property(TARGET CycloneDDS::ddsc PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${_stub_include}")
endif()
