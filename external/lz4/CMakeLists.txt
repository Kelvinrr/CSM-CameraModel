# Static build of the LZ4 compression library, vendored so it is compiled
# directly into libusgscsm (a CSM plugin cannot depend on runtime dylibs).
# Only the core library sources are vendored (see lz4 v1.10.0 lib/).

cmake_minimum_required(VERSION 3.10)

add_library(usgscsm_lz4 STATIC
    lz4.c
    lz4hc.c
    lz4frame.c
    xxhash.c
)

# Position-independent code so the static lib can be linked into the usgscsm
# shared library.
set_target_properties(usgscsm_lz4 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

# Consumers include <lz4.h> etc. from this directory.
target_include_directories(usgscsm_lz4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
