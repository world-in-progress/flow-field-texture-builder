FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG LOCAL_HTTP_PROXY
ARG LOCAL_HTTPS_PROXY
ARG LOCAL_NO_PROXY
ARG LOCAL_DEVCONTAINER_HTTP_PROXY
ARG LOCAL_DEVCONTAINER_HTTPS_PROXY
ARG LOCAL_DEVCONTAINER_NO_PROXY

ENV DEBIAN_FRONTEND=noninteractive \
    BASH_ENV=/usr/local/share/devcontainer/proxy-build-env.sh
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

COPY proxy-env.sh proxy.env /usr/local/share/devcontainer/

RUN . /usr/local/share/devcontainer/proxy-env.sh \
    && load_proxy_env_file /usr/local/share/devcontainer/proxy.env \
    && apply_normalized_proxy_env \
    && write_shell_proxy_exports "${BASH_ENV}" \
    && write_curl_config /root/.curlrc \
    && write_curl_config /home/vscode/.curlrc vscode:vscode \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        curl \
        gdb \
        git \
        lldb \
        ninja-build \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        python3-venv \
        swig \
        tar \
        unzip \
        zip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER vscode
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/home/vscode/.local/bin:${PATH}"

USER root
