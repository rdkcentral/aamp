#!/bin/bash

echo $OSTYPE
aampdir=$PWD/..
echo $aampdir
linuxbuilddir=$aampdir/Linux
defaultcodebranch=dev_sprint_24_2
googletestreference="tags/release-1.11.0"
defaultprotobufreference="3.11.x"
defaultrialtoreference="v0.2.2"

# pull in general utility finctions
source $aampdir/install-script-utilities.sh


# Parse optional command line parameters
# Note that options inherited from install-aamp.sh will not be processed here
if  [[ $1 = "rialto" ]]; then
    buildrialto=true
    shift
fi

while getopts ":d:b:g:p:r:" opt; do
  case ${opt} in
    d ) # process option d install base directory name
    linuxbuilddir=${OPTARG}
    echo "${OPTARG}"
      ;;
    b ) # process option b code branch name
    codebranch=${OPTARG}
      ;;
    g ) # process option g googletest reference
    googletestreference=${OPTARG}
      ;;
    p ) # process option p protobuf reference
      protobufreference=${OPTARG}
      echo "protobufreference : ${OPTARG}"
      ;;
    r ) # process option r rialto reference
      rialtoreference=${OPTARG}
      echo "rialtoreference : ${OPTARG}"
      ;;
    * )
      echo "Usage: $0 [rialto] [-b aamp branch name]"
      echo "                        [-d local setup directory name]"
      echo "                        [-g googletest reference] [-p protobuf reference] [-r rialto reference]"
      exit
      ;;
  esac
done

# Check and, if needed, setup default aamp code branch name and local environment directory name
if [[ $codebranch == "" ]]; then
    codebranch=${defaultcodebranch}
    echo "using default code branch: $defaultcodebranch"
fi

if [[ $buildrialto == "" ]]; then
    buildrialto=false
fi

if [[ $protobufreference == "" ]]; then
    protobufreference=$defaultprotobufreference
fi

if [[ $rialtoreference == "" ]]; then
    rialtoreference=$defaultrialtoreference
fi

mkdir -p $linuxbuilddir
mkdir -p $linuxbuilddir/bin
cd $linuxbuilddir
echo "linuxbuilddir: $linuxbuilddir"

#### CLONE_PACKAGES
do_clone_rdk_repo $codebranch aampabr
do_clone_rdk_repo $codebranch gst-plugins-rdk-aamp


do_clone_github_repo https://github.com/DaveGamble/cJSON cJSON

if [ $buildrialto = true ]; then
    do_clone_github_repo https://github.com/protocolbuffers/protobuf.git protobuf -b $protobufreference --recursive

    if [ -d "rialto" ]; then
        echo "rialto exists"
    else
        do_clone https://github.com/rdkcentral/rialto.git rialto
        pushd rialto
            echo "Checkout rialto '$rialtoreference'"
            git checkout $rialtoreference
        popd
    fi

    if [ -d "rialto-gstreamer" ]; then
        echo "rialto-gstreamer exists"
    else
        do_clone https://github.com/rdkcentral/rialto-gstreamer.git rialto-gstreamer
        pushd rialto-gstreamer
            echo "Checkout rialto-gstreamer '$rialtoreference'"
            git checkout $rialtoreference
        popd
    fi
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
        echo "Checkout googletest '$googletestreference'"
        git checkout $googletestreference
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
        shift
        mkdir -p build
        cd build
        env PKG_CONFIG_PATH=$linuxbuilddir/lib/pkgconfig cmake .. -DCMAKE_LIBRARY_PATH=$linuxbuilddir/lib -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_INSTALL_PREFIX=$linuxbuilddir "$@"
        make
        make install
    popd
}

# export env PKG_CONFIG_PATH=$linuxbuilddir/lib/pkgconfig

build_repo cJSON
build_repo aampabr
build_repo aampmetrics

if [ $buildrialto = true ]; then
    echo "Building protobuf"
    pushd protobuf
    ./autogen.sh
    ./configure --prefix=$linuxbuilddir
    make
    make install
    popd

    build_repo rialto -DNATIVE_BUILD=ON -DRIALTO_BUILD_TYPE=Debug
    build_repo rialto-gstreamer -DCMAKE_INCLUDE_PATH="${linuxbuilddir}/rialto/stubs/opencdm/;${linuxbuilddir}/rialto/media/public/include/" -DCMAKE_LIBRARY_PATH="${linuxbuilddir}/rialto/build/stubs/opencdm;${linuxbuilddir}/rialto/build/media/client/main/" -DNATIVE_BUILD=ON
fi


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
