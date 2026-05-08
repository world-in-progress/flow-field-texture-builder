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
