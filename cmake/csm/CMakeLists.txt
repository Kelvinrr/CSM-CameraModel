# Static CSM API from pristine upstream sources, whose own CMakeLists hardcodes SHARED.
# Reached by WASM, which has no loader to share a csmapi through, and by
# USGSCSM_STATIC_CSM, which builds a self-contained plugin for hosts that cannot supply
# csmapi. Both give the plugin a private csm::Plugin::theList, so it cannot register with
# a host holding its own csmapi -- native builds default to conda's shared csmapi for that
# reason. The parent sets CSM_SRC_DIR and CSM_PARENT_INCLUDE_DIR; it may point them at a
# downloaded release rather than the submodule.

cmake_minimum_required(VERSION 3.10)

if(NOT CSM_SRC_DIR)
  set(CSM_SRC_DIR "${CMAKE_SOURCE_DIR}/external/csm")
endif()

if(NOT CSM_PARENT_INCLUDE_DIR)
  set(CSM_PARENT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external")
endif()

if(NOT EXISTS "${CSM_SRC_DIR}/csm.h")
  message(FATAL_ERROR
    "CSM sources not found at ${CSM_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

# Globbed rather than listed: the file set differs between CSM releases (3.1.0 adds the
# correlation-function family), and this tree is pinned third-party source, not ours.
file(GLOB CSM_SRC_FILES "${CSM_SRC_DIR}/*.cpp")

add_library(csmapi STATIC ${CSM_SRC_FILES})

# PIC so the static lib links into the usgscsm shared library.
set_target_properties(csmapi PROPERTIES POSITION_INDEPENDENT_CODE ON)

# PUBLIC: without CSM_LIBRARY, csm.h makes every symbol __declspec(dllimport) and
# MSVC looks for import stubs a static archive has none of.
target_compile_definitions(csmapi PUBLIC CSM_LIBRARY $<$<PLATFORM_ID:Linux>:LINUX_BUILD>)

target_include_directories(csmapi
    PRIVATE $<BUILD_INTERFACE:${CSM_SRC_DIR}>
    PUBLIC $<BUILD_INTERFACE:${CSM_PARENT_INCLUDE_DIR}>)
