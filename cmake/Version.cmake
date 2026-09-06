# Derives version metadata for the build.
#
# RED_VERSION        -> "5.0.0" (from project(VERSION))
# RED_GIT_REVISION   -> short hash or "unknown"
#
# These are handed to the code via a generated header (see src/app/CMakeLists.txt),
# never via string literals in sources.

set(RED_VERSION "${PROJECT_VERSION}")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE RED_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT RED_GIT_REVISION)
    set(RED_GIT_REVISION "unknown")
endif()

message(STATUS "red ${RED_VERSION} (${RED_GIT_REVISION})")
