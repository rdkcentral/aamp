#!/bin/bash

if [[ "$OSTYPE" != "darwin"* ]]; then
    sudo apt-get install -y libgstreamer-plugins-base1.0-dev
    sudo apt-get install -y gstreamer1.0-libav
    sudo apt-get install -y gstreamer1.0-plugins-base
    sudo apt-get install -y gstreamer1.0-plugins-good
    sudo apt-get install -y gstreamer1.0-plugins-bad
    sudo apt-get install -y gstreamer1.0-plugins-ugly
fi