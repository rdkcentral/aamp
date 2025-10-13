#!/usr/bin/env bash


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
       echo "Checkout rialto-gstreamer '${OPTION_RIALTO_REFERENCE}'"
       git checkout ${OPTION_RIALTO_REFERENCE}
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
    env PKG_CONFIG_PATH="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig" cmake .. -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" "$@"
    make
    make install
    popd
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
    fi

    if [ ${OPTION_RIALTO_BUILD} == false ] ; then
        return 0
    fi

    rialto_install_fn

    rialto_build_fn ${LOCAL_DEPS_BUILD_DIR}
}
