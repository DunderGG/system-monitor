find_package(spdlog CONFIG REQUIRED)

add_library(sysmon_platform_windows STATIC
    adapter_details_cache.cpp
    adapter_details_cache.h
    connectivity_collector.cpp
    connectivity_collector.h
    connectivity_hint_mapping.cpp
    connectivity_hint_mapping.h
    cpu_collector.cpp
    cpu_collector.h
    disk_collector.cpp
    disk_collector.h
    memory_collector.cpp
    memory_collector.h
    network_collector.cpp
    network_collector.h
    performance_info_reader.cpp
    performance_info_reader.h
    repeated_failure_log.h
    steady_clock_reader.h
    system_activity_collector.cpp
    system_activity_collector.h
    uptime_collector.cpp
    uptime_collector.h
)

target_include_directories(sysmon_platform_windows
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# The collectors implement the interfaces in monitoring/collector.h. That header
# is the only part of monitoring this module may use.
target_link_libraries(sysmon_platform_windows
    PUBLIC
        sysmon_collector_interfaces
        sysmon_domain
    PRIVATE
        spdlog::spdlog
        iphlpapi
        powrprof
        ws2_32
)

