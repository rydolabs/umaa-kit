cmake_minimum_required(VERSION 3.20)
project(UMAABatteryCollectionHostJoiner LANGUAGES CXX)
include(CTest)
add_executable(umaa_battery_collection_joiner_test
  "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/tests/official_battery_collection_joiner_test.cpp")
target_compile_features(umaa_battery_collection_joiner_test PRIVATE cxx_std_17)
target_include_directories(umaa_battery_collection_joiner_test PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(umaa_battery_collection_joiner_test PRIVATE /W4 /WX)
else()
  target_compile_options(umaa_battery_collection_joiner_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME umaa_battery_large_list_event_joiner
  COMMAND umaa_battery_collection_joiner_test)

add_executable(umaa_battery_subscription_test
  "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/tests/official_battery_subscription_test.cpp")
target_compile_features(umaa_battery_subscription_test PRIVATE cxx_std_17)
target_include_directories(umaa_battery_subscription_test PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/include")
if(MSVC)
  target_compile_options(umaa_battery_subscription_test PRIVATE /W4 /WX)
else()
  target_compile_options(umaa_battery_subscription_test PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME umaa_battery_handler_subscription
  COMMAND umaa_battery_subscription_test)
