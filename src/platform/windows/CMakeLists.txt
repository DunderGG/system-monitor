find_package(spdlog CONFIG REQUIRED)

add_library(sysmon_platform_windows STATIC
    adapter_details_cache.cpp
    adapter_details_cache.h
    adapter_type_mapping.cpp
    adapter_type_mapping.h
    byte_rate.cpp
    byte_rate.h
    connectivity_collector.cpp
    connectivity_collector.h
    connectivity_hint_mapping.cpp
    connectivity_hint_mapping.h
    cpu_collector.cpp
    cpu_collector.h
    current_process.cpp
    current_process.h
    disk_collector.cpp
    disk_collector.h
    ip_address_mapping.cpp
    ip_address_mapping.h
    memory_collector.cpp
    memory_collector.h
    network_category_mapping.cpp
    network_category_mapping.h
    network_collector.cpp
    network_collector.h
    network_profile_collector.cpp
    network_profile_collector.h
    performance_info_reader.cpp
    performance_info_reader.h
    process_access_policy.cpp
    process_access_policy.h
    process_collector.cpp
    process_collector.h
    process_details_reader.cpp
    process_details_reader.h
    process_elevation.cpp
    process_elevation.h
    process_handle.cpp
    process_handle.h
    process_identity_cache.cpp
    process_identity_cache.h
    process_terminator.cpp
    process_terminator.h
    repeated_failure_log.h
    steady_clock_reader.h
    system_activity_collector.cpp
    system_activity_collector.h
    system_process_information.cpp
    system_process_information.h
    system_process_information_reader.cpp
    system_process_information_reader.h
    unicast_address.h
    uptime_collector.cpp
    uptime_collector.h
    utf8_conversion.cpp
    utf8_conversion.h
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
        ole32
        oleaut32
        powrprof
        ws2_32
)

