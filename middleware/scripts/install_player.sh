#!/usr/bin/env bash

function player_install_build_linux_fn
{
    echo "Build Player Interface..."

    cd $PLAYER_DIR

    # Clean and prepare build directory
    BUILD_DIR="${PLAYER_DIR}/build"
    if [ -d "$BUILD_DIR" ]; then
        echo "Cleaning existing build directory..."
        rm -rf "$BUILD_DIR"
    fi
    mkdir -p "$BUILD_DIR"
    # Local built dependencies
    PKG_CONFIG="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"
    PKG_CONFIG_PATH="${PKG_CONFIG}" cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCOVERAGE_ENABLED=${OPTION_COVERAGE} -DUTEST_ENABLED=ON -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_ENABLE_PTS_RESTAMP:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S$PWD -B"${PLAYER_DIR}/build" -G "Unix Makefiles"

    echo "Making Player Interface..."
    cd "$BUILD_DIR"
    make -j$(nproc)
    make install
}

function player_install_build_fn()
{
    if [[ "$OSTYPE" != "darwin"* ]]; then
	    player_install_build_linux_fn
    fi
}
