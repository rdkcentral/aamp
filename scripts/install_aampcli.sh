#!/usr/bin/env bash
#
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

function aampcli_install_postbuild_fn()
{
    cd "$AAMP_DIR/build" || { echo "Failed to change to build directory: ${AAMP_DIR}/build"; return 1; }

    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo ""
        if [ "$OPTION_DONT_RUN_AAMPCLI" = false ];then
            # Launch Xcode
            (open AAMP.xcodeproj) &
        else
            echo "To use Xcode, open aamp/build/AAMP.xcodeproj project file"
        fi
    fi
}

function aampcli_install_prebuild_fn()
{
    cd "$AAMP_DIR" || { echo "Failed to change to AAMP_DIR: ${AAMP_DIR}"; return 1; }

    # $OPTION_CLEAN == true
    if [ "${1}" = true ] ; then
        echo "aampcli clean"
        if [ -d build ] ; then
            rm -rf build
            # uninstall?
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock.a
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock_main.a
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgtest.a
        fi
    fi


    if [ -d "build" ]; then
        echo "aamp-cli is already installed"
        INSTALL_STATUS_ARR+=("aamp-cli prebuild was already installed.")
    else
        mkdir -p build
        touch build/install_manifest.txt
        if [[ "$OSTYPE" == "darwin"* ]]; then
            # Create directories and mark them as managed by Xcode build system
            # This allows Xcode's "Clean Build Folder" to delete them properly
            mkdir -p build/Debug
            xattr -w com.apple.xcode.CreatedByBuildSystem true build/Debug
            
            mkdir -p build/XcodeDerivedData
            xattr -w com.apple.xcode.CreatedByBuildSystem true build/XcodeDerivedData
        fi
    fi

    #Create default channel ~/aampcli.csv – supports local configuration overrides
    if [ -f "${HOME}/aampcli.csv" ]; then
        echo "${HOME}/aampcli.csv exists."
    else
        echo "Creating default channel list file ${HOME}/aampcli.csv"
        cp ./OSX/aampcli.csv ${HOME}/aampcli.csv
    fi
}

function aampcli_install_build_darwin_fn()
{

    echo "Build aamp-cli"

    cd "$AAMP_DIR" || { echo "Failed to change to AAMP_DIR: $AAMP_DIR"; return 1; }


    # Local built dependencies
    PKG_CONFIG="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"

    # GStreamer: prefer the macOS framework installer; fall back to homebrew.
    # Fail early with a clear message rather than letting cmake produce an opaque error.
    local _GST_FRAMEWORK_PKG="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"
    if [ -d "${_GST_FRAMEWORK_PKG}" ]; then
        PKG_CONFIG="${_GST_FRAMEWORK_PKG}:${PKG_CONFIG}"
    else
        local _GST_BREW_PREFIX
        _GST_BREW_PREFIX=$(brew --prefix gstreamer 2>/dev/null) || true
        if [ -n "${_GST_BREW_PREFIX}" ] && [ -d "${_GST_BREW_PREFIX}/lib/pkgconfig" ]; then
            PKG_CONFIG="${_GST_BREW_PREFIX}/lib/pkgconfig:${PKG_CONFIG}"
            # gstreamer-app-1.0 lives in gst-plugins-base
            local _GST_BASE_PREFIX
            _GST_BASE_PREFIX=$(brew --prefix gst-plugins-base 2>/dev/null) || true
            if [ -n "${_GST_BASE_PREFIX}" ] && [ -d "${_GST_BASE_PREFIX}/lib/pkgconfig" ]; then
                PKG_CONFIG="${_GST_BASE_PREFIX}/lib/pkgconfig:${PKG_CONFIG}"
            fi
        else
            echo "ERROR: GStreamer not found. Please install one of:"
            echo "  GStreamer macOS framework: https://gstreamer.freedesktop.org/download/"
            echo "  OR via homebrew: brew install gstreamer gst-plugins-base"
            return 1
        fi
    fi
    if [[ "$ARCH" == "x86_64" ]]; then
        PKG_CONFIG="${PKG_CONFIG}:/usr/local/lib/pkgconfig"
    elif [[ "$ARCH" == "arm64" ]]; then
        PKG_CONFIG="${PKG_CONFIG}:/opt/homebrew/lib/pkgconfig"
    fi
    # MacOS provides a curl installation, but we'd like a newer version where was it installed?
    PKG_CONFIG_CURL=$(install_pkgs_pkgconfig_darwin_fn curl)
    if [ -n "${PKG_CONFIG_CURL}" ] ; then
        PKG_CONFIG="${PKG_CONFIG_CURL}:${PKG_CONFIG}"
    fi

    cd build && PKG_CONFIG_PATH=${PKG_CONFIG}:${PKG_CONFIG_PATH} cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCOVERAGE_ENABLED=${OPTION_COVERAGE} \
        -DUTEST_ENABLED=ON \
        -DCMAKE_INBUILT_AAMP_DEPENDENCIES=1 \
        -DCMAKE_ENABLE_PTS_RESTAMP:BOOL=TRUE \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-26.0}" \
        -DCMAKE_XCODE_ATTRIBUTE_SYMROOT="${AAMP_DIR}/build/XcodeDerivedData" \
        -DCMAKE_XCODE_ATTRIBUTE_OBJROOT="${AAMP_DIR}/build/XcodeDerivedData" \
        $(if [ "${OPTION_PLAYER_INTERFACE_SOURCE}" = "external" ]; then echo "-DCMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES=ON"; fi) \
        ${OPTION_BUILD_ARGS} \
        -G Xcode ../

    # the cmake Xcode generator can not set this scheme property (Debug -> Options -> Console -> Use Terminal
    patch ./AAMP.xcodeproj/xcshareddata/xcschemes/aamp-cli.xcscheme < ../OSX/patches/aamp-cli.xscheme.patch


    if [ -d "AAMP.xcodeproj" ]; then
        echo "AAMP Environment Successfully Installed."
        arr_install_status+=("AAMP Environment Successfully Installed.")
    else
        echo "AAMP Environment FAILED to Install."
        arr_install_status+=("AAMP Environment FAILED to Install.")
    fi

    echo "Starting Xcode, open aamp/build/AAMP.xcodeproj project file OR Execute ./aamp-cli or /playbintest <url> binaries"
    echo "Opening AAMP project in Xcode..."
    # Changed "\-bash" as that signifies login shell, running ./install-aamp.sh (as opposed to source install-aamp.sh) and that would not be the case
    if ps -o comm= $$ | grep -q "bash"; then
        echo "Running in bash"
    else
        echo "Changing login shell to bash"
        chsh -s /bin/bash
    fi


    echo "Now Building aamp-cli"
    xcodebuild -scheme aamp-cli  build

    if [ "${OPTION_AAMPCLIKOTLIN_SKIP}" != true ]; then
        echo "Making aamp-cli on kotlin..."
        xcodebuild -scheme aampKotlin  build
    fi

    if [  -f "./Debug/aamp-cli" ]; then
        echo "OSX AAMP Build PASSED"
        arr_install_status+=("OSX AAMP Build PASSED")

	subtec_install_run_script_fn   # after build/Debug directory created by xcodebuild
    else
        echo "OSX AAMP Build FAILED"
        arr_install_status+=("OSX AAMP Build FAILED")
        return 1
    fi

}

function aampcli_install_build_linux_fn
{
    echo "Build aamp-cli"

    cd "$AAMP_DIR" || { echo "Failed to change to AAMP_DIR: ${AAMP_DIR}"; return 1; }

    # Local built dependencies
    PKG_CONFIG="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"

    # Always build the simulator version (default, fast, no protobuf/Rialto build required).
    # The real-Rialto build follows below when OPTION_RIALTO_BUILD is set.
    PKG_CONFIG_PATH="${PKG_CONFIG}" cmake --no-warn-unused-cli -DSANITIZER_ENABLED=${OPTION_UBUNTU_SANITIZER} -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCOVERAGE_ENABLED=${OPTION_COVERAGE} -DUTEST_ENABLED=ON -DCMAKE_INBUILT_AAMP_DEPENDENCIES=1 -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_ENABLE_PTS_RESTAMP:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ ${OPTION_BUILD_ARGS} $(if [ "${OPTION_PLAYER_INTERFACE_SOURCE}" = "external" ]; then echo "-DCMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES=ON"; fi) -DRIALTO_FORCE_SIMULATOR=ON -S$PWD -B"${AAMP_DIR}/build" -G "Unix Makefiles"

   echo "Making aamp-cli..."
   cd build || { echo "Failed to change to build directory"; return 1; }
   make aamp-cli

    if [ "${OPTION_AAMPCLIKOTLIN_SKIP}" != true ]; then
        echo "Making aamp-cli on kotlin..."
        make aampKotlin
    fi
   make install

   if [  -f "./aamp-cli" ]; then
        echo "****Linux AAMP Build PASSED****"
        ldd ./aamp-cli
        arr_install_status+=("Linux AAMP Build PASSED")
    else
        echo "****Linux AAMP Build FAILED****"
        arr_install_status+=("Linux AAMP Build FAILED")
	return 1
    fi

    # When the 'rialto' option was given, also build aamp-cli against the real
    # Rialto client and install to a separate prefix (.libs-rialto/).
    # This binary is used by L2 tests that request an actual video window
    # (--aamp_video).  The two builds coexist because the simulator uses
    # libRialtoClient.so.0 and the real Rialto uses libRialtoClient.so.1 —
    # different DT_NEEDED entries, no file conflicts.
    if [ "${OPTION_RIALTO_BUILD}" = true ]; then
        echo ""
        echo "Building aamp-cli against real Rialto (build-rialto/)..."
        cd "$AAMP_DIR" || { echo "Failed to change to AAMP_DIR: ${AAMP_DIR}"; return 1; }

        # Explicitly set RIALTO_LIBRARY and RIALTO_INCLUDE_DIR on the command
        # line so cmake never reads a stale cached value.  The simulator build
        # that runs first installs libRialtoClient.so.0 to ${LOCAL_DEPS_BUILD_DIR}/lib
        # and sets the libRialtoClient.so symlink there to point to .so.0.
        # If cmake's find_library cached that path in a previous run it would
        # silently link against the simulator even in this real-Rialto build.
        # Passing -DRIALTO_LIBRARY:FILEPATH=... forces cmake to use the
        # real Rialto library (SONAME libRialtoClient.so.1) built from source.
        local RIALTO_LIB_DIR="${LOCAL_DEPS_BUILD_DIR}/rialto/build/media/client/main"
        local RIALTO_INC_DIR="${LOCAL_DEPS_BUILD_DIR}/include/rialto"
        PKG_CONFIG_PATH="${PKG_CONFIG}" cmake --no-warn-unused-cli \
            -DSANITIZER_ENABLED=${OPTION_UBUNTU_SANITIZER} \
            -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}-rialto" \
            -DCMAKE_PLATFORM_UBUNTU=1 \
            -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" \
            -DRIALTO_LIBRARY:FILEPATH="${RIALTO_LIB_DIR}/libRialtoClient.so" \
            -DRIALTO_INCLUDE_DIR:PATH="${RIALTO_INC_DIR}" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
            -DCOVERAGE_ENABLED=${OPTION_COVERAGE} \
            -DUTEST_ENABLED=ON \
            -DCMAKE_INBUILT_AAMP_DEPENDENCIES=1 \
            -DCMAKE_BUILD_TYPE:STRING=Debug \
            -DCMAKE_ENABLE_PTS_RESTAMP:BOOL=TRUE \
            -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc \
            -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ \
            ${OPTION_BUILD_ARGS} \
            $(if [ "${OPTION_PLAYER_INTERFACE_SOURCE}" = "external" ]; then echo "-DCMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES=ON"; fi) \
            -S$PWD -B"${AAMP_DIR}/build-rialto" -G "Unix Makefiles"

        cd "${AAMP_DIR}/build-rialto" || { echo "Failed to change to build-rialto directory"; return 1; }
        make aamp-cli
        make install

        if [ -f "./aamp-cli" ]; then
            echo "****Linux AAMP Real Rialto Build PASSED****"
            ldd ./aamp-cli
            arr_install_status+=("Linux AAMP Real Rialto Build PASSED")
        else
            echo "****Linux AAMP Real Rialto Build FAILED****"
            arr_install_status+=("Linux AAMP Real Rialto Build FAILED")
            return 1
        fi
    fi
}

function aampcli_install_build_fn()
{
    if [[ "$OSTYPE" == "darwin"* ]]; then
	    aampcli_install_build_darwin_fn
    else
	    aampcli_install_build_linux_fn
    fi
}

