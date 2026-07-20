# Static build of OpenSSL, built at configure/build time from the pristine
# submodule at external/openssl (pinned to openssl-3.4.0), so it is baked into
# libusgscsm (a CSM plugin cannot depend on runtime dylibs). This build glue
# lives in the main repo so the submodule stays unmodified.
#
# STARDS uses libcrypto for S3 SigV4 signing (SHA256/HMAC) on every platform.
# On macOS, curl uses the SecureTransport TLS backend, so only libcrypto is
# needed. On Linux there is no SecureTransport, so curl uses OpenSSL for TLS and
# also needs libssl -- built here alongside libcrypto.

cmake_minimum_required(VERSION 3.11)  # ExternalProject + BUILD_BYPRODUCTS
include(ExternalProject)

set(OPENSSL_SRC_DIR "${CMAKE_SOURCE_DIR}/external/openssl")

if(NOT EXISTS "${OPENSSL_SRC_DIR}/Configure")
  message(FATAL_ERROR
    "OpenSSL submodule not found at ${OPENSSL_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

set(OPENSSL_PREFIX  "${CMAKE_CURRENT_BINARY_DIR}/openssl")
set(OPENSSL_BUILD   "${OPENSSL_PREFIX}/build")
set(OPENSSL_INSTALL "${OPENSSL_PREFIX}/install")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_INSTALL}/include")
set(OPENSSL_CRYPTO_LIB  "${OPENSSL_INSTALL}/lib/libcrypto.a")
set(OPENSSL_SSL_LIB     "${OPENSSL_INSTALL}/lib/libssl.a")

# Pick the OpenSSL Configure target for the host platform + architecture.
if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(OPENSSL_TARGET darwin64-arm64-cc)
  else()
    set(OPENSSL_TARGET darwin64-x86_64-cc)
  endif()
else()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(OPENSSL_TARGET linux-aarch64)
  else()
    set(OPENSSL_TARGET linux-x86_64)
  endif()
endif()

# On macOS only libcrypto is needed (SecureTransport provides TLS to curl). On
# Linux curl links OpenSSL for TLS, so libssl is required too. When both are
# built, install libssl first so libcrypto's byproduct marks the whole build
# done.
if(APPLE)
  set(OPENSSL_BUILD_LIBS make -j4 libcrypto.a)
  set(OPENSSL_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}")
else()
  set(OPENSSL_BUILD_LIBS make -j4 libssl.a libcrypto.a)
  set(OPENSSL_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}" "${OPENSSL_SSL_LIB}")
endif()

# A minimal static OpenSSL: no shared libs, apps, tests, docs, or legacy/engine
# bits. `build_generated` must run before the object files, or generated headers
# (opensslv.h, ...) are missing. Configure is invoked from a separate build
# directory (out-of-tree) so the submodule source stays pristine.
#
# install_dev installs the headers plus whichever static libs were built (it
# installs the archives present in the build tree), so it covers both the
# macOS (libcrypto only) and Linux (libcrypto + libssl) cases.
ExternalProject_Add(openssl_ep
  SOURCE_DIR      "${OPENSSL_SRC_DIR}"
  BINARY_DIR      "${OPENSSL_BUILD}"
  CONFIGURE_COMMAND
    ${OPENSSL_SRC_DIR}/Configure ${OPENSSL_TARGET} no-shared no-tests no-docs no-apps
      no-legacy no-engine --prefix=${OPENSSL_INSTALL} --openssldir=${OPENSSL_INSTALL}/ssl
  BUILD_COMMAND   make build_generated COMMAND ${OPENSSL_BUILD_LIBS}
  INSTALL_COMMAND make install_dev
  BUILD_BYPRODUCTS ${OPENSSL_BYPRODUCTS}
  LOG_CONFIGURE 1 LOG_BUILD 1 LOG_INSTALL 1
)

# Make the include dir exist at configure time so INTERFACE usage validates.
file(MAKE_DIRECTORY "${OPENSSL_INCLUDE_DIR}")

# IMPORTED target consumers link as usgscsm_crypto (libcrypto, needed on all
# platforms for S3 signing).
add_library(usgscsm_crypto STATIC IMPORTED GLOBAL)
set_target_properties(usgscsm_crypto PROPERTIES
    IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
add_dependencies(usgscsm_crypto openssl_ep)

# On Linux, static libcrypto uses dlopen and threads, so the final link needs
# -ldl and -pthread. macOS provides these in libSystem, so nothing is added.
if(NOT APPLE)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set_target_properties(usgscsm_crypto PROPERTIES
      INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")
endif()

# usgscsm_ssl (libssl) is only built on non-Apple platforms, where curl needs it
# for its OpenSSL TLS backend.
if(NOT APPLE)
  add_library(usgscsm_ssl STATIC IMPORTED GLOBAL)
  set_target_properties(usgscsm_ssl PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_SSL_LIB}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
  add_dependencies(usgscsm_ssl openssl_ep)
endif()

# Expose the install paths to the parent scope so curl can use this OpenSSL.
set(USGSCSM_OPENSSL_INSTALL "${OPENSSL_INSTALL}" PARENT_SCOPE)
set(USGSCSM_OPENSSL_INCLUDE_DIR "${OPENSSL_INCLUDE_DIR}" PARENT_SCOPE)
set(USGSCSM_OPENSSL_CRYPTO_LIB "${OPENSSL_CRYPTO_LIB}" PARENT_SCOPE)
set(USGSCSM_OPENSSL_SSL_LIB "${OPENSSL_SSL_LIB}" PARENT_SCOPE)
