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
# --gtest_filter, e.g. verdandi_tests --gtest_filter=RingBuffer.*.
cmake_host_system_information(RESULT verdandiLogicalProcessors QUERY NUMBER_OF_LOGICAL_CORES)
math(EXPR verdandiTestShards "2 * ${verdandiLogicalProcessors}")

# Adds the tests of target as shardCount CTest tests named
# <target>_shard_<index>_of_<shardCount>. Each shard writes GoogleTest's JSON
# report of its tests to test_results/<target>_shard_<index>.json in the build
# directory (DC-119), which build.ps1 empties before the tests run. The shards
# run in parallel, so each names its own file: given only a directory,
# GoogleTest picks a name that no file has yet when the process starts, and
# writes the file when it ends, so shards running together pick the same name.
function(verdandi_add_sharded_tests target shardCount)
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

add_executable(verdandi_tests
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
    support/settings_samples.h
    support/snapshots.h
    support/sparkline_samples.h
    support/temporary_directory.h
    support/text_contrast.h
    unit/domain_types_test.cpp
    unit/ring_buffer_test.cpp
    unit/sampling_scheduler_test.cpp
    unit/sampling_scheduler_event_driven_test.cpp
    unit/sampling_scheduler_slow_collectors_test.cpp
    unit/sampling_scheduler_power_test.cpp
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
    unit/settings_file_test.cpp
    unit/settings_dialog_test.cpp
    unit/settings_service_test.cpp
    unit/app_logging_test.cpp
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
    integration/application_executable_test.cpp
    integration/application_manifest_test.cpp
    integration/ip_address_formatting_test.cpp
    integration/settings_file_test.cpp
    integration/settings_service_test.cpp
    integration/power_event_filter_test.cpp
    # Same manifest as the app, so tests run with its UTF-8 process code page
    # and its DPI awareness.
    ${PROJECT_SOURCE_DIR}/src/app/verdandi.manifest
)

set_target_properties(verdandi_tests PROPERTIES
    AUTOMOC ON
)

# Shared test builders and harnesses are included as "support/<name>.h".
target_include_directories(verdandi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# An integration test reads the application's executable, so it is built first.
add_dependencies(verdandi_tests verdandi)
target_compile_definitions(verdandi_tests PRIVATE VERDANDI_APPLICATION_PATH="$<TARGET_FILE:verdandi>")

target_link_libraries(verdandi_tests
    PRIVATE
        GTest::gtest
        verdandi_app_collectors
        verdandi_app_logging
        verdandi_app_power_events
        verdandi_app_settings
        verdandi_domain
        verdandi_monitoring
        verdandi_persistence
        verdandi_platform_windows
        verdandi_ui
        Qt6::Core
        Qt6::Widgets
        spdlog::spdlog
)

# The same plugins as the app, so the tests run with the app's style.
verdandi_deploy_qt_plugins(verdandi_tests)

verdandi_add_sharded_tests(verdandi_tests ${verdandiTestShards})

# Platform unit tests: deterministic tests of platform/windows code that need
# Windows SDK types or constants (e.g. Windows-to-domain mapping functions).
# They may include Windows headers but must not call Windows APIs. Kept in a
# separate target so tests/unit/ stays free of Windows headers. No Qt needed.
add_executable(verdandi_platform_tests
    platform/adapter_type_mapping_test.cpp
    platform/connectivity_hint_mapping_test.cpp
    platform/ip_address_mapping_test.cpp
    platform/network_category_mapping_test.cpp
    platform/power_transition_test.cpp
    platform/system_process_information_test.cpp
    platform/process_elevation_test.cpp
)

target_link_libraries(verdandi_platform_tests
    PRIVATE
        GTest::gtest_main
        verdandi_domain
        verdandi_platform_windows
)

# Without Qt to start, all of these run in a fraction of a second in one process.
verdandi_add_sharded_tests(verdandi_platform_tests 1)
