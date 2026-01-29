#!/usr/bin/env bash
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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

# PREREQUISITES:
# This script requires the following variables to be set:
# - LOCAL_DEPS_BUILD_DIR: Directory where dependencies will be built and installed
# - MIDDLEWARE_PLAYER_INTERFACE_BRANCH: Git branch to checkout for middleware-player-interface
# - INSTALL_STATUS_ARR: Array to store installation status messages
#
# This script requires the following functions to be available:
# - do_clone_fn: Function to clone git repositories
#
# Required tools: git, cmake, make, pkg-config

function install_build_middleware_interface_fn()
{
    # Validate required variables
    if [[ -z "$LOCAL_DEPS_BUILD_DIR" ]]; then
        echo "Error: LOCAL_DEPS_BUILD_DIR variable is not set"
        return 1
    fi

    if [[ -z "$MIDDLEWARE_PLAYER_INTERFACE_BRANCH" ]]; then
        echo "Error: MIDDLEWARE_PLAYER_INTERFACE_BRANCH variable is not set"
        return 1
    fi

    # Check if LOCAL_DEPS_BUILD_DIR exists or can be created
    if [[ ! -d "$LOCAL_DEPS_BUILD_DIR" ]]; then
        echo "Creating LOCAL_DEPS_BUILD_DIR: $LOCAL_DEPS_BUILD_DIR"
        mkdir -p "$LOCAL_DEPS_BUILD_DIR" || {
            echo "Error: Failed to create LOCAL_DEPS_BUILD_DIR: $LOCAL_DEPS_BUILD_DIR"
            return 1
        }
    fi

    # Check if do_clone_fn function exists
    if ! declare -f do_clone_fn > /dev/null; then
        echo "Error: do_clone_fn function is not available"
        return 1
    fi

    # Check for required tools
    for tool in git cmake make pkg-config; do
        if ! command -v "$tool" &> /dev/null; then
            echo "Error: Required tool '$tool' is not installed"
            return 1
        fi
    done

    # Initialize INSTALL_STATUS_ARR if not already initialized
    if [[ -z "${INSTALL_STATUS_ARR+x}" ]]; then
        declare -g -a INSTALL_STATUS_ARR=()
    fi

    cd "$LOCAL_DEPS_BUILD_DIR" || {
        echo "Error: Failed to change directory to $LOCAL_DEPS_BUILD_DIR"
        return 1
    }

    # $OPTION_CLEAN == true
    if [[ "$1" == true ]] ; then
        echo "middleware-player-interface clean"
        if [ -d middleware-player-interface ] ; then
            rm -rf middleware-player-interface || {
                echo "Error: Failed to remove middleware-player-interface directory"
                return 1
            }
            # uninstall?
            rm -rf "$LOCAL_DEPS_BUILD_DIR/include/middleware-player-interface" || {
                echo "Warning: Failed to remove middleware-player-interface include directory"
            }
        fi
    fi

    if [ -d "middleware-player-interface" ]; then
        echo "middleware-player-interface is already installed"
        INSTALL_STATUS_ARR+=("middleware-player-interface was already installed.")
    else
        echo "Installing middleware-player-interface..."
        
        # Clone the repository with error handling
        if ! do_clone_fn https://github.com/rdkcentral/middleware-player-interface.git; then
            echo "Error: Failed to clone middleware-player-interface repository"
            return 1
        fi

        cd middleware-player-interface || {
            echo "Error: Failed to change directory to middleware-player-interface"
            return 1
        }
        
        local middleware_branch="${MIDDLEWARE_PLAYER_INTERFACE_BRANCH}"
        echo "Checking out branch: $middleware_branch"
        if ! git checkout "$middleware_branch"; then
            echo "Error: Failed to checkout branch '$middleware_branch'"
            return 1
        fi
        
        mkdir -p build || {
            echo "Error: Failed to create build directory"
            return 1
        }
        
        cd build || {
            echo "Error: Failed to change directory to build"
            return 1
        }
        
        echo "Running cmake configuration..."
        if ! cmake .. -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" -DCMAKE_PLATFORM_UBUNTU=ON; then
            echo "Error: CMake configuration failed"
            return 1
        fi
        
        echo "Building middleware-player-interface..."
        if ! make; then
            echo "Error: Build failed"
            return 1
        fi
        
        echo "Installing middleware-player-interface..."
        if ! make install; then
            echo "Error: Installation failed"
            return 1
        fi

        # Create pkg-config directory if it doesn't exist
        mkdir -p "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig" || {
            echo "Error: Failed to create pkgconfig directory"
            return 1
        }

        echo "Creating pkg-config files..."
        
        # Create libplayerfbinterface.pc
        if ! echo -e 'prefix='"$LOCAL_DEPS_BUILD_DIR"'/lib \nexec_prefix='"$LOCAL_DEPS_BUILD_DIR"' \nlibdir='"$LOCAL_DEPS_BUILD_DIR"'/lib \nincludedir='"$LOCAL_DEPS_BUILD_DIR"'/include \n \nName: playerfbinterface \nDescription: player externals interface library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerfbinterface \nCflags: -I${includedir}' > "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerfbinterface.pc"; then
            echo "Error: Failed to create libplayerfbinterface.pc"
            return 1
        fi
        
        # Create libbaseconversion.pc
        if ! echo -e 'prefix='"$LOCAL_DEPS_BUILD_DIR"'/lib \nexec_prefix='"$LOCAL_DEPS_BUILD_DIR"' \nlibdir='"$LOCAL_DEPS_BUILD_DIR"'/lib \nincludedir='"$LOCAL_DEPS_BUILD_DIR"'/include \n \nName: baseconversion \nDescription: base 16 and 64 conversion library \nVersion: 1.0 \nLibs: -L${libdir} -lbaseconversion \nCflags: -I${includedir}' > "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libbaseconversion.pc"; then
            echo "Error: Failed to create libbaseconversion.pc"
            return 1
        fi
        
        # Create libplayerlogmanager.pc
        if ! echo -e 'prefix='"$LOCAL_DEPS_BUILD_DIR"'/lib \nexec_prefix='"$LOCAL_DEPS_BUILD_DIR"' \nlibdir='"$LOCAL_DEPS_BUILD_DIR"'/lib \nincludedir='"$LOCAL_DEPS_BUILD_DIR"'/include \n \nName: playerlogmanager \nDescription: player log manager library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerlogmanager \nCflags: -I${includedir}' > "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerlogmanager.pc"; then
            echo "Error: Failed to create libplayerlogmanager.pc"
            return 1
        fi
        
        # Create libplayergstinterface.pc
        if ! echo -e 'prefix='"$LOCAL_DEPS_BUILD_DIR"'/lib \nexec_prefix='"$LOCAL_DEPS_BUILD_DIR"' \nlibdir='"$LOCAL_DEPS_BUILD_DIR"'/lib \nincludedir='"$LOCAL_DEPS_BUILD_DIR"'/include \n \nName: playergstinterface \nDescription: player gstreamer interfaces library \nVersion: 1.0 \nLibs: -L${libdir} -lplayergstinterface \nCflags: -I${includedir}' > "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayergstinterface.pc"; then
            echo "Error: Failed to create libplayergstinterface.pc"
            return 1
        fi
        
        # Create libsubtec.pc
        if ! echo -e 'prefix='"$LOCAL_DEPS_BUILD_DIR"'/lib \nexec_prefix='"$LOCAL_DEPS_BUILD_DIR"' \nlibdir='"$LOCAL_DEPS_BUILD_DIR"'/lib \nincludedir='"$LOCAL_DEPS_BUILD_DIR"'/include \n \nName: subtec \nDescription: player subtitle and teletext processing library \nVersion: 1.0 \nLibs: -L${libdir} -lsubtec \nCflags: -I${includedir}' > "$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libsubtec.pc"; then
            echo "Error: Failed to create libsubtec.pc"
            return 1
        fi

        echo "middleware-player-interface installation completed successfully"
        INSTALL_STATUS_ARR+=("middleware was successfully installed.")
    fi
}
