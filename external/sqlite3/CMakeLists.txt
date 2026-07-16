# Static build of SQLite3, vendored (as a submodule under upstream/) so it is
# compiled directly into libusgscsm. A CSM plugin is dlopen'd at runtime and
# cannot depend on a libsqlite3 dylib; PROJ is the only consumer of sqlite3.
#
# Defines two targets:
#   usgscsm_sqlite3        - static library linked into libusgscsm (and PROJ)
#   usgscsm_sqlite3_shell  - the sqlite3 CLI, used at build time as EXE_SQLITE3
#                            to generate proj.db from PROJ's SQL sources.

cmake_minimum_required(VERSION 3.10)

set(SQLITE3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream")

if(NOT EXISTS "${SQLITE3_SRC_DIR}/sqlite3.c")
  message(FATAL_ERROR
    "Vendored SQLite amalgamation not found at ${SQLITE3_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

# Compile-time options. PROJ requires sqlite >= 3.11 and uses column metadata
# APIs; enable threadsafe serialized mode since PROJ opens the DB with
# SQLITE_OPEN_FULLMUTEX from concurrent threads.
set(USGSCSM_SQLITE3_DEFS
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_THREADSAFE=1
)

# Static library compiled into libusgscsm and libproj.
add_library(usgscsm_sqlite3 STATIC "${SQLITE3_SRC_DIR}/sqlite3.c")
set_target_properties(usgscsm_sqlite3 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)
target_compile_definitions(usgscsm_sqlite3 PRIVATE ${USGSCSM_SQLITE3_DEFS})
# Consumers (PROJ, the embedded VFS) include <sqlite3.h> from the upstream dir.
target_include_directories(usgscsm_sqlite3 PUBLIC
    $<BUILD_INTERFACE:${SQLITE3_SRC_DIR}>)

# The sqlite3 command-line shell, used only as a build-time host tool
# (EXE_SQLITE3) to generate proj.db. Skip under Emscripten: a cross-compiled
# shell cannot run on the host; the WASM build supplies a native shell
# separately (see the WASM follow-up in the plan).
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_executable(usgscsm_sqlite3_shell
      "${SQLITE3_SRC_DIR}/shell.c"
      "${SQLITE3_SRC_DIR}/sqlite3.c")
  target_compile_definitions(usgscsm_sqlite3_shell PRIVATE ${USGSCSM_SQLITE3_DEFS})
  target_include_directories(usgscsm_sqlite3_shell PRIVATE "${SQLITE3_SRC_DIR}")
  # dl is needed by the shell's default extension-loading support on Unix.
  if(UNIX)
    find_library(DL_LIB dl)
    if(DL_LIB)
      target_link_libraries(usgscsm_sqlite3_shell PRIVATE ${DL_LIB})
    endif()
  endif()
endif()
