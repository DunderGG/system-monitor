find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

add_library(sysmon_monitoring STATIC
    collector.h
    ring_buffer.h
    sampling_scheduler.cpp
    sampling_scheduler.h
    synthetic_cpu_collector.cpp
    synthetic_cpu_collector.h
    synthetic_memory_collector.cpp
    synthetic_memory_collector.h
)

set_target_properties(sysmon_monitoring PROPERTIES
    AUTOMOC ON
)

target_include_directories(sysmon_monitoring
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_monitoring
    PUBLIC
        Qt6::Core
        sysmon_domain
    PRIVATE
        spdlog::spdlog
)

