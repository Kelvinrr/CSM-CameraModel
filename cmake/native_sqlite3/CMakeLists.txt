# Minimal NATIVE (host) build of the sqlite3 command-line shell, used only to
# generate proj.db during a cross-compiled (e.g. Emscripten/WASM) build where
# the in-tree usgscsm_sqlite3_shell would itself be cross-compiled and thus
# unable to run on the host. Built via ExternalProject with the host toolchain.
#
# SQLITE3_SRC_DIR (the vendored amalgamation dir) is passed in by the caller.

cmake_minimum_required(VERSION 3.10)
project(native_sqlite3 C)

if(NOT DEFINED SQLITE3_SRC_DIR)
  message(FATAL_ERROR "SQLITE3_SRC_DIR must be set to the sqlite amalgamation dir")
endif()

add_executable(sqlite3 "${SQLITE3_SRC_DIR}/shell.c" "${SQLITE3_SRC_DIR}/sqlite3.c")
target_include_directories(sqlite3 PRIVATE "${SQLITE3_SRC_DIR}")
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1)

# dl provides sqlite's optional extension loading on Unix; harmless if absent.
find_library(DL_LIB dl)
if(DL_LIB)
  target_link_libraries(sqlite3 PRIVATE ${DL_LIB})
endif()

install(TARGETS sqlite3 RUNTIME DESTINATION bin)
