#!/bin/bash

# exit if any command fails
set -euo pipefail

pushd ../../../
cmake -B./build/gst_subtec ./Linux/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./Linux -DCMAKE_PLATFORM_UBUNTU=1
make -C ./build/gst_subtec
make -C ./build/gst_subtec install
popd
