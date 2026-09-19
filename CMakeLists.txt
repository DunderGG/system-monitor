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

if(BUILD_TESTING)
    add_sysmon_module(tests)
endif()
