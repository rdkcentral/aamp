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

# @file xione-armhf.cmake
# @brief CMake cross-toolchain file for Sky XiOne (SKXI11BED) — armv7 NEON
#        hard-float, glibc 2.35, gcc 11.3.0, new C++11 string ABI.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/xione-armhf.cmake \
#         [-DXIONE_TOOLCHAIN_DIR=/opt/toolchain] \
#         [-DXIONE_SYSROOT=/opt/xione-sysroot] \
#         <source-dir>
#
# The sysroot need not exist when this toolchain file is first parsed; it is
# assembled by the sysroot build script.
# Set XIONE_SYSROOT via -D or environment variable to override the default.

# ---------------------------------------------------------------------------
# Toolchain directory — override via -DXIONE_TOOLCHAIN_DIR or env var
# ---------------------------------------------------------------------------
if(NOT DEFINED XIONE_TOOLCHAIN_DIR)
    set(XIONE_TOOLCHAIN_DIR "$ENV{XIONE_TOOLCHAIN_DIR}")
    if(NOT XIONE_TOOLCHAIN_DIR)
        set(XIONE_TOOLCHAIN_DIR "/opt/toolchain")
    endif()
endif()

# ---------------------------------------------------------------------------
# Device sysroot — override via -DXIONE_SYSROOT or env var
# Populated by the sysroot build script; parameterised here.
# ---------------------------------------------------------------------------
if(NOT DEFINED XIONE_SYSROOT)
    set(XIONE_SYSROOT "$ENV{XIONE_SYSROOT}")
    if(NOT XIONE_SYSROOT)
        set(XIONE_SYSROOT "/opt/xione-sysroot")
    endif()
endif()

# ---------------------------------------------------------------------------
# System identification
# ---------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ---------------------------------------------------------------------------
# try_compile as a static library (compile-only, no link).
#
# CMAKE_SYSROOT points at the device sysroot (XIONE_SYSROOT), which is empty
# until the sysroot build script runs. CMake's compiler-sanity try_compile would
# otherwise attempt a full LINK and fail — gcc receives --sysroot=${XIONE_SYSROOT}
# and cannot find crt1.o / the C++ runtime there yet, aborting every configure.
# STATIC_LIBRARY makes the sanity check compile-only so configure succeeds before
# the sysroot is populated. (Standard cross-compile pattern; code-review CR-01.)
# ---------------------------------------------------------------------------
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---------------------------------------------------------------------------
# Compiler triple — Bootlin Buildroot prefix (NOT arm-linux-gnueabihf)
# ---------------------------------------------------------------------------
set(CROSS_TRIPLE "arm-buildroot-linux-gnueabihf")

set(CMAKE_C_COMPILER   "${XIONE_TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${XIONE_TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-g++")
set(CMAKE_AR           "${XIONE_TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-ar"
    CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       "${XIONE_TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-ranlib"
    CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP        "${XIONE_TOOLCHAIN_DIR}/bin/${CROSS_TRIPLE}-strip"
    CACHE FILEPATH "Strip")

# ---------------------------------------------------------------------------
# Sysroot — directs library and header searches to the device sysroot.
# The sysroot need not exist yet (populated by the sysroot build script).
# ---------------------------------------------------------------------------
set(CMAKE_SYSROOT        "${XIONE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${XIONE_SYSROOT}")

# ---------------------------------------------------------------------------
# Sysroot library/include search paths
#
# gcc's --sysroot only searches <sysroot>/usr/lib and <sysroot>/lib, NOT the
# arm-linux-gnueabihf multiarch subdir where the device libraries actually
# live.  Hardcoded -l dependencies (e.g. -lsystemd, -lethanlog) therefore fail
# to link without an explicit -L.  Add the multiarch dir to every link, and the
# libdash include dir (config.h lives in a non-standard location) to every
# compile, so builds using this toolchain need no extra flags.
# ---------------------------------------------------------------------------
set(_xione_libdir "${XIONE_SYSROOT}/usr/lib/arm-linux-gnueabihf")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-L${_xione_libdir}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-L${_xione_libdir}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-L${_xione_libdir}")
set(CMAKE_CXX_FLAGS_INIT           "-I${XIONE_SYSROOT}/include/libdash")
set(CMAKE_C_FLAGS_INIT             "-I${XIONE_SYSROOT}/include/libdash")

# ---------------------------------------------------------------------------
# Find-root path modes
#
# PROGRAM NEVER  — host tools (make, pkg-config, python3) must be found on
#                  the host PATH, not inside the device sysroot.  Setting
#                  PROGRAM to ONLY breaks cmake's ability to find make,
#                  pkg-config, etc.
# LIBRARY ONLY   — link only against cross-sysroot libs; never pull in host
#                  x86_64 libraries.
# INCLUDE ONLY   — headers must come from the cross-sysroot only; never use
#                  host x86_64 system headers.
# PACKAGE ONLY   — find_package() searches the sysroot only.
# ---------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# Target ABI flags — matching RDK kirkstone armv7 lib32 build:
#   -march=armv7-a       : ARMv7-A instruction set (BCM72164 supports)
#   -mfpu=neon           : NEON SIMD unit present on BCM72164
#   -mfloat-abi=hard     : hard-float ABI (VFP registers for args) — armhf
#   -D_GLIBCXX_USE_CXX11_ABI=1 : new C++11 string ABI (B5cxx11 tag in
#                          device binary confirmed from device nm -D output)
# ---------------------------------------------------------------------------
set(CMAKE_C_FLAGS_INIT
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard"
    CACHE STRING "Initial C compiler flags for XiOne cross-build")
set(CMAKE_CXX_FLAGS_INIT
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard -D_GLIBCXX_USE_CXX11_ABI=1"
    CACHE STRING "Initial C++ compiler flags for XiOne cross-build")

# ---------------------------------------------------------------------------
# Status messages
# ---------------------------------------------------------------------------
message(STATUS "XiOne cross-toolchain : ${XIONE_TOOLCHAIN_DIR}")
message(STATUS "XiOne compiler triple : ${CROSS_TRIPLE}")
message(STATUS "XiOne sysroot         : ${XIONE_SYSROOT}")
message(STATUS "  (sysroot is populated by the sysroot build script)")
