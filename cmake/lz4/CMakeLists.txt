# Static build of the LZ4 compression library. LZ4 is vendored as an unmodified
# submodule at external/lz4 (pinned to v1.10.0); this build glue lives in the
# main repo so the submodule stays pristine. Only the core library sources
# (external/lz4/lib/) are compiled, statically, directly into libusgscsm --
# a CSM plugin is dlopen'd at runtime and cannot depend on runtime dylibs.

cmake_minimum_required(VERSION 3.10)

set(LZ4_SRC_DIR "${CMAKE_SOURCE_DIR}/external/lz4/lib")

if(NOT EXISTS "${LZ4_SRC_DIR}/lz4.c")
  message(FATAL_ERROR
    "LZ4 submodule sources not found at ${LZ4_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

add_library(usgscsm_lz4 STATIC
    "${LZ4_SRC_DIR}/lz4.c"
    "${LZ4_SRC_DIR}/lz4hc.c"
    "${LZ4_SRC_DIR}/lz4frame.c"
    "${LZ4_SRC_DIR}/xxhash.c"
)

# Position-independent code so the static lib can be linked into the usgscsm
# shared library.
set_target_properties(usgscsm_lz4 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

# Consumers include <lz4.h> etc. from the submodule's lib directory.
target_include_directories(usgscsm_lz4 PUBLIC
    $<BUILD_INTERFACE:${LZ4_SRC_DIR}>)
