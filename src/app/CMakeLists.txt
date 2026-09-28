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
        sysmon_app_logging
        sysmon_monitoring
        sysmon_platform_windows
        sysmon_ui
)

if(WIN32)
    # vcpkg's qtbase port does not ship windeployqt, so qt_generate_deploy_app_script()
    # has no deploy tool to call. VCPKG_APPLOCAL_DEPS already copies the linked Qt DLLs;
    # only the platform plugin, which is loaded dynamically rather than linked, needs
    # to be placed manually next to the executable.
    add_custom_command(TARGET system_monitor POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:system_monitor>/platforms"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:system_monitor>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        COMMENT "Deploying Qt platform plugin"
    )
endif()
