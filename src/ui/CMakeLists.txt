find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

add_library(sysmon_ui STATIC
    cpu_performance_page.cpp
    cpu_performance_page.h
    dashboard_formatting.cpp
    dashboard_formatting.h
    dashboard_view.cpp
    dashboard_view.h
    disk_performance_page.cpp
    disk_performance_page.h
    main_window.cpp
    main_window.h
    memory_performance_page.cpp
    memory_performance_page.h
    network_performance_page.cpp
    network_performance_page.h
    network_view.cpp
    network_view.h
    performance_formatting.cpp
    performance_formatting.h
    performance_page.cpp
    performance_page.h
    performance_view.cpp
    performance_view.h
    processes_view.cpp
    processes_view.h
    resource_card.cpp
    resource_card.h
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
        sysmon_ui_charts
)

