#!/usr/bin/env bash
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

function install_build_glib_fn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    # $OPTION_CLEAN == true
    if [ $1 = true ] ; then
        echo "glib clean"
        if [ -d glib ] ; then
            rm -rf glib
            # uninstall?
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock.a	
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock_main.a	
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgtest.a
        fi
    fi

    if [ -d "glib" ]; then
        echo "glib is already installed"
        INSTALL_STATUS_ARR+=("glib was already installed.")
    else
        # TODO: create a pyton virtual env so we don't have to globally install this pkg and avoid "error: externally-managed-environment"
        PIP_BREAK_SYSTEM_PACKAGES=1 pip3 install setuptools

        echo "Installing glib..."
        do_clone_fn  https://github.com/GNOME/glib.git -b 2.83.1
        pushd glib
        meson build && cd build
        meson compile
        INSTALL_STATUS_ARR+=("glib was successfully installed.")
        popd
    fi
}
