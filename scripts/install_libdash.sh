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

function install_build_libdash_fn()
{
    cd "$LOCAL_DEPS_BUILD_DIR" || { echo "Failed to change to LOCAL_DEPS_BUILD_DIR: ${LOCAL_DEPS_BUILD_DIR}"; return 1; }

    # $OPTION_CLEAN == true
    if [ "${1}" = true ] ; then
        echo "libdash clean"
        if [ -d libdash ] ; then
            rm -rf libdash
            # uninstall?
            rm -rf $LOCAL_DEPS_BUILD_DIR/include/libdash
        fi
    fi

    if [ -d "libdash" ]; then
        echo "libdash is already installed"
        INSTALL_STATUS_ARR+=("libdash was already installed.")
    else
        echo "Installing libdash..."
        git clone https://github.com/bitmovin/libdash.git || {
            echo "ERROR: Failed to clone libdash from bitmovin/libdash.git"
            return 1
        }

        cd libdash/libdash || { echo "ERROR: Failed to change to libdash/libdash directory"; return 1; }
        
        git checkout stable_3_0 || {
            echo "ERROR: Failed to checkout stable_3_0 branch"
            return 1
        }
        
        git clone https://code.rdkcentral.com/r/rdk/components/generic/rdk-oe/meta-rdk-ext -b rdk-next || {
            echo "ERROR: Failed to clone meta-rdk-ext repository"
            return 1
        }
        
        # Apply RDK patches - critical for AAMP compatibility
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0001-libdash-build.patch || { echo "ERROR: Failed to apply patch 0001"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0002-libdash-starttime-uint64.patch || { echo "ERROR: Failed to apply patch 0002"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0003-libdash-presentationTimeOffset-uint64.patch || { echo "ERROR: Failed to apply patch 0003"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0004-Support-of-EventStream.patch || { echo "ERROR: Failed to apply patch 0004"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0005-DELIA-39460-libdash-memleak.patch || { echo "ERROR: Failed to apply patch 0005"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0006-RDK-32003-LLD-Support.patch || { echo "ERROR: Failed to apply patch 0006"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0007-DELIA-51645-Event-Stream-RawAttributes-Support.patch || { echo "ERROR: Failed to apply patch 0007"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0008-DELIA-53263-Use-Label-TAG.patch || { echo "ERROR: Failed to apply patch 0008"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0009-RDK-35134-Support-for-FailoverContent.patch || { echo "ERROR: Failed to apply patch 0009"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0010-RDKAAMP-121-Failover-Tag-on-SegmentTemplate.patch || { echo "ERROR: Failed to apply patch 0010"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0011-RDKAAMP-61-AAMP-low-latency-dash-stream-evaluation.patch || { echo "ERROR: Failed to apply patch 0011"; return 1; }
        patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0012-To-retrieves-the-text-content-of-CDATA-section.patch || { echo "ERROR: Failed to apply patch 0012"; return 1; }

        # Fix: remove UB C-style cast in GetRepresentation; align member type
        # with the declared return type so no cast is needed.

        # Better to have this patch in the meta-rdk-ext/recipes-multimedia/libdash repo, but for now we can just fix it here. The cast is technically UB and causes issues with some compilers (e.g. clang 16+). The fix is to change the return type of GetRepresentation to match the member variable type, which eliminates the need for the cast.
        sed -i.bak \
            's|return (std::vector<IRepresentation \*> \&) this->representation;|return this->representation;|' \
            libdash/source/mpd/AdaptationSet.cpp
        [ $? -eq 0 ] || { echo "ERROR: Failed to fix AdaptationSet.cpp"; return 1; }
        rm -f libdash/source/mpd/AdaptationSet.cpp.bak

        sed -i.bak \
            's|std::vector<Representation \*>[[:space:]]*representation;|std::vector<IRepresentation *>  representation;|' \
            libdash/source/mpd/AdaptationSet.h
        [ $? -eq 0 ] || { echo "ERROR: Failed to fix AdaptationSet.h"; return 1; }
        rm -f libdash/source/mpd/AdaptationSet.h.bak

        # CMake build
        mkdir -p build
        cd build || { echo "ERROR: Failed to change to build directory"; return 1; }
        
        # Propagate sanitizer flags to libdash so it matches the ASAN
        # instrumentation level of libaamp. ASAN is enabled by the build
        # configuration (for example, on macOS builds or on Ubuntu via the
        # -u flag).
        local SANITIZER_FLAGS=""
        if [[ "${PLATFORM}" == "darwin" || "${OPTION_UBUNTU_SANITIZER}" == "true" ]]; then
            SANITIZER_FLAGS="-fsanitize=address"
        fi

        local EXTRA_C_FLAGS="${SANITIZER_FLAGS}"
        local EXTRA_CXX_FLAGS="${SANITIZER_FLAGS}"
        local EXTRA_LINK_FLAGS="${SANITIZER_FLAGS}"

        cmake .. \
            -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" \
            -DCMAKE_MACOSX_RPATH=TRUE \
            -DCMAKE_C_FLAGS="${EXTRA_C_FLAGS}" \
            -DCMAKE_CXX_FLAGS="${EXTRA_CXX_FLAGS}" \
            -DCMAKE_SHARED_LINKER_FLAGS="${EXTRA_LINK_FLAGS}" || {
            echo "ERROR: CMake configuration failed"
            return 1
        }
        
        make || {
            echo "ERROR: Make build failed"
            return 1
        }
        
        make install || {
            echo "ERROR: Make install failed"
            return 1
        }

        # why doesn't make install do this for us
        cd .. || { echo "Failed to navigate to parent directory"; return 1; }
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/xml"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/mpd"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/network"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/portable"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/helpers"
        mkdir -p "${LOCAL_DEPS_BUILD_DIR}/include/libdash/metrics"
        cp -p libdash/include/* "${LOCAL_DEPS_BUILD_DIR}/include/libdash"
        cp -p libdash/source/xml/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/xml"
        cp -p libdash/source/mpd/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/mpd"
        cp -p libdash/source/network/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/network"
        cp -p libdash/source/portable/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/portable"
        cp -p libdash/source/helpers/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/helpers"
        cp -p libdash/source/metrics/*.h "${LOCAL_DEPS_BUILD_DIR}/include/libdash/metrics"
        echo -e "prefix=${LOCAL_DEPS_BUILD_DIR}/lib \nexec_prefix=${LOCAL_DEPS_BUILD_DIR} \nlibdir=${LOCAL_DEPS_BUILD_DIR}/lib \nincludedir=${LOCAL_DEPS_BUILD_DIR}/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L\${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I\${includedir}" > "${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig/libdash.pc"

        INSTALL_STATUS_ARR+=("libdash was successfully installed.")
    fi
}
