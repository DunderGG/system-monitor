cmake_minimum_required(VERSION 3.25)

project(SystemMonitor
    VERSION 0.1.0
    DESCRIPTION "A lightweight Windows desktop system monitor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
    )
elseif(WIN32)
    message(FATAL_ERROR "System Monitor requires the MSVC compiler on Windows. "
                        "Please run from a Developer PowerShell / Developer Command Prompt, "
                        "or use '.\\scripts\\build.ps1' which initializes the MSVC environment automatically.")
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

include(CTest)

# Copies the Qt plugins that Qt loads at run time, rather than links, into
# the directories next to target where Qt looks for them: the Windows platform
# plugin, and the style plugin that provides the windows11 and windowsvista
# styles (DC-104). vcpkg's qtbase port does not ship windeployqt, so
# qt_generate_deploy_app_script() has no deploy tool to call, and
# VCPKG_APPLOCAL_DEPS copies only the linked DLLs. The caller must have found
# Qt6 Widgets, which imports both plugin targets.
function(sysmon_deploy_qt_plugins target)
    if(NOT WIN32)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${target}>/platforms"
            "$<TARGET_FILE_DIR:${target}>/styles"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:${target}>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>"
            "$<TARGET_FILE_DIR:${target}>/styles/$<TARGET_FILE_NAME:Qt6::QModernWindowsStylePlugin>"
        COMMENT "Deploying Qt plugins for ${target}"
    )
endfunction()

function(add_sysmon_module moduleDirectory)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${moduleDirectory}/CMakeLists.txt")
        add_subdirectory("${moduleDirectory}")
    endif()
endfunction()

add_sysmon_module(src/app)
add_sysmon_module(src/domain)
add_sysmon_module(src/monitoring)
add_sysmon_module(src/platform/windows)
add_sysmon_module(src/persistence)
add_sysmon_module(src/ui)
add_sysmon_module(src/ui/charts)
add_sysmon_module(benchmarks)

if(BUILD_TESTING)
    add_sysmon_module(tests)
endif()
