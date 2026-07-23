# Static build of zlib. zlib is vendored as an unmodified submodule at
# external/zlib (pinned to v1.3.1); this build glue lives in the main repo so the
# submodule stays pristine. Only the core library sources are compiled,
# statically, directly into libusgscsm -- a CSM plugin is dlopen'd at runtime and
# cannot depend on runtime dylibs, and on Windows this keeps the plugin free of a
# zlib1.dll dependency.
#
# STARDS uses zlib to (de)compress state files. Building from source (rather than
# find_package on a system/conda zlib) guarantees a single, self-contained static
# archive with position-independent code on every platform.

cmake_minimum_required(VERSION 3.10)

set(ZLIB_SRC_DIR "${CMAKE_SOURCE_DIR}/external/zlib")

if(NOT EXISTS "${ZLIB_SRC_DIR}/zlib.h")
  message(FATAL_ERROR
    "zlib submodule sources not found at ${ZLIB_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

# zlib ships a usable checked-in zconf.h, so no configure step is needed. These
# are the core compression/decompression sources (the gz* file helpers are
# included so STARDS can read gzip-wrapped streams).
add_library(usgscsm_zlib STATIC
    "${ZLIB_SRC_DIR}/adler32.c"
    "${ZLIB_SRC_DIR}/compress.c"
    "${ZLIB_SRC_DIR}/crc32.c"
    "${ZLIB_SRC_DIR}/deflate.c"
    "${ZLIB_SRC_DIR}/gzclose.c"
    "${ZLIB_SRC_DIR}/gzlib.c"
    "${ZLIB_SRC_DIR}/gzread.c"
    "${ZLIB_SRC_DIR}/gzwrite.c"
    "${ZLIB_SRC_DIR}/infback.c"
    "${ZLIB_SRC_DIR}/inffast.c"
    "${ZLIB_SRC_DIR}/inflate.c"
    "${ZLIB_SRC_DIR}/inftrees.c"
    "${ZLIB_SRC_DIR}/trees.c"
    "${ZLIB_SRC_DIR}/uncompr.c"
    "${ZLIB_SRC_DIR}/zutil.c"
)

# Position-independent code so the static lib links into the usgscsm shared
# library (mandatory on ELF; the earlier CI failure was a non-PIC system libz.a).
set_target_properties(usgscsm_zlib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

# On Windows, silence zlib's CRT deprecation warnings (fopen/sprintf) so a
# warnings-as-errors consumer build doesn't trip on vendored code.
if(MSVC)
  target_compile_definitions(usgscsm_zlib PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
endif()

# Consumers include <zlib.h>/<zconf.h> from the submodule root.
target_include_directories(usgscsm_zlib PUBLIC
    $<BUILD_INTERFACE:${ZLIB_SRC_DIR}>)
