import tomllib
from pathlib import Path


def main() -> None:
    pyproject = tomllib.loads(Path("pyproject.toml").read_text())

    uv_config = pyproject.get("tool", {}).get("uv", {})
    if uv_config.get("package") is not True:
        raise AssertionError("[tool.uv].package must be true so uv sync builds this package")

    dev_group = pyproject.get("dependency-groups", {}).get("dev", [])
    for dependency in ("build", "pytest"):
        if not any(item == dependency or item.startswith(f"{dependency}>") for item in dev_group):
            raise AssertionError(f"dev dependency group must include {dependency}")

    cache_key_files = {
        entry.get("file")
        for entry in uv_config.get("cache-keys", [])
        if isinstance(entry, dict)
    }
    required = {
        "pyproject.toml",
        "CMakeLists.txt",
        "vcpkg.json",
        "cmake/**/*.cmake",
        "include/**/*.h",
        "src/**/*.cpp",
        "modules/quikgrid/**/*.cpp",
        "modules/quikgrid/**/*.h",
        "python/flow_field_texture_builder/**/*.py",
    }
    missing = sorted(required - cache_key_files)
    if missing:
        raise AssertionError(f"[tool.uv].cache-keys missing entries: {missing}")


if __name__ == "__main__":
    main()
