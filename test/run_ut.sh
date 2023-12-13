#!/bin/sh

# This is the RDK-E L1 test build and run script. It is expected to be run from a  
# rdke-builds/ci-container-image:latest docker container image.

defaultlibdashversion="libdash = 3.0"
pkg-config --exists $defaultlibdashversion
if [ $? != 0 ]; then
    ../install_libdash.sh
fi

#Build aamp components
echo "Building following aamp components"

#Build aampabr
echo "Building aampabr..."
cd /home/aampabr
mkdir -p build
cd build
cmake ..
make
make install

#Build aampmetrics
echo "Building aampmetrics..."
cd /home/aampmetrics
mkdir -p build
cd build
cmake ..
make
make install

apt update
echo "Installing gstreamer"
dpkg -s libgstreamer1.0-dev
if [ $? != 0 ]; then
    apt install libgstreamer1.0-dev
fi

dpkg -s libgstreamer-plugins-base1.0-dev
if [ $? != 0 ]; then
    apt install libgstreamer-plugins-base1.0-dev
fi

dpkg -s libreadline-dev
if [ $? != 0 ]; then
    apt install  libreadline-dev
fi

echo "Running L1 tests"
cd /home/aamp/test
cd utests
# apt install places gstreamer pc files as noted here
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig ./run.sh
