find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)
find_package(spdlog CONFIG REQUIRED)

# Opt-in tick-cost benchmarks (DC-66). Built with the project so they
# keep compiling, but not registered with CTest: their timings depend on the
# machine and are not a pass/fail check. Run them with .\scripts\build.ps1 -Benchmark,
# and record the figures in docs/performance.md.
function(add_sysmon_benchmark name source)
    add_executable(${name}
        ${source}
        # Same manifest as the app, so the benchmark runs with the UTF-8 process code page.
        ${PROJECT_SOURCE_DIR}/src/app/system_monitor.manifest
    )

    target_link_libraries(${name}
        PRIVATE
            Qt6::Widgets
            spdlog::spdlog
            sysmon_domain
            sysmon_ui
    )

    # The same plugins as the app, so the benchmark paints with the app's style.
    sysmon_deploy_qt_plugins(${name})
endfunction()

add_sysmon_benchmark(system_monitor_benchmark performance_tick_benchmark.cpp)
add_sysmon_benchmark(system_monitor_process_benchmark process_table_benchmark.cpp)
