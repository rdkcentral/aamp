#!/bin/bash -x
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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


# do_clone <params>
# Pass all params to 'git clone'
# If the clone fails then exit the script
function do_clone()
{
    arglist=""
    while [ "$1" ]; do
        arglist+=" $1"
        shift
    done

    echo && echo "Executing: 'git clone $arglist'"
    git clone $arglist

    if [ $? != 0 ]; then
        echo "'git clone $arglist' FAILED"
        exit 1
    fi
}

echo "Please do make sure you have access to github and code.rdkcentral.com"
cd ..
do_clone https://github.com/bitmovin/libdash.git
cd libdash/libdash
git checkout stable_3_0
do_clone -b rdk-next "https://code.rdkcentral.com/r/rdk/components/generic/rdk-oe/meta-rdk-ext"
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0001-libdash-build.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0002-libdash-starttime-uint64.patch 
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0003-libdash-presentationTimeOffset-uint64.patch 
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0004-Support-of-EventStream.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0005-DELIA-39460-libdash-memleak.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0006-RDK-32003-LLD-Support.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0007-DELIA-51645-Event-Stream-RawAttributes-Support.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0008-DELIA-53263-Use-Label-TAG.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0009-RDK-35134-Support-for-FailoverContent.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0010-RDKAAMP-121-Failover-Tag-on-SegmentTemplate.patch
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0011-RDKAAMP-61-AAMP-low-latency-dash-stream-evaluation.patch	
patch -p1 < meta-rdk-ext/recipes-multimedia/libdash/libdash/0012-To-retrieves-the-text-content-of-CDATA-section.patch
mkdir -p build
cd build
cmake ..
make

if [[ "$OSTYPE" == "darwin"* ]]; then
    cp bin/libdash.dylib /usr/local/lib/
elif [[ "$OSTYPE" == "linux"* ]]; then
    cp bin/libdash.so /usr/local/lib/
else
    echo "WARNING - unrecognized platform!"
    exit
fi

mkdir /usr/local/include
mkdir /usr/local/include/libdash
mkdir /usr/local/include/libdash/xml
mkdir /usr/local/include/libdash/mpd
mkdir /usr/local/include/libdash/helpers
mkdir /usr/local/include/libdash/network
mkdir /usr/local/include/libdash/portable
mkdir /usr/local/include/libdash/metrics
cp -pr ../libdash/include/*.h /usr/local/include/libdash
cp -pr ../libdash/source/xml/*.h /usr/local/include/libdash/xml
cp -pr ../libdash/source/mpd/*.h /usr/local/include/libdash/mpd
cp -pr ../libdash/source/network/*.h /usr/local/include/libdash/network
cp -pr ../libdash/source/portable/*.h /usr/local/include/libdash/portable
cp -pr ../libdash/source/helpers/*.h /usr/local/include/libdash/helpers
cp -pr ../libdash/source/metrics/*.h /usr/local/include/libdash/metrics

echo -e 'prefix=/usr/local \nexec_prefix=${prefix} \nlibdir=${exec_prefix}/lib \nincludedir=${prefix}/include/libdash \n \nName: libdash \nDescription: ISO/IEC MPEG-DASH library \nVersion: 3.0 \nRequires: libxml-2.0 \nLibs: -L${libdir} -ldash \nLibs.private: -lxml2 \nCflags: -I${includedir}'  > /usr/local/lib/pkgconfig/libdash.pc
echo "Libdash patched and installed successfully"
echo "Your current path is " $PWD 
