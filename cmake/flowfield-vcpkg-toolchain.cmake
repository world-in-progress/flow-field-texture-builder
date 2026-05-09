include("${CMAKE_CURRENT_LIST_DIR}/flowfield-auto-triplet.cmake")

set(_flowfield_vcpkg_toolchain "${CMAKE_CURRENT_LIST_DIR}/../.vcpkg/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${_flowfield_vcpkg_toolchain}" AND DEFINED ENV{VCPKG_ROOT})
    set(_flowfield_vcpkg_toolchain "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

if(NOT EXISTS "${_flowfield_vcpkg_toolchain}")
    message(FATAL_ERROR
            "Could not find vcpkg toolchain. Clone vcpkg to .vcpkg/ or set VCPKG_ROOT.")
endif()

include("${_flowfield_vcpkg_toolchain}")
