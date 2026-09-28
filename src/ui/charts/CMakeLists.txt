find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

# Custom QPainter-based chart widgets. Same dependency rules as src/ui/:
# Qt Widgets and domain types only, no monitoring, platform, or Windows headers.
add_library(sysmon_ui_charts STATIC
    sparkline_widget.cpp
    sparkline_widget.h
)

set_target_properties(sysmon_ui_charts PROPERTIES
    AUTOMOC ON
)

target_include_directories(sysmon_ui_charts
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

target_link_libraries(sysmon_ui_charts
    PUBLIC
        Qt6::Widgets
        sysmon_domain
)
