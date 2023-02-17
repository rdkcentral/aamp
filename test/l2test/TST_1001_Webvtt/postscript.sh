#!/bin/bash
pushd ../../../
if [[ "$OSTYPE" == "darwin"* ]]; then
    #Restore original CMakeLists.txt file, modified by prescript.sh
    if [ -f "CMakeLists.txt.bak" ]; then
        echo "Restore original CMakeLists.txt file"
        mv CMakeLists.txt.bak CMakeLists.txt
    else
        echo "Ensure XCODE_SCHEME_ADDRESS_SANITIZER &  XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN are disabled in aamp/CMakeLists.txt"
    fi
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig/
    cmake -Bbuild/gst_subtec -H./.libs/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./build/Debug
    make -C build/gst_subtec
    make -C build/gst_subtec install
else
    cmake -B./build/gst_subtec ./Linux/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./Linux -DCMAKE_PLATFORM_UBUNTU=1
    make -C ./build/gst_subtec
    make -C ./build/gst_subtec install
fi
popd