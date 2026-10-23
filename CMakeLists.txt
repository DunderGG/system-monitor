cmake_minimum_required(VERSION 3.25)

project(Verdandi
    VERSION 0.1.0
    DESCRIPTION "A lightweight Windows desktop system monitor"
    HOMEPAGE_URL "https://github.com/DunderGG/verdandi"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A warning fails the build, in every preset and with every compiler, so a new
# one cannot pass CI; the linker's are errors below (DC-139).
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# The version, description, homepage and copyright, as macros in
# verdandi_version.h, which C++ code and resource scripts include through the
# verdandi_version target (DC-136). Raise the version here and in vcpkg.json
# together (DC-133).
set(VERDANDI_COPYRIGHT "Copyright (C) 2026 David Bennehag")
configure_file(cmake/verdandi_version.h.in "${PROJECT_BINARY_DIR}/generated/verdandi_version.h" @ONLY)
add_library(verdandi_version INTERFACE)
target_include_directories(verdandi_version INTERFACE "${PROJECT_BINARY_DIR}/generated")

if(MSVC)
    # Control Flow Guard and CET shadow-stack compatibility in every build, and
    # a program database in Release too, linked without the identical code
    # and unreferenced functions that /DEBUG would otherwise keep (DC-137).
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        /guard:cf
    )
    add_link_options(
        /WX
        /guard:cf
        /CETCOMPAT
        $<$<CONFIG:Release>:/DEBUG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
elseif(WIN32)
    message(FATAL_ERROR "Verdandi requires the MSVC compiler on Windows. "
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

# LLVM source-based coverage, which counts lines and branches (DC-118). The
# coverage preset turns it on and compiles with clang-cl. The options reach
# every target of this project, tests included, so that a header's inline
# functions are instrumented in every object that holds a copy. vcpkg builds
# the dependencies with MSVC and its own flags, so they are not instrumented.
# Each object names the profile runtime as a default library, which lld-link
# finds next to clang-cl, so linking needs no options.
option(VERDANDI_COVERAGE "Instrument the project's targets for LLVM source-based coverage (clang-cl only)" OFF)
if(VERDANDI_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        message(FATAL_ERROR "VERDANDI_COVERAGE needs clang-cl, but the compiler is ${CMAKE_CXX_COMPILER}. "
                            "Use '.\\scripts\\build.ps1 -Coverage', which configures the coverage preset.")
    endif()
    add_compile_options(
        -fprofile-instr-generate
        -fcoverage-mapping
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
function(verdandi_deploy_qt_plugins target)
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

function(add_verdandi_module moduleDirectory)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${moduleDirectory}/CMakeLists.txt")
        add_subdirectory("${moduleDirectory}")
    endif()
endfunction()

add_verdandi_module(src/app)
add_verdandi_module(src/domain)
add_verdandi_module(src/monitoring)
add_verdandi_module(src/platform/windows)
add_verdandi_module(src/persistence)
add_verdandi_module(src/ui)
add_verdandi_module(src/ui/charts)
add_verdandi_module(benchmarks)

if(BUILD_TESTING)
    add_verdandi_module(tests)
endif()
