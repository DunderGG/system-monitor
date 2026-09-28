find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

# Collector interfaces (collector.h): standard library and domain types only, no Qt.
# platform/windows implements these interfaces and links this target rather than
# sysmon_monitoring, which would bring Qt into a module that must not depend on it.
add_library(sysmon_collector_interfaces INTERFACE
    collector.h
)

target_include_directories(sysmon_collector_interfaces
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_collector_interfaces
    INTERFACE
        sysmon_domain
)

add_library(sysmon_monitoring STATIC
    health_evaluator.cpp
    health_evaluator.h
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
        sysmon_collector_interfaces
        sysmon_domain
    PRIVATE
        spdlog::spdlog
)

