# Static build of OpenSSL, built at configure/build time from the pristine
# submodule at external/openssl (pinned to openssl-3.4.0), so it is baked into
# libusgscsm (a CSM plugin cannot depend on runtime dylibs). This build glue
# lives in the main repo so the submodule stays unmodified.
#
# STARDS uses libcrypto for S3 SigV4 signing (SHA256/HMAC) on every platform.
# curl's TLS backend is platform-native, so libssl is only needed where curl
# links OpenSSL for TLS:
#   * macOS   -- curl uses SecureTransport; only libcrypto is built.
#   * Windows -- curl uses Schannel; only libcrypto is built (nmake/VC-WIN64A).
#   * Linux   -- no OS TLS for curl, so curl links OpenSSL; libssl is built too.

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
# Static archive extension differs by toolchain: MSVC produces .lib, Unix .a.
if(WIN32)
  set(OPENSSL_CRYPTO_LIB "${OPENSSL_INSTALL}/lib/libcrypto.lib")
  set(OPENSSL_SSL_LIB    "${OPENSSL_INSTALL}/lib/libssl.lib")
else()
  set(OPENSSL_CRYPTO_LIB "${OPENSSL_INSTALL}/lib/libcrypto.a")
  set(OPENSSL_SSL_LIB    "${OPENSSL_INSTALL}/lib/libssl.a")
endif()

# Pick the OpenSSL Configure target for the host platform + architecture.
if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(OPENSSL_TARGET darwin64-arm64-cc)
  else()
    set(OPENSSL_TARGET darwin64-x86_64-cc)
  endif()
elseif(WIN32)
  # VC-WIN64A = 64-bit MSVC (AMD64). Perl (Strawberry/ActiveState) is a build
  # dependency for OpenSSL's Configure on Windows and is present on the standard
  # windows GitHub runners.
  set(OPENSSL_TARGET VC-WIN64A)
else()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(OPENSSL_TARGET linux-aarch64)
  else()
    set(OPENSSL_TARGET linux-x86_64)
  endif()
endif()

# The build tool is nmake on Windows and make elsewhere. On Windows, no-asm
# avoids a hard NASM dependency (perl is still required and ships on the standard
# windows runner); the SHA256/HMAC that STARDS needs work fine without the asm
# fast paths.
if(WIN32)
  set(OPENSSL_MAKE nmake)
  set(OPENSSL_ASM_ARG no-asm)
else()
  set(OPENSSL_MAKE make -j4)
  set(OPENSSL_ASM_ARG)  # empty: keep asm on Unix
endif()

# libssl is only needed on Linux, where curl links OpenSSL for TLS. macOS
# (SecureTransport) and Windows (Schannel) use the OS TLS backend, so only
# libcrypto is built there. Naming the static archive file(s) -- by their
# build-tree names (libcrypto.a on Unix, libcrypto.lib with MSVC) -- as the make
# target builds exactly those and nothing else. There is no separate "just
# libcrypto" configure flag (no-ssl only disables protocol negotiation; it does
# not skip building libssl).
if(WIN32)
  set(OPENSSL_CRYPTO_LIBNAME libcrypto.lib)
  set(OPENSSL_SSL_LIBNAME    libssl.lib)
else()
  set(OPENSSL_CRYPTO_LIBNAME libcrypto.a)
  set(OPENSSL_SSL_LIBNAME    libssl.a)
endif()
if(APPLE OR WIN32)
  set(OPENSSL_BUILD_LIBS ${OPENSSL_MAKE} ${OPENSSL_CRYPTO_LIBNAME})
  set(OPENSSL_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}")
else()
  set(OPENSSL_BUILD_LIBS ${OPENSSL_MAKE} ${OPENSSL_SSL_LIBNAME} ${OPENSSL_CRYPTO_LIBNAME})
  set(OPENSSL_BYPRODUCTS "${OPENSSL_CRYPTO_LIB}" "${OPENSSL_SSL_LIB}")
endif()

# A minimal static OpenSSL: no shared libs, apps, tests, docs, or legacy/engine
# bits. `build_generated` must run before the object files, or generated headers
# (opensslv.h, ...) are missing. Building the archive file target(s) then
# produces just the static libs needed. Configure is invoked from a separate
# build directory (out-of-tree) so the submodule source stays pristine. It is
# run through perl explicitly so the Configure Perl script is invoked the same
# way on Windows (no shebang/exec-bit) as on Unix.
#
# install_dev installs the headers plus whichever static libs were built, so it
# covers the libcrypto-only (macOS/Windows) and libcrypto+libssl (Linux) cases.
ExternalProject_Add(openssl_ep
  SOURCE_DIR      "${OPENSSL_SRC_DIR}"
  BINARY_DIR      "${OPENSSL_BUILD}"
  # --libdir=lib forces the install layout to ${prefix}/lib on every platform.
  # Without it, OpenSSL defaults to lib64 on many 64-bit Linux distros, which
  # breaks the hardcoded lib/ paths below (and the ones curl is handed).
  CONFIGURE_COMMAND
    perl ${OPENSSL_SRC_DIR}/Configure ${OPENSSL_TARGET} no-shared no-tests no-docs no-apps
      no-legacy no-engine ${OPENSSL_ASM_ARG}
      --prefix=${OPENSSL_INSTALL} --openssldir=${OPENSSL_INSTALL}/ssl
      --libdir=lib
  BUILD_COMMAND   ${OPENSSL_MAKE} build_generated COMMAND ${OPENSSL_BUILD_LIBS}
  INSTALL_COMMAND ${OPENSSL_MAKE} install_dev
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

# System libraries the final link needs to satisfy static libcrypto's symbols:
#   * Linux   -- dlopen + threads (-ldl, -pthread).
#   * Windows -- the Win32 libs OpenSSL's VC-common config links (ws2_32, gdi32,
#                advapi32, crypt32, user32). These are OS-provided, so linking
#                them keeps the plugin free of any non-system runtime DLL.
#   * macOS   -- provided by libSystem, so nothing is added.
if(WIN32)
  set_target_properties(usgscsm_crypto PROPERTIES
      INTERFACE_LINK_LIBRARIES "ws2_32;gdi32;advapi32;crypt32;user32")
elseif(NOT APPLE)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  set_target_properties(usgscsm_crypto PROPERTIES
      INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS}")
endif()

# usgscsm_ssl (libssl) is only built on Linux, where curl links OpenSSL for its
# TLS backend. macOS (SecureTransport) and Windows (Schannel) use the OS TLS, so
# libssl is neither built nor exposed there.
if(NOT APPLE AND NOT WIN32)
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
