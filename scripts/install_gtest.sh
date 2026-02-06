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

declare GOOGLETEST_REFERENCE="tags/release-1.11.0"

function install_build_googletest_fn()
{

    cd "$LOCAL_DEPS_BUILD_DIR" || { echo "Failed to change to LOCAL_DEPS_BUILD_DIR: ${LOCAL_DEPS_BUILD_DIR}"; return 1; }

    # $OPTION_CLEAN == true
    if [ "${1}" = true ] ; then
        echo "googletest clean"
        if [ -d googletest ] ; then
            rm -rf googletest
            # uninstall?
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock.a	
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock_main.a	
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgtest.a
        fi
    fi

    if [ -d "googletest" ]; then
        echo "googletest is already installed"
        INSTALL_STATUS_ARR+=("googletest was already installed.")
    else
        echo "Installing googletest..."
        git clone https://github.com/google/googletest || {
            echo "ERROR: Failed to clone googletest repository"
            return 1
        }
        
        pushd googletest || {
            echo "ERROR: Failed to change to googletest directory"
            return 1
        }
        
        echo "Checkout googletest '$GOOGLETEST_REFERENCE'"
        git checkout "$GOOGLETEST_REFERENCE" || {
            echo "ERROR: Failed to checkout googletest reference: $GOOGLETEST_REFERENCE"
            popd
            return 1
        }

        ###Build gtest
        echo "Building googletest"
        mkdir -p build
        cd build || {
            echo "ERROR: Failed to change to build directory"
            popd
            return 1
        }
        
        if [[ "$OSTYPE" == "darwin"* ]]; then    
            env PKG_CONFIG_PATH="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig" cmake .. -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" || {
                echo "ERROR: CMake configuration failed for googletest"
                popd
                return 1
            }
        elif [[ "$OSTYPE" == "linux"* ]]; then
            env PKG_CONFIG_PATH="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig" cmake .. -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX="${LOCAL_DEPS_BUILD_DIR}" || {
                echo "ERROR: CMake configuration failed for googletest"
                popd
                return 1
            }
        fi
        
        make || {
            echo "ERROR: Make build failed for googletest"
            popd
            return 1
        }
        
        make install || {
            echo "ERROR: Make install failed for googletest"
            popd
            return 1
        }
        
        INSTALL_STATUS_ARR+=("googletest was successfully installed.")
        popd
    fi
}

