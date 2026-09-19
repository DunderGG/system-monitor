find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(system_monitor_tests
    unit/placeholder_test.cpp
)

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(system_monitor_tests)
