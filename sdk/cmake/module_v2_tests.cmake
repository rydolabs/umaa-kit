# Portable native component and DDS backend ABI contract tests.
add_library(umaa_demo_vehicle_v2 MODULE examples/modules/vehicle_component.cpp)
target_link_libraries(umaa_demo_vehicle_v2 PRIVATE UMAA::Core)
add_library(umaa_mock_dds_backend MODULE tests/plugins/mock_backend.cpp)
target_link_libraries(umaa_mock_dds_backend PRIVATE UMAA::Core)
add_executable(umaa_component_module_v2_test tests/test_module_v2.cpp)
target_link_libraries(umaa_component_module_v2_test PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
add_test(NAME umaa_component_module_v2_test COMMAND umaa_component_module_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>")
add_executable(umaa_dds_backend_abi_test tests/test_dds_backend_abi.cpp)
target_link_libraries(umaa_dds_backend_abi_test PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
add_test(NAME umaa_dds_backend_abi_test COMMAND umaa_dds_backend_abi_test
  "$<TARGET_FILE:umaa_mock_dds_backend>")
add_executable(umaa_application_facade_test tests/test_application_facade.cpp)
target_link_libraries(umaa_application_facade_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_application_facade_test COMMAND umaa_application_facade_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>")

add_executable(umaa_report_publisher_v2_test tests/test_report_publisher_v2.cpp)
target_link_libraries(umaa_report_publisher_v2_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_report_publisher_v2_test COMMAND umaa_report_publisher_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# Regression: a pending device command must fence DLL/SO unload until drained.
add_executable(umaa_component_safe_shutdown_v2_test tests/test_safe_shutdown_v2.cpp)
target_link_libraries(umaa_component_safe_shutdown_v2_test PRIVATE UMAA::Core ${CMAKE_DL_LIBS})
target_compile_features(umaa_component_safe_shutdown_v2_test PRIVATE cxx_std_17)
add_test(NAME umaa_component_safe_shutdown_v2_test COMMAND umaa_component_safe_shutdown_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>")

# Independent DLL/SO safety test: an executing callback must drain before
# unloading, and a failed stop must not be retried.
find_package(Threads REQUIRED)
add_library(umaa_slow_stop_v2_fixture MODULE tests/modules/slow_stop_v2.cpp)
target_link_libraries(umaa_slow_stop_v2_fixture PRIVATE UMAA::Core Threads::Threads)
add_executable(umaa_module_inflight_v2_test tests/test_module_inflight_v2.cpp)
target_link_libraries(umaa_module_inflight_v2_test PRIVATE UMAA::Core Threads::Threads ${CMAKE_DL_LIBS})
add_test(NAME umaa_module_inflight_v2_test COMMAND umaa_module_inflight_v2_test
  "$<TARGET_FILE:umaa_slow_stop_v2_fixture>")

# Event-driven Report consumer facade (mock backend, no official IDL claim).
add_executable(umaa_report_watch_v2_test tests/test_report_watch_v2.cpp)
target_link_libraries(umaa_report_watch_v2_test PRIVATE UMAA::Runtime Threads::Threads)
add_test(NAME umaa_report_watch_v2_test COMMAND umaa_report_watch_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# Automatic ReportSource sampling through a persistent DDS writer (mock only).
add_executable(umaa_report_schedule_v2_test tests/test_report_schedule_v2.cpp)
target_link_libraries(umaa_report_schedule_v2_test PRIVATE UMAA::Runtime Threads::Threads)
add_test(NAME umaa_report_schedule_v2_test COMMAND umaa_report_schedule_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# A module is replaceable only after pending sessions and publishers drain.
add_executable(umaa_runtime_reload_v2_test tests/test_runtime_reload_v2.cpp)
target_link_libraries(umaa_runtime_reload_v2_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_runtime_reload_v2_test COMMAND umaa_runtime_reload_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# Public typed application Report producer/consumer; mock DDS ABI backend.
add_executable(umaa_typed_report_application_v2_test tests/test_typed_report_application_v2.cpp)
target_link_libraries(umaa_typed_report_application_v2_test PRIVATE UMAA::Runtime Threads::Threads)
add_test(NAME umaa_typed_report_application_v2_test COMMAND umaa_typed_report_application_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# Remote Report Consumer must never need a local Provider Component DLL/SO.
add_executable(umaa_remote_report_consumer_v2_test tests/test_remote_report_consumer_v2.cpp)
target_link_libraries(umaa_remote_report_consumer_v2_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_remote_report_consumer_v2_test COMMAND umaa_remote_report_consumer_v2_test
  "$<TARGET_FILE:umaa_mock_dds_backend>")

# Minimal strict configuration facade: one file selects backend, domain and modules.
add_executable(umaa_runtime_config_v2_test tests/test_runtime_config_v2.cpp)
target_link_libraries(umaa_runtime_config_v2_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_runtime_config_v2_test COMMAND umaa_runtime_config_v2_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>" "$<TARGET_FILE:umaa_mock_dds_backend>")

# Host business Command -> protocol FSM -> outbound abstract event sink.
add_executable(umaa_component_command_provider_test tests/test_component_command_provider.cpp)
target_link_libraries(umaa_component_command_provider_test PRIVATE UMAA::Runtime)
add_test(NAME umaa_component_command_provider_test COMMAND umaa_component_command_provider_test
  "$<TARGET_FILE:umaa_demo_vehicle_v2>")
