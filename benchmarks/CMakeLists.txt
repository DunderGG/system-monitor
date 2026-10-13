find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)
find_package(spdlog CONFIG REQUIRED)

# Opt-in tick-cost benchmark (DC-66). Built with the project so it keeps
# compiling, but not registered with CTest: its timings depend on the machine
# and are not a pass/fail check. Run it with .\scripts\build.ps1 -Benchmark.
add_executable(system_monitor_benchmark
    performance_tick_benchmark.cpp
    # Same manifest as the app, so the benchmark runs with the UTF-8 process code page.
    ${PROJECT_SOURCE_DIR}/src/app/system_monitor.manifest
)

target_link_libraries(system_monitor_benchmark
    PRIVATE
        Qt6::Widgets
        spdlog::spdlog
        sysmon_domain
        sysmon_ui
)

if(WIN32)
    add_custom_command(TARGET system_monitor_benchmark POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:system_monitor_benchmark>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:system_monitor_benchmark>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        COMMENT "Deploying Qt platform plugin for the benchmark"
    )
endif()
