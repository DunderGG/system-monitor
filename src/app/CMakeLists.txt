find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Network Widgets)

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

# Keeping the monitor to one instance for each user in each logon session
# (DC-142), in its own library so tests can exercise it.
add_library(verdandi_app_single_instance STATIC
    instance_server.cpp
    instance_server.h
    single_instance.cpp
    single_instance.h
)

target_include_directories(verdandi_app_single_instance
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_app_single_instance
    PUBLIC
        Qt6::Network
        verdandi_platform_windows
    PRIVATE
        spdlog::spdlog
)

# WIN32 links a GUI program, which Windows starts without a console window;
# Qt supplies its WinMain, which calls main() (DC-134). The manifest sets the
# process code page to UTF-8, makes the process per-monitor DPI aware, declares
# Windows 10 and 11 and runs the process as its starter (DC-137). MSVC merges
# it with the linker-generated manifest. verdandi_tests and the benchmarks
# embed the same file. The resource script compiles in the application icon
# (DC-132) and the version information (DC-136).
qt_add_executable(verdandi WIN32
    main.cpp
    verdandi.manifest
    verdandi.rc
)

# Qt's entry-point library defines both WinMain and wWinMain. MSVC's linker and
# lld-link both start the program through WinMain, but lld-link, which links
# the coverage build, warns that it chose, and warnings fail the build (DC-139).
target_link_options(verdandi PRIVATE /ENTRY:WinMainCRTStartup)

target_link_libraries(verdandi
    PRIVATE
        Qt6::Widgets
        spdlog::spdlog
        verdandi_app_collectors
        verdandi_app_logging
        verdandi_app_power_events
        verdandi_app_settings
        verdandi_app_single_instance
        verdandi_domain
        verdandi_monitoring
        verdandi_persistence
        verdandi_platform_windows
        verdandi_ui
        verdandi_version
)

# The version with the commit the build is made from, which --version, the
# About box, the log and the version information give (DC-136). The commit is
# read on every build, and the header rewritten only when it changes.
find_package(Git QUIET)
set(verdandiProductVersionHeader "${CMAKE_CURRENT_BINARY_DIR}/generated/verdandi_product_version.h")
add_custom_target(verdandi_product_version
    COMMAND "${CMAKE_COMMAND}"
        "-DVERSION=${PROJECT_VERSION}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT=${verdandiProductVersionHeader}"
        -P "${PROJECT_SOURCE_DIR}/cmake/write_product_version_header.cmake"
    BYPRODUCTS "${verdandiProductVersionHeader}"
    COMMENT "Reading the commit for the product version"
    VERBATIM
)
add_dependencies(verdandi verdandi_product_version)
target_include_directories(verdandi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# VCPKG_APPLOCAL_DEPS copies the linked Qt DLLs; the plugins are copied here.
verdandi_deploy_qt_plugins(verdandi)

# In Release, the Microsoft C++ runtime that verdandi.exe and the Qt DLLs
# import goes beside them, so that a copy of the directory runs without the
# Visual C++ Redistributable (DC-137). Debug builds import the debug runtime,
# which Microsoft does not license for redistribution.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        message(FATAL_ERROR "CMake found no Microsoft C++ runtime libraries to copy beside verdandi.exe. "
                            "Install the MSVC build tools with the C++ workload (see scripts\\bootstrap.ps1).")
    endif()
    add_custom_command(TARGET verdandi POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "$<TARGET_FILE_DIR:verdandi>"
        COMMENT "Deploying the C++ runtime for verdandi"
        VERBATIM
    )
endif()

# The license, and the licenses and notices of the third-party code beside or
# in verdandi.exe, from vcpkg's copyright files (DC-138), in the directory where
# verdandi.exe is built. The notices are written again whenever verdandi.exe,
# with its DLLs, the script or a vcpkg package changes. nlohmann/json is
# compiled in; every other package ships as a DLL, which the script finds.
file(GLOB verdandiVcpkgPackageFiles
    "${VCPKG_INSTALLED_DIR}/vcpkg/info/*.list"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright"
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt"
    COMMAND "${CMAKE_COMMAND}"
        "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
        "-DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
        "-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
        "-DCOMPILED_IN_PORTS=nlohmann-json"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt"
        -P "${PROJECT_SOURCE_DIR}/cmake/write_third_party_notices.cmake"
    DEPENDS verdandi "${PROJECT_SOURCE_DIR}/cmake/write_third_party_notices.cmake" ${verdandiVcpkgPackageFiles}
    COMMENT "Writing the third-party notices for verdandi"
    VERBATIM
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy "${PROJECT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt"
    DEPENDS "${PROJECT_SOURCE_DIR}/LICENSE"
    COMMENT "Copying the license beside verdandi"
    VERBATIM
)
add_custom_target(verdandi_notices ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt" "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt"
)
