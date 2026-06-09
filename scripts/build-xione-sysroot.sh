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

# @file build-xione-sysroot.sh
# @brief XiOne sysroot assembler — idempotent script that scaffolds
#        /opt/xione-sysroot inside the xione-build container, extracts Debian
#        armhf dev .deb packages for headers/.pc/bare symlinks, harvests
#        ABI-exact runtime .so files from the live XiOne device, creates the
#        libethanlog bare symlink, and verifies all SYS-02 bare .so symlinks.

set -euo pipefail

# ---------------------------------------------------------------------------
# Module-level constants
# ---------------------------------------------------------------------------
SYSROOT=/opt/xione-sysroot
MULTIARCH=arm-linux-gnueabihf
CONTAINER=xione-build
DEVICE_SSH="ssh -p 10022 -o StrictHostKeyChecking=no root@10.0.10.12"

# ---------------------------------------------------------------------------
# pkgconfig_env — emit the three SYS-03 pkg-config isolation exports.
# Source or eval this in any shell that will call cmake or pkg-config against
# the sysroot; it replaces (not prepends) the search path so host x86_64
# paths cannot leak into the cross build.
# ---------------------------------------------------------------------------
pkgconfig_env() {
    echo "export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}"
    echo "export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig:${SYSROOT}/lib/pkgconfig"
    echo "unset PKG_CONFIG_PATH"
}

# ---------------------------------------------------------------------------
# scaffold_sysroot_fn — create the three required sysroot directories.
# Idempotent: mkdir -p is a no-op if they already exist.
# ---------------------------------------------------------------------------
scaffold_sysroot_fn() {
    echo "[scaffold] Creating sysroot directory layout..."
    docker exec "${CONTAINER}" mkdir -p \
        "${SYSROOT}/usr/include" \
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig" \
        "${SYSROOT}/lib"
    echo "[scaffold] Done."
}

# ---------------------------------------------------------------------------
# extract_debs_fn — download and extract Debian armhf dev .deb packages into
# the sysroot.  dpkg-deb --extract places everything at the correct multiarch
# paths: headers -> usr/include/, .pc -> usr/lib/arm-linux-gnueabihf/pkgconfig/,
# bare .so symlinks -> usr/lib/arm-linux-gnueabihf/.
# Each package is guarded by a presence sentinel (.pc file or header dir) so
# re-runs skip already-extracted packages.
# ---------------------------------------------------------------------------
extract_debs_fn() {
    echo "[debs] Extracting Debian armhf dev packages into sysroot..."

    # All 9 verified URLs (https:// — official Debian mirrors)
    local DEBS=(
        "https://security.debian.org/debian-security/pool/updates/main/g/gstreamer1.0/libgstreamer1.0-dev_1.18.4-2.1+deb11u1_armhf.deb"
        "https://security.debian.org/debian-security/pool/updates/main/g/gst-plugins-base1.0/libgstreamer-plugins-base1.0-dev_1.18.4-2+deb11u5_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/g/glib2.0/libglib2.0-dev_2.74.6-2+deb12u9_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/libx/libxml2/libxml2-dev_2.9.14+dfsg-1.3~deb12u5_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/c/cjson/libcjson-dev_1.7.15-1+deb12u4_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/o/openssl/libssl-dev_3.0.20-1~deb12u1_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/c/curl/libcurl4-openssl-dev_7.88.1-10+deb12u14_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/u/util-linux/uuid-dev_2.38.1-5+deb12u3_armhf.deb"
        "https://ftp.debian.org/debian/pool/main/s/systemd/libsystemd-dev_252.39-1~deb12u2_armhf.deb"
    )

    # Per-package presence sentinels (a .pc file or key header that the deb provides).
    # Indexed 1-to-1 with DEBS above.
    local SENTINELS=(
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/gstreamer-1.0.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/gstreamer-app-1.0.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/glib-2.0.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/libxml-2.0.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/libcjson.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/openssl.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/libcurl.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/uuid.pc"
        "${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/libsystemd.pc"
    )

    local NUM_DEBS=${#DEBS[@]}
    for (( i=0; i<NUM_DEBS; i++ )); do
        local URL="${DEBS[$i]}"
        local SENTINEL="${SENTINELS[$i]}"
        local PKG_NAME
        PKG_NAME=$(basename "${URL}" .deb)

        # Run the presence check and extraction inside the container
        docker exec "${CONTAINER}" bash -c "
set -euo pipefail
SENTINEL='${SENTINEL}'
URL='${URL}'
PKG_NAME='${PKG_NAME}'
SYSROOT='${SYSROOT}'
if [ -e \"\${SENTINEL}\" ]; then
    echo \"[debs] SKIP \${PKG_NAME} (sentinel exists: \${SENTINEL})\"
    exit 0
fi
echo \"[debs] Extracting \${PKG_NAME} ...\"
TMPFILE=\$(mktemp /tmp/pkg-XXXXXX.deb)
wget -q \"\${URL}\" -O \"\${TMPFILE}\"
dpkg-deb --extract \"\${TMPFILE}\" \"\${SYSROOT}\"
rm -f \"\${TMPFILE}\"
echo \"[debs] OK \${PKG_NAME}\"
"
    done

    echo "[debs] All packages processed."
}

# ---------------------------------------------------------------------------
# harvest_device_fn — stream runtime .so files from the live XiOne device into
# the sysroot multiarch directory via ssh tar pipe.
# Guarded by a presence check on libgstreamer-1.0.so.0.1805.0 so re-runs skip
# the slow harvest when it has already been done.
# NEVER harvests glibc/libstdc++/libgcc (toolchain provides these).
# NEVER harvests libplayerjsonobject.so (not in DT_NEEDED).
# ---------------------------------------------------------------------------
harvest_device_fn() {
    local SENTINEL="${SYSROOT}/usr/lib/${MULTIARCH}/libgstreamer-1.0.so.0.1805.0"

    if docker exec "${CONTAINER}" test -e "${SENTINEL}" 2>/dev/null; then
        echo "[harvest] SKIP — sentinel exists: ${SENTINEL}"
    else
        echo "[harvest] Streaming /usr/lib libs from device..."
        # shellcheck disable=SC2029
        ${DEVICE_SSH} 'cd /usr/lib && tar -cf - \
            libgstreamer-1.0.so* \
            libgstapp-1.0.so* libgstbase-1.0.so* libgstvideo-1.0.so* \
            libglib-2.0.so* libgio-2.0.so* libgobject-2.0.so* \
            libxml2.so* libcjson.so* \
            libssl.so.3* libcrypto.so.3* \
            libcurl.so* libuuid.so* \
            libplayergstinterface.so libplayerfbinterface.so \
            libplayerlogmanager.so libbaseconversion.so \
            libabr.so libmetrics.so libsubtec.so \
            libethanlog.so* \
            2>/dev/null' \
        | docker exec -i "${CONTAINER}" \
            tar -C "${SYSROOT}/usr/lib/${MULTIARCH}" -xf -

        echo "[harvest] Streaming /lib/libsystemd from device..."
        # systemd lives in /lib on this device, not /usr/lib; harvest into
        # the same multiarch dir so deb-provided bare symlink resolves correctly.
        # shellcheck disable=SC2029
        ${DEVICE_SSH} 'cd /lib && tar -cf - libsystemd.so* 2>/dev/null' \
        | docker exec -i "${CONTAINER}" \
            tar -C "${SYSROOT}/usr/lib/${MULTIARCH}" -xf -

        echo "[harvest] Device harvest complete."
    fi

    # Create the libethanlog bare symlink (no deb provides it; device has it but
    # the glob above may have captured it — create unconditionally, ln -sf is safe).
    echo "[harvest] Ensuring libethanlog.so bare symlink..."
    docker exec "${CONTAINER}" bash -c "
cd '${SYSROOT}/usr/lib/${MULTIARCH}'
ln -sf libethanlog.so.3 libethanlog.so
echo '[harvest] libethanlog.so -> libethanlog.so.3 OK'
"

    # Fix libcurl.so bare symlink: the bookworm deb (7.88.1) points libcurl.so at
    # libcurl.so.4.8.0 but the device runtime has soname libcurl.so.4.7.0.  Re-point
    # libcurl.so at the device-harvested versioned file so the bare symlink resolves.
    echo "[harvest] Fixing libcurl.so bare symlink to device soname..."
    docker exec "${CONTAINER}" bash -c "
cd '${SYSROOT}/usr/lib/${MULTIARCH}'
# Detect the actual versioned libcurl file from the device harvest
VERSIONED=\$(ls libcurl.so.4.*.* 2>/dev/null | head -1)
if [ -n \"\${VERSIONED}\" ]; then
    ln -sf \"\${VERSIONED}\" libcurl.so
    echo \"[harvest] libcurl.so -> \${VERSIONED} OK\"
else
    echo '[harvest] WARNING: no libcurl.so.4.x.x found to fix symlink'
fi
"
}

# ---------------------------------------------------------------------------
# verify_symlinks_fn — confirm every DT_NEEDED bare .so symlink resolves under
# the sysroot multiarch directory.  Exits non-zero if any are missing/broken;
# prints SYS-02 PASS on success.
# libdash is excluded here (built in plan 02-03).
# ---------------------------------------------------------------------------
verify_symlinks_fn() {
    echo "[verify] Checking SYS-02 bare .so symlinks..."

    local NEEDED_BARE=(
        libgstreamer-1.0
        libgstapp-1.0
        libgstbase-1.0
        libgstvideo-1.0
        libglib-2.0
        libgio-2.0
        libgobject-2.0
        libxml2
        libcjson
        libssl
        libcrypto
        libcurl
        libuuid
        libsystemd
        libplayergstinterface
        libplayerfbinterface
        libplayerlogmanager
        libbaseconversion
        libabr
        libmetrics
        libsubtec
        libethanlog
    )

    local MISSING=0
    for LIB in "${NEEDED_BARE[@]}"; do
        local SO="${SYSROOT}/usr/lib/${MULTIARCH}/${LIB}.so"
        if ! docker exec "${CONTAINER}" bash -c "[ -e '${SO}' ]" 2>/dev/null; then
            echo "[verify] BROKEN_OR_MISSING: ${SO}"
            MISSING=$(( MISSING + 1 ))
        fi
    done

    if [ "${MISSING}" -gt 0 ]; then
        echo "[verify] FAIL: ${MISSING} bare .so symlink(s) missing or broken"
        exit 1
    fi

    echo "[verify] SYS-02 PASS: all ${#NEEDED_BARE[@]} bare .so symlinks present and resolve"
}

# ---------------------------------------------------------------------------
# author_stubs_fn — write the 5 external-player-interface .pc stub files and
# the minimal ethanlog.h stub header into the sysroot.
# Each write is guarded by a presence check for idempotency.
# The .pc stubs use a relative libdir= so PKG_CONFIG_SYSROOT_DIR prepends the
# sysroot path at query time — no hardcoded /opt/xione-sysroot inside .pc.
# ethanlog.h is needed because the device has no ethanlog headers; the stub
# declares exactly the vethanlog()/ethanlog() signatures that AAMP compiles
# against (from aamplogging.cpp / jsutils.cpp / PlayerLogManager.cpp).
# ---------------------------------------------------------------------------
author_stubs_fn() {
    echo "[stubs] Authoring external-player-interface .pc stubs and ethanlog.h..."

    local PC_DIR="${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig"
    local INC_DIR="${SYSROOT}/usr/include"

    # -----------------------------------------------------------------------
    # Helper: write a single .pc stub if it does not already exist.
    # Usage: write_pc_stub <filename> <Name> <Description> <short-lib>
    # -----------------------------------------------------------------------
    write_pc_stub() {
        local FILENAME="$1"
        local NAME="$2"
        local DESCRIPTION="$3"
        local SHORTLIB="$4"
        local DEST="${PC_DIR}/${FILENAME}"

        docker exec "${CONTAINER}" bash -c "
if [ -f '${DEST}' ]; then
    echo '[stubs] SKIP ${FILENAME} (already exists)'
    exit 0
fi
cat > '${DEST}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: ${NAME}
Description: ${DESCRIPTION} (XiOne device stub)
Version: 1.0
Libs: -L\${libdir} -l${SHORTLIB}
Cflags:
PCEOF
echo '[stubs] WROTE ${FILENAME}'
"
    }

    write_pc_stub "libbaseconversion.pc"    "libbaseconversion"    "Base Conversion Library"          "baseconversion"
    write_pc_stub "libplayerlogmanager.pc"  "libplayerlogmanager"  "Player Log Manager Library"       "playerlogmanager"
    write_pc_stub "libplayerfbinterface.pc" "libplayerfbinterface" "Player Framebuffer Interface"     "playerfbinterface"
    write_pc_stub "libplayergstinterface.pc" "libplayergstinterface" "Player GStreamer Interface"     "playergstinterface"
    write_pc_stub "libsubtec.pc"            "libsubtec"            "Subtitle Technology Library"      "subtec"

    # -----------------------------------------------------------------------
    # orc-0.4 stub .pc — gstreamer-video-1.0.pc carries
    # "Requires.private: orc-0.4". pkg-config follows Requires.private when
    # evaluating --cflags, so cmake's pkg_check_modules(... gstreamer-video-1.0)
    # will fail with "Package 'orc-0.4' not found" unless this stub is present.
    # orc is an internal GStreamer JIT-compiler library; AAMP never calls it
    # directly, so an empty stub is safe.
    # -----------------------------------------------------------------------
    local ORC_PC="${PC_DIR}/orc-0.4.pc"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${ORC_PC}' ]; then
    echo '[stubs] SKIP orc-0.4.pc (already exists)'
    exit 0
fi
cat > '${ORC_PC}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: orc-0.4
Description: Oil Runtime Compiler (internal GStreamer dep, XiOne stub)
Version: 0.4.33
Libs: -L\${libdir} -lorc-0.4
Cflags:
PCEOF
echo '[stubs] WROTE orc-0.4.pc'
"

    # -----------------------------------------------------------------------
    # libpcre2-8 stub .pc — glib-2.0.pc (bookworm) carries
    # "Requires.private: libpcre2-8 >= 10.32".  pkg-config follows this
    # when computing --cflags of any package that Requires: glib-2.0
    # (e.g. gstreamer-1.0), even for a shared-library build.  Without this
    # stub the verify_pkgconfig_fn --cflags check aborts.  Version must
    # satisfy the ">= 10.32" constraint; 10.42 matches the bookworm package.
    # pcre2 is an internal glib implementation detail; AAMP never includes
    # pcre2 headers directly, so an empty-Cflags stub is safe.
    # -----------------------------------------------------------------------
    local PCRE2_PC="${PC_DIR}/libpcre2-8.pc"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${PCRE2_PC}' ] && grep -q 'Version: 10' '${PCRE2_PC}'; then
    echo '[stubs] SKIP libpcre2-8.pc (already exists with correct version)'
    exit 0
fi
cat > '${PCRE2_PC}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: libpcre2-8
Description: Perl-Compatible Regular Expressions (internal glib dep, XiOne stub)
Version: 10.42
Libs: -L\${libdir} -lpcre2-8
Cflags:
PCEOF
echo '[stubs] WROTE libpcre2-8.pc (version 10.42)'
"

    # -----------------------------------------------------------------------
    # libffi stub .pc — gobject-2.0.pc carries "Requires.private: libffi >= 3.0.0".
    # pkg-config propagates this when --cflags is requested transitively through
    # gstreamer-1.0 -> gobject-2.0.  Version 3.4 satisfies the >= 3.0.0 constraint.
    # -----------------------------------------------------------------------
    local LIBFFI_PC="${PC_DIR}/libffi.pc"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${LIBFFI_PC}' ] && grep -q 'Version: 3' '${LIBFFI_PC}'; then
    echo '[stubs] SKIP libffi.pc (already exists with correct version)'
    exit 0
fi
cat > '${LIBFFI_PC}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: libffi
Description: Foreign Function Interface Library (internal gobject dep, XiOne stub)
Version: 3.4.4
Libs: -L\${libdir} -lffi
Cflags:
PCEOF
echo '[stubs] WROTE libffi.pc (version 3.4.4)'
"

    # -----------------------------------------------------------------------
    # libunwind + libdw stub .pc files — gstreamer-1.0.pc carries
    # "Requires.private: gmodule-no-export-2.0 libunwind libdw".
    # pkg-config propagates these when --cflags is requested on gstreamer-1.0.
    # -----------------------------------------------------------------------
    local LIBUNWIND_PC="${PC_DIR}/libunwind.pc"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${LIBUNWIND_PC}' ]; then
    echo '[stubs] SKIP libunwind.pc (already exists)'
    exit 0
fi
cat > '${LIBUNWIND_PC}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: libunwind
Description: Portable Call-Chain Unwinding Library (internal gstreamer dep, XiOne stub)
Version: 1.6.2
Libs: -L\${libdir} -lunwind
Cflags:
PCEOF
echo '[stubs] WROTE libunwind.pc'
"

    local LIBDW_PC="${PC_DIR}/libdw.pc"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${LIBDW_PC}' ]; then
    echo '[stubs] SKIP libdw.pc (already exists)'
    exit 0
fi
cat > '${LIBDW_PC}' <<'PCEOF'
libdir=/usr/lib/${MULTIARCH}

Name: libdw
Description: DWARF library (internal gstreamer dep, XiOne stub)
Version: 0.189
Libs: -L\${libdir} -ldw
Cflags:
PCEOF
echo '[stubs] WROTE libdw.pc'
"

    # -----------------------------------------------------------------------
    # ethanlog.h stub — declares the vethanlog()/ethanlog() signatures that
    # AAMP guards with #ifdef USE_ETHAN_LOG.  Real symbols come from the
    # harvested libethanlog.so.3 at link time.
    # -----------------------------------------------------------------------
    local ETHANLOG_H="${INC_DIR}/ethanlog.h"
    docker exec "${CONTAINER}" bash -c "
if [ -f '${ETHANLOG_H}' ]; then
    echo '[stubs] SKIP ethanlog.h (already exists)'
    exit 0
fi
cat > '${ETHANLOG_H}' <<'HEOF'
/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the \"License\");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an \"AS IS\" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file ethanlog.h
 * @brief Stub header for XiOne cross-build — declares the ethanlog API.
 *        Real symbols are resolved from libethanlog.so.3 on the device.
 */

#ifndef ETHANLOG_H
#define ETHANLOG_H

#include <stdarg.h>

/* Log level constants */
#define ETHAN_LOG_INFO      0
#define ETHAN_LOG_DEBUG     1
#define ETHAN_LOG_WARNING   2
#define ETHAN_LOG_ERROR     3
#define ETHAN_LOG_FATAL     4
#define ETHAN_LOG_MILESTONE 5

#ifdef __cplusplus
extern \"C\" {
#endif

void vethanlog(int level, const char *filename, const char *function,
               int line, const char *format, va_list ap);

void ethanlog(int level, const char *filename, const char *function,
              int line, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* ETHANLOG_H */
HEOF
echo '[stubs] WROTE ethanlog.h'
"

    echo "[stubs] Done."
}

# ---------------------------------------------------------------------------
# verify_pkgconfig_fn — SYS-03 isolation + SYS-05 stub resolution check.
# Runs entirely inside the container under the SYS-03 env triple so that
# results reflect cross-build pkg-config behaviour.
# Asserts:
#   - named --modversion values for gstreamer-1.0, glib-2.0, libxml-2.0, libcjson
#   - --cflags output for gstreamer-1.0 starts with -I/opt/xione-sysroot
#   - --libs output for gstreamer-1.0 references -L/opt/xione-sysroot
#   - 5 external-player-interface stubs + libcjson resolve via --exists
#   - NO x86_64 host path in any --cflags output (SYS-03 isolation)
# ---------------------------------------------------------------------------
verify_pkgconfig_fn() {
    echo "[pkgcfg] Running SYS-03 pkg-config isolation + SYS-05 stub resolution checks..."

    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig:${SYSROOT}/lib/pkgconfig
unset PKG_CONFIG_PATH

FAIL=0

# ---- SYS-03: named modversion assertions ----
check_modversion() {
    local PKG=\"\$1\"
    local EXPECTED=\"\$2\"
    local ACTUAL
    ACTUAL=\$(pkg-config --modversion \"\${PKG}\" 2>&1) || { echo \"[pkgcfg] FAIL: pkg-config --modversion \${PKG} returned error\"; FAIL=1; return; }
    if [ \"\${ACTUAL}\" = \"\${EXPECTED}\" ]; then
        echo \"[pkgcfg] OK: \${PKG} = \${ACTUAL}\"
    else
        echo \"[pkgcfg] FAIL: \${PKG} expected \${EXPECTED} got \${ACTUAL}\"
        FAIL=1
    fi
}

check_modversion gstreamer-1.0  1.18.4
check_modversion glib-2.0       2.74.6
check_modversion libxml-2.0     2.9.14
check_modversion libcjson       1.7.15

# ---- SYS-03: --cflags must contain sysroot-rooted -I paths ----
# Note: the output may begin with compiler flags such as -pthread before the
# first -I path (glib adds -pthread); we check that sysroot -I paths are
# present anywhere in the output, not that the output starts with -I.
CFLAGS_GST=\$(pkg-config --cflags gstreamer-1.0 2>&1)
echo \"[pkgcfg] gstreamer-1.0 --cflags: \${CFLAGS_GST}\"
if echo \"\${CFLAGS_GST}\" | grep -q -- \"-I${SYSROOT}\"; then
    echo '[pkgcfg] OK: --cflags contains -I${SYSROOT} (sysroot-rooted includes present)'
else
    echo '[pkgcfg] FAIL: --cflags contains no -I${SYSROOT} paths'
    FAIL=1
fi

# ---- SYS-03: --cflags must NOT contain host x86_64 paths ----
if echo \"\${CFLAGS_GST}\" | grep -q '/usr/lib/x86_64'; then
    echo '[pkgcfg] FAIL: host x86_64 path detected in --cflags (SYS-03 VIOLATION)'
    FAIL=1
else
    echo '[pkgcfg] OK: no x86_64 host path in --cflags (SYS-03 isolation confirmed)'
fi

# ---- SYS-03: --libs must reference sysroot ----
LIBS_GST=\$(pkg-config --libs gstreamer-1.0 2>&1)
echo \"[pkgcfg] gstreamer-1.0 --libs: \${LIBS_GST}\"
if echo \"\${LIBS_GST}\" | grep -q -- \"-L${SYSROOT}\"; then
    echo '[pkgcfg] OK: --libs references -L${SYSROOT}'
else
    echo '[pkgcfg] FAIL: --libs does not reference -L${SYSROOT}'
    FAIL=1
fi

# ---- SYS-05: all 5 stubs + libcjson must resolve ----
if pkg-config --exists libbaseconversion libplayerlogmanager libplayerfbinterface libplayergstinterface libsubtec libcjson; then
    echo '[pkgcfg] OK: all 5 external-player-interface stubs + libcjson resolve'
else
    echo '[pkgcfg] FAIL: one or more external-player-interface stubs not found'
    FAIL=1
fi

if [ \"\${FAIL}\" -eq 0 ]; then
    echo '[pkgcfg] SYS-03 + SYS-05 PASS'
else
    echo '[pkgcfg] FAIL: one or more pkg-config checks failed'
    exit 1
fi
"

    echo "[pkgcfg] Done."
}

# ---------------------------------------------------------------------------
# bootstrap_sysroot_runtime_fn — populate the sysroot with glibc system
# headers and C runtime objects needed for cross-compiling libdash.
#
# The Bootlin toolchain has its own internal sysroot at
# /opt/toolchain/arm-buildroot-linux-gnueabihf/sysroot, which contains
# glibc headers (features.h, time.h, stdlib.h, etc.) and the C runtime
# startup objects (crt1.o, crti.o, crtn.o) and stub linker scripts
# (libc.so, libm.so).  When CMAKE_SYSROOT=/opt/xione-sysroot, the compiler
# gets --sysroot=/opt/xione-sysroot and ONLY looks there for C system
# headers.  Until the toolchain's glibc headers are mirrored into the
# xione sysroot, compiling any C/C++ code with the toolchain file fails
# with "features.h: No such file or directory".
#
# This function is idempotent: guarded by presence of features.h.
# ---------------------------------------------------------------------------
bootstrap_sysroot_runtime_fn() {
    echo "[bootstrap] Populating xione sysroot with toolchain glibc headers + C runtime..."

    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
SYSROOT='${SYSROOT}'
TOOLCHAIN_SYSROOT='/opt/toolchain/arm-buildroot-linux-gnueabihf/sysroot'
MULTIARCH='${MULTIARCH}'

# ---- glibc system headers -------------------------------------------------
if [ -f \"\${SYSROOT}/usr/include/features.h\" ]; then
    echo '[bootstrap] SKIP glibc headers (features.h already present)'
else
    echo '[bootstrap] Copying glibc system headers from toolchain sysroot...'
    cp -rn \"\${TOOLCHAIN_SYSROOT}/usr/include/.\" \"\${SYSROOT}/usr/include/\"
    echo '[bootstrap] glibc headers copied'
fi

# ---- curl.h multiarch symlink (libcurl4-openssl-dev puts curl.h in
# /usr/include/arm-linux-gnueabihf/curl/; create the non-multiarch alias) --
if [ ! -d \"\${SYSROOT}/usr/include/curl\" ]; then
    ln -sfn \"\${SYSROOT}/usr/include/\${MULTIARCH}/curl\" \"\${SYSROOT}/usr/include/curl\"
    echo '[bootstrap] Created /usr/include/curl -> /usr/include/\${MULTIARCH}/curl'
fi

# ---- zlib (zlib1g-dev puts libz.so in arm-linux-gnueabihf/;
# cmake FindZLIB uses PATH_SUFFIXES=lib which only looks in /usr/lib) --------
if [ ! -f \"\${SYSROOT}/usr/lib/libz.a\" ]; then
    echo '[bootstrap] WARNING: libz.a not found in /usr/lib — zlib1g-dev may not be extracted'
fi
# Ensure /usr/lib/ symlinks for find_library PATH_SUFFIXES=lib
if [ ! -e \"\${SYSROOT}/usr/lib/libz.so\" ]; then
    ln -sf \"\${SYSROOT}/usr/lib/\${MULTIARCH}/libz.so\" \"\${SYSROOT}/usr/lib/libz.so\" 2>/dev/null || true
fi
if [ ! -e \"\${SYSROOT}/usr/lib/libz.a\" ]; then
    ln -sf \"\${SYSROOT}/usr/lib/\${MULTIARCH}/libz.a\" \"\${SYSROOT}/usr/lib/libz.a\" 2>/dev/null || true
fi

# ---- C runtime startup objects (crt1.o, crti.o, crtn.o) ------------------
if [ ! -f \"\${SYSROOT}/usr/lib/crti.o\" ]; then
    echo '[bootstrap] Copying C runtime startup objects from toolchain sysroot...'
    for F in crt1.o crti.o crtn.o Scrt1.o; do
        [ -f \"\${TOOLCHAIN_SYSROOT}/usr/lib/\${F}\" ] && \
            cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/\${F}\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    done
    echo '[bootstrap] crt objects copied'
fi

# ---- C runtime linker stubs (libc.so, libm.so, libpthread.so) -------------
# These are linker-script stubs pointing to the versioned runtime .so files.
if [ ! -e \"\${SYSROOT}/usr/lib/libc.so\" ]; then
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libc.a\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libc.so\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libc_nonshared.a\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    echo '[bootstrap] libc linker stubs copied'
fi
if [ ! -e \"\${SYSROOT}/usr/lib/libm.so\" ]; then
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libm.a\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libm.so\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    echo '[bootstrap] libm linker stubs copied'
fi
if [ ! -e \"\${SYSROOT}/usr/lib/libpthread.so\" ]; then
    # libpthread.so is a linker script in the toolchain; if it's absent from
    # the toolchain sysroot /usr/lib, create a minimal GNU ld script manually.
    if [ -f \"\${TOOLCHAIN_SYSROOT}/usr/lib/libpthread.so\" ]; then
        cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libpthread.so\" \"\${SYSROOT}/usr/lib/\"
    else
        echo '/* GNU ld script */
GROUP ( /lib/libpthread.so.0 )' > \"\${SYSROOT}/usr/lib/libpthread.so\"
    fi
    echo '[bootstrap] libpthread.so linker script created'
fi
if [ ! -e \"\${SYSROOT}/usr/lib/libdl.so\" ]; then
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libdl.a\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
    cp -n \"\${TOOLCHAIN_SYSROOT}/usr/lib/libdl.so\" \"\${SYSROOT}/usr/lib/\" 2>/dev/null || true
fi

# ---- lib/ runtime shared objects from toolchain sysroot -------------------
# libm.so.6, libpthread.so.0, libc.so.6 are in /lib/ of the toolchain sysroot
if [ ! -f \"\${SYSROOT}/lib/libpthread.so.0\" ]; then
    cp -rn \"\${TOOLCHAIN_SYSROOT}/lib/.\" \"\${SYSROOT}/lib/\" 2>/dev/null || true
    echo '[bootstrap] toolchain /lib/ runtime objects copied'
fi

echo '[bootstrap] Sysroot runtime bootstrap complete'
"
    echo "[bootstrap] Done."
}

# ---------------------------------------------------------------------------
# install_zlib_sysroot_fn — download and extract the Debian armhf zlib1g-dev
# package into the sysroot so cmake's FindZLIB can resolve it during the
# libdash cross-build.
#
# Idempotency: guarded by presence of zlib.h.
# ---------------------------------------------------------------------------
install_zlib_sysroot_fn() {
    local ZLIB_PC="${SYSROOT}/usr/lib/${MULTIARCH}/pkgconfig/zlib.pc"
    if docker exec "${CONTAINER}" bash -c "[ -f '${ZLIB_PC}' ]" 2>/dev/null; then
        echo "[zlib] SKIP — zlib already installed (${ZLIB_PC} present)"
        return 0
    fi

    echo "[zlib] Installing Debian armhf zlib1g-dev into sysroot..."
    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
SYSROOT='${SYSROOT}'
URL='https://ftp.debian.org/debian/pool/main/z/zlib/zlib1g-dev_1.2.13.dfsg-1_armhf.deb'
TMPFILE=\$(mktemp /tmp/pkg-XXXXXX.deb)
wget -q \"\${URL}\" -O \"\${TMPFILE}\"
dpkg-deb --extract \"\${TMPFILE}\" \"\${SYSROOT}\"
rm -f \"\${TMPFILE}\"
echo '[zlib] OK zlib1g-dev extracted'
# Fix the broken absolute symlink from the deb (points to /lib/arm-linux-gnueabihf/...)
MULTIARCH='${MULTIARCH}'
cd \"\${SYSROOT}/usr/lib/\${MULTIARCH}\"
ln -sf libz.a libz.so
echo '[zlib] libz.so -> libz.a symlink fixed'
"
    echo "[zlib] Done."
}

# ---------------------------------------------------------------------------
# build_libdash_cross_fn — clone, patch, cross-compile, and install libdash
# into /opt/xione-sysroot using cmake/xione-armhf.cmake.
#
# Idempotency: if /opt/xione-sysroot/lib/libdash.so already exists and is
# ELF 32-bit ARM, skip the build entirely.
#
# This function INLINES the clone + checkout(stable_3_0) + 12 RDK patch
# steps from scripts/install_libdash.sh (but does NOT modify that shared
# script).  The cmake invocation is overridden to inject the cross toolchain
# file and the sysroot install prefix.
#
# SYS-03 env (PKG_CONFIG_SYSROOT_DIR / PKG_CONFIG_LIBDIR / unset
# PKG_CONFIG_PATH) is exported BEFORE cmake so libdash's cmake configure
# resolves libxml-2.0 from the sysroot, not from host x86_64 (Pitfall 3).
#
# Deviation notes:
# - cmake/xione-armhf.cmake sets CMAKE_SYSROOT=/opt/xione-sysroot, so the
#   cross compiler gets --sysroot=/opt/xione-sysroot.  This requires glibc
#   system headers (features.h, time.h, etc.) to be present in the sysroot;
#   bootstrap_sysroot_runtime_fn() copies them from the toolchain's own
#   internal sysroot before this function runs.
# - libdash's CMakeLists.txt also calls find_package(ZLIB); zlib1g-dev is
#   extracted by install_zlib_sysroot_fn() before this function runs.
# - libdash_networkpart_test (a test binary) fails to link due to transitive
#   libcap / libidn2 deps not present in the sysroot.  We build only the
#   libdash target (not the test binary) using make dash directly.
# ---------------------------------------------------------------------------
build_libdash_cross_fn() {
    echo "[libdash] Checking idempotency guard..."

    # Idempotency: skip if a valid armhf libdash.so is already installed
    if docker exec "${CONTAINER}" bash -c \
        "file '${SYSROOT}/lib/libdash.so' 2>/dev/null | grep -q 'ELF 32-bit.*ARM'"; then
        echo "[libdash] SKIP — ${SYSROOT}/lib/libdash.so already ELF 32-bit ARM"
        return 0
    fi

    echo "[libdash] Starting armhf cross-build of libdash (stable_3_0 + 12 RDK patches)..."

    # Ensure the sysroot lib/pkgconfig directory exists for the .pc file
    docker exec "${CONTAINER}" mkdir -p \
        "${SYSROOT}/lib" \
        "${SYSROOT}/lib/pkgconfig" \
        "${SYSROOT}/include/libdash"

    # Run everything inside the container in a single shell invocation to
    # preserve cwd across steps.  Build in /tmp/libdash-xione-build to
    # keep the container clean.
    docker exec "${CONTAINER}" bash -c "
set -euo pipefail

SYSROOT='${SYSROOT}'
MULTIARCH='${MULTIARCH}'
WORKDIR='/tmp/libdash-xione-build'
TOOLCHAIN_FILE='/work/cmake/xione-armhf.cmake'

# SYS-03 env: isolate pkg-config to the sysroot BEFORE cmake (Pitfall 3)
export PKG_CONFIG_SYSROOT_DIR=\"\${SYSROOT}\"
export PKG_CONFIG_LIBDIR=\"\${SYSROOT}/usr/lib/\${MULTIARCH}/pkgconfig:\${SYSROOT}/lib/pkgconfig\"
unset PKG_CONFIG_PATH

# ---- clone ----------------------------------------------------------------
mkdir -p \"\${WORKDIR}\"
cd \"\${WORKDIR}\"

if [ -d libdash ]; then
    echo '[libdash] Reusing existing clone in '\"\${WORKDIR}/libdash\"
else
    echo '[libdash] Cloning bitmovin/libdash ...'
    git clone https://github.com/bitmovin/libdash.git || {
        echo 'ERROR: Failed to clone bitmovin/libdash'; exit 1
    }
fi

cd libdash/libdash

git checkout stable_3_0 || { echo 'ERROR: checkout stable_3_0 failed'; exit 1; }

# ---- apply 12 RDK patches -------------------------------------------------
if [ ! -d meta-rdk-ext ]; then
    echo '[libdash] Cloning meta-rdk-ext (rdk-next) for RDK patches ...'
    git clone https://code.rdkcentral.com/r/rdk/components/generic/rdk-oe/meta-rdk-ext -b rdk-next || {
        echo 'ERROR: Failed to clone meta-rdk-ext'; exit 1
    }
fi

PATCH_DIR='meta-rdk-ext/recipes-multimedia/libdash/libdash'

# Guard: only apply patches once (check for existing build dir with cache)
if [ -d build ] && [ -f build/CMakeCache.txt ]; then
    echo '[libdash] Build dir exists; patches assumed already applied'
else
    for PFILE in \$(ls \"\${PATCH_DIR}\"/*.patch | sort); do
        echo \"[libdash] Applying \${PFILE} ...\"
        patch -p1 < \"\${PFILE}\" || { echo \"ERROR: patch \${PFILE} failed\"; exit 1; }
    done
fi

# ---- cmake cross-configure ------------------------------------------------
mkdir -p build
cd build

echo '[libdash] Running cross cmake configure with xione-armhf.cmake ...'
cmake .. \\
    -DCMAKE_TOOLCHAIN_FILE=\"\${TOOLCHAIN_FILE}\" \\
    -DCMAKE_INSTALL_PREFIX=\"\${SYSROOT}\" \\
    -DCMAKE_MACOSX_RPATH=FALSE \\
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \\
    -DCMAKE_C_FLAGS='' \\
    -DCMAKE_CXX_FLAGS='' \\
    -DCMAKE_SHARED_LINKER_FLAGS='' || {
    echo 'ERROR: libdash cmake configure failed'; exit 1
}

# ---- build only the dash target (not the test binary) ---------------------
# libdash_networkpart_test requires transitive libcap/libidn2 not in sysroot;
# we only need the libdash.so shared library.
echo '[libdash] Building dash target ...'
make -j\$(nproc) dash || { echo 'ERROR: libdash make dash failed'; exit 1; }

# ---- install libdash.so directly (cmake install would try to build test) --
echo '[libdash] Installing libdash.so to \${SYSROOT}/lib/ ...'
mkdir -p \"\${SYSROOT}/lib\"
cp bin/libdash.so \"\${SYSROOT}/lib/libdash.so\" || {
    echo 'ERROR: failed to copy libdash.so'; exit 1
}

# ---- copy headers ---------------------------------------------------------
# make install does not copy headers; replicate install_libdash.sh lines 106-120
echo '[libdash] Copying headers ...'
cd ..   # back to libdash/libdash

mkdir -p \"\${SYSROOT}/include/libdash\"
mkdir -p \"\${SYSROOT}/include/libdash/xml\"
mkdir -p \"\${SYSROOT}/include/libdash/mpd\"
mkdir -p \"\${SYSROOT}/include/libdash/network\"
mkdir -p \"\${SYSROOT}/include/libdash/portable\"
mkdir -p \"\${SYSROOT}/include/libdash/helpers\"
mkdir -p \"\${SYSROOT}/include/libdash/metrics\"

cp -p libdash/include/*               \"\${SYSROOT}/include/libdash/\"
cp -p libdash/source/xml/*.h          \"\${SYSROOT}/include/libdash/xml/\"
cp -p libdash/source/mpd/*.h          \"\${SYSROOT}/include/libdash/mpd/\"
cp -p libdash/source/network/*.h      \"\${SYSROOT}/include/libdash/network/\"
cp -p libdash/source/portable/*.h     \"\${SYSROOT}/include/libdash/portable/\"
cp -p libdash/source/helpers/*.h      \"\${SYSROOT}/include/libdash/helpers/\"
cp -p libdash/source/metrics/*.h      \"\${SYSROOT}/include/libdash/metrics/\"

# ---- write libdash.pc -----------------------------------------------------
# Replicate install_libdash.sh line 121 with LOCAL_DEPS_BUILD_DIR=\${SYSROOT}.
# Use relative libdir/includedir so PKG_CONFIG_SYSROOT_DIR prepends the
# sysroot path at query time.
echo '[libdash] Writing libdash.pc ...'
mkdir -p \"\${SYSROOT}/lib/pkgconfig\"
cat > \"\${SYSROOT}/lib/pkgconfig/libdash.pc\" <<'PCEOF'
libdir=/lib
includedir=/include/libdash

Name: libdash
Description: ISO/IEC MPEG-DASH library
Version: 3.0
Requires: libxml-2.0
Libs: -L\${libdir} -ldash
Libs.private: -lxml2
Cflags: -I\${includedir}
PCEOF
echo '[libdash] libdash.pc written to \${SYSROOT}/lib/pkgconfig/libdash.pc'
echo '[libdash] Cross-build complete.'
"
}

# ---------------------------------------------------------------------------
# verify_libdash_fn — SYS-04 gate checks.
# Asserts:
#   - /opt/xione-sysroot/lib/libdash.so is ELF 32-bit ARM (not x86_64)
#   - pkg-config --modversion libdash returns 3.0 under the SYS-03 env
#   - /opt/xione-sysroot/lib/pkgconfig/libdash.pc contains "Requires: libxml-2.0"
# ---------------------------------------------------------------------------
verify_libdash_fn() {
    echo "[libdash-verify] Running SYS-04 verification checks..."

    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
SYSROOT='${SYSROOT}'
MULTIARCH='${MULTIARCH}'
FAIL=0

# ---- ELF architecture check -----------------------------------------------
LIBDASH_SO=\"\${SYSROOT}/lib/libdash.so\"
if [ ! -f \"\${LIBDASH_SO}\" ]; then
    echo '[libdash-verify] FAIL: \${LIBDASH_SO} does not exist'
    exit 1
fi

ELF_INFO=\$(file \"\${LIBDASH_SO}\")
echo \"[libdash-verify] file output: \${ELF_INFO}\"
if echo \"\${ELF_INFO}\" | grep -q 'ELF 32-bit.*ARM'; then
    echo '[libdash-verify] OK: libdash.so is ELF 32-bit ARM'
else
    echo '[libdash-verify] FAIL: libdash.so is NOT ELF 32-bit ARM (architecture mismatch!)'
    FAIL=1
fi

# ---- pkg-config modversion check ------------------------------------------
export PKG_CONFIG_SYSROOT_DIR=\"\${SYSROOT}\"
export PKG_CONFIG_LIBDIR=\"\${SYSROOT}/usr/lib/\${MULTIARCH}/pkgconfig:\${SYSROOT}/lib/pkgconfig\"
unset PKG_CONFIG_PATH

MODVER=\$(pkg-config --modversion libdash 2>&1)
if [ \"\${MODVER}\" = '3.0' ]; then
    echo \"[libdash-verify] OK: pkg-config --modversion libdash = \${MODVER}\"
else
    echo \"[libdash-verify] FAIL: expected 3.0 got '\${MODVER}'\"
    FAIL=1
fi

# ---- .pc Requires check ---------------------------------------------------
PC_FILE=\"\${SYSROOT}/lib/pkgconfig/libdash.pc\"
if grep -q 'Requires: libxml-2.0' \"\${PC_FILE}\"; then
    echo '[libdash-verify] OK: libdash.pc contains Requires: libxml-2.0'
else
    echo '[libdash-verify] FAIL: libdash.pc missing Requires: libxml-2.0'
    FAIL=1
fi

if [ \"\${FAIL}\" -eq 0 ]; then
    echo '[libdash-verify] SYS-04 PASS'
else
    echo '[libdash-verify] SYS-04 FAIL'
    exit 1
fi
"
    echo "[libdash-verify] Done."
}

# ---------------------------------------------------------------------------
# verify_dryrun_configure_fn — cmake configure dry-run.
#
# Runs AAMP's cmake configure CONFIGURE-ONLY (no build, no make) with the
# xione-armhf.cmake toolchain + the feature flags, in an out-of-tree
# build dir at /tmp/aamp-dryrun inside the container.
#
# The SYS-03 env is passed INTO cmake's subprocess environment via
# `cmake -E env ...` so that cmake's transitive pkg-config resolution
# (e.g. libdash → libxml-2.0) also runs under sysroot isolation.
#
# Gate: PASS iff the configure log contains no unresolved-dependency lines
# for the named sysroot deps:
#   - find_package(EthanLog): no "Could NOT find EthanLog"
#   - pkg_check_modules for gstreamer-1.0, gstreamer-app-1.0, libxml-2.0,
#     libdash, openssl, libcjson, uuid, libcurl, and the 5 external stubs
#
# SCOPE FENCE: do NOT run make/build libaamp. A configure failure caused by
# a build-stage concern (source-level shim, missing middleware header, etc.)
# is logged as a build-stage item, not a dependency-resolution failure.  Only
# an UNRESOLVED sysroot dependency fails SYS-04/05.
# ---------------------------------------------------------------------------
verify_dryrun_configure_fn() {
    echo "[dryrun] Running cmake configure dry-run (CONFIGURE ONLY)..."

    local LOG_FILE="/tmp/aamp-dryrun.log"

    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
SYSROOT='${SYSROOT}'
MULTIARCH='${MULTIARCH}'
LOG_FILE='${LOG_FILE}'
BUILD_DIR='/tmp/aamp-dryrun'

# Remove stale build dir so we get a clean configure
rm -rf \"\${BUILD_DIR}\"

echo '[dryrun] Launching cmake configure with SYS-03 env ...'
cmake -E env \
    PKG_CONFIG_SYSROOT_DIR=\"\${SYSROOT}\" \
    PKG_CONFIG_LIBDIR=\"\${SYSROOT}/usr/lib/\${MULTIARCH}/pkgconfig:\${SYSROOT}/lib/pkgconfig\" \
    cmake -S /work -B \"\${BUILD_DIR}\" \
        -DCMAKE_TOOLCHAIN_FILE=/work/cmake/xione-armhf.cmake \
        -DCMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES=ON \
        -DCMAKE_INBUILT_AAMP_DEPENDENCIES=ON \
        -DCMAKE_SYSTEMD_JOURNAL=ON \
        -DCMAKE_USE_ETHAN_LOG=ON \
        -DCMAKE_TELEMETRY_2_0_REQUIRED=OFF \
    > \"\${LOG_FILE}\" 2>&1 || true

echo '[dryrun] Configure completed (exit code ignored — parsing log for dependency resolution)'
echo '[dryrun] --- Configure log ---'
cat \"\${LOG_FILE}\"
echo '[dryrun] --- End of configure log ---'
" 2>&1 | tee /tmp/aamp-dryrun-host.log

    echo "[dryrun] Parsing configure log for unresolved sysroot dependencies..."

    # Check the log inside the container
    docker exec "${CONTAINER}" bash -c "
set -euo pipefail
LOG_FILE='${LOG_FILE}'
FAIL=0

# ---- Dependency resolution checks (these are Phase-2 gates) ---------------
SYSROOT_DEPS=(
    'gstreamer-1.0'
    'gstreamer-app-1.0'
    'gstreamer-app-1.0'
    'libxml-2.0'
    'libdash'
    'openssl'
    'libcjson'
    'uuid'
    'libcurl'
    'libbaseconversion'
    'libplayerlogmanager'
    'libplayerfbinterface'
    'libplayergstinterface'
    'libsubtec'
)

# Check for pkg-config not-found patterns
for DEP in \"\${SYSROOT_DEPS[@]}\"; do
    if grep -qiE \"No package '?\${DEP}'? found\" \"\${LOG_FILE}\" 2>/dev/null; then
        echo \"[dryrun] FAIL: pkg-config dependency unresolved: \${DEP}\"
        FAIL=1
    fi
done

# Check for find_package EthanLog failure
if grep -qiE 'Could NOT find EthanLog' \"\${LOG_FILE}\" 2>/dev/null; then
    echo '[dryrun] FAIL: Could NOT find EthanLog'
    FAIL=1
fi

# ---- Report Phase-3-only issues (non-blocking for Phase-2 gate) -----------
if grep -qiE '(error|could not find|not found|fatal)' \"\${LOG_FILE}\" 2>/dev/null; then
    echo '[dryrun] Non-fatal issues found in configure log (may be build-stage concerns):'
    grep -iE '(error|could not find|not found|fatal)' \"\${LOG_FILE}\" | head -30 || true
fi

if [ \"\${FAIL}\" -eq 0 ]; then
    echo 'PHASE2_DRYRUN_PASS'
    echo '[dryrun] All sysroot dependency resolutions PASS.'
else
    echo '[dryrun] Dependency resolution FAIL — sysroot gap(s) detected'
    exit 1
fi
"
    echo "[dryrun] Done."
}

# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
main() {
    scaffold_sysroot_fn
    extract_debs_fn
    harvest_device_fn
    verify_symlinks_fn
    author_stubs_fn
    verify_pkgconfig_fn
    install_zlib_sysroot_fn
    bootstrap_sysroot_runtime_fn
    build_libdash_cross_fn
    verify_libdash_fn
    verify_dryrun_configure_fn
}

main "$@"
