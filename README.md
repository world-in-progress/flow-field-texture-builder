# flow-field-texture-builder

Flow field texture builder.

## Dependencies

External dependencies are declared in `vcpkg.json`:

- GDAL
- nlohmann-json
- PROJ
- stb

The modified quikgrid sources are kept in this repository as an internal CMake
module under `modules/quikgrid`.

## Build

Install CMake, Ninja, and vcpkg, then configure with one of the provided
presets. This repository expects a local vcpkg root at `.vcpkg/`, which is
ignored by git.

Project overlay triplets live under `cmake/vcpkg-triplets`. They keep vcpkg
dependency configuration from accidentally discovering optional system packages
during GDAL's configure step.

```sh
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
```

Release presets are available for the Python-wheel target platforms:

- `macos-x64-release`
- `macos-arm64-release`
- `linux-x64-release`
- `linux-arm64-release`
- `windows-x64-release`
- `windows-arm64-release`

vcpkg manifest mode installs project dependencies into `vcpkg_installed/`,
which is intentionally ignored by git.
