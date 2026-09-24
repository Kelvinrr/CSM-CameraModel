# Fetches a pinned CSM API release so one source tree can build plugins for several CSM
# versions. ngageoint/csm is the canonical home of the API and the same tarball
# conda-forge builds its csm package from; the external/csm submodule tracks a USGS fork
# whose tags stop at 3.0.3, so anything newer has to come from upstream.

set(USGSCSM_CSM_SHA256_3.1.0 "ec3530716f3e2dd752948f80fd7e3b3808ca31f0bdbe5d62d82da6cb71d5981a")
set(USGSCSM_CSM_SHA256_3.0.3 "4e60176876d31da45bcb48ef29f380cdbf3d64d5738066be7d4af4d6536b7776")

# Sets CSM_SRC_DIR and CSM_PARENT_INCLUDE_DIR for cmake/csm to consume.
function(usgscsm_fetch_csm_release version)
  set(root "${CMAKE_BINARY_DIR}/csm-upstream/${version}")
  # The directory must be named "csm" so #include <csm/csm.h> resolves against its parent.
  set(src "${root}/csm")

  if(NOT EXISTS "${src}/csm.h")
    if(USGSCSM_CSM_TARBALL_SHA256)
      set(sha "${USGSCSM_CSM_TARBALL_SHA256}")
    elseif(DEFINED USGSCSM_CSM_SHA256_${version})
      set(sha "${USGSCSM_CSM_SHA256_${version}}")
    else()
      message(FATAL_ERROR
        "No checksum on file for CSM ${version}. Pass -DUSGSCSM_CSM_TARBALL_SHA256=<sha256>.")
    endif()

    set(tarball "${root}/csm-${version}.tar.gz")
    file(MAKE_DIRECTORY "${root}")
    message(STATUS "Downloading CSM ${version} from ngageoint/csm")
    file(DOWNLOAD
         "https://github.com/ngageoint/csm/archive/refs/tags/v${version}.tar.gz"
         "${tarball}"
         EXPECTED_HASH SHA256=${sha}
         STATUS download_status)
    list(GET download_status 0 code)
    if(NOT code EQUAL 0)
      list(GET download_status 1 reason)
      message(FATAL_ERROR "Failed to download CSM ${version}: ${reason}")
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${tarball}"
                    WORKING_DIRECTORY "${root}"
                    RESULT_VARIABLE extract_result)
    if(NOT extract_result EQUAL 0)
      message(FATAL_ERROR "Failed to extract ${tarball}")
    endif()
    file(RENAME "${root}/csm-${version}" "${src}")
  endif()

  set(CSM_SRC_DIR "${src}" PARENT_SCOPE)
  set(CSM_PARENT_INCLUDE_DIR "${root}" PARENT_SCOPE)
endfunction()
