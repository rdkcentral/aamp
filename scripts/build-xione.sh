#!/usr/bin/env bash
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

# @file build-xione.sh
# @brief Cross-build the AAMP workspace for the Sky XiOne (armv7 hard-float).
#
# Runs cmake configure + build inside the `xione-build` amd64 container against
# the Bootlin gcc 11.3 toolchain (/opt/toolchain) and the device sysroot
# (/opt/xione-sysroot), via cmake/xione-armhf.cmake. The toolchain file carries
# the sysroot -L/-I paths, so no per-invocation flags are needed.
#
# Prerequisites (one-time): scripts/setup-xione-container.sh (build host) and
# scripts/build-xione-sysroot.sh (sysroot). Run this from the repo on the host.
#
# Usage:
#   scripts/build-xione.sh                 # build libaamp + middleware/abr/metrics/tsb
#   scripts/build-xione.sh --jsbindings    # also build libaampjsbindings.so (needs WPE in sysroot:
#                                          #   scripts/add-wpe-to-sysroot.sh)
#   scripts/build-xione.sh --all           # also build aamp-cli + gstTestHarness
#   scripts/build-xione.sh --clean         # wipe the build dir first
#   scripts/build-xione.sh --jobs 4
#   scripts/build-xione.sh -- -DSOME_FLAG=ON   # pass extra -D flags to cmake
#
# Env overrides: XIONE_CONTAINER, XIONE_SYSROOT, XIONE_BUILD_DIR.
set -euo pipefail

CONTAINER="${XIONE_CONTAINER:-xione-build}"
SYSROOT="${XIONE_SYSROOT:-/opt/xione-sysroot}"
BUILD="${XIONE_BUILD_DIR:-/work/build-xione}"
TOOLCHAIN="/work/cmake/xione-armhf.cmake"
JOBS=8
TARGET=(--target aamp)   # default: libaamp + its in-tree deps
JSBINDINGS=OFF
CLEAN=0
EXTRA=()

while [ $# -gt 0 ]; do
  case "$1" in
    --jsbindings) JSBINDINGS=ON; TARGET=(--target aamp --target aampjsbindings) ;;
    --all)        TARGET=(); JSBINDINGS=ON ;;         # default cmake target = everything
    --clean)      CLEAN=1 ;;
    --jobs|-j)    JOBS="$2"; shift ;;
    --)           shift; EXTRA=("$@"); break ;;
    -h|--help)    sed -n '20,42p' "$0"; exit 0 ;;
    *)            echo "build-xione: unknown arg '$1' (try --help)" >&2; exit 2 ;;
  esac
  shift
done

if ! docker ps --format '{{.Names}}' | grep -qx "$CONTAINER"; then
  echo "build-xione: container '$CONTAINER' is not running." >&2
  echo "  Set it up with: scripts/setup-xione-container.sh" >&2
  exit 1
fi

# pkg-config must resolve the device sysroot's .pc files, not the container's x86 ones.
PCENV="PKG_CONFIG_SYSROOT_DIR=${SYSROOT} \
PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/lib/pkgconfig \
PKG_CONFIG_PATH="

[ "$CLEAN" = "1" ] && { echo "[xione] clean ${BUILD}"; docker exec "$CONTAINER" rm -rf "$BUILD"; }

echo "[xione] configure (toolchain: ${TOOLCHAIN}, sysroot: ${SYSROOT})"
docker exec "$CONTAINER" bash -lc "env ${PCENV} cmake -S /work -B ${BUILD} \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN} \
  -DCMAKE_INBUILT_AAMP_DEPENDENCIES=ON \
  -DCMAKE_SYSTEMD_JOURNAL=ON -DCMAKE_USE_ETHAN_LOG=ON \
  -DCMAKE_TELEMETRY_2_0_REQUIRED=OFF \
  -DCMAKE_WPEWEBKIT_JSBINDINGS=${JSBINDINGS} \
  -DCMAKE_BUILD_KOTLIN_ENABLED=OFF \
  ${EXTRA[*]:-}"

echo "[xione] build (-j${JOBS})"
docker exec "$CONTAINER" bash -lc "env ${PCENV} cmake --build ${BUILD} ${TARGET[*]:-} -j${JOBS}"

echo "[xione] done -> ${BUILD#/work/}/libaamp.so (host: ${BUILD#/work/}/)"
