#!/bin/bash
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

# Build script for AAMP ABR Simulator
#
# Usage:
#   ./build.sh           - Build standalone version (simple ABR)
#   ./build.sh --real    - Build with real AAMP ABR integration

set -e

# Verify we're running from the correct directory
if [ ! -f "abrsim.cpp" ] || [ ! -f "build.sh" ]; then
    echo "✗ Error: Must be run from the abrsim directory"
    echo "  Current directory: $(pwd)"
    echo "  Expected files: abrsim.cpp, build.sh"
    exit 1
fi

echo "Building AAMP ABR Simulator..."

# Compiler settings
CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

# Check if real ABR build requested
if [ "$1" == "--real" ]; then
    echo "Building with AAMP's real ABR algorithm..."
    
    # Check if ABR sources exist
    if [ ! -f "../abr/abr.cpp" ]; then
        echo "✗ Error: ABR sources not found in ../abr/"
        echo "  Make sure you're building from the abrsim directory"
        exit 1
    fi
    
    $CXX $CXXFLAGS -DUSE_REAL_ABR -I../abr -I.. -o abrsim \
        abrsim.cpp \
        AbrSimAdapter.cpp \
        aamp_stubs.cpp \
        ../abr/abr.cpp \
        ../abr/HarmonicEwmaEstimator.cpp \
        ../abr/RollingMedianOutlierEstimator.cpp
    
    if [ $? -eq 0 ]; then
        echo "✓ Build successful: ./abrsim (with real AAMP ABR)"
        echo ""
        echo "Usage examples:"
        echo "  ./abrsim --persona sample_network.json --duration 3600 --out report.csv"
        echo "  ./abrsim --help"
    else
        echo "✗ Build failed"
        exit 1
    fi
else
    # Build standalone version (no AAMP dependencies)
    echo "Building standalone version with simple ABR..."
    $CXX $CXXFLAGS -o abrsim abrsim.cpp
    
    if [ $? -eq 0 ]; then
        echo "✓ Build successful: ./abrsim (standalone mode)"
        echo ""
        echo "To build with real AAMP ABR: ./build.sh --real"
        echo ""
        echo "Usage examples:"
        echo "  ./abrsim --persona sample_network.json --duration 3600 --out report.csv"
        echo "  ./abrsim --help"
    else
        echo "✗ Build failed"
        exit 1
    fi
fi

