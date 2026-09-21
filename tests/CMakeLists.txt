find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
include(GoogleTest)

add_executable(system_monitor_tests
    unit/placeholder_test.cpp
    unit/domain_types_test.cpp
    unit/ring_buffer_test.cpp
    unit/synthetic_collectors_test.cpp
    unit/sampling_scheduler_test.cpp
)

set_target_properties(system_monitor_tests PROPERTIES
    AUTOMOC ON
)

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest_main
        sysmon_domain
        sysmon_monitoring
        Qt6::Core
)

gtest_discover_tests(system_monitor_tests)
