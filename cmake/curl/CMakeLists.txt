# Static build of libcurl, built at configure/build time from the pristine
# submodule at external/curl (pinned to curl-8_11_0), so it is baked into
# libusgscsm (a CSM plugin cannot depend on runtime dylibs). This build glue
# lives in the main repo so the submodule stays unmodified.
#
# STARDS uses libcurl only for simple HTTP(S) GET/HEAD/PUT of STARDS files
# (vsicurl / S3 transport). TLS backend is platform-specific: macOS uses the
# built-in SecureTransport backend (no OpenSSL/libssl needed); Linux uses the
# vendored static OpenSSL (libssl+libcrypto) built by cmake/openssl. Protocols
# and optional features are disabled down to just HTTP/HTTPS to keep the static
# archive small and dependency-free.

cmake_minimum_required(VERSION 3.11)
include(ExternalProject)

set(CURL_SRC_DIR "${CMAKE_SOURCE_DIR}/external/curl")

if(NOT EXISTS "${CURL_SRC_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "curl submodule not found at ${CURL_SRC_DIR}. "
    "Run: git submodule update --init --recursive")
endif()

set(CURL_PREFIX  "${CMAKE_CURRENT_BINARY_DIR}/curl")
set(CURL_BUILD   "${CURL_PREFIX}/build")
set(CURL_INSTALL "${CURL_PREFIX}/install")
set(CURL_INCLUDE_DIR "${CURL_INSTALL}/include")
set(CURL_STATIC_LIB  "${CURL_INSTALL}/lib/libcurl.a")

# TLS backend selection. On macOS use SecureTransport (no external TLS lib). On
# Linux use the vendored static OpenSSL from cmake/openssl (USGSCSM_OPENSSL_*
# are set in the parent scope there).
if(APPLE)
  set(CURL_TLS_ARGS
      -DCURL_USE_SECTRANSP=ON
      -DCURL_USE_OPENSSL=OFF)
  set(CURL_TLS_DEPENDS "")
else()
  set(CURL_TLS_ARGS
      -DCURL_USE_SECTRANSP=OFF
      -DCURL_USE_OPENSSL=ON
      -DOPENSSL_ROOT_DIR=${USGSCSM_OPENSSL_INSTALL}
      -DOPENSSL_INCLUDE_DIR=${USGSCSM_OPENSSL_INCLUDE_DIR}
      -DOPENSSL_SSL_LIBRARY=${USGSCSM_OPENSSL_SSL_LIB}
      -DOPENSSL_CRYPTO_LIBRARY=${USGSCSM_OPENSSL_CRYPTO_LIB}
      -DOPENSSL_USE_STATIC_LIBS=ON)
  # curl must configure only after the static OpenSSL is built and installed.
  set(CURL_TLS_DEPENDS openssl_ep)
endif()

# Build libcurl with CMake: static only, HTTP/HTTPS only. Configured from a
# separate build directory (out-of-tree) so the submodule source stays pristine.
ExternalProject_Add(curl_ep
  SOURCE_DIR    "${CURL_SRC_DIR}"
  BINARY_DIR    "${CURL_BUILD}"
  DEPENDS       ${CURL_TLS_DEPENDS}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CURL_INSTALL}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
    -DBUILD_CURL_EXE=OFF
    -DBUILD_TESTING=OFF
    ${CURL_TLS_ARGS}
    -DCURL_USE_LIBPSL=OFF
    -DCURL_USE_LIBSSH2=OFF
    -DUSE_NGHTTP2=OFF
    -DUSE_LIBIDN2=OFF
    -DCURL_ZLIB=OFF
    -DCURL_BROTLI=OFF
    -DCURL_ZSTD=OFF
    -DCURL_DISABLE_LDAP=ON
    -DCURL_DISABLE_LDAPS=ON
    # Restrict to just what STARDS needs: HTTP(S) file transfer.
    -DCURL_DISABLE_FTP=ON
    -DCURL_DISABLE_FILE=ON
    -DCURL_DISABLE_DICT=ON
    -DCURL_DISABLE_TELNET=ON
    -DCURL_DISABLE_TFTP=ON
    -DCURL_DISABLE_RTSP=ON
    -DCURL_DISABLE_POP3=ON
    -DCURL_DISABLE_IMAP=ON
    -DCURL_DISABLE_SMTP=ON
    -DCURL_DISABLE_GOPHER=ON
    -DCURL_DISABLE_MQTT=ON
    -DCURL_DISABLE_SMB=ON
  BUILD_BYPRODUCTS "${CURL_STATIC_LIB}"
  LOG_CONFIGURE 1 LOG_BUILD 1 LOG_INSTALL 1
)

file(MAKE_DIRECTORY "${CURL_INCLUDE_DIR}")

# IMPORTED target. Static libcurl needs CURL_STATICLIB defined by consumers so
# the headers don't emit dllimport/visibility decorations.
add_library(usgscsm_curl STATIC IMPORTED GLOBAL)
set_target_properties(usgscsm_curl PROPERTIES
    IMPORTED_LOCATION "${CURL_STATIC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS "CURL_STATICLIB")

# Platform TLS dependencies libcurl pulls in: macOS SecureTransport frameworks;
# Linux the vendored static OpenSSL (libssl+libcrypto).
if(APPLE)
  target_link_libraries(usgscsm_curl INTERFACE
      "-framework CoreFoundation"
      "-framework SystemConfiguration"
      "-framework Security")
else()
  target_link_libraries(usgscsm_curl INTERFACE usgscsm_ssl usgscsm_crypto)
endif()
add_dependencies(usgscsm_curl curl_ep)
