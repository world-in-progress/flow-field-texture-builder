#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/proxy-env.sh"

VCPKG_ROOT="${VCPKG_ROOT:-${PWD}/.vcpkg}"
NPM_PREFIX="${NPM_CONFIG_PREFIX:-${HOME}/.npm-global}"

configure_proxy() {
    local http_proxy_value="${HTTP_PROXY:-${http_proxy:-}}"
    local https_proxy_value="${HTTPS_PROXY:-${https_proxy:-}}"
    local no_proxy_value="${NO_PROXY:-${no_proxy:-}}"

    if [[ -n "${http_proxy_value}" ]]; then
        git config --global http.proxy "${http_proxy_value}"
        npm config set proxy "${http_proxy_value}" >/dev/null
    fi

    if [[ -n "${https_proxy_value}" ]]; then
        git config --global https.proxy "${https_proxy_value}"
        npm config set https-proxy "${https_proxy_value}" >/dev/null
    fi

    if [[ -n "${no_proxy_value}" ]]; then
        npm config set noproxy "${no_proxy_value}" >/dev/null
    fi
}

bootstrap_vcpkg() {
    sudo mkdir -p "${VCPKG_ROOT}"
    sudo chown -R "$(id -u):$(id -g)" "${VCPKG_ROOT}"

    if [[ ! -d "${VCPKG_ROOT}/.git" ]]; then
        git clone --depth 1 https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
    fi

    if [[ ! -x "${VCPKG_ROOT}/vcpkg" ]]; then
        "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
    fi
}

install_codex_cli() {
    mkdir -p "${NPM_PREFIX}/bin"
    npm config set prefix "${NPM_PREFIX}" >/dev/null

    if ! command -v codex >/dev/null 2>&1; then
        npm install -g @openai/codex
    fi

    codex --version || true
}

apply_normalized_proxy_env
configure_vscode_proxy_settings
configure_proxy
load_git_identity_file "${SCRIPT_DIR}/git.env"
configure_git_identity
bootstrap_vcpkg
install_codex_cli

echo "Dev container setup complete."
