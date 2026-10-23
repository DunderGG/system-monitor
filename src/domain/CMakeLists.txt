# Domain module — `src/domain/`
#
# Header-only INTERFACE library: no compiled sources, no Qt, no Windows SDK.
# Any module that needs domain types links verdandi_domain and gets the include
# path automatically via target_link_libraries.
#
# The include root is src/ (the parent of this directory), so headers are
# included as:
#   #include "domain/system_snapshot.h"
# from outside this module, or as:
#   #include "system_snapshot.h"
# from within this module.

add_library(verdandi_domain INTERFACE)

target_include_directories(verdandi_domain
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

