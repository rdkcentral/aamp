#!/bin/bash

# Build script for AAMP ABR Simulator
# Copyright 2026 RDK Management

set -e

echo "Building AAMP ABR Simulator..."

# Compiler settings
CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

# Build standalone version (no AAMP dependencies)
echo "Building standalone version..."
$CXX $CXXFLAGS -o abrsim abrsim.cpp

if [ $? -eq 0 ]; then
    echo "✓ Build successful: ./abrsim"
    echo ""
    echo "Usage examples:"
    echo "  ./abrsim --persona sample_network.json --duration 3600 --out report.csv"
    echo "  ./abrsim --help"
else
    echo "✗ Build failed"
    exit 1
fi

# Future: Build with full ABR integration
# echo "Building with ABR integration..."
# $CXX $CXXFLAGS -I../abr -I.. -o abrsim_full abrsim.cpp \
#     ../abr/abr.cpp ../abr/HarmonicEwmaEstimator.cpp \
#     ../abr/RollingMedianOutlierEstimator.cpp
