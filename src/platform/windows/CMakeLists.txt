find_package(spdlog CONFIG REQUIRED)

add_library(sysmon_platform_windows STATIC
    cpu_collector.cpp
    cpu_collector.h
    disk_collector.cpp
    disk_collector.h
    memory_collector.cpp
    memory_collector.h
)

target_include_directories(sysmon_platform_windows
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

target_link_libraries(sysmon_platform_windows
    PUBLIC
        sysmon_domain
    PRIVATE
        spdlog::spdlog
)

