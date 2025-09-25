# syntax=docker/dockerfile:1
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       cmake \
       ninja-build \
       clang \
       clang-format \
       gdb lldb \
       pkg-config \
       liblz4-dev \
       libzstd-dev \
       libreadline-dev \
       git curl ca-certificates \
       ripgrep \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user (vscode) with passwordless sudo (optional)
RUN useradd -ms /bin/bash vscode && \
    apt-get update && apt-get install -y sudo && \
    echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER vscode
WORKDIR /workspaces/app

