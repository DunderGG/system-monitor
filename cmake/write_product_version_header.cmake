# Writes OUTPUT, a header that defines VERDANDI_PRODUCT_VERSION as VERSION
# followed by the commit checked out in SOURCE_DIR, such as "0.1.0 (edbcb13)",
# or "0.1.0 (edbcb13-dirty)" when tracked files have changes (DC-136). Without
# GIT_EXECUTABLE, or outside a git checkout, it is VERSION alone. The build runs
# this script every time, and the file is rewritten only when its content
# changes, so that only the files that include it are compiled again.
#
#   cmake -D VERSION=<version> -D GIT_EXECUTABLE=<git> -D SOURCE_DIR=<dir>
#         -D OUTPUT=<header> -P write_product_version_header.cmake

set(productVersion "${VERSION}")
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --short HEAD
        RESULT_VARIABLE result
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(result EQUAL 0 AND NOT commit STREQUAL "")
        # Without --no-optional-locks, git status may lock the index to refresh
        # it, and a git command run during the build would find it locked.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" --no-optional-locks status --porcelain --untracked-files=no
            RESULT_VARIABLE result
            OUTPUT_VARIABLE changes
            ERROR_QUIET
        )
        if(result EQUAL 0 AND NOT changes STREQUAL "")
            string(APPEND commit "-dirty")
        endif()
        string(APPEND productVersion " (${commit})")
    endif()
endif()

file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT [[
#pragma once

// Generated at build time by cmake/write_product_version_header.cmake (DC-136).
// verdandi.rc includes it too, so it holds only macros.

#define VERDANDI_PRODUCT_VERSION "@productVersion@"
]] @ONLY)
