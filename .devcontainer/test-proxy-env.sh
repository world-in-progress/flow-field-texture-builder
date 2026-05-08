#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/proxy-env.sh"

assert_eq() {
    local actual="$1"
    local expected="$2"
    local label="$3"

    if [[ "${actual}" != "${expected}" ]]; then
        printf 'FAIL: %s\nexpected: %s\nactual:   %s\n' "${label}" "${expected}" "${actual}" >&2
        exit 1
    fi
}

assert_eq "$(normalize_proxy_url 'http://127.0.0.1:7897')" \
    "http://host.docker.internal:7897" \
    "rewrites IPv4 loopback proxy"
assert_eq "$(normalize_proxy_url 'https://localhost:7897')" \
    "https://host.docker.internal:7897" \
    "rewrites localhost proxy"
assert_eq "$(normalize_proxy_url 'socks5://user:pass@127.0.0.1:7897')" \
    "socks5://user:pass@host.docker.internal:7897" \
    "preserves scheme and credentials"
assert_eq "$(normalize_proxy_url 'http://192.168.1.10:7897')" \
    "http://192.168.1.10:7897" \
    "leaves non-loopback proxy alone"

unset HTTP_PROXY HTTPS_PROXY NO_PROXY http_proxy https_proxy no_proxy
unset DEVCONTAINER_HTTP_PROXY DEVCONTAINER_HTTPS_PROXY DEVCONTAINER_NO_PROXY
export LOCAL_HTTP_PROXY="http://127.0.0.1:7897"
export LOCAL_HTTPS_PROXY="http://localhost:7897"
export LOCAL_NO_PROXY="localhost,127.0.0.1"
apply_normalized_proxy_env

assert_eq "${HTTP_PROXY}" "http://host.docker.internal:7897" "exports normalized HTTP_PROXY"
assert_eq "${HTTPS_PROXY}" "http://host.docker.internal:7897" "exports normalized HTTPS_PROXY"
assert_eq "${http_proxy}" "${HTTP_PROXY}" "exports lowercase http_proxy"
assert_eq "${https_proxy}" "${HTTPS_PROXY}" "exports lowercase https_proxy"
assert_eq "${NO_PROXY}" "localhost,127.0.0.1" "preserves NO_PROXY"

tmp_env="$(mktemp)"
write_proxy_env_file "${tmp_env}"
assert_eq "$(grep '^HTTP_PROXY=' "${tmp_env}")" \
    "HTTP_PROXY=http://host.docker.internal:7897" \
    "writes normalized env file"
rm -f "${tmp_env}"

unset HTTP_PROXY HTTPS_PROXY NO_PROXY http_proxy https_proxy no_proxy
unset DEVCONTAINER_HTTP_PROXY DEVCONTAINER_HTTPS_PROXY DEVCONTAINER_NO_PROXY
unset LOCAL_HTTP_PROXY LOCAL_HTTPS_PROXY LOCAL_NO_PROXY
unset LOCAL_DEVCONTAINER_HTTP_PROXY LOCAL_DEVCONTAINER_HTTPS_PROXY LOCAL_DEVCONTAINER_NO_PROXY
tmp_settings="$(mktemp)"
cat >"${tmp_settings}" <<'JSON'
{
  "https.proxy": "http://127.0.0.1:9981",
  "http.proxy": "http://127.0.0.1:7897"
}
JSON
VSCODE_SETTINGS_FILE="${tmp_settings}" apply_normalized_proxy_env
assert_eq "${HTTP_PROXY}" "http://host.docker.internal:7897" "reads VS Code http.proxy for HTTP"
assert_eq "${HTTPS_PROXY}" "http://host.docker.internal:7897" "uses VS Code http.proxy for HTTPS"
rm -f "${tmp_settings}"

unset HTTP_PROXY HTTPS_PROXY NO_PROXY http_proxy https_proxy no_proxy
tmp_scutil="$(mktemp)"
cat >"${tmp_scutil}" <<'SCUTIL'
<dictionary> {
  HTTPEnable : 1
  HTTPPort : 7897
  HTTPProxy : 127.0.0.1
  HTTPSEnable : 1
  HTTPSPort : 7897
  HTTPSProxy : 127.0.0.1
}
SCUTIL
VSCODE_SETTINGS_FILE="/does/not/exist" SCUTIL_PROXY_FILE="${tmp_scutil}" apply_normalized_proxy_env
assert_eq "${HTTP_PROXY}" "http://host.docker.internal:7897" "reads macOS HTTP proxy"
assert_eq "${HTTPS_PROXY}" "http://host.docker.internal:7897" "reads macOS HTTPS proxy"
rm -f "${tmp_scutil}"

grep -q 'COPY proxy-env.sh proxy.env /usr/local/share/devcontainer/' "${SCRIPT_DIR}/Dockerfile"
grep -q 'BASH_ENV=/usr/local/share/devcontainer/proxy-build-env.sh' "${SCRIPT_DIR}/Dockerfile"
grep -q 'write_curl_config /root/.curlrc' "${SCRIPT_DIR}/Dockerfile"
grep -q 'write_curl_config /home/vscode/.curlrc vscode:vscode' "${SCRIPT_DIR}/Dockerfile"

tmp_home="$(mktemp -d)"
trap 'rm -rf "${tmp_home}"' EXIT
HOME="${tmp_home}" configure_vscode_proxy_settings

python3 - "${tmp_home}/.vscode-server/data/Machine/settings.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as settings_file:
    settings = json.load(settings_file)

assert settings["http.proxy"] == "http://host.docker.internal:7897"
assert settings["http.useLocalProxyConfiguration"] is False
PY

tmp_curlrc="$(mktemp)"
write_curl_config "${tmp_curlrc}"
grep -q '^http1.1$' "${tmp_curlrc}"
grep -q '^retry = 5$' "${tmp_curlrc}"
grep -q '^retry-all-errors$' "${tmp_curlrc}"
rm -f "${tmp_curlrc}"

echo "Proxy normalization tests passed."
