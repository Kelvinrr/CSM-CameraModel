cmake_minimum_required(VERSION 3.10)
project(usgscsm VERSION 2.0.1 DESCRIPTION "usgscsm library")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

message(STATUS "CMake Module Path: " ${CMAKE_MODULE_PATH})

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)

# For dependencies minimum version
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Detect Emscripten and configure for WebAssembly
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  message(STATUS "Building for WebAssembly with Emscripten")
  set(EMSCRIPTEN ON)
  set(USGSCSM_WASM_BUILD ON)
  # Disable unnecessary components for WASM
  set(USGSCSM_BUILD_TESTS OFF CACHE BOOL "Disable tests for WASM" FORCE)
  set(USGSCSM_BUILD_DOCS OFF CACHE BOOL "Disable docs for WASM" FORCE)
  set(BUILD_APPS OFF CACHE BOOL "Disable PROJ apps" FORCE)
  set(ENABLE_CURL OFF CACHE BOOL "No curl in WASM" FORCE)
  set(ENABLE_TIFF OFF CACHE BOOL "No TIFF in WASM" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static libs for WASM" FORCE)
  set(CMAKE_USE_PTHREADS_INIT OFF)  # Disable threading for WASM
  # Use embedded dependencies for WASM
  set(USGSCSM_EXTERNAL_DEPS OFF CACHE BOOL "Use embedded deps for WASM" FORCE)
  # Override optimization flags to avoid binaryen version mismatch
  set(CMAKE_CXX_FLAGS_RELEASE "-O1 -DNDEBUG" CACHE STRING "" FORCE)
  set(CMAKE_C_FLAGS_RELEASE "-O1 -DNDEBUG" CACHE STRING "" FORCE)
  # Conda's default compiler flags include -fstack-protector-strong, but
  # emscripten's wasm-ld provides no __stack_chk_guard/__stack_chk_fail, so PROJ's
  # C sources fail to link. Strip stack-protector from the inherited flags and
  # disable it explicitly for the WASM build.
  string(REGEX REPLACE "-fstack-protector[a-z-]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-fstack-protector[a-z-]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector")
endif()

# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
endif()

set(VISIBILITY_INLINES_HIDDEN 1)

# Use external or embedded dependencies
option(USGSCSM_EXTERNAL_DEPS "If the library should be built with external or embedded dependencies" OFF)
option(ENABLE_CURL "Set to build the curl components of proj" OFF)
option(ENABLE_TIFF "Set to build the TIFF components of proj" OFF)
option(BUILD_TESTING "Set to build the proj tests" OFF)
option(BUILD_APPS "Set to build the proj apps" OFF)

# STARDS state-file input support. Off by default: enabling it compiles the
# header-only STARDS reader (external/stards) into usgscsm and requires the
# STARDS compression dependencies to be available/embedded (no dynamic linking).
option(USGSCSM_ENABLE_STARDS "Build support for instantiating models from STARDS state files" ON)

# To find JSON's config file
set (CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_PREFIX_PATH}")

set(USGSCSM_SRC_FILES src/UsgsAstroPlugin.cpp
                      src/UsgsAstroFrameSensorModel.cpp
                      src/UsgsAstroPushFrameSensorModel.cpp
                      src/UsgsAstroLsSensorModel.cpp
                      src/UsgsAstroProjectedSensorModel.cpp
                      src/UsgsAstroSarSensorModel.cpp
                      src/Distortion.cpp
                      src/Utilities.cpp
                      src/VariantMap.cpp
                      src/EigenUtilities.cpp
)

set(USGSCSM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include/usgscsm"
                         "${CMAKE_CURRENT_SOURCE_DIR}/include"
                         "${CMAKE_CURRENT_SOURCE_DIR}"
                         # Submodules now live under external/; this makes the
                         # angle/quote includes that are relative to a submodule
                         # root resolve, e.g. #include "csm/Plugin.h".
                         "${CMAKE_CURRENT_SOURCE_DIR}/external"
)

if(USGSCSM_EXTERNAL_DEPS)
  message(STATUS "Using external dependencies")
  # CSM API library
  find_path(CSM_INCLUDE_DIR NAMES "csm.h"
                            PATH_SUFFIXES "csm"
                            PATHS $ENV{CONDA_PREFIX}/include/)
  find_library(CSM_LIBRARY csmapi PATHS $ENV{CONDA_PREFIX}/lib)
  message("-- Found external CSM Library: ${CSM_LIBRARY}")
  message("-- Found external CSM Include Directory: ${CSM_INCLUDE_DIR}")

  # Nlohmann JSON
  find_package(nlohmann_json REQUIRED)

  # Eigen
  find_package(Eigen3 3.3 REQUIRED NO_MODULE)

  # ALE
  find_package(ale REQUIRED CONFIG)
  set(ALE_TARGET ale::ale)

  # Proj
  find_package(PROJ REQUIRED CONFIG)
  set(PROJ_TARGET PROJ::proj)
  add_library(usgscsm SHARED ${USGSCSM_SRC_FILES})
else()
  message(STATUS "Using embedded dependencies")
  # CSM API library
  add_subdirectory(external/csm)
  # Directory holding the CSM headers (RasterGM.h, Error.h, ...). Consumers that
  # include them bare (e.g. #include <RasterGM.h>) need this on their path.
  set(CSM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/csm")
  set(CSM_LIBRARY csmapi)

  # PROJ (for the projected sensor model) is built for both native and WASM.
  # Its database, proj.db, is embedded and served from memory via a custom
  # SQLite VFS, so no runtime file or PROJ_DATA is needed on either platform.
  if(TRUE)
    # First configure proj library
    set(BUILD_SHARED_LIBS OFF)
    set(PROJ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/PROJ")
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/PROJ/cmake ${CMAKE_MODULE_PATH})
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/PROJ/src ${CMAKE_MODULE_PATH})

    ################################################################################
    # SQLite3 (vendored, static)
    ################################################################################
    # Build the vendored SQLite3 statically into the plugin (no runtime dylib) and
    # use its command-line shell as the build-time proj.db generator, so no host
    # sqlite3 binary or libsqlite3 dylib is required.
    add_subdirectory(external/sqlite3)

    if(NOT EMSCRIPTEN)
      # Native build: the vendored shell runs on the host directly.
      set(EXE_SQLITE3 $<TARGET_FILE:usgscsm_sqlite3_shell>)
    else()
      # WASM build: the vendored shell would be cross-compiled to WASM and cannot
      # run on the host to generate proj.db. Use a host sqlite3 instead -- prefer
      # one already on PATH, else build a native shell from the vendored
      # amalgamation with the host toolchain via ExternalProject.
      find_program(HOST_SQLITE3 sqlite3)
      if(HOST_SQLITE3)
        set(EXE_SQLITE3 "${HOST_SQLITE3}")
        message(STATUS "WASM: using host sqlite3 for proj.db generation: ${EXE_SQLITE3}")
      else()
        include(ExternalProject)
        set(NATIVE_SQLITE3_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/native_sqlite3")
        ExternalProject_Add(native_sqlite3
          SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/native_sqlite3"
          CMAKE_ARGS
            -DCMAKE_TOOLCHAIN_FILE=
            -DCMAKE_BUILD_TYPE=Release
            "-DSQLITE3_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/external/sqlite3/upstream"
            "-DCMAKE_INSTALL_PREFIX=${NATIVE_SQLITE3_PREFIX}"
          BUILD_BYPRODUCTS "${NATIVE_SQLITE3_PREFIX}/bin/sqlite3")
        set(EXE_SQLITE3 "${NATIVE_SQLITE3_PREFIX}/bin/sqlite3")
        set(NATIVE_SQLITE3_TARGET native_sqlite3)
        message(STATUS "WASM: building native sqlite3 for proj.db generation")
      endif()
    endif()

  include(ProjUtilities)
  include(Ccache)
  ################################################################################
  # ProjConfig.cmake - CMake build configuration of PROJ library
  ################################################################################
  # Copyright (C) 2010 Mateusz Loskot <mateusz@loskot.net>
  #
  # Distributed under the Boost Software License, Version 1.0.
  # (See accompanying file LICENSE_1_0.txt or copy at
  # https://www.boost.org/LICENSE_1_0.txt)
  ################################################################################
  include(CheckLibraryExists)
  include(CheckFunctionExists)

  # if C flags have -Werror, temporarily remove these while running some checks
  string(FIND "${CMAKE_C_FLAGS}" "-Werror" FIND_WERROR)
  if(FIND_WERROR GREATER_EQUAL 0)
    # we must be careful about not matching -Werror=something, so let's append
    # a space at the end of CMAKE_C_FLAGS and match -Werror with a trailing space
    string(REPLACE "-Werror " " " _tmp_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ")
    set(_prev_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${_tmp_CMAKE_C_FLAGS}")
  endif()

  # check needed include file
  check_function_exists(localeconv HAVE_LOCALECONV)
  check_function_exists(strerror HAVE_STRERROR)
  if(NOT WIN32 AND NOT EMSCRIPTEN)
    check_library_exists(dl dladdr "" HAVE_LIBDL)
    check_library_exists(m exp "" HAVE_LIBM)
  endif()
  # Emscripten has no libdl; force HAVE_LIBDL off so the only <dlfcn.h>/dladdr
  # use in PROJ (filemanager.cpp, runtime data-file discovery -- irrelevant here
  # since proj.db is served from the embedded VFS) is compiled out.
  if(EMSCRIPTEN)
    set(HAVE_LIBDL "" CACHE INTERNAL "" FORCE)
  endif()

  # restore CMAKE_C_FLAGS as before
  if(FIND_WERROR GREATER_EQUAL 0)
    set(CMAKE_C_FLAGS "${_prev_CMAKE_C_FLAGS}")
  endif()

  # check if a second proj_config.h exists (created by ./configure)
  # as this is within CMake's C_INCLUDES / CXX_INCLUDES
  set(AUTOCONF_PROJ_CONFIG_H "${PROJ_SOURCE_DIR}/src/proj_config.h")
  if(EXISTS ${AUTOCONF_PROJ_CONFIG_H})
    message(WARNING
      "Autoconf's ${AUTOCONF_PROJ_CONFIG_H} may interfere with this "
      "CMake build. Run 'make distclean' in the source directory "
      "before CMake's build.")
  endif()

  message("${CMAKE_CURRENT_SOURCE_DIR}")
  configure_file(${PROJ_SOURCE_DIR}/cmake/proj_config.cmake.in ${PROJ_SOURCE_DIR}/src/proj_config.h)
  include(ProjMac)
  include(policies)

  set(PROJ_DATA_PATH "${CMAKE_INSTALL_FULL_DATADIR}/usgscsm")

  include_directories(${PROJ_SOURCE_DIR}/src)
  add_subdirectory(external/PROJ/data)

  # proj.db is generated by running EXE_SQLITE3, so that tool must be built
  # first. Native builds use the in-tree vendored shell; WASM builds use a host
  # sqlite3 (found on PATH, or the native ExternalProject shell).
  if(NOT EMSCRIPTEN)
    add_dependencies(generate_proj_db usgscsm_sqlite3_shell)
  elseif(DEFINED NATIVE_SQLITE3_TARGET)
    add_dependencies(generate_proj_db ${NATIVE_SQLITE3_TARGET})
  endif()

  # Embed the freshly generated proj.db as a C byte array so it is compiled into
  # libusgscsm and served from memory via the custom SQLite VFS (src/ProjDbVfs.cpp).
  # No loose proj.db or PROJ_DATA is needed at runtime.
  set(PROJ_DB_FILE "${CMAKE_CURRENT_BINARY_DIR}/external/PROJ/data/proj.db")
  set(PROJ_DB_DATA_C "${CMAKE_CURRENT_BINARY_DIR}/proj_db_data.cpp")
  add_custom_command(
    OUTPUT ${PROJ_DB_DATA_C}
    COMMAND ${CMAKE_COMMAND} "-DPROJ_DB=${PROJ_DB_FILE}" "-DOUT_C=${PROJ_DB_DATA_C}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_proj_db.cmake"
    DEPENDS generate_proj_db ${PROJ_DB_FILE}
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_proj_db.cmake"
    COMMENT "Embedding proj.db into libusgscsm"
    VERBATIM)
  # Sources added to the usgscsm library for the embedded-PROJ path.
  set(USGSCSM_EMBED_PROJ_DB_SRCS ${PROJ_DB_DATA_C} src/ProjDbVfs.cpp)

  message(STATUS "Configuring proj library:")

  ##############################################
  ### SWITCH BETWEEN STATIC OR SHARED LIBRARY###
  ##############################################

  # default config is shared
  option(BUILD_SHARED_LIBS
    "Build PROJ library shared." OFF)

  find_package(Threads QUIET)
  if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    add_definitions(-DPROJ_HAS_PTHREADS)
  endif()

  option(ENABLE_IPO
    "Build library with interprocedural optimization (if available)." OFF)
  if(ENABLE_IPO)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ENABLE_IPO)
  endif()
  print_variable(ENABLE_IPO)


  ##############################################
  ###  library source list and include_list  ###
  ##############################################

  set(SRC_LIBPROJ_PROJECTIONS
    ${PROJ_SOURCE_DIR}/src/projections/aeqd.cpp
    ${PROJ_SOURCE_DIR}/src/projections/adams.cpp
    ${PROJ_SOURCE_DIR}/src/projections/gnom.cpp
    ${PROJ_SOURCE_DIR}/src/projections/laea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/mod_ster.cpp
    ${PROJ_SOURCE_DIR}/src/projections/nsper.cpp
    ${PROJ_SOURCE_DIR}/src/projections/nzmg.cpp
    ${PROJ_SOURCE_DIR}/src/projections/ortho.cpp
    ${PROJ_SOURCE_DIR}/src/projections/stere.cpp
    ${PROJ_SOURCE_DIR}/src/projections/sterea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/aea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/bipc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/bonne.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eqdc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/isea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/ccon.cpp
    ${PROJ_SOURCE_DIR}/src/projections/imw_p.cpp
    ${PROJ_SOURCE_DIR}/src/projections/krovak.cpp
    ${PROJ_SOURCE_DIR}/src/projections/lcc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/poly.cpp
    ${PROJ_SOURCE_DIR}/src/projections/rpoly.cpp
    ${PROJ_SOURCE_DIR}/src/projections/sconics.cpp
    ${PROJ_SOURCE_DIR}/src/projections/rouss.cpp
    ${PROJ_SOURCE_DIR}/src/projections/cass.cpp
    ${PROJ_SOURCE_DIR}/src/projections/cc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/cea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eqc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/gall.cpp
    ${PROJ_SOURCE_DIR}/src/projections/labrd.cpp
    ${PROJ_SOURCE_DIR}/src/projections/som.cpp
    ${PROJ_SOURCE_DIR}/src/projections/merc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/mill.cpp
    ${PROJ_SOURCE_DIR}/src/projections/ocea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/omerc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/somerc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/tcc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/tcea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/times.cpp
    ${PROJ_SOURCE_DIR}/src/projections/tmerc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/tobmerc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/airy.cpp
    ${PROJ_SOURCE_DIR}/src/projections/aitoff.cpp
    ${PROJ_SOURCE_DIR}/src/projections/august.cpp
    ${PROJ_SOURCE_DIR}/src/projections/bacon.cpp
    ${PROJ_SOURCE_DIR}/src/projections/bertin1953.cpp
    ${PROJ_SOURCE_DIR}/src/projections/chamb.cpp
    ${PROJ_SOURCE_DIR}/src/projections/hammer.cpp
    ${PROJ_SOURCE_DIR}/src/projections/lagrng.cpp
    ${PROJ_SOURCE_DIR}/src/projections/larr.cpp
    ${PROJ_SOURCE_DIR}/src/projections/lask.cpp
    ${PROJ_SOURCE_DIR}/src/projections/latlong.cpp
    ${PROJ_SOURCE_DIR}/src/projections/nicol.cpp
    ${PROJ_SOURCE_DIR}/src/projections/ob_tran.cpp
    ${PROJ_SOURCE_DIR}/src/projections/oea.cpp
    ${PROJ_SOURCE_DIR}/src/projections/tpeqd.cpp
    ${PROJ_SOURCE_DIR}/src/projections/vandg.cpp
    ${PROJ_SOURCE_DIR}/src/projections/vandg2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/vandg4.cpp
    ${PROJ_SOURCE_DIR}/src/projections/wag7.cpp
    ${PROJ_SOURCE_DIR}/src/projections/lcca.cpp
    ${PROJ_SOURCE_DIR}/src/projections/geos.cpp
    ${PROJ_SOURCE_DIR}/src/projections/boggs.cpp
    ${PROJ_SOURCE_DIR}/src/projections/collg.cpp
    ${PROJ_SOURCE_DIR}/src/projections/comill.cpp
    ${PROJ_SOURCE_DIR}/src/projections/crast.cpp
    ${PROJ_SOURCE_DIR}/src/projections/denoy.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eck1.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eck2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eck3.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eck4.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eck5.cpp
    ${PROJ_SOURCE_DIR}/src/projections/fahey.cpp
    ${PROJ_SOURCE_DIR}/src/projections/fouc_s.cpp
    ${PROJ_SOURCE_DIR}/src/projections/gins8.cpp
    ${PROJ_SOURCE_DIR}/src/projections/gstmerc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/gn_sinu.cpp
    ${PROJ_SOURCE_DIR}/src/projections/goode.cpp
    ${PROJ_SOURCE_DIR}/src/projections/igh.cpp
    ${PROJ_SOURCE_DIR}/src/projections/igh_o.cpp
    ${PROJ_SOURCE_DIR}/src/projections/imoll.cpp
    ${PROJ_SOURCE_DIR}/src/projections/imoll_o.cpp
    ${PROJ_SOURCE_DIR}/src/projections/hatano.cpp
    ${PROJ_SOURCE_DIR}/src/projections/loxim.cpp
    ${PROJ_SOURCE_DIR}/src/projections/mbt_fps.cpp
    ${PROJ_SOURCE_DIR}/src/projections/mbtfpp.cpp
    ${PROJ_SOURCE_DIR}/src/projections/mbtfpq.cpp
    ${PROJ_SOURCE_DIR}/src/projections/moll.cpp
    ${PROJ_SOURCE_DIR}/src/projections/nell.cpp
    ${PROJ_SOURCE_DIR}/src/projections/nell_h.cpp
    ${PROJ_SOURCE_DIR}/src/projections/patterson.cpp
    ${PROJ_SOURCE_DIR}/src/projections/putp2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/putp3.cpp
    ${PROJ_SOURCE_DIR}/src/projections/putp4p.cpp
    ${PROJ_SOURCE_DIR}/src/projections/putp5.cpp
    ${PROJ_SOURCE_DIR}/src/projections/putp6.cpp
    ${PROJ_SOURCE_DIR}/src/projections/qsc.cpp
    ${PROJ_SOURCE_DIR}/src/projections/robin.cpp
    ${PROJ_SOURCE_DIR}/src/projections/s2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/sch.cpp
    ${PROJ_SOURCE_DIR}/src/projections/sts.cpp
    ${PROJ_SOURCE_DIR}/src/projections/urm5.cpp
    ${PROJ_SOURCE_DIR}/src/projections/urmfps.cpp
    ${PROJ_SOURCE_DIR}/src/projections/wag2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/wag3.cpp
    ${PROJ_SOURCE_DIR}/src/projections/wink1.cpp
    ${PROJ_SOURCE_DIR}/src/projections/wink2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/healpix.cpp
    ${PROJ_SOURCE_DIR}/src/projections/natearth.cpp
    ${PROJ_SOURCE_DIR}/src/projections/natearth2.cpp
    ${PROJ_SOURCE_DIR}/src/projections/calcofi.cpp
    ${PROJ_SOURCE_DIR}/src/projections/eqearth.cpp
    ${PROJ_SOURCE_DIR}/src/projections/col_urban.cpp
  )

  set(SRC_LIBPROJ_CONVERSIONS
    ${PROJ_SOURCE_DIR}/src/conversions/axisswap.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/cart.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/geoc.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/geocent.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/noop.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/topocentric.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/set.cpp
    ${PROJ_SOURCE_DIR}/src/conversions/unitconvert.cpp
  )

  set(SRC_LIBPROJ_TRANSFORMATIONS
    ${PROJ_SOURCE_DIR}/src/transformations/affine.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/deformation.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/gridshift.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/helmert.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/hgridshift.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/horner.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/molodensky.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/vgridshift.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/xyzgridshift.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/defmodel.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/tinshift.cpp
    ${PROJ_SOURCE_DIR}/src/transformations/vertoffset.cpp
  )

  set(SRC_LIBPROJ_ISO19111
    ${PROJ_SOURCE_DIR}/src/iso19111/static.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/util.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/metadata.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/common.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/coordinates.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/crs.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/datum.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/coordinatesystem.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/io.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/internal.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/factory.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/c_api.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/concatenatedoperation.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/coordinateoperationfactory.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/conversion.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/esriparammappings.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/oputils.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/parammappings.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/projbasedoperation.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/singleoperation.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/transformation.cpp
    ${PROJ_SOURCE_DIR}/src/iso19111/operation/vectorofvaluesparams.cpp
  )

  set(SRC_LIBPROJ_CORE
    ${PROJ_SOURCE_DIR}/src/4D_api.cpp
    ${PROJ_SOURCE_DIR}/src/aasincos.cpp
    ${PROJ_SOURCE_DIR}/src/adjlon.cpp
    ${PROJ_SOURCE_DIR}/src/auth.cpp
    ${PROJ_SOURCE_DIR}/src/ctx.cpp
    ${PROJ_SOURCE_DIR}/src/datum_set.cpp
    ${PROJ_SOURCE_DIR}/src/datums.cpp
    ${PROJ_SOURCE_DIR}/src/deriv.cpp
    ${PROJ_SOURCE_DIR}/src/dmstor.cpp
    ${PROJ_SOURCE_DIR}/src/ell_set.cpp
    ${PROJ_SOURCE_DIR}/src/ellps.cpp
    ${PROJ_SOURCE_DIR}/src/factors.cpp
    ${PROJ_SOURCE_DIR}/src/fwd.cpp
    ${PROJ_SOURCE_DIR}/src/gauss.cpp
    ${PROJ_SOURCE_DIR}/src/generic_inverse.cpp
    ${PROJ_SOURCE_DIR}/src/geodesic.c
    ${PROJ_SOURCE_DIR}/src/init.cpp
    ${PROJ_SOURCE_DIR}/src/initcache.cpp
    ${PROJ_SOURCE_DIR}/src/internal.cpp
    ${PROJ_SOURCE_DIR}/src/inv.cpp
    ${PROJ_SOURCE_DIR}/src/list.cpp
    ${PROJ_SOURCE_DIR}/src/log.cpp
    ${PROJ_SOURCE_DIR}/src/malloc.cpp
    ${PROJ_SOURCE_DIR}/src/mlfn.cpp
    ${PROJ_SOURCE_DIR}/src/msfn.cpp
    ${PROJ_SOURCE_DIR}/src/mutex.cpp
    ${PROJ_SOURCE_DIR}/src/param.cpp
    ${PROJ_SOURCE_DIR}/src/phi2.cpp
    ${PROJ_SOURCE_DIR}/src/pipeline.cpp
    ${PROJ_SOURCE_DIR}/src/pj_list.h
    ${PROJ_SOURCE_DIR}/src/pr_list.cpp
    ${PROJ_SOURCE_DIR}/src/proj_internal.h
    ${PROJ_SOURCE_DIR}/src/proj_mdist.cpp
    ${PROJ_SOURCE_DIR}/src/qsfn.cpp
    ${PROJ_SOURCE_DIR}/src/release.cpp
    ${PROJ_SOURCE_DIR}/src/rtodms.cpp
    ${PROJ_SOURCE_DIR}/src/strerrno.cpp
    ${PROJ_SOURCE_DIR}/src/strtod.cpp
    ${PROJ_SOURCE_DIR}/src/tsfn.cpp
    ${PROJ_SOURCE_DIR}/src/units.cpp
    ${PROJ_SOURCE_DIR}/src/wkt1_generated_parser.c
    ${PROJ_SOURCE_DIR}/src/wkt1_generated_parser.h
    ${PROJ_SOURCE_DIR}/src/wkt1_parser.cpp
    ${PROJ_SOURCE_DIR}/src/wkt1_parser.h
    ${PROJ_SOURCE_DIR}/src/wkt2_generated_parser.c
    ${PROJ_SOURCE_DIR}/src/wkt2_generated_parser.h
    ${PROJ_SOURCE_DIR}/src/wkt2_parser.cpp
    ${PROJ_SOURCE_DIR}/src/wkt2_parser.h
    ${PROJ_SOURCE_DIR}/src/wkt_parser.cpp
    ${PROJ_SOURCE_DIR}/src/wkt_parser.hpp
    ${PROJ_SOURCE_DIR}/src/zpoly1.cpp
    ${PROJ_SOURCE_DIR}/src/proj_json_streaming_writer.hpp
    ${PROJ_SOURCE_DIR}/src/proj_json_streaming_writer.cpp
    ${PROJ_SOURCE_DIR}/src/tracing.cpp
    ${PROJ_SOURCE_DIR}/src/grids.hpp
    ${PROJ_SOURCE_DIR}/src/grids.cpp
    ${PROJ_SOURCE_DIR}/src/filemanager.hpp
    ${PROJ_SOURCE_DIR}/src/filemanager.cpp
    ${PROJ_SOURCE_DIR}/src/networkfilemanager.cpp
    ${PROJ_SOURCE_DIR}/src/sqlite3_utils.hpp
    ${PROJ_SOURCE_DIR}/src/sqlite3_utils.cpp
    ${PROJ_SOURCE_DIR}/src/proj_config.h
  )

  set(HEADERS_LIBPROJ
    ${PROJ_SOURCE_DIR}/src/proj.h
    ${PROJ_SOURCE_DIR}/src/proj_experimental.h
    ${PROJ_SOURCE_DIR}/src/proj_constants.h
    ${PROJ_SOURCE_DIR}/src/proj_symbol_rename.h
    ${PROJ_SOURCE_DIR}/src/geodesic.h
  )

  # Group source files for IDE source explorers (e.g. Visual Studio)
  source_group("Header Files"
    FILES ${HEADERS_LIBPROJ})
  source_group("Source Files\\Core"
    FILES ${SRC_LIBPROJ_CORE})
  source_group("Source Files\\Conversions"
    FILES ${SRC_LIBPROJ_CONVERSIONS})
  source_group("Source Files\\Projections"
    FILES ${SRC_LIBPROJ_PROJECTIONS})
  source_group("Source Files\\Transformations"
    FILES ${SRC_LIBPROJ_TRANSFORMATIONS})
  source_group("Source Files\\ISO19111"
    FILES ${SRC_LIBPROJ_ISO19111})

  include_directories(${PROJ_SOURCE_DIR}/include)

  include_directories(${CMAKE_CURRENT_BINARY_DIR})
  source_group("CMake Files" FILES CMakeLists.txt)

  # Embed PROJ_DATA data files location
  add_definitions(-DPROJ_DATA="${PROJ_DATA_PATH}")


  ###########################################################
  # targets to refresh wkt1_parser.cpp and wkt2_parser.cpp
  ###########################################################

  # Those targets need to be run manually each time wkt1_grammar.y / wkt2_grammar.y
  # is modified.
  # We could of course run them automatically, but that would make building
  # PROJ harder.

  # This target checks that wkt1_grammar.y md5sum has not changed
  # If it has, then it should be updated and the generate_wkt1_parser target
  # should be manually run
  add_custom_target(check_wkt1_grammar_md5 ALL
                    COMMAND ${CMAKE_COMMAND}
                        "-DIN_FILE=wkt1_grammar.y"
                        "-DTARGET=generate_wkt1_parser"
                        "-DEXPECTED_MD5SUM=5b4495c1ec6d2ae26b7028a9bb5d8819"
                        -P "${PROJ_SOURCE_DIR}/src/check_md5sum.cmake"
                    WORKING_DIRECTORY "${PROJ_SOURCE_DIR}/src"
                    DEPENDS "${PROJ_SOURCE_DIR}/src/wkt1_grammar.y"
                    VERBATIM)

  add_custom_target(generate_wkt1_parser
                    COMMAND ${CMAKE_COMMAND}
                        "-DPREFIX=pj_wkt1_"
                        "-DIN_FILE=wkt1_grammar.y"
                        "-DOUT_FILE=wkt1_generated_parser.c"
                        -P "${PROJ_SOURCE_DIR}/src/generate_wkt_parser.cmake"
                    WORKING_DIRECTORY "${PROJ_SOURCE_DIR}/src"
                    VERBATIM)

  # This target checks that wkt2_grammar.y md5sum has not changed
  # If it has, then it should be updated and the generate_wkt2_parser target
  # should be manually run
  add_custom_target(check_wkt2_grammar_md5 ALL
                    COMMAND ${CMAKE_COMMAND}
                        "-DIN_FILE=wkt2_grammar.y"
                        "-DTARGET=generate_wkt2_parser"
                        "-DEXPECTED_MD5SUM=289572eebe9dab3c7225bd48c445c287"
                        -P "${PROJ_SOURCE_DIR}/src/check_md5sum.cmake"
                    WORKING_DIRECTORY "${PROJ_SOURCE_DIR}/src"
                    DEPENDS "${PROJ_SOURCE_DIR}/src/wkt2_grammar.y"
                    VERBATIM)

  add_custom_target(generate_wkt2_parser
                    COMMAND ${CMAKE_COMMAND}
                        "-DPREFIX=pj_wkt2_"
                        "-DIN_FILE=wkt2_grammar.y"
                        "-DOUT_FILE=wkt2_generated_parser.c"
                        -P "${PROJ_SOURCE_DIR}/src/generate_wkt_parser.cmake"
                    WORKING_DIRECTORY "${PROJ_SOURCE_DIR}/src"
                    VERBATIM)

  #################################################
  ## targets: libproj and proj_config.h
  #################################################
  set(ALL_LIBPROJ_SOURCES
    ${SRC_LIBPROJ_CORE}
    ${SRC_LIBPROJ_CONVERSIONS}
    ${SRC_LIBPROJ_PROJECTIONS}
    ${SRC_LIBPROJ_TRANSFORMATIONS}
    ${SRC_LIBPROJ_ISO19111}
  )
  set(ALL_LIBPROJ_HEADERS ${HEADERS_LIBPROJ})

  # Configuration for the core target "proj"
  proj_target_output_name(proj PROJ_CORE_TARGET_OUTPUT_NAME)

  add_library(proj OBJECT
    ${ALL_LIBPROJ_SOURCES}
    ${ALL_LIBPROJ_HEADERS}
    ${PROJ_RESOURCES}
  )

  if(MSVC OR MINGW)
      target_compile_definitions(proj PRIVATE -DNOMINMAX)
  endif()

  # Tell Intel compiler to do arithmetic accurately.  This is needed to stop the
  # compiler from ignoring parentheses in expressions like (a + b) + c and from
  # simplifying 0.0 + x to x (which is wrong if x = -0.0).
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    if(MSVC)
      set(FP_PRECISE "/fp:precise")
    else()
      set(FP_PRECISE "-fp-model precise")
    endif()
    # Apply to source files that require this option
    set_source_files_properties(
      geodesic.c
      PROPERTIES COMPILE_FLAGS ${FP_PRECISE})
  endif()

  if(ENABLE_IPO)
    set_property(TARGET proj
      PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  target_include_directories(proj INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PROJ_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

  if(WIN32)
    set_target_properties(proj
      PROPERTIES
      VERSION "${PROJ_VERSION}"
      OUTPUT_NAME "${PROJ_CORE_TARGET_OUTPUT_NAME}"
      ARCHIVE_OUTPUT_NAME proj
      CLEAN_DIRECT_OUTPUT 1)
  elseif(BUILD_FRAMEWORKS_AND_BUNDLE)
    set_target_properties(proj
      PROPERTIES
      VERSION "${PROJ_VERSION}"
      INSTALL_NAME_DIR ${PROJ_INSTALL_NAME_DIR}
      CLEAN_DIRECT_OUTPUT 1)
  else()
    set_target_properties(proj
      PROPERTIES
      VERSION "${PROJ_BUILD_VERSION}"
      SOVERSION "${PROJ_SOVERSION}"
      CLEAN_DIRECT_OUTPUT 1)
  endif()

  set_target_properties(proj
    PROPERTIES
    LINKER_LANGUAGE CXX
    CXX_VISIBILITY_PRESET hidden
  )

  ##############################################
  # Link properties
  ##############################################
  set(PROJ_LIBRARIES proj)
  # Emscripten folds libm into libc and has no libdl; skip these host-lib probes
  # so no bogus -ldl reaches wasm-ld (the dl code path is compiled out via
  # HAVE_LIBDL being unset above).
  if(UNIX AND NOT EMSCRIPTEN)
    find_library(M_LIB m)
    if(M_LIB)
      target_link_libraries(proj PRIVATE -lm)
    endif()
    find_library(DL_LIB dl)
    if(DL_LIB)
      target_link_libraries(proj PRIVATE -ldl)
    endif()
  endif()
  if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    target_link_libraries(proj PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  endif()

  # Vendored static SQLite3 (its PUBLIC include dir supplies sqlite3.h).
  target_link_libraries(proj PRIVATE usgscsm_sqlite3)

  if(NLOHMANN_JSON STREQUAL "external")
    target_compile_definitions(proj PRIVATE EXTERNAL_NLOHMANN_JSON)
    target_link_libraries(proj
      PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
  endif()

  if(BUILD_SHARED_LIBS)
    if(MSVC)
      target_compile_definitions(proj PRIVATE PROJ_MSVC_DLL_EXPORT=1)
    endif()
  else()
    target_compile_definitions(proj PUBLIC PROJ_DLL=)
  endif()

  ##############################################
  # Core configuration summary
  ##############################################
  print_variable(PROJ_CORE_TARGET_OUTPUT_NAME)
  print_variable(BUILD_SHARED_LIBS)
  print_variable(PROJ_LIBRARIES)

    set(PROJ_TARGET proj)
    set(PROJ_INCLUDE_DIR /PROJ)
  endif()  # PROJ build (native + WASM)

  # Nlohmann JSON
  add_library (nlohmann_json INTERFACE)
  add_library (nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  target_include_directories (nlohmann_json INTERFACE
                              $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/json/include>
                              $<INSTALL_INTERFACE:include>)
  set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/json/include)

  # ALE
  # Use Eigen included with ALE
  add_library (eigen INTERFACE)
  set_target_properties(eigen PROPERTIES CXX_VISIBILITY_PRESET hidden)
  add_library (Eigen3::Eigen ALIAS eigen)
  target_include_directories (eigen INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/eigen)
  set(EIGEN3_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/eigen)

  set(ALE_BUILD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/ale/include/ale")

  # Ensure the embedded ALE and nlohmann/json headers take precedence over any
  # copies a conda environment may have installed under $CONDA_PREFIX/include.
  # Conda's compiler wrapper injects -I$CONDA_PREFIX/include, which otherwise
  # shadows the vendored headers:
  #   * ale/Distortion.h: conda's older copy lacks newer enum values (e.g.
  #     KPLOSHADOWCAM), breaking the tests.
  #   * nlohmann/json.hpp: conda ships a 3.11.2 built WITHOUT the CustomBaseClass
  #     template parameter, while external/json (also 3.11.2) has it. Both stamp
  #     the same json_abi_v3_11_2 inline namespace, so basic_json mangles to
  #     different symbols (extra trailing 'v'/void arg) depending on which header
  #     a TU sees. The library (Utilities.cpp) compiles against external/json but
  #     the test TUs were picking up conda's, producing undefined-symbol link
  #     errors for every ISD getter. BEFORE forces the vendored copies first on
  #     every target's include path.
  include_directories(BEFORE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/ale/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/json/include")

  set(ALE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/InterpUtils.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/Rotation.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/Orientations.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/States.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/Util.cpp
                       ${CMAKE_CURRENT_SOURCE_DIR}/external/ale/src/Vectors.cpp)

  add_library(ale OBJECT ${ALE_SOURCE_FILES})
  set_target_properties(ale PROPERTIES CXX_VISIBILITY_PRESET hidden)
  set(ALE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/ale/include/ale/")

  target_include_directories(ale
                            PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external/ale/include>
                            $<INSTALL_INTERFACE:include>
                            PRIVATE)

  target_include_directories(ale
                             PRIVATE
                             ${ALE_INCLUDE_DIRS}
  )
  set(ALE_LINKS nlohmann_json::nlohmann_json
                Eigen3::Eigen
  )
  target_link_libraries(ale PRIVATE ${ALE_LINKS})
  set(ALE_TARGET ale)

  # PROJ is now built for WASM too, so its headers are needed on both paths.
  set(USGSCSM_INCLUDE_DIRS "${USGSCSM_INCLUDE_DIRS}"
                           "${NLOHMANN_JSON_INCLUDE_DIR}"
                           "${CMAKE_CURRENT_SOURCE_DIR}/external/ale/include"
                           "${CMAKE_CURRENT_SOURCE_DIR}/external/PROJ/include")
  
  set( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fPIC" )
  set( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC" )

  add_library(usgscsm SHARED ${USGSCSM_SRC_FILES}
                             ${USGSCSM_EMBED_PROJ_DB_SRCS}
                             $<TARGET_OBJECTS:ale>
  )
  # The projected model serves proj.db from the embedded blob via the custom VFS.
  target_link_libraries(usgscsm PRIVATE usgscsm_sqlite3)
  target_compile_definitions(usgscsm PRIVATE USGSCSM_EMBED_PROJ_DB)
endif(USGSCSM_EXTERNAL_DEPS)

set_target_properties(usgscsm PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)
set(USGSCSM_INCLUDE_DIRS "${USGSCSM_INCLUDE_DIRS}"
                         "${EIGEN3_INCLUDE_DIR}"
                         "${PROJ_INCLUDE_DIR}")

# These will be copied upon installation to ${CMAKE_INSTALL_INCLUDEDIR}/include
set(USGSCSM_INSTALL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include/usgscsm"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/include/spdlog")

target_include_directories(usgscsm
                           PUBLIC
                           ${USGSCSM_INCLUDE_DIRS}
                           ${CSM_INCLUDE_DIR}
)

# STARDS support: compile the header-only STARDS reader (from the external/StarDS
# submodule) into usgscsm. We do NOT use the submodule's own CMakeLists (it builds
# tools/tests/SWIG bindings and resolves its compression/HTTP deps via
# find_package to dynamic system libs). Instead we consume just its header via our
# own INTERFACE target and supply the dependencies the way the usgscsm plugin
# needs: vendored static libs (no runtime dylibs) natively, and the emscripten
# zlib port for WASM. This target carries the STARDS feature defines plus the
# dependency include dirs and link flags/libraries, propagated to consumers.
if(USGSCSM_ENABLE_STARDS)
  message(STATUS "STARDS support enabled")

  set(STARDS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/StarDS/StarDS/include")
  if(NOT EXISTS "${STARDS_INCLUDE_DIR}/stards.h")
    message(FATAL_ERROR
      "STARDS header not found at ${STARDS_INCLUDE_DIR}. The StarDS submodule is "
      "missing; run: git submodule update --init --recursive")
  endif()

  add_library(usgscsm_stards INTERFACE)
  target_include_directories(usgscsm_stards INTERFACE "${STARDS_INCLUDE_DIR}")

  # Compression is always available. lz4 is vendored (compiles to WASM too).
  add_subdirectory(external/lz4)
  target_compile_definitions(usgscsm_stards INTERFACE ENABLE_LZ4)
  target_link_libraries(usgscsm_stards INTERFACE usgscsm_lz4)

  if(EMSCRIPTEN)
    # zlib from the emscripten port (-sUSE_ZLIB=1 is a compile+link flag, not an
    # archive). curl/S3 (native static ExternalProject builds using macOS
    # SecureTransport + OpenSSL) do not apply to a browser sandbox, so STARDS is
    # local-file only (zlib + lz4) under WASM -- all a browser consumer can read.
    target_compile_definitions(usgscsm_stards INTERFACE ENABLE_ZLIB)
    target_compile_options(usgscsm_stards INTERFACE "-sUSE_ZLIB=1")
    target_link_options(usgscsm_stards INTERFACE "-sUSE_ZLIB=1")
  else()
    # Native: prefer a static libz.a (baked in, no runtime dylib), else shared.
    target_compile_definitions(usgscsm_stards INTERFACE ENABLE_ZLIB)
    find_path(ZLIB_STATIC_INCLUDE_DIR zlib.h PATHS ENV CONDA_PREFIX PATH_SUFFIXES include)
    find_library(ZLIB_STATIC_LIB NAMES libz.a PATHS ENV CONDA_PREFIX PATH_SUFFIXES lib)
    if(ZLIB_STATIC_LIB AND ZLIB_STATIC_INCLUDE_DIR)
      message(STATUS "STARDS zlib: static ${ZLIB_STATIC_LIB}")
      target_include_directories(usgscsm_stards INTERFACE ${ZLIB_STATIC_INCLUDE_DIR})
      target_link_libraries(usgscsm_stards INTERFACE ${ZLIB_STATIC_LIB})
    else()
      find_package(ZLIB REQUIRED)
      target_include_directories(usgscsm_stards INTERFACE ${ZLIB_INCLUDE_DIRS})
      target_link_libraries(usgscsm_stards INTERFACE ${ZLIB_LIBRARIES})
    endif()

    # curl (vsicurl/HTTP) + OpenSSL libcrypto (S3 SigV4), both vendored static.
    add_subdirectory(external/openssl)
    add_subdirectory(external/curl)
    target_compile_definitions(usgscsm_stards INTERFACE ENABLE_CURL ENABLE_S3)
    target_link_libraries(usgscsm_stards INTERFACE usgscsm_curl usgscsm_crypto)
  endif()

  # Gate that un-guards the STARDS code in usgscsm's sources. Link the STARDS
  # INTERFACE target PUBLIC so its feature defines, include dirs, and link
  # requirements propagate to executables that link libusgscsm. This is required
  # under Emscripten where -sUSE_ZLIB=1 is a link flag (not an archive): every
  # final executable with STARDS/zlib object code (usgscsm_cam_test,
  # csm_translate) must inherit it, or wasm-ld fails on undefined zlib symbols.
  target_compile_definitions(usgscsm PRIVATE USGSCSM_ENABLE_STARDS)
  target_link_libraries(usgscsm PUBLIC usgscsm_stards)
endif()

# PROJ is linked on both native and WASM (the projected model is built for both).
target_link_libraries(usgscsm
                      PUBLIC
                      nlohmann_json::nlohmann_json
                      ${CSM_LIBRARY}
                      ${ALE_TARGET}
                      PRIVATE
                      ${PROJ_TARGET})

add_executable(usgscsm_cam_test bin/usgscsm_cam_test.cc)
target_link_libraries(usgscsm_cam_test
    usgscsm
    ${CSM_LIBRARY})

# The CLI calls the STARDS helper functions (defined in libusgscsm) when STARDS
# support is enabled, so it only needs the gate define to un-guard those code
# paths. The stards.h header itself is used only inside libusgscsm.
if(USGSCSM_ENABLE_STARDS)
  target_compile_definitions(usgscsm_cam_test PRIVATE USGSCSM_ENABLE_STARDS)
endif()

install(TARGETS usgscsm LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/csmplugins/)
install(DIRECTORY ${USGSCSM_INSTALL_INCLUDE_DIRS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS usgscsm_cam_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Ensure usgscsm_cam_test can find the library in csmplugins
if(APPLE)
  set_target_properties(usgscsm_cam_test PROPERTIES
    INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}/csmplugins")
else()
  set_target_properties(usgscsm_cam_test PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/csmplugins")
endif()

# csm_translate: file-format conversion tool (json <-> msgpack <-> stards).
add_executable(csm_translate bin/csm_translate.cc)
target_link_libraries(csm_translate
    usgscsm
    ${CSM_LIBRARY})
# Only the gate define is needed; STARDS helpers live in libusgscsm.
if(USGSCSM_ENABLE_STARDS)
  target_compile_definitions(csm_translate PRIVATE USGSCSM_ENABLE_STARDS)
endif()
install(TARGETS csm_translate RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(APPLE)
  set_target_properties(csm_translate PROPERTIES
    INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}/csmplugins")
else()
  set_target_properties(csm_translate PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/csmplugins")
endif()

# Optional build tests
option (USGSCSM_BUILD_TESTS "Build tests" ON)
if(USGSCSM_BUILD_TESTS)

  include(GoogleTest)
  include(cmake/gtest.cmake)

  # Setup for GoogleTest
  find_package (Threads)

  target_link_libraries(usgscsm
                        PUBLIC
                        gtest ${CMAKE_THREAD_LIBS_INIT})
  include(CTest)
  enable_testing()
  add_subdirectory(tests)

endif()

option (USGSCSM_BUILD_DOCS "Build the USGSCSM Docs" OFF)
if(USGSCSM_BUILD_DOCS)
  add_subdirectory ("docs")
else()
  message(STATUS "Skipping Docs")
endif()

# WebAssembly target
if(EMSCRIPTEN)
  message(STATUS "Configuring WebAssembly build")

  # Clear any inherited linker flags that may be incompatible with wasm-ld
  # (e.g., macOS-specific flags from conda environments)
  set(CMAKE_EXE_LINKER_FLAGS "")
  set(CMAKE_SHARED_LINKER_FLAGS "")
  set(CMAKE_MODULE_LINKER_FLAGS "")
  message(STATUS "Cleared incompatible linker flags for WebAssembly")

  # Set sensible defaults for WASM builds
  if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release for WASM")
  endif()

  # Source files for WASM. Includes the projected sensor model: PROJ is compiled
  # to WASM and its database (proj.db) is embedded and served from memory via the
  # custom SQLite VFS (src/ProjDbVfs.cpp + the generated proj_db_data.cpp), so no
  # runtime file or PROJ_DATA is required.
  set(USGSCSM_WASM_SRC_FILES
      src/UsgsAstroPlugin.cpp
      src/UsgsAstroFrameSensorModel.cpp
      src/UsgsAstroPushFrameSensorModel.cpp
      src/UsgsAstroLsSensorModel.cpp
      src/UsgsAstroProjectedSensorModel.cpp
      src/UsgsAstroSarSensorModel.cpp
      src/Distortion.cpp
      src/Utilities.cpp
      src/VariantMap.cpp
      src/EigenUtilities.cpp
      src/wasm/usgscsm_bindings.cpp
      ${USGSCSM_EMBED_PROJ_DB_SRCS}
  )

  # Create WASM executable
  add_executable(usgscsm_wasm ${USGSCSM_WASM_SRC_FILES}
                              $<TARGET_OBJECTS:ale>)

  # Include directories
  target_include_directories(usgscsm_wasm PRIVATE
                             ${USGSCSM_INCLUDE_DIRS}
                             ${CSM_INCLUDE_DIR}
                             ${NLOHMANN_JSON_INCLUDE_DIR}
                             ${EIGEN3_INCLUDE_DIR}
  )

  # Link libraries. proj + usgscsm_sqlite3 provide the projected model; the
  # embedded proj.db is used through the in-memory VFS (USGSCSM_EMBED_PROJ_DB).
  target_link_libraries(usgscsm_wasm
                        PRIVATE
                        nlohmann_json::nlohmann_json
                        ${CSM_LIBRARY}
                        ${ALE_TARGET}
                        ${PROJ_TARGET}
                        usgscsm_sqlite3
  )
  target_compile_definitions(usgscsm_wasm PRIVATE USGSCSM_EMBED_PROJ_DB)

  # STARDS support (local-file only for WASM: zlib via emscripten port + vendored
  # lz4; curl/S3 are disabled for the browser). The usgscsm_stards INTERFACE
  # target carries the ENABLE_* defines, include dirs, and zlib/lz4 link flags.
  if(USGSCSM_ENABLE_STARDS)
    target_compile_definitions(usgscsm_wasm PRIVATE USGSCSM_ENABLE_STARDS)
    target_link_libraries(usgscsm_wasm PRIVATE usgscsm_stards)
  endif()

  # Enable exception support for WASM
  target_compile_options(usgscsm_wasm PRIVATE "-fexceptions")

  # Optimization options for WASM builds
  option(USGSCSM_WASM_OPTIMIZE "Enable WASM optimization" ON)
  option(USGSCSM_WASM_DEBUG "Build with debug symbols" OFF)

  # Base Emscripten link flags
  set(WASM_LINK_FLAGS
    "--bind"
    "-lembind"
    "-s WASM=1"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='USGSCSM'"
    "-s EXPORT_ES6=1"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=128MB"
    "-s MAXIMUM_MEMORY=512MB"
    "-s STACK_SIZE=100MB"
    "-s FILESYSTEM=1"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS']"
    "-fexceptions"
    # Async URL/fetch helpers (loadFromURL / loadFrom) layered on the bindings.
    "--post-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/usgscsm_post.js"
  )

  # Add optimization flags
  if(USGSCSM_WASM_OPTIMIZE)
    list(APPEND WASM_LINK_FLAGS "-O2")
    message(STATUS "WASM optimization enabled: -O2")
  else()
    list(APPEND WASM_LINK_FLAGS "-O1")
    message(STATUS "WASM optimization disabled: -O1")
  endif()

  # Add debug flags
  if(USGSCSM_WASM_DEBUG)
    list(APPEND WASM_LINK_FLAGS "-g" "-gsource-map")
    message(STATUS "WASM debug symbols enabled")
  endif()

  # Convert list to string
  string(REPLACE ";" " " WASM_LINK_FLAGS_STR "${WASM_LINK_FLAGS}")

  # Apply link flags
  set_target_properties(usgscsm_wasm PROPERTIES
    OUTPUT_NAME "usgscsm"
    SUFFIX ".js"
    LINK_FLAGS "${WASM_LINK_FLAGS_STR}"
  )

  # Copy output files to dist/ directory in build folder for NPM packaging
  add_custom_command(TARGET usgscsm_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/usgscsm.js
            ${CMAKE_BINARY_DIR}/dist/usgscsm.js
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/usgscsm.wasm
            ${CMAKE_BINARY_DIR}/dist/usgscsm.wasm
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/src/wasm/usgscsm.d.ts
            ${CMAKE_BINARY_DIR}/dist/usgscsm.d.ts
    COMMENT "Copying WASM output and TypeScript definitions to dist/ directory"
  )

  message(STATUS "WebAssembly target configured: usgscsm_wasm")
  message(STATUS "Output will be in: ${CMAKE_BINARY_DIR}/dist/")
endif()

