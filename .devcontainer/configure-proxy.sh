#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/proxy-env.sh"

write_shell_proxy_profile() {
    emit_proxy_exports | sudo tee /etc/profile.d/devcontainer-proxy.sh >/dev/null
}

apply_normalized_proxy_env
write_shell_proxy_profile
configure_vscode_proxy_settings
load_git_identity_file "${SCRIPT_DIR}/git.env"
configure_git_identity
