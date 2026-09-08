#!/usr/bin/env bash
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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
#

function jsbindings_install_build_darwin_fn()
{   
    echo "Build aamp-jsbindings"
    cd "${AAMP_DIR}/build" || { echo "Failed to change to build directory: ${AAMP_DIR}/build"; return 1; }

    xcodebuild -scheme aampjsbindings build
    # Override SYMROOT so uveExecuter lands in build/Debug/uveExecuter.
    # CMake config sets SYMROOT to "${AAMP_DIR}/build/XcodeDerivedData", which would
    # otherwise place the binary in build/XcodeDerivedData/Debug/uveExecuter.
    xcodebuild -scheme uveExecuter SYMROOT="$AAMP_DIR/build" build
}

function jsbindings_install_build_linux_fn()
{
    echo "Build aamp-jsbindings and uveExecuter"
    cd "${AAMP_DIR}/build" || { echo "Failed to change to build directory: ${AAMP_DIR}/build"; return 1; }

    make aampjsbindings
    if [[ ! -f "libaampjsbindings.so" ]]; then
        echo "****aampjsbindings build FAILED****"
        return 1
    fi
    echo "****aampjsbindings build PASSED****"

    make uveExecuter
    if [[ ! -f "uveExecuter" ]]; then
        echo "****uveExecuter build FAILED****"
        return 1
    fi
    echo "****uveExecuter build PASSED****"

    make install
}

function jsbindings_install_build_fn()
{
    if [[ "$OSTYPE" == "darwin"* ]]; then
        jsbindings_install_build_darwin_fn
    else
        jsbindings_install_build_linux_fn
    fi
}