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

declare DEFAULT_GSTVERSION="1.24.9"


function install_gstreamer_fn()
{
    # There is no CLEAN function

    if [[ "$OSTYPE" == "darwin"* ]]; then    
        # homebrew versions are not equivalent to the install package so we don't use it.  In the future that may change.

        if [[ $ARCH == "x86_64" ]]; then
            DEFAULT_GSTVERSION="1.18.6"
        elif [[ $ARCH == "arm64" ]]; then
            DEFAULT_GSTVERSION="1.24.9" 
        else
            echo "Architecture $ARCH is unsupported"
            return 1
        fi

        # Install Gstreamer and plugins if not installed
        if [ -f  /Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 ];then
            if [ $(/Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 --version | head -n1 |cut -d " " -f 3) == $DEFAULT_GSTVERSION ] ; then
                echo "gstreamer $DEFAULT_GSTVERSION is already installed"
                return
            fi
        fi
        echo "Installing GStreamer packages..."

        if [[ $ARCH == "x86_64" ]]; then
            curl -o gstreamer-1.0-$DEFAULT_GSTVERSION-x86_64.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$DEFAULT_GSTVERSION/gstreamer-1.0-$DEFAULT_GSTVERSION-x86_64.pkg
            sudo installer -pkg gstreamer-1.0-$DEFAULT_GSTVERSION-x86_64.pkg -target /
            rm gstreamer-1.0-$DEFAULT_GSTVERSION-x86_64.pkg
            curl -o gstreamer-1.0-devel-$DEFAULT_GSTVERSION-x86_64.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$DEFAULT_GSTVERSION/gstreamer-1.0-devel-$DEFAULT_GSTVERSION-x86_64.pkg
            sudo installer -pkg gstreamer-1.0-devel-$DEFAULT_GSTVERSION-x86_64.pkg  -target /
            rm gstreamer-1.0-devel-$DEFAULT_GSTVERSION-x86_64.pkg

        elif [[ $ARCH == "arm64" ]]; then
            curl -o gstreamer-1.0-$DEFAULT_GSTVERSION-universal.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$DEFAULT_GSTVERSION/gstreamer-1.0-$DEFAULT_GSTVERSION-universal.pkg
            sudo installer -pkg gstreamer-1.0-$DEFAULT_GSTVERSION-universal.pkg -target /
            rm gstreamer-1.0-$DEFAULT_GSTVERSION-universal.pkg
            curl -o gstreamer-1.0-devel-$DEFAULT_GSTVERSION-universal.pkg https://gstreamer.freedesktop.org/data/pkg/osx/$DEFAULT_GSTVERSION/gstreamer-1.0-devel-$DEFAULT_GSTVERSION-universal.pkg
            sudo installer -pkg gstreamer-1.0-devel-$DEFAULT_GSTVERSION-universal.pkg  -target /
            rm gstreamer-1.0-devel-$DEFAULT_GSTVERSION-universal.pkg
            # Gstreamer has a broken .pc prefix that can't be worked around with meson
            grep non_existent_on_purpose /Library/Frameworks/GStreamer.framework/Libraries/pkgconfig/*.pc
            retVal=$?
            if [ $retVal -eq 0 ]; then
                echo "Fixing Gstreamer.framework .pc files."
                sudo sed -i '.bak'  's#prefix=.*#prefix=/Library/Frameworks/GStreamer.framework/Versions/1.0#' /Library/Frameworks/GStreamer.framework/Libraries/pkgconfig/*
            fi
            
        fi
        INSTALL_STATUS_ARR+=("gstreamer $DEFAULT_GSTVERSION was successfully installed.")

    elif [[ "$OSTYPE" == "linux"* ]]; then
        echo "gstreamer is installed via apt on Linux targets."
    fi
}



function install_gstpluginsgoodfn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    if [[ "$OSTYPE" == "darwin"* ]]; then    
        # homebrew versions are not equivalent to the install package so we don't use it.  In the future that may change.

        # $OPTION_CLEAN == true
        if [ $1 = true ] ; then
            echo "gst-plugins-good clean"
            if [ -d gst-plugins-good-$DEFAULT_GSTVERSION ] ; then
                rm -rf gst-plugins-good-$DEFAULT_GSTVERSION
            fi
        fi

        if [ -d "gst-plugins-good-$DEFAULT_GSTVERSION" ]; then
            echo "gst-plugins-good-$DEFAULT_GSTVERSION is already installed"
            INSTALL_STATUS_ARR+=("gst-plugins-good-$DEFAULT_GSTVERSION was already installed.")
        else

            curl -o gst-plugins-good-$DEFAULT_GSTVERSION.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-$DEFAULT_GSTVERSION.tar.xz
            tar -xvzf gst-plugins-good-$DEFAULT_GSTVERSION.tar.xz
            cd gst-plugins-good-$DEFAULT_GSTVERSION
 
            patch -p1 < ../../OSx/patches/0009-qtdemux-aamp-tm_gst-1.16.patch
            patch -p1 < ../../OSx/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
            patch -p1 < ../../OSx/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch
            patch -p1 < ../../OSx/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch
            sed -in 's/gstglproto_dep\x27], required: true/gstglproto_dep\x27], required: false/g' meson.build

            PKG_CONFIG+=":/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"
            if [[ $ARCH == "x86_64" ]]; then
                PKG_CONFIG+=":/usr/local/lib/pkgconfig"
            elif [[ $ARCH == "arm64" ]]; then
                PKG_CONFIG+=":/opt/homebrew/lib/pkgconfig"
            fi

            echo "Building gst-plugins-good with --pkg-config path $PKG_CONFIG..."
            meson --pkg-config-path="${PKG_CONFIG}" -Dauto_features=disabled -Disomp4=enabled build
            ninja -C build
            sudo ninja -C build install

            # ARM vs x86 have different installation directories
            if [ -d /usr/local/lib/gstreamer-1.0 ]; then
                sudo cp  /usr/local/lib/gstreamer-1.0/libgstisomp4.dylib /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0/libgstisomp4.dylib
            else
                sudo cp  /opt/homebrew/lib/gstreamer-1.0/libgstisomp4.dylib /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0/libgstisomp4.dylib
            fi
            INSTALL_STATUS_ARR+=("gst-plugins-good-$DEFAULT_GSTVERSION was successfully installed.")
        fi

    elif [[ "$OSTYPE" == "linux"* ]]; then
        echo "gst-plugins-good-$DEFAULT_GSTVERSION is installed via apt on Linux targets."
    fi
}
