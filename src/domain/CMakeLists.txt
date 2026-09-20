# Domain module — `src/domain/`
#
# Header-only INTERFACE library: no compiled sources, no Qt, no Windows SDK.
# Any module that needs domain types links sysmon_domain and gets the include
# path automatically via target_link_libraries.
#
# The include root is src/ (the parent of this directory), so headers are
# included as:
#   #include "domain/system_snapshot.h"
# from outside this module, or as:
#   #include "system_snapshot.h"
# from within this module and from unit tests that add src/domain explicitly.

add_library(sysmon_domain INTERFACE)

target_include_directories(sysmon_domain
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

