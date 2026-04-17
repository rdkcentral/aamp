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

function rialto_install_fn() {

    # Install
    if [ -d "protobuf" ]; then
        echo "rialto is already installed"
        INSTALL_STATUS_ARR+=("protobuf was already installed.")
    else
        do_clone_github_repo_fn https://github.com/protocolbuffers/protobuf.git protobuf -b ${OPTION_PROTOBUF_REFERENCE} --recursive
    fi

    if [ -d "rialto" ]; then
        echo "rialto exists"
        INSTALL_STATUS_ARR+=("rialto was already installed.")
    else
        do_clone_fn https://github.com/rdkcentral/rialto.git rialto
        pushd rialto
        echo "Checkout rialto '${OPTION_RIALTO_REFERENCE}'"
        git checkout ${OPTION_RIALTO_REFERENCE}
        popd
    fi

    if [ -d "rialto-gstreamer" ]; then
        echo "rialto-gstreamer exists"
        INSTALL_STATUS_ARR+=("rialto-streamer was already installed.")
    else
       do_clone_fn https://github.com/rdkcentral/rialto-gstreamer.git rialto-gstreamer
       pushd rialto-gstreamer
       echo "Checkout rialto-gstreamer '${OPTION_RIALTO_GSTREAMER_REFERENCE}'"
       git checkout ${OPTION_RIALTO_GSTREAMER_REFERENCE}
       popd
    fi

    # TODO: rialto-ocdm clone/checkout added to support direct-rialto
    #       AampDrmBridge; should have been a separate, independently reviewed
    #       change per direct-rialto.instructions.md scope boundary rules.
    if [ -d "rialto-ocdm" ]; then
        echo "rialto-ocdm exists"
        INSTALL_STATUS_ARR+=("rialto-ocdm was already installed.")
    else
       do_clone_fn https://github.com/rdkcentral/rialto-ocdm.git rialto-ocdm
       pushd rialto-ocdm
       echo "Checkout rialto-ocdm '${OPTION_RIALTO_OCDM_REFERENCE}'"
       git checkout ${OPTION_RIALTO_OCDM_REFERENCE}
       popd
    fi

}

function rialto_build_repo_fn()
{
    echo "Building $1 "
    pushd $1
    shift
    mkdir -p build
    cd build
    env PKG_CONFIG_PATH="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig" cmake .. -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" -DCMAKE_CXX_FLAGS="-I${LOCAL_DEPS_BUILD_DIR}/include -Wno-sign-compare" "$@"
    make
    make install
    popd
}   

function rialto_copy_headers_fn()
{
    local dest="${LOCAL_DEPS_BUILD_DIR}/include/rialto"
    echo "Copying rialto public headers to ${dest}"
    mkdir -p "${dest}"
    cp "${LOCAL_DEPS_BUILD_DIR}/rialto/media/public/include/"*.h "${dest}/"
}

function rialto_build_fn() 
{
    echo "Building protobuf"
    pushd protobuf
    ./autogen.sh
    ./configure --prefix="${1}"
    make
    make install
    popd 
    
    rialto_build_repo_fn rialto -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE=Debug
    INSTALL_STATUS_ARR+=("rialto was successfully installed.")

    rialto_build_repo_fn rialto-gstreamer -DCMAKE_INCLUDE_PATH="${1}/rialto/stubs/opencdm/;${1}/rialto/media/public/include/" -DCMAKE_LIBRARY_PATH="${1}/rialto/build/stubs/opencdm;${1}/build/rialto/build/media/client/main/" -DNATIVE_BUILD=ON
    INSTALL_STATUS_ARR+=("rialto was successfully built.")
}

function rialto_install_build_fn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    # OPTION_CLEAN == true
    if [ ${1} == true ] ; then
        echo " clean"
        rm -rf protobuf
        rm -rf rialto
        rm -rf rialto-gstreamer
        rm -rf rialto-ocdm
    fi

    # Clone repositories regardless of OPTION_RIALTO_BUILD so that header
    # files are available for L1 tests (where the libraries are mocked).
    rialto_install_fn

    # Copy rialto public headers to the expected include directory so that
    # L1 tests can find them without building the rialto libraries.
    rialto_copy_headers_fn

    if [ ${OPTION_RIALTO_BUILD} == false ] ; then
        return 0
    fi

    rialto_build_fn ${LOCAL_DEPS_BUILD_DIR}
}
