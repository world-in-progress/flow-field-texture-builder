#!/usr/bin/env bash

normalize_proxy_url() {
    local value="${1:-}"

    if [[ -z "${value}" ]]; then
        return 0
    fi

    if [[ "${value}" =~ ^([A-Za-z][A-Za-z0-9+.-]*://)([^/@]*@)?(127\.0\.0\.1|localhost|\[::1\])(:[0-9]+)?(.*)$ ]]; then
        printf '%s%s%s%s%s\n' \
            "${BASH_REMATCH[1]}" \
            "${BASH_REMATCH[2]}" \
            "host.docker.internal" \
            "${BASH_REMATCH[4]}" \
            "${BASH_REMATCH[5]}"
        return 0
    fi

    printf '%s\n' "${value}"
}

first_proxy_value() {
    local value

    for value in "$@"; do
        if [[ -n "${value}" ]]; then
            printf '%s\n' "${value}"
            return 0
        fi
    done
}

read_vscode_proxy_setting() {
    local key="$1"
    local settings_file="${VSCODE_SETTINGS_FILE:-${HOME}/Library/Application Support/Code/User/settings.json}"

    if [[ ! -f "${settings_file}" ]]; then
        return 0
    fi

    sed -nE "s/^[[:space:]]*\"${key}\\.proxy\"[[:space:]]*:[[:space:]]*\"([^\"]+)\".*/\\1/p" \
        "${settings_file}" | tail -n 1
}

read_vscode_proxy_value() {
    local value

    value="$(read_vscode_proxy_setting "http")"
    if [[ -n "${value}" ]]; then
        printf '%s\n' "${value}"
        return 0
    fi

    read_vscode_proxy_setting "https"
}

scutil_proxy_output() {
    if [[ -n "${SCUTIL_PROXY_FILE:-}" ]]; then
        cat "${SCUTIL_PROXY_FILE}"
        return 0
    fi

    if command -v scutil >/dev/null 2>&1; then
        scutil --proxy 2>/dev/null
    fi
}

read_macos_proxy_url() {
    local scheme="$1"
    local output
    local prefix
    local enabled
    local host
    local port

    output="$(scutil_proxy_output)"
    if [[ -z "${output}" ]]; then
        return 0
    fi

    case "${scheme}" in
        http)
            prefix="HTTP"
            ;;
        https)
            prefix="HTTPS"
            ;;
        *)
            return 0
            ;;
    esac

    enabled="$(awk -v key="${prefix}Enable" '$1 == key && $2 == ":" { print $3; exit }' <<<"${output}")"
    host="$(awk -v key="${prefix}Proxy" '$1 == key && $2 == ":" { print $3; exit }' <<<"${output}")"
    port="$(awk -v key="${prefix}Port" '$1 == key && $2 == ":" { print $3; exit }' <<<"${output}")"

    if [[ "${enabled}" == "1" && -n "${host}" && -n "${port}" ]]; then
        printf 'http://%s:%s\n' "${host}" "${port}"
    fi
}

apply_normalized_proxy_env() {
    local raw_http_proxy
    local raw_https_proxy
    local raw_no_proxy
    local vscode_proxy_value

    vscode_proxy_value="$(read_vscode_proxy_value)"
    raw_http_proxy="$(first_proxy_value \
        "${DEVCONTAINER_HTTP_PROXY:-}" \
        "${LOCAL_DEVCONTAINER_HTTP_PROXY:-}" \
        "${LOCAL_HTTP_PROXY:-}" \
        "${HTTP_PROXY:-}" \
        "${http_proxy:-}" \
        "${vscode_proxy_value}" \
        "$(read_macos_proxy_url "http")")"
    raw_https_proxy="$(first_proxy_value \
        "${DEVCONTAINER_HTTPS_PROXY:-}" \
        "${LOCAL_DEVCONTAINER_HTTPS_PROXY:-}" \
        "${LOCAL_HTTPS_PROXY:-}" \
        "${HTTPS_PROXY:-}" \
        "${https_proxy:-}" \
        "${vscode_proxy_value}" \
        "$(read_macos_proxy_url "https")")"
    raw_no_proxy="$(first_proxy_value \
        "${DEVCONTAINER_NO_PROXY:-}" \
        "${LOCAL_DEVCONTAINER_NO_PROXY:-}" \
        "${LOCAL_NO_PROXY:-}" \
        "${NO_PROXY:-}" \
        "${no_proxy:-}")"

    export HTTP_PROXY
    export HTTPS_PROXY
    export NO_PROXY
    export http_proxy
    export https_proxy
    export no_proxy

    HTTP_PROXY="$(normalize_proxy_url "${raw_http_proxy}")"
    HTTPS_PROXY="$(normalize_proxy_url "${raw_https_proxy}")"
    NO_PROXY="${raw_no_proxy}"
    http_proxy="${HTTP_PROXY}"
    https_proxy="${HTTPS_PROXY}"
    no_proxy="${NO_PROXY}"
}

load_proxy_env_file() {
    local env_file="$1"

    if [[ ! -f "${env_file}" ]]; then
        return 0
    fi

    set -a
    source "${env_file}"
    set +a
}

emit_proxy_exports() {
    printf 'export HTTP_PROXY=%q\n' "${HTTP_PROXY:-}"
    printf 'export HTTPS_PROXY=%q\n' "${HTTPS_PROXY:-}"
    printf 'export NO_PROXY=%q\n' "${NO_PROXY:-}"
    printf 'export http_proxy=%q\n' "${http_proxy:-}"
    printf 'export https_proxy=%q\n' "${https_proxy:-}"
    printf 'export no_proxy=%q\n' "${no_proxy:-}"
}

write_shell_proxy_exports() {
    local profile_file="$1"

    mkdir -p "$(dirname "${profile_file}")"
    emit_proxy_exports >"${profile_file}"
    chmod 0644 "${profile_file}"
}

write_curl_config() {
    local config_file="$1"
    local owner="${2:-}"

    mkdir -p "$(dirname "${config_file}")"
    cat >"${config_file}" <<'EOF'
http1.1
retry = 5
retry-delay = 2
retry-all-errors
retry-connrefused
connect-timeout = 30
EOF
    chmod 0644 "${config_file}"
    if [[ -n "${owner}" ]]; then
        chown "${owner}" "${config_file}"
    fi
}

configure_vscode_proxy_settings() {
    local proxy_value="${HTTPS_PROXY:-${HTTP_PROXY:-}}"
    local settings_file="${HOME}/.vscode-server/data/Machine/settings.json"

    mkdir -p "$(dirname "${settings_file}")"
    VSCODE_PROXY_VALUE="${proxy_value}" python3 - "${settings_file}" <<'PY'
import json
import os
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
try:
    settings = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(settings, dict):
        settings = {}
except FileNotFoundError:
    settings = {}
except json.JSONDecodeError:
    settings = {}

settings["http.useLocalProxyConfiguration"] = False
proxy = os.environ.get("VSCODE_PROXY_VALUE", "")
if proxy:
    settings["http.proxy"] = proxy
else:
    settings.pop("http.proxy", None)

tmp_path = path.with_suffix(path.suffix + ".tmp")
tmp_path.write_text(json.dumps(settings, indent=2, sort_keys=True) + "\n", encoding="utf-8")
tmp_path.replace(path)
PY
}

write_proxy_env_file() {
    local env_file="$1"

    mkdir -p "$(dirname "${env_file}")"
    umask 077
    {
        echo "# Generated by .devcontainer/initialize-proxy.sh."
        echo "# Do not commit this file."
        printf 'HTTP_PROXY=%s\n' "${HTTP_PROXY:-}"
        printf 'HTTPS_PROXY=%s\n' "${HTTPS_PROXY:-}"
        printf 'NO_PROXY=%s\n' "${NO_PROXY:-}"
        printf 'http_proxy=%s\n' "${http_proxy:-}"
        printf 'https_proxy=%s\n' "${https_proxy:-}"
        printf 'no_proxy=%s\n' "${no_proxy:-}"
    } >"${env_file}"
}
