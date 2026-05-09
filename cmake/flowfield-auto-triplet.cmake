if(DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
    return()
endif()

set(_flowfield_triplet_system "")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_flowfield_triplet_system "osx")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(_flowfield_triplet_system "linux")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_flowfield_triplet_system "windows")
else()
    message(FATAL_ERROR
            "Unsupported host system for automatic vcpkg triplet: ${CMAKE_HOST_SYSTEM_NAME}. "
            "Set VCPKG_TARGET_TRIPLET explicitly.")
endif()

set(_flowfield_processor "${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND DEFINED CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
    list(LENGTH CMAKE_OSX_ARCHITECTURES _flowfield_osx_arch_count)
    if(NOT _flowfield_osx_arch_count EQUAL 1)
        message(FATAL_ERROR
                "Automatic vcpkg triplet selection expects one CMAKE_OSX_ARCHITECTURES value. "
                "Set VCPKG_TARGET_TRIPLET explicitly for universal or custom macOS builds.")
    endif()
    list(GET CMAKE_OSX_ARCHITECTURES 0 _flowfield_processor)
endif()

string(TOLOWER "${_flowfield_processor}" _flowfield_processor_lower)
if(_flowfield_processor_lower MATCHES "^(arm64|aarch64)$")
    set(_flowfield_triplet_arch "arm64")
elseif(_flowfield_processor_lower MATCHES "^(x86_64|amd64|x64)$")
    set(_flowfield_triplet_arch "x64")
else()
    message(FATAL_ERROR
            "Unsupported host architecture for automatic vcpkg triplet: ${_flowfield_processor}. "
            "Set VCPKG_TARGET_TRIPLET explicitly.")
endif()

set(VCPKG_TARGET_TRIPLET
        "${_flowfield_triplet_arch}-${_flowfield_triplet_system}-flowfield"
        CACHE STRING
        "vcpkg target triplet selected for flow-field-texture-builder"
        FORCE)
message(STATUS "Auto-selected VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
