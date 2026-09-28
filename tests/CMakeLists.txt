find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(spdlog CONFIG REQUIRED)
include(GoogleTest)

add_executable(system_monitor_tests
    test_main.cpp
    unit/domain_types_test.cpp
    unit/ring_buffer_test.cpp
    unit/synthetic_collectors_test.cpp
    unit/sampling_scheduler_test.cpp
    unit/main_window_test.cpp
    unit/dashboard_view_test.cpp
    unit/dashboard_formatting_test.cpp
    unit/sparkline_widget_test.cpp
    unit/cpu_collector_test.cpp
    unit/memory_collector_test.cpp
    unit/disk_collector_test.cpp
    unit/network_collector_test.cpp
    unit/adapter_details_cache_test.cpp
    unit/repeated_failure_log_test.cpp
    unit/log_capture.h
    unit/connectivity_collector_test.cpp
    unit/uptime_collector_test.cpp
    unit/health_evaluator_test.cpp
    integration/cpu_collector_test.cpp
    integration/memory_collector_test.cpp
    integration/disk_collector_test.cpp
    integration/network_collector_test.cpp
    integration/connectivity_collector_test.cpp
    integration/uptime_collector_test.cpp
    integration/app_logging_test.cpp
    # Same manifest as the app, so tests run with the UTF-8 process code page.
    ${PROJECT_SOURCE_DIR}/src/app/system_monitor.manifest
)

set_target_properties(system_monitor_tests PROPERTIES
    AUTOMOC ON
)

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest
        sysmon_app_logging
        sysmon_domain
        sysmon_monitoring
        sysmon_platform_windows
        sysmon_ui
        Qt6::Core
        Qt6::Widgets
        spdlog::spdlog
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

# Platform unit tests: deterministic tests of platform/windows code that need
# Windows SDK types or constants (e.g. Windows-to-domain mapping functions).
# They may include Windows headers but must not call Windows APIs. Kept in a
# separate target so tests/unit/ stays free of Windows headers. No Qt needed.
add_executable(system_monitor_platform_tests
    platform/connectivity_hint_mapping_test.cpp
)

target_link_libraries(system_monitor_platform_tests
    PRIVATE
        GTest::gtest_main
        sysmon_domain
        sysmon_platform_windows
)

gtest_discover_tests(system_monitor_platform_tests)
