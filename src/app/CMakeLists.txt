find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Widgets)

# Logging setup, in its own library so tests can exercise it.
add_library(verdandi_app_logging STATIC
    app_logging.cpp
    app_logging.h
)

target_include_directories(verdandi_app_logging
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_app_logging
    PUBLIC
        spdlog::spdlog
        verdandi_domain
)

# Logging, applying and saving the settings (DC-127), in its own library so
# tests can exercise it.
add_library(verdandi_app_settings STATIC
    settings_service.cpp
    settings_service.h
)

target_include_directories(verdandi_app_settings
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_app_settings
    PUBLIC
        verdandi_domain
        verdandi_monitoring
        verdandi_persistence
    PRIVATE
        spdlog::spdlog
        verdandi_app_logging
)

# The collectors the app registers, in their own library so the scheduler
# pipeline integration test registers the same ones.
add_library(verdandi_app_collectors STATIC
    system_collectors.cpp
    system_collectors.h
)

target_include_directories(verdandi_app_collectors
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_app_collectors
    PUBLIC
        verdandi_monitoring
    PRIVATE
        verdandi_platform_windows
)

# Passing the system's suspend and resume to the scheduler (DC-129), in its
# own library so tests can exercise it.
add_library(verdandi_app_power_events STATIC
    power_event_filter.cpp
    power_event_filter.h
)

target_include_directories(verdandi_app_power_events
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_app_power_events
    PUBLIC
        Qt6::Core
        verdandi_monitoring
    PRIVATE
        verdandi_platform_windows
)

# The manifest sets the process code page to UTF-8 and makes the process
# per-monitor DPI aware. MSVC merges it with the linker-generated manifest.
# verdandi_tests and the benchmarks embed the same file. The resource
# script compiles in the application icon (DC-132).
qt_add_executable(verdandi
    main.cpp
    verdandi.manifest
    verdandi.rc
)

target_link_libraries(verdandi
    PRIVATE
        Qt6::Widgets
        spdlog::spdlog
        verdandi_app_collectors
        verdandi_app_logging
        verdandi_app_power_events
        verdandi_app_settings
        verdandi_domain
        verdandi_monitoring
        verdandi_persistence
        verdandi_platform_windows
        verdandi_ui
)

# VCPKG_APPLOCAL_DEPS copies the linked Qt DLLs; the plugins are copied here.
verdandi_deploy_qt_plugins(verdandi)
