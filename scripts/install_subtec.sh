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

function subtec_install_fn() {

    # Need AAMP_DIR passed in so we can find patch files
    if [ -z "${1}" ]; then
        echo "ERROR: AAMP directory parameter is required but not provided"
        echo "Usage: subtec_install_fn <AAMP_DIR> <LOCAL_DEPS_BUILD_DIR>"
        return 1
    fi
    if [ ! -d "${1}" ]; then
        echo "ERROR: AAMP directory does not exist or is not a directory: '${1}'"
        return 1
    fi
    
    # Need LOCAL_DEPS_BUILD_DIR passed in so we can patch the location of glib
    if [ -z "${2}" ]; then
        echo "ERROR: Dependency directory parameter is required but not provided"
        echo "Usage: subtec_install_fn <AAMP_DIR> <LOCAL_DEPS_BUILD_DIR>"
        return 1
    fi
    if [ ! -d "${2}" ]; then
        echo "ERROR: Dependency directory does not exist or is not a directory: '${2}'"
        return 1
    fi 

    echo "Cloning subtec-app..."
    do_clone_fn "https://code.rdkcentral.com/r/components/generic/subtec-app" || {
        echo "ERROR: Failed to clone subtec-app repository"
        return 1
    }
    
    # Checkout known stable commit - last version verified compatible with AAMP patches
    # Commit a95f7591: Stable version before breaking changes (verified 2024-01)
    git -C subtec-app checkout a95f7591fff3fb8777781dfdc76d95fc0a1c382b || {
        echo "ERROR: Failed to checkout subtec-app commit a95f7591"
        return 1
    }

    echo
    echo "Cloning websocket-ipplayer2-utils..."
    do_clone_fn https://code.rdkcentral.com/r/components/generic/websocket-ipplayer2-utils subtec-app/websocket-ipplayer2-utils || {
        echo "ERROR: Failed to clone websocket-ipplayer2-utils repository"
        return 1
    }
    
    # Checkout known stable commit - required for compatibility with subtec-app
    # Commit 2287fea4: Version with required websocket protocol support
    git -C subtec-app/websocket-ipplayer2-utils checkout 2287fea4d1af0a632aed5f1b8bfba8babbdade1f || {
        echo "ERROR: Failed to checkout websocket-ipplayer2-utils commit 2287fea4"
        return 1
    }


    pushd subtec-app || {
        echo "ERROR: Failed to change to subtec-app directory"
        return 1
    }
    
    echo "Patching subtec-app from ${1}"
    
    # Apply critical patches for AAMP compatibility
    git apply -p1 "${1}/OSX/patches/subttxrend-app-xkbcommon.patch" || {
        echo "ERROR: Failed to apply subttxrend-app-xkbcommon.patch"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/subttxrend-app-packet.patch" || {
        echo "ERROR: Failed to apply subttxrend-app-packet.patch"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/websocket-ipplayer2-link.patch" --directory websocket-ipplayer2-utils || {
        echo "ERROR: Failed to apply websocket-ipplayer2-link.patch"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/websocket-ipplayer2-typescpp.patch" --directory websocket-ipplayer2-utils || {
        echo "ERROR: Failed to apply websocket-ipplayer2-typescpp.patch"
        popd
        return 1
    }
    
    cp "${1}/OSX/patches/RDKLogoBlack.png" subttxrend-gfx/quartzcpp/assets/RDKLogo.png || {
        echo "ERROR: Failed to copy RDKLogoBlack.png"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/subttxrend-app-ubuntu_24_04_build.patch" || {
        echo "ERROR: Failed to apply subttxrend-app-ubuntu_24_04_build.patch"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/websocket-ipplayer2-ubuntu_24_04_build.patch" --directory websocket-ipplayer2-utils || {
        echo "ERROR: Failed to apply websocket-ipplayer2-ubuntu_24_04_build.patch"
        popd
        return 1
    }
    
    git apply -p1 "${1}/OSX/patches/websocket-ipplayer2-mac_sequoia_build.patch" --directory websocket-ipplayer2-utils || {
        echo "ERROR: Failed to apply websocket-ipplayer2-mac_sequoia_build.patch"
        popd
        return 1
    }

    git apply -p1 "${1}/OSX/patches/subttxrend-app-homebrew-paths.patch" || {
        echo "ERROR: Failed to apply subttxrend-app-homebrew-paths.patch"
        popd
        return 1
    }

    git apply -p1 "${1}/OSX/patches/websocket-ipplayer2-homebrew-includes.patch" --directory websocket-ipplayer2-utils || {
        echo "ERROR: Failed to apply websocket-ipplayer2-homebrew-includes.patch"
        popd
        return 1
    }
 
    echo "subtec-app source prepared"
    popd
}


function subtec_install_run_script_fn()
{
    # Create a subtec run script in the build dir
    # This will contain all the paths to the subtec build so wherever aamp-cli is
    # run from it can run subtec


    # Link subtec build into build directory for aampcli-run-subtec use
    ln -s  $LOCAL_DEPS_BUILD_DIR/subtec-app/subttxrend-app/x86_builder ${AAMP_DIR}/build/subtec-app || true

    if [[ "$OSTYPE" == "darwin"* ]]; then    
        SUBTEC_RUNSCRIPT=${AAMP_DIR}/build/Debug/aampcli-run-subtec.sh
    elif [[ "$OSTYPE" == "linux"* ]]; then
        SUBTEC_RUNSCRIPT=${AAMP_DIR}/build/aampcli-run-subtec.sh
    else
        echo "WARNING - unrecognized platform!"
        SUBTEC_RUNSCRIPT=${AAMP_DIR}/aampcli-run-subtec.sh
    fi    

    echo '#!/bin/bash' > $SUBTEC_RUNSCRIPT

    if [[ "$OSTYPE" == "darwin"* ]]; then
        cat <<AAMPCLI_RUN_SUBTEC >> $SUBTEC_RUNSCRIPT
# Verify the Unix domain socket size settings to support large sidecar subtitle
# files.
SYSCTL_MAXDGRAM=\`sysctl net.local.dgram.maxdgram\`
MIN_MAXDGRAM=102400
if [[ "\${SYSCTL_MAXDGRAM}" =~ net.local.dgram.maxdgram:\ ([0-9]+) ]]
then
    MAXDGRAM=\${BASH_REMATCH[1]}
    if (( MAXDGRAM < MIN_MAXDGRAM ))
    then
        echo "To support the loading of sidecar subtitle files, increase the Unix domain"
        echo "socket size settings. For example:"
        echo "    sudo sysctl net.local.dgram.maxdgram=\$((MIN_MAXDGRAM))"
        echo "    sudo sysctl net.local.dgram.recvspace=\$((MIN_MAXDGRAM * 2))"
    fi
fi

AAMPCLI_RUN_SUBTEC
    fi

    if [[ "$OSTYPE" == "linux"* ]]; then
        # start a weston window to display subtitles
        echo 'export XDG_RUNTIME_DIR=/tmp/subtec' >> $SUBTEC_RUNSCRIPT
        echo 'mkdir -p /tmp/subtec' >> $SUBTEC_RUNSCRIPT
        echo 'weston &' >> $SUBTEC_RUNSCRIPT
        echo 'sleep 5' >> $SUBTEC_RUNSCRIPT
    fi    

    echo 'cd '${AAMP_DIR}'/build/subtec-app' >> $SUBTEC_RUNSCRIPT
    echo 'THIS_DIR=$PWD' >> $SUBTEC_RUNSCRIPT
    echo 'MSP=${MSP:-/tmp/pes_data_main}' >> $SUBTEC_RUNSCRIPT
    echo 'INSTALL_DIR=$PWD/build/install' >> $SUBTEC_RUNSCRIPT
    echo 'LD_LIBRARY_PATH=$INSTALL_DIR/usr/local/lib $INSTALL_DIR/usr/local/bin/subttxrend-app -msp=$MSP -cfp=$THIS_DIR/config.ini' >> $SUBTEC_RUNSCRIPT
}


function subtec_install_build_fn() {

    cd $LOCAL_DEPS_BUILD_DIR

    # OPTION_CLEAN == true
    if [ "${1}" = true ] ; then
        echo "subtec clean"
        rm -rf subtec-app
    fi


    # Install
    if [ -d "subtec-app" ]; then
        echo "subtec-app is already installed"
        INSTALL_STATUS_ARR+=("subtec-app was already installed.")
    else
        # Tell installer where DEPs are so cmake can be patched
        subtec_install_fn ${AAMP_DIR} ${LOCAL_DEPS_BUILD_DIR}
    fi
    
    # Build
    cd subtec-app/subttxrend-app/x86_builder/ || { echo "Failed to change to subtec build directory"; return 1; }

    if [ ! -d build/install ] ; then
        BOOST_ROOT="$(brew --prefix boost@1.85)" CMAKE_PREFIX_PATH="$(brew --prefix tinyxml2);$(brew --prefix websocketpp);$(brew --prefix boost@1.85)" PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH ./build.sh fast

        if [ -f ./build/install/usr/local/bin/subttxrend-app ]; then
            echo "subtec-app has been built."
            INSTALL_STATUS_ARR+=("subtec-app has been built.")
        else
            echo "subtec-app build has failed."
            return 1
        fi
    fi
}

