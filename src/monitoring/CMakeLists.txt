find_package(spdlog CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)

# Collector interfaces (collector.h): standard library and domain types only, no Qt.
# platform/windows implements these interfaces and links this target rather than
# verdandi_monitoring, which would bring Qt into a module that must not depend on it.
add_library(verdandi_collector_interfaces INTERFACE
    collector.h
)

target_include_directories(verdandi_collector_interfaces
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_collector_interfaces
    INTERFACE
        verdandi_domain
)

add_library(verdandi_monitoring STATIC
    health_evaluator.cpp
    health_evaluator.h
    sampling_scheduler.cpp
    sampling_scheduler.h
)

set_target_properties(verdandi_monitoring PROPERTIES
    AUTOMOC ON
)

target_include_directories(verdandi_monitoring
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_monitoring
    PUBLIC
        Qt6::Core
        verdandi_collector_interfaces
        verdandi_domain
    PRIVATE
        spdlog::spdlog
)

