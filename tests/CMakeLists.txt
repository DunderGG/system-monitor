find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(system_monitor_tests
    unit/placeholder_test.cpp
    unit/domain_types_test.cpp
)

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest_main
        sysmon_domain
)

gtest_discover_tests(system_monitor_tests)
