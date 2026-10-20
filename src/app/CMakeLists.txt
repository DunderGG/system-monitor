find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

# Logging setup, in its own library so tests can exercise it.
add_library(sysmon_app_logging STATIC
    app_logging.cpp
    app_logging.h
)

target_include_directories(sysmon_app_logging
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_app_logging
    PUBLIC
        spdlog::spdlog
        sysmon_domain
)

# Logging, applying and saving the settings (DC-127), in its own library so
# tests can exercise it.
add_library(sysmon_app_settings STATIC
    settings_service.cpp
    settings_service.h
)

target_include_directories(sysmon_app_settings
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_app_settings
    PUBLIC
        sysmon_domain
        sysmon_monitoring
        sysmon_persistence
    PRIVATE
        spdlog::spdlog
        sysmon_app_logging
)

# The collectors the app registers, in their own library so the scheduler
# pipeline integration test registers the same ones.
add_library(sysmon_app_collectors STATIC
    system_collectors.cpp
    system_collectors.h
)

target_include_directories(sysmon_app_collectors
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_app_collectors
    PUBLIC
        sysmon_monitoring
    PRIVATE
        sysmon_platform_windows
)

# The manifest sets the process code page to UTF-8. MSVC merges it with the
# linker-generated manifest. system_monitor_tests embeds the same file.
qt_add_executable(system_monitor
    main.cpp
    system_monitor.manifest
)

target_link_libraries(system_monitor
    PRIVATE
        Qt6::Widgets
        spdlog::spdlog
        sysmon_app_collectors
        sysmon_app_logging
        sysmon_app_settings
        sysmon_domain
        sysmon_monitoring
        sysmon_persistence
        sysmon_platform_windows
        sysmon_ui
)

# VCPKG_APPLOCAL_DEPS copies the linked Qt DLLs; the plugins are copied here.
sysmon_deploy_qt_plugins(system_monitor)
