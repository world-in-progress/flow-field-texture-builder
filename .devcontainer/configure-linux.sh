#!/usr/bin/env bash
set -euo pipefail

case "$(uname -m)" in
    x86_64 | amd64)
        preset="linux-x64-release"
        ;;
    aarch64 | arm64)
        preset="linux-arm64-release"
        ;;
    *)
        echo "Unsupported Linux architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

cmake --preset "${preset}"
cmake --build --preset "${preset}"
