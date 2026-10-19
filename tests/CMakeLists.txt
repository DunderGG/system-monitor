find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(spdlog CONFIG REQUIRED)

# Each test executable runs as GoogleTest shards, one CTest test per shard,
# rather than one process per test (DC-113): a process of the main test
# executable creates a QApplication and loads its plugins before its first
# test, which costs more than most tests. CTest runs the shards in parallel
# (build.ps1 passes --parallel). GoogleTest deals the tests out round-robin,
# so twice as many shards as logical processors keeps the processors busy
# while the slowest shards finish. Run a single test with the executable's
# --gtest_filter, e.g. system_monitor_tests --gtest_filter=RingBuffer.*.
cmake_host_system_information(RESULT sysmonLogicalProcessors QUERY NUMBER_OF_LOGICAL_CORES)
math(EXPR sysmonTestShards "2 * ${sysmonLogicalProcessors}")

# Adds the tests of target as shardCount CTest tests named
# <target>_shard_<index>_of_<shardCount>. Each shard writes GoogleTest's JSON
# report of its tests to test_results/<target>_shard_<index>.json in the build
# directory (DC-119), which build.ps1 empties before the tests run. The shards
# run in parallel, so each names its own file: given only a directory,
# GoogleTest picks a name that no file has yet when the process starts, and
# writes the file when it ends, so shards running together pick the same name.
function(sysmon_add_sharded_tests target shardCount)
    math(EXPR lastShard "${shardCount} - 1")
    foreach(shard RANGE ${lastShard})
        set(testName "${target}_shard_${shard}_of_${shardCount}")
        set(resultsPath "${CMAKE_BINARY_DIR}/test_results/${target}_shard_${shard}.json")
        add_test(NAME "${testName}" COMMAND "${target}")
        set_tests_properties("${testName}" PROPERTIES
            ENVIRONMENT "GTEST_TOTAL_SHARDS=${shardCount};GTEST_SHARD_INDEX=${shard};GTEST_OUTPUT=json:${resultsPath}"
        )
    endforeach()
endfunction()

add_executable(system_monitor_tests
    test_main.cpp
    support/adapter_lists.h
    support/fake_collectors.h
    support/fake_subscription.h
    support/log_capture.h
    support/network_samples.h
    support/process_model_harness.h
    support/process_samples.h
    support/readouts.h
    support/scheduler_harness.h
    support/snapshots.h
    support/sparkline_samples.h
    support/text_contrast.h
    unit/domain_types_test.cpp
    unit/ring_buffer_test.cpp
    unit/sampling_scheduler_test.cpp
    unit/sampling_scheduler_event_driven_test.cpp
    unit/sampling_scheduler_slow_collectors_test.cpp
    unit/main_window_test.cpp
    unit/dashboard_view_test.cpp
    unit/dashboard_formatting_test.cpp
    unit/performance_view_test.cpp
    unit/performance_formatting_test.cpp
    unit/performance_readouts_test.cpp
    unit/adapter_throughput_history_test.cpp
    unit/network_formatting_test.cpp
    unit/network_view_test.cpp
    unit/process_formatting_test.cpp
    unit/process_model_test.cpp
    unit/process_model_highlights_test.cpp
    unit/process_model_roles_test.cpp
    unit/process_model_tree_test.cpp
    unit/process_sort_filter_proxy_model_test.cpp
    unit/processes_view_test.cpp
    unit/qt_configuration_test.cpp
    unit/readout_grid_test.cpp
    unit/sparkline_geometry_test.cpp
    unit/sparkline_widget_test.cpp
    unit/text_tone_test.cpp
    unit/cpu_collector_test.cpp
    unit/memory_collector_test.cpp
    unit/disk_collector_test.cpp
    unit/network_collector_test.cpp
    unit/adapter_details_cache_test.cpp
    unit/byte_rate_test.cpp
    unit/repeated_failure_log_test.cpp
    unit/connectivity_collector_test.cpp
    unit/network_profile_collector_test.cpp
    unit/uptime_collector_test.cpp
    unit/system_activity_collector_test.cpp
    unit/system_process_information_reader_test.cpp
    unit/process_access_policy_test.cpp
    unit/process_collector_test.cpp
    unit/process_identity_cache_test.cpp
    unit/utf8_conversion_test.cpp
    unit/health_evaluator_test.cpp
    integration/cpu_collector_test.cpp
    integration/memory_collector_test.cpp
    integration/disk_collector_test.cpp
    integration/network_collector_test.cpp
    integration/connectivity_collector_test.cpp
    integration/network_profile_collector_test.cpp
    integration/uptime_collector_test.cpp
    integration/system_activity_collector_test.cpp
    integration/system_process_information_reader_test.cpp
    integration/process_collector_test.cpp
    integration/process_details_reader_test.cpp
    integration/process_terminator_test.cpp
    integration/process_tracking_test.cpp
    integration/scheduler_pipeline_test.cpp
    integration/suspended_child_process.h
    integration/current_process_elevation.h
    integration/app_logging_test.cpp
    integration/ip_address_formatting_test.cpp
    # Same manifest as the app, so tests run with the UTF-8 process code page.
    ${PROJECT_SOURCE_DIR}/src/app/system_monitor.manifest
)

set_target_properties(system_monitor_tests PROPERTIES
    AUTOMOC ON
)

# Shared test builders and harnesses are included as "support/<name>.h".
target_include_directories(system_monitor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(system_monitor_tests
    PRIVATE
        GTest::gtest
        sysmon_app_collectors
        sysmon_app_logging
        sysmon_domain
        sysmon_monitoring
        sysmon_platform_windows
        sysmon_ui
        Qt6::Core
        Qt6::Widgets
        spdlog::spdlog
)

# The same plugins as the app, so the tests run with the app's style.
sysmon_deploy_qt_plugins(system_monitor_tests)

sysmon_add_sharded_tests(system_monitor_tests ${sysmonTestShards})

# Platform unit tests: deterministic tests of platform/windows code that need
# Windows SDK types or constants (e.g. Windows-to-domain mapping functions).
# They may include Windows headers but must not call Windows APIs. Kept in a
# separate target so tests/unit/ stays free of Windows headers. No Qt needed.
add_executable(system_monitor_platform_tests
    platform/adapter_type_mapping_test.cpp
    platform/connectivity_hint_mapping_test.cpp
    platform/ip_address_mapping_test.cpp
    platform/network_category_mapping_test.cpp
    platform/system_process_information_test.cpp
    platform/process_elevation_test.cpp
)

target_link_libraries(system_monitor_platform_tests
    PRIVATE
        GTest::gtest_main
        sysmon_domain
        sysmon_platform_windows
)

# Without Qt to start, all of these run in a fraction of a second in one process.
sysmon_add_sharded_tests(system_monitor_platform_tests 1)
