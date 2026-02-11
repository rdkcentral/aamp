#!/bin/bash -x
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

# Build script for SimNet (LL-DASH Network Persona Simulator)

set -e

echo "Building SimNet..."

# Compiler settings
CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

# Build standalone version (no dependencies)
cd simnet
$CXX $CXXFLAGS -o simnet simnet.cpp

if [ $? -eq 0 ]; then
    echo "✓ Build successful: ./simnet/simnet"
    echo ""
    echo "Usage examples:"
    echo "  cd simnet"
    echo "  ./simnet --persona ../personas/lldash_persona.json --sizes 1400000 24000 --out sim"
    echo "  ./simnet --help"
else
    echo "✗ Build failed"
    exit 1
fi
