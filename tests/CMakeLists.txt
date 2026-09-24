find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets)
include(GoogleTest)

add_executable(system_monitor_tests
    test_main.cpp
    unit/domain_types_test.cpp
    unit/ring_buffer_test.cpp
    unit/synthetic_collectors_test.cpp
    unit/sampling_scheduler_test.cpp
    unit/main_window_test.cpp
    unit/cpu_collector_test.cpp
    unit/memory_collector_test.cpp
    unit/disk_collector_test.cpp
    integration/cpu_collector_test.cpp
    integration/memory_collector_test.cpp
    integration/disk_collector_test.cpp
)

set_target_properties(system_monitor_tests PROPERTIES
    AUTOMOC ON
)

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest
        sysmon_domain
        sysmon_monitoring
        sysmon_platform_windows
        sysmon_ui
        Qt6::Core
        Qt6::Widgets
)

if(WIN32)
    add_custom_command(TARGET system_monitor_tests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:system_monitor_tests>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:system_monitor_tests>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        COMMENT "Deploying Qt platform plugin for tests"
    )
endif()

gtest_discover_tests(system_monitor_tests)
