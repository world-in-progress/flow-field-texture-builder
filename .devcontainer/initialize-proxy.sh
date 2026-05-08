#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "${SCRIPT_DIR}/proxy-env.sh"

apply_normalized_proxy_env
write_proxy_env_file "${SCRIPT_DIR}/proxy.env"
write_git_identity_file "${SCRIPT_DIR}/git.env" "${PROJECT_ROOT}"
