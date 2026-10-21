find_package(nlohmann_json CONFIG REQUIRED)

# Persistence module: the JSON settings file (DC-123). The standard library,
# nlohmann/json and domain types only: no Qt, no Windows headers. Its headers
# show only domain and standard library types, so nlohmann/json is private.
add_library(verdandi_persistence STATIC
    settings_file.cpp
    settings_file.h
)

target_include_directories(verdandi_persistence
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(verdandi_persistence
    PUBLIC
        verdandi_domain
    PRIVATE
        nlohmann_json::nlohmann_json
)
