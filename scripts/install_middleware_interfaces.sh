#!/usr/bin/env bash
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


function install_build_middleware_interface_fn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    # $OPTION_CLEAN == true
    if [[ "$1" == true ]] ; then
        echo "middleware clean"
        if [ -d middleware ] ; then
            rm -rf middleware
            # uninstall?
            rm -rf $LOCAL_DEPS_BUILD_DIR/include/middleware
        fi
    fi

    if [ -d "middleware-player-interface" ]; then
        echo "middleware is already installed"
        INSTALL_STATUS_ARR+=("middleware was already installed.")
    else
        echo "Installing middleware..."
        do_clone_fn  https://github.com/rdkcentral/middleware-player-interface.git

        cd middleware-player-interface
        git checkout feature/RDKEMW-11639
        mkdir -p build
        cd build
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_PLATFORM_UBUNTU=ON || return 1
        make || return 1
        make install || return 1

        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playerfbinterface \nDescription: player externals interface library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerfbinterface \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerfbinterface.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: baseconversion \nDescription: base 16 and 64 conversion library \nVersion: 1.0 \nLibs: -L${libdir} -lbaseconversion \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libbaseconversion.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playerlogmanager \nDescription: player log manager library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerlogmanager \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerlogmanager.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playergstinterface \nDescription: player gstreamer interfaces library \nVersion: 1.0 \nLibs: -L${libdir} -lplayergstinterface \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayergstinterface.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: subtec \nDescription: player subtitle and teletext processing library \nVersion: 1.0 \nLibs: -L${libdir} -lsubtec \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libsubtec.pc

        INSTALL_STATUS_ARR+=("middleware was successfully installed.")
    fi
}