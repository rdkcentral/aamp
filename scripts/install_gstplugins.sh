#!/usr/bin/env bash


function gstplugin_install_build_fn() {

    cd $LOCAL_DEPS_BUILD_DIR

    # OPTION_CLEAN == true
    if [ $1 = true ] ; then
        echo " clean"
        rm -rf gst-plugins-rdk-aamp
    fi


    # This is an odd duck being build from aamp directory and installed to build/lib.
    # Maybe can be changed in the future
    if [ -d "gst-plugins-rdk-aamp" ]; then
        echo "gst-plugins-rdk-aamp is already installed"
        INSTALL_STATUS_ARR+=("gst-plugins-rdk-aamp was already installed.")
    else
        do_clone_rdk_repo_fn ${OPTION_AAMP_BRANCH} gst-plugins-rdk-aamp

        pushd ..

        if [[ "$OSTYPE" == "darwin"* ]]; then
            export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig/
            cmake -B ./build/gst_subtec ./.libs/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=./build/Debug
            make -C ./build/gst_subtec
            make -C ./build/gst_subtec install
        else
            cmake -B./build/gst_subtec ./.libs/gst-plugins-rdk-aamp/gst_subtec -DCMAKE_INSTALL_PREFIX=$LOCAL_DEPS_BUILD_DIR -DCMAKE_PLATFORM_UBUNTU=1
            make -C ./build/gst_subtec
            make -C ./build/gst_subtec install
        fi

        popd
        INSTALL_STATUS_ARR+=("gst-plugins-rdk-aamp was successfully installed.")
    fi

}
