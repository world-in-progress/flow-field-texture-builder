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
presets. The build uses `.vcpkg/` when it exists, or `VCPKG_ROOT` when vcpkg is
installed elsewhere. The local `.vcpkg/` directory is ignored by git.

Project overlay triplets live under `cmake/vcpkg-triplets`. They keep vcpkg
dependency configuration from accidentally discovering optional system packages
during GDAL's configure step.

```sh
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
```

The Python binding is built by default with the CMake presets. During local C++
development, import it from the CMake build tree:

```sh
PYTHONPATH=build/macos-arm64-debug/python python3 tests/python_binding_smoke.py
```

## Python Development

Python development is managed with `uv`. `uv sync` installs dependencies,
builds the native extension through `scikit-build-core`, and installs the local
package into `.venv`:

```sh
uv sync
```

After C++ or binding source changes, `uv` should rebuild automatically from the
configured cache keys. To force a rebuild explicitly:

```sh
uv sync --reinstall-package flow-field-texture-builder
```

Run the Python smoke test through the uv environment:

```sh
uv run python tests/python_binding_smoke.py
```

## Python Package

`pyproject.toml` uses `scikit-build-core` and the same CMake/vcpkg build. The
package build auto-selects the repository's `*-flowfield` vcpkg triplet from
the host platform and architecture:

```sh
uv build --wheel
```

To force a specific triplet, pass `VCPKG_TARGET_TRIPLET` explicitly:

```sh
uv build --wheel \
  -Ccmake.define.VCPKG_TARGET_TRIPLET=arm64-osx-flowfield
```

Supported release triplets live under `cmake/vcpkg-triplets`.

Install the produced wheel into the Python environment that will call the
builder:

```sh
python -m pip install dist/flow_field_texture_builder-*.whl
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

## Python Binding

The Python package exposes the stateful builder directly. Domain geometry is
set once and reused across texture builds. Each `build_uv_texture(...)` call
accepts one model output step and writes one UV texture plus one seed texture.
It returns `None`; paths and state are inferred from the builder object.

```python
from pathlib import Path

import numpy as np

from flow_field_texture_builder import FlowFieldBuilder


xs = np.asarray(model_xs, dtype=np.float64)
ys = np.asarray(model_ys, dtype=np.float64)
us = np.asarray(step_u, dtype=np.float32)
vs = np.asarray(step_v, dtype=np.float32)

builder = FlowFieldBuilder("result")
try:
    builder.set_texture_size(1024, 1024)

    # Use the point extent directly.
    builder.set_domain_from_arrays(xs, ys)

    # Or use vector data as the domain mask. The extent still comes from xs/ys.
    # Supported vector formats depend on GDAL, for example .shp or .geojson.
    # builder.set_domain_from_vector(xs, ys, "domain.geojson")

    # Optional. Defaults are valid if this is not called.
    builder.set_quikgrid_options(
        scan_ratio=16,
        density_ratio=150,
        edge_factor=100,
        undefined_value=-99999.0,
        sample=1,
    )

    builder.build_uv_texture(us, vs, "step_0001")

    uv_path = Path(builder.uv_texture_path("step_0001"))
    seed_path = Path(builder.seed_texture_path("step_0001"))
    texture_size = builder.texture_size
    domain_extent = builder.domain_extent
finally:
    builder.close()
```

Input arrays are one-dimensional and must have matching lengths. The wrapper
passes `xs` and `ys` as contiguous `float64` arrays. `us` and `vs` are converted
to contiguous `float32` arrays at the Python FFI boundary, so callers may pass
`float64` velocity arrays when that is more convenient.

`output_name` must be a file stem such as `step_0001`; directory components and
absolute paths are rejected. Output files are written under the builder output
directory as:

- `uv_<output_name>.png`
- `seed_<output_name>.png`

The configured texture size is `(width, height)`. The UV PNG is RGBA8 with
physical size `(width * 2, height)`: each grid cell uses two adjacent RGBA
pixels, the first containing the raw little-endian `float32` bytes for `u` and
the second containing the raw little-endian `float32` bytes for `v`. There is no
`flow_boundary` normalization step.

The seed PNG is RGBA8 with physical size `(width, height)`. It stores the seed
coordinate as two unsigned 16-bit values split across RGBA bytes:
`R/G -> x`, `B/A -> y`.

## VSCode

Install the recommended CMake Tools and C/C++ extensions. Select the matching
CMake configure preset, then run `CMake: Configure`.

CMake generates `compile_commands.json` and VSCode uses CMake Tools as the
C/C++ configuration provider, so IntelliSense reads the same include paths as
the real build.

## Dev Container

The repository includes a Linux dev container under `.devcontainer/` for
building and testing the Linux side from macOS. It uses Ubuntu 24.04 with the
C++ toolchain, CMake, Ninja, Python development headers, uv, Node.js, and the
Codex CLI.

The container keeps vcpkg in a named Docker volume mounted at `.vcpkg/`, so it
does not reuse or overwrite a host macOS vcpkg checkout.

Use VS Code's Dev Containers commands, such as `Reopen in Container` or
`Rebuild and Reopen in Container`, to enter the container. The Dev Containers
extension runs the setup commands automatically; the `npx @devcontainers/cli`
command is only useful for debugging the container setup from a terminal.

If you need a proxy, export the usual `HTTP_PROXY`, `HTTPS_PROXY`, and
`NO_PROXY` values on the host before reopening the workspace in the container.
Local loopback proxy URLs such as `http://127.0.0.1:<port>` and
`http://localhost:<port>` are rewritten automatically to `host.docker.internal`
for Docker Desktop. The normalized proxy is passed to the Docker build, the
container environment, VS Code's container-scoped `http.proxy` setting, git,
and npm. The container also sets `http.useLocalProxyConfiguration` to `false`,
because the host-local VS Code proxy address is not valid inside Docker.
`DEVCONTAINER_HTTP_PROXY`, `DEVCONTAINER_HTTPS_PROXY`, and
`DEVCONTAINER_NO_PROXY` can be used as explicit overrides.

The host git identity is also synchronized automatically. During
initialization, the dev container reads the host workspace's effective
`git config user.name` and `git config user.email`, writes them to the ignored
`.devcontainer/git.env`, and applies them to the container user's global git
config during `postCreate` and `postStart`.

If the host proxy address or port changes after the container has been
created, run `Dev Containers: Rebuild and Reopen in Container` so Docker gets a
fresh generated proxy environment. Container start still refreshes VS Code's
container-scoped proxy settings automatically.

After the container starts, run:

```sh
.devcontainer/configure-linux.sh
```

The script selects `linux-x64-release` or `linux-arm64-release` from the
container architecture. On Apple Silicon Docker Desktop, the default container
architecture is usually Linux arm64.

`post-create.sh` installs `@openai/codex` if `codex` is not already available.
Authentication is still container-local; run `codex login` inside the container
when the CLI asks for credentials. Sharing a host `~/.codex` directory with the
container is possible, but it is intentionally not enabled by default because it
mounts local credentials into the container.
