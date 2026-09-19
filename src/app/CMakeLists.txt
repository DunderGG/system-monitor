find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)

qt_add_executable(system_monitor
    main.cpp
)

target_link_libraries(system_monitor
    PRIVATE
        Qt6::Widgets
)
