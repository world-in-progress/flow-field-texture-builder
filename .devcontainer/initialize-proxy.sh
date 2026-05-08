#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/proxy-env.sh"

apply_normalized_proxy_env
write_proxy_env_file "${SCRIPT_DIR}/proxy.env"
