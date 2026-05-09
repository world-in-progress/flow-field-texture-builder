cmake_minimum_required(VERSION 3.20)

set(AUTO_TRIPLET_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../cmake/flowfield-auto-triplet.cmake")

function(assert_auto_triplet name host_system host_processor osx_arch expected)
    unset(VCPKG_TARGET_TRIPLET CACHE)
    unset(VCPKG_TARGET_TRIPLET)

    set(CMAKE_HOST_SYSTEM_NAME "${host_system}")
    set(CMAKE_HOST_SYSTEM_PROCESSOR "${host_processor}")
    if(osx_arch STREQUAL "-")
        unset(CMAKE_OSX_ARCHITECTURES)
    else()
        set(CMAKE_OSX_ARCHITECTURES "${osx_arch}")
    endif()

    include("${AUTO_TRIPLET_SCRIPT}")

    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "${expected}")
        message(FATAL_ERROR
                "${name}: expected ${expected}, got ${VCPKG_TARGET_TRIPLET}")
    endif()
endfunction()

assert_auto_triplet("macOS arm64 from osx architectures" "Darwin" "x86_64" "arm64" "arm64-osx-flowfield")
assert_auto_triplet("macOS x64 from osx architectures" "Darwin" "arm64" "x86_64" "x64-osx-flowfield")
assert_auto_triplet("Linux x64" "Linux" "x86_64" "-" "x64-linux-flowfield")
assert_auto_triplet("Linux arm64" "Linux" "aarch64" "-" "arm64-linux-flowfield")
assert_auto_triplet("Windows x64" "Windows" "AMD64" "-" "x64-windows-flowfield")
assert_auto_triplet("Windows arm64" "Windows" "ARM64" "-" "arm64-windows-flowfield")

unset(VCPKG_TARGET_TRIPLET CACHE)
set(VCPKG_TARGET_TRIPLET "custom-triplet" CACHE STRING "" FORCE)
set(CMAKE_HOST_SYSTEM_NAME "Linux")
set(CMAKE_HOST_SYSTEM_PROCESSOR "x86_64")
unset(CMAKE_OSX_ARCHITECTURES)
include("${AUTO_TRIPLET_SCRIPT}")
if(NOT VCPKG_TARGET_TRIPLET STREQUAL "custom-triplet")
    message(FATAL_ERROR "explicit VCPKG_TARGET_TRIPLET should not be overwritten")
endif()
