#!/bin/bash

echo $OSTYPE
aampdir=$PWD/..
echo $aampdir
linuxbuilddir=$aampdir/Linux
defaultcodebranch=dev_sprint_23_1

# pull in general utility finctions
source $aampdir/install-script-utilities.sh


while getopts ":d:b:" opt; do
  case ${opt} in
    d ) # process option d install base directory name
    linuxbuilddir=${OPTARG}
    echo "${OPTARG}"
      ;;
    b ) # process option b code branch name
    codebranch=${OPTARG}
      ;;
    * ) echo "Usage: $0 [-b aamp branch name] [-d local setup directory name]"
     exit
      ;;
  esac
done

#Check and if needed setup default aamp code branch name and local environment directory name
if [[ $codebranch == "" ]]; then
    codebranch=${defaultcodebranch}
    echo "using default code branch: $defaultcodebranch"
fi 

mkdir -p $linuxbuilddir
cd $linuxbuilddir
echo "linuxbuilddir: $linuxbuilddir"

#### CLONE_PACKAGES
do_clone_rdk_repo $codebranch aampabr
do_clone_rdk_repo $codebranch gst-plugins-rdk-aamp


if [ -d "cJSON" ]; then
    echo "exist cJSON"
else
    do_clone https://github.com/DaveGamble/cJSON
fi

do_clone_rdk_repo $codebranch aampmetrics
if [ $? = 0 ]; then
    echo "Patching aampmetrics CMakeLists.txt"
    pushd aampmetrics
    	patch < ../patches/aampmetrics.patch
    popd
else
    echo "skipping ampmetrics patch"
fi

if [ -d "googletest" ]; then
    echo "googletest exists"
else
    do_clone https://github.com/google/googletest
    pushd googletest
        git checkout $googletestbranch
    popd
fi    


#### CLONE_PACKAGES

#  TODO: check
# apt-get install gstreamer gst-validate gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-validate gst-libav

# Patch qtdemux, The version of gst-plugins-good is selected based on the OS version.
echo "Patching qtdemux"

ver=$(grep -oP 'VERSION_ID="\K[\d.]+' /etc/os-release)
if [ ${ver:0:2} -eq 20 ]; then
    defaultgstversion="1.16.3"
elif [ ${ver:0:2} -eq 22 ]; then
    defaultgstversion="1.20.3"
else
    defaultgstversion="1.18.4"
fi

curl -o	gst-plugins-good-$defaultgstversion.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-$defaultgstversion.tar.xz
tar -xf gst-plugins-good-$defaultgstversion.tar.xz
cd gst-plugins-good-$defaultgstversion
pwd
patch -p1 < $aampdir/OSX/patches/0009-qtdemux-aamp-tm_gst-1.16.patch
patch -p1 < $aampdir/OSX/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
patch -p1 < $aampdir/OSX/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch
patch -p1 < $aampdir/OSX/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch
meson --prefix=/usr build
ninja -C build
sudo ninja -C build install
cd ..

### Install libdash
if [ -d "libdash" ]; then
    echo "libdash installed"
    libdash_build_dir=$linuxbuilddir/libdash/libdash/build/
else
    echo "Installing libdash"
    mkdir tmp
    pushd tmp
        echo $PWD
        source $aampdir/install_libdash.sh
        libdash_build_dir=$PWD
    pushd
    rm -rf tmp
fi
### Install libdash

function build_repo()
{
    echo "Building $1 "
    pushd $1
        mkdir -p build
        cd build
        env PKG_CONFIG_PATH=$linuxbuilddir/lib/pkgconfig cmake .. -DCMAKE_LIBRARY_PATH=$linuxbuilddir/lib -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX=$linuxbuilddir
        make
        make install
    popd
}

# export env PKG_CONFIG_PATH=$linuxbuilddir/lib/pkgconfig

build_repo cJSON
build_repo aampabr
build_repo aampmetrics


###Build gtest
echo "Building googletest"
pushd googletest
    mkdir -p build
    cd build
    env PKG_CONFIG_PATH=$linuxbuilddir/lib/pkgconfig cmake .. -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX=$linuxbuilddir
    make
    make install
popd
###End build gtest


#### COPY LIBDASH FILES
pushd $libdash_build_dir
    cp ./bin/libdash.so $linuxbuilddir/lib/
    mkdir -p $linuxbuilddir/include/libdash
    mkdir -p $linuxbuilddir/include/libdash/xml
    mkdir -p $linuxbuilddir/include/libdash/mpd
    mkdir -p $linuxbuilddir/include/libdash/helpers
    mkdir -p $linuxbuilddir/include/libdash/network
    mkdir -p $linuxbuilddir/include/libdash/portable
    mkdir -p $linuxbuilddir/include/libdash/metrics
    cp -pr ../libdash/include/*.h $linuxbuilddir/include/libdash
    cp -pr ../libdash/source/xml/*.h $linuxbuilddir/include/libdash/xml
    cp -pr ../libdash/source/mpd/*.h $linuxbuilddir/include/libdash/mpd
    cp -pr ../libdash/source/network/*.h $linuxbuilddir/include/libdash/network
    cp -pr ../libdash/source/portable/*.h $linuxbuilddir/include/libdash/portable
    cp -pr ../libdash/source/helpers/*.h $linuxbuilddir/include/libdash/helpers
    cp -pr ../libdash/source/metrics/*.h $linuxbuilddir/include/libdash/metrics
popd
echo -e 'prefix='$linuxbuilddir'/lib \nexec_prefix='$linuxbuilddir' \nlibdir='$linuxbuilddir'/lib \nincludedir='$linuxbuilddir'/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}' > $linuxbuilddir/lib/pkgconfig/libdash.pc

echo "AAMP Workspace Sucessfully prepared" 
echo "Please Start VS Code, open workspace from file: ubuntu-aamp-cli.code-workspace"
