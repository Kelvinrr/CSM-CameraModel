# Static build of OpenSSL libcrypto, fetched and built at configure/build time,
# so it is baked into libusgscsm (a CSM plugin cannot depend on runtime dylibs).
# STARDS uses OpenSSL only for S3 SigV4 signing (SHA256/HMAC), so only
# libcrypto is needed -- not libssl. curl uses the macOS SecureTransport backend
# for TLS, so it does not need libssl either.

cmake_minimum_required(VERSION 3.11)  # ExternalProject + BUILD_BYPRODUCTS
include(ExternalProject)

set(OPENSSL_VERSION 3.4.0)
# The source tarball is vendored next to this file so the build is fully
# self-contained (no network fetch, which also avoids CMake downloader TLS
# issues). The hash is still verified for integrity.
set(OPENSSL_URL "${CMAKE_CURRENT_SOURCE_DIR}/openssl-${OPENSSL_VERSION}.tar.gz")
set(OPENSSL_URL_HASH
    "SHA256=e15dda82fe2fe8139dc2ac21a36d4ca01d5313c75f99f46c4e8a27709b7294bf")

set(OPENSSL_PREFIX  "${CMAKE_CURRENT_BINARY_DIR}/openssl")
set(OPENSSL_INSTALL "${OPENSSL_PREFIX}/install")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_INSTALL}/include")
set(OPENSSL_CRYPTO_LIB  "${OPENSSL_INSTALL}/lib/libcrypto.a")

# Pick the OpenSSL Configure target for the host architecture.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  set(OPENSSL_TARGET darwin64-arm64-cc)
else()
  set(OPENSSL_TARGET darwin64-x86_64-cc)
endif()

# A minimal static libcrypto: no shared libs, apps, tests, docs, or legacy/engine
# bits. `build_generated` must run before the object files, or generated headers
# (opensslv.h, ...) are missing.
ExternalProject_Add(openssl_ep
  URL           "${OPENSSL_URL}"
  URL_HASH      "${OPENSSL_URL_HASH}"
  DOWNLOAD_NAME "openssl-${OPENSSL_VERSION}.tar.gz"
  PREFIX        "${OPENSSL_PREFIX}"
  CONFIGURE_COMMAND
    <SOURCE_DIR>/Configure ${OPENSSL_TARGET} no-shared no-tests no-docs no-apps
      no-legacy no-engine --prefix=${OPENSSL_INSTALL} --openssldir=${OPENSSL_INSTALL}/ssl
  BUILD_COMMAND   make build_generated COMMAND make -j4 libcrypto.a
  INSTALL_COMMAND make install_dev
  BUILD_IN_SOURCE 1
  BUILD_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}"
  LOG_DOWNLOAD 1 LOG_CONFIGURE 1 LOG_BUILD 1 LOG_INSTALL 1
)

# Make the include dir exist at configure time so INTERFACE usage validates.
file(MAKE_DIRECTORY "${OPENSSL_INCLUDE_DIR}")

# IMPORTED target consumers link as usgscsm_crypto.
add_library(usgscsm_crypto STATIC IMPORTED GLOBAL)
set_target_properties(usgscsm_crypto PROPERTIES
    IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
add_dependencies(usgscsm_crypto openssl_ep)

# Expose the install paths to the parent scope so curl can use this OpenSSL.
set(USGSCSM_OPENSSL_INSTALL "${OPENSSL_INSTALL}" PARENT_SCOPE)
set(USGSCSM_OPENSSL_INCLUDE_DIR "${OPENSSL_INCLUDE_DIR}" PARENT_SCOPE)
set(USGSCSM_OPENSSL_CRYPTO_LIB "${OPENSSL_CRYPTO_LIB}" PARENT_SCOPE)
