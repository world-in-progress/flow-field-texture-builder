# Keep vcpkg port configuration isolated from optional system packages that
# GDAL may probe even when their features are disabled in the manifest.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_DISABLE_FIND_PACKAGE_Arrow=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_Parquet=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_ArrowDataset=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_ArrowCompute=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_AWSSDK=ON
)
