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

# Quick start script for ABR Simulator Web UI

set -e

echo "🎬 AAMP ABR Simulator - Quick Start"
echo "===================================="
echo ""

# Check if abrsim is built
if [ ! -f "./abrsim" ]; then
    echo "⚠️  abrsim binary not found. Building now..."
    ./build.sh
    echo ""
fi

# Check Python version
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is required but not found"
    exit 1
fi

PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo "✓ Using Python $PYTHON_VERSION"
echo "✓ abrsim binary ready"
echo ""

# Start the server
echo "Starting web server on http://localhost:8080"
echo "Press Ctrl+C to stop"
echo ""

exec python3 ./abrsim_server.py
