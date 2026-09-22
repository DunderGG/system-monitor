find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

add_library(sysmon_ui STATIC
    dashboard_view.cpp
    dashboard_view.h
    main_window.cpp
    main_window.h
    network_view.cpp
    network_view.h
    performance_view.cpp
    performance_view.h
    processes_view.cpp
    processes_view.h
)

set_target_properties(sysmon_ui PROPERTIES
    AUTOMOC ON
)

target_include_directories(sysmon_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(sysmon_ui
    PUBLIC
        Qt6::Widgets
        sysmon_domain
)

