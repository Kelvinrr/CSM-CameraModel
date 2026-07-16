# Static build of libcurl, fetched and built at configure/build time, so it is
# baked into libusgscsm (a CSM plugin cannot depend on runtime dylibs).
#
# STARDS uses libcurl only for simple HTTP(S) GET/HEAD/PUT of STARDS files
# (vsicurl / S3 transport). TLS is provided by the macOS SecureTransport
# backend, so libcurl needs no OpenSSL and no other TLS library. Protocols and
# optional features are disabled down to just HTTP/HTTPS to keep the static
# archive small and dependency-free.

cmake_minimum_required(VERSION 3.11)
include(ExternalProject)

set(CURL_VERSION 8.11.0)
# The source tarball is vendored next to this file so the build is fully
# self-contained (no network fetch). The hash is still verified for integrity.
set(CURL_URL  "${CMAKE_CURRENT_SOURCE_DIR}/curl-${CURL_VERSION}.tar.gz")
set(CURL_URL_HASH
    "SHA256=264537d90e58d2b09dddc50944baf3c38e7089151c8986715e2aaeaaf2b8118f")

set(CURL_PREFIX  "${CMAKE_CURRENT_BINARY_DIR}/curl")
set(CURL_INSTALL "${CURL_PREFIX}/install")
set(CURL_INCLUDE_DIR "${CURL_INSTALL}/include")
set(CURL_STATIC_LIB  "${CURL_INSTALL}/lib/libcurl.a")

# Build libcurl with CMake: static only, SecureTransport TLS, HTTP/HTTPS only.
ExternalProject_Add(curl_ep
  URL           "${CURL_URL}"
  URL_HASH      "${CURL_URL_HASH}"
  DOWNLOAD_NAME "curl-${CURL_VERSION}.tar.gz"
  PREFIX        "${CURL_PREFIX}"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CURL_INSTALL}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_STATIC_LIBS=ON
    -DBUILD_CURL_EXE=OFF
    -DBUILD_TESTING=OFF
    -DCURL_USE_SECTRANSP=ON      # macOS SecureTransport TLS backend
    -DCURL_USE_OPENSSL=OFF
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
  LOG_DOWNLOAD 1 LOG_CONFIGURE 1 LOG_BUILD 1 LOG_INSTALL 1
)

file(MAKE_DIRECTORY "${CURL_INCLUDE_DIR}")

# IMPORTED target. Static libcurl needs CURL_STATICLIB defined by consumers so
# the headers don't emit dllimport/visibility decorations, plus the macOS
# frameworks SecureTransport/CoreFoundation depend on.
add_library(usgscsm_curl STATIC IMPORTED GLOBAL)
set_target_properties(usgscsm_curl PROPERTIES
    IMPORTED_LOCATION "${CURL_STATIC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    INTERFACE_COMPILE_DEFINITIONS "CURL_STATICLIB")
target_link_libraries(usgscsm_curl INTERFACE
    "-framework CoreFoundation"
    "-framework SystemConfiguration"
    "-framework Security")
add_dependencies(usgscsm_curl curl_ep)
