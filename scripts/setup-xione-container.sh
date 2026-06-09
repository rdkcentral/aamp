#!/usr/bin/env bash
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# setup-xione-container.sh
#
# LOCKED BUILD APPROACH: Approach 3 — public Bootlin
#   armv7-eabihf--glibc--stable-2022.08-1 (gcc 11.3.0 / glibc 2.35, new C++11 string ABI);
#   bitbake/OE-SDK kept only as opportunistic fidelity upgrades.
#
# This script satisfies TOOL-01 (toolchain acquired and available on the build host) and
# TOOL-02 (build approach selected — Approach 3 Bootlin — and recorded as committed tooling).
#
# Idempotent: safe to re-run. Run from macOS host; drives the Docker container via docker exec.
# The container binds the AAMP repo at /work; the macOS arm64 host is orchestration-only.
#
# Usage:
#   bash scripts/setup-xione-container.sh [--repo-dir /path/to/aamp]

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CONTAINER_NAME="xione-build"
CONTAINER_IMAGE="ubuntu:22.04"
REPO_DIR="${REPO_DIR:-${HOME}/Development/aamp}"

# Parse CLI args (the documented --repo-dir flag overrides REPO_DIR). (code-review WR-01)
while [ "$#" -gt 0 ]; do
    case "$1" in
        --repo-dir)
            [ "$#" -ge 2 ] || { echo "ERROR: --repo-dir requires a path argument" >&2; exit 2; }
            REPO_DIR="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: bash scripts/setup-xione-container.sh [--repo-dir /path/to/aamp]"
            exit 0
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            echo "Usage: bash scripts/setup-xione-container.sh [--repo-dir /path/to/aamp]" >&2
            exit 2
            ;;
    esac
done

TOOLCHAIN_DIR="/opt/toolchain"
TARBALL="armv7-eabihf--glibc--stable-2022.08-1.tar.bz2"
TOOLCHAIN_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/${TARBALL}"
EXPECTED_SHA256="64329b3e72350ceda65997368395a945ef83769013d82414dc5f2021c33f2d44"
CROSS_TRIPLE="arm-buildroot-linux-gnueabihf"

# ---------------------------------------------------------------------------
# Helper: run a command inside the container
# ---------------------------------------------------------------------------

container_exec() {
    docker exec "${CONTAINER_NAME}" bash -c "$1"
}

# ---------------------------------------------------------------------------
# Step 1: Create or reuse the persistent named container
# ---------------------------------------------------------------------------

setup_container_fn() {
    echo "=== Step 1: Container setup ==="

    if docker ps -a --format '{{.Names}}' | grep -qx "${CONTAINER_NAME}"; then
        echo "Container '${CONTAINER_NAME}' already exists — ensuring it is running."
        if ! docker ps --format '{{.Names}}' | grep -qx "${CONTAINER_NAME}"; then
            echo "Container is stopped; starting it."
            docker start "${CONTAINER_NAME}"
        else
            echo "Container is already running."
        fi
    else
        echo "Creating container '${CONTAINER_NAME}' (linux/amd64, bind-mounting ${REPO_DIR} -> /work)."
        docker run -d \
            --name "${CONTAINER_NAME}" \
            --platform linux/amd64 \
            -v "${REPO_DIR}:/work" \
            -w /work \
            "${CONTAINER_IMAGE}" \
            sleep infinity
        echo "Container created."
    fi
}

# ---------------------------------------------------------------------------
# Step 2: Install apt prerequisites (idempotent — skip if already installed)
# ---------------------------------------------------------------------------

install_apt_prereqs_fn() {
    echo "=== Step 2: apt prerequisites ==="

    # Guard: if gcc (from build-essential) is already present, prerequisites are installed.
    if container_exec "command -v gcc >/dev/null 2>&1 && command -v wget >/dev/null 2>&1"; then
        echo "apt prerequisites already installed — skipping."
        return 0
    fi

    echo "Installing apt packages inside container..."
    container_exec "
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -q
        apt-get install -y \
            wget curl bzip2 tar \
            build-essential cmake \
            binutils-multiarch \
            git python3 pkg-config \
            file
    "
    echo "apt prerequisites installed."
}

# ---------------------------------------------------------------------------
# Step 3: Download, SHA256-verify, and extract the Bootlin toolchain
# ---------------------------------------------------------------------------

install_toolchain_fn() {
    echo "=== Step 3: Bootlin toolchain install ==="

    # Skip if the compiler binary already exists (idempotent).
    if container_exec "test -x '${TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-gcc'"; then
        echo "Toolchain already installed at ${TOOLCHAIN_DIR} — skipping download/extract."
    else
        echo "Downloading Bootlin toolchain tarball (approx 126 MB — please wait)..."
        container_exec "
            set -euo pipefail
            cd /tmp
            wget -q '${TOOLCHAIN_URL}' -O '${TARBALL}'

            echo '--- SHA256 verification ---'
            echo '${EXPECTED_SHA256}  ${TARBALL}' | sha256sum -c
            echo 'SHA256 OK'

            echo '--- Inspecting tarball top-level directory ---'
            TOP_DIR=\$(tar tjf '${TARBALL}' | head -3 | awk -F/ '{print \$1}' | sort -u | head -1)
            echo \"Top-level directory: \${TOP_DIR}\"

            echo '--- Extracting to ${TOOLCHAIN_DIR} ---'
            mkdir -p '${TOOLCHAIN_DIR}'
            tar xf '${TARBALL}' --strip-components=1 -C '${TOOLCHAIN_DIR}'
            echo 'Extraction complete.'
        "
    fi

    # Ensure PATH entry in ~/.bashrc (guarded against duplicate appends).
    container_exec "
        if ! grep -q 'export PATH=${TOOLCHAIN_DIR}/bin' ~/.bashrc 2>/dev/null; then
            echo 'export PATH=${TOOLCHAIN_DIR}/bin:\$PATH' >> ~/.bashrc
            echo 'Added toolchain to ~/.bashrc PATH.'
        else
            echo 'PATH entry already in ~/.bashrc — skipping.'
        fi
    "
}

# ---------------------------------------------------------------------------
# Step 4: Self-check — verify gcc version and host architecture
# ---------------------------------------------------------------------------

verify_toolchain_fn() {
    echo "=== Step 4: Toolchain self-check ==="

    GCC_VERSION=$(container_exec "export PATH=${TOOLCHAIN_DIR}/bin:\$PATH; ${CROSS_TRIPLE}-gcc --version" | head -1)
    echo "gcc --version: ${GCC_VERSION}"
    if ! echo "${GCC_VERSION}" | grep -q "11.3.0"; then
        echo "ERROR: Expected gcc 11.3.0 but got: ${GCC_VERSION}" >&2
        exit 1
    fi
    echo "gcc version check: PASSED (11.3.0 confirmed)"

    ARCH=$(container_exec "uname -m")
    echo "Container uname -m: ${ARCH}"
    if [ "${ARCH}" != "x86_64" ]; then
        echo "ERROR: Expected x86_64 but got: ${ARCH}" >&2
        exit 1
    fi
    echo "Architecture check: PASSED (x86_64 confirmed)"

    echo ""
    echo "=== setup-xione-container.sh: ALL CHECKS PASSED ==="
    echo "Container: ${CONTAINER_NAME}"
    echo "Toolchain: ${TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-gcc (gcc 11.3.0)"
    echo "Approach 3 (Bootlin armv7-eabihf--glibc--stable-2022.08-1) is active."
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

setup_container_fn
install_apt_prereqs_fn
install_toolchain_fn
verify_toolchain_fn
