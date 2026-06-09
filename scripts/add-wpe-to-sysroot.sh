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

# @file add-wpe-to-sysroot.sh
# @brief Add WPE WebKit (for libaampjsbindings.so) to the XiOne cross-sysroot.
#
# jsbindings/*.cpp use only the stable JavaScriptCore C API
# (<JavaScriptCore/JavaScript.h>), not WPE/WebKit browser headers, so we need:
#   1. JSC C-API headers   — from Ubuntu's libjavascriptcoregtk dev package
#                            (the C API is ABI-stable across WebKit versions).
#   2. libWPEWebKit-1.1.so — harvested from the device (the production image
#      + libwpe-1.0.so       ships the runtime .so but no dev headers / .pc).
#   3. a wpe-webkit-1.1.pc  — synthesized so pkg_check_modules() resolves it.
#
# Run once after build-xione-sysroot.sh, then build with:
#   scripts/build-xione.sh --jsbindings
#
# Env overrides: XIONE_CONTAINER, XIONE_SYSROOT, XIONE_DEVICE (user@host),
#                XIONE_DEVICE_PORT.
set -euo pipefail

CONTAINER="${XIONE_CONTAINER:-xione-build}"
SYSROOT="${XIONE_SYSROOT:-/opt/xione-sysroot}"
DEVICE="${XIONE_DEVICE:-root@10.0.10.12}"
PORT="${XIONE_DEVICE_PORT:-10022}"

# WPE runtime objects shipped on the device (versioned real files).
WPE_WEBKIT="libWPEWebKit-1.1.so.0.2.17"
LIBWPE="libwpe-1.0.so.1.9.5"

cexec() { docker exec "$CONTAINER" bash -lc "$1"; }

echo "[wpe] 1/4 JSC C-API headers via apt (container)"
cexec "apt-get install -y --no-install-recommends libjavascriptcoregtk-4.1-dev >/tmp/apt-jsc.log 2>&1 \
  || apt-get install -y --no-install-recommends libjavascriptcoregtk-4.0-dev >/tmp/apt-jsc.log 2>&1"
JSC_SRC=$(cexec "find /usr/include -ipath '*JavaScriptCore/JavaScript.h' -printf '%h\n' | head -n1")
[ -n "$JSC_SRC" ] || { echo "[wpe] ERROR: JavaScriptCore headers not found after apt install" >&2; exit 1; }

echo "[wpe] 2/4 harvest WPE runtime libs from device (${DEVICE})"
ssh -p "$PORT" "$DEVICE" "cd /usr/lib && tar -cf - ${WPE_WEBKIT} ${LIBWPE}" \
  | docker exec -i "$CONTAINER" tar -xf - -C "${SYSROOT}/usr/lib/arm-linux-gnueabihf"

echo "[wpe] 3/4 install headers + symlinks into sysroot"
cexec "set -e
  LIBDIR=${SYSROOT}/usr/lib/arm-linux-gnueabihf
  INCDIR=${SYSROOT}/usr/include/wpe-webkit-1.1
  mkdir -p \"\$INCDIR/JavaScriptCore\"
  cp -a ${JSC_SRC}/JavaScriptCore/. \"\$INCDIR/JavaScriptCore/\"
  cd \"\$LIBDIR\"
  ln -sf ${WPE_WEBKIT} libWPEWebKit-1.1.so.0
  ln -sf ${WPE_WEBKIT} libWPEWebKit-1.1.so
  ln -sf ${LIBWPE}     libwpe-1.0.so.1
  ln -sf ${LIBWPE}     libwpe-1.0.so"

echo "[wpe] 4/4 synthesize wpe-webkit-1.1.pc"
cexec "cat > ${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig/wpe-webkit-1.1.pc <<'PC'
prefix=/usr
exec_prefix=\${prefix}
libdir=\${prefix}/lib/arm-linux-gnueabihf
includedir=\${prefix}/include/wpe-webkit-1.1

Name: wpe-webkit-1.1
Description: WPE WebKit (XiOne-harvested runtime; JSC C-API headers for aampjsbindings)
Version: 2.17.0
Libs: -L\${libdir} -lWPEWebKit-1.1
Cflags: -I\${includedir}
PC"

echo "[wpe] done. Build jsbindings with: scripts/build-xione.sh --jsbindings"
