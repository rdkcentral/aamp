#!/usr/bin/env bash


function install_build_middleware_interface_fn()
{
    cd $LOCAL_DEPS_BUILD_DIR

    # $OPTION_CLEAN == true
    if [ $1 = true ] ; then
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
        git checkout feature/RDKEMW-5744
        mkdir -p build
        cd build
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR}
        make
        make install

        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playerfbinterface \nDescription: iarm rfc interfaces library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerfbinterface \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerfbinterface.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: baseconversion \nDescription: base 16 and 64 conversion library \nVersion: 1.0 \nLibs: -L${libdir} -lbaseconversion \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libbaseconversion.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playerlogmanager \nDescription: player log manager library \nVersion: 1.0 \nLibs: -L${libdir} -lplayerlogmanager \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayerlogmanager.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: playergstinterface \nDescription: player gstreamer interfaces library \nVersion: 1.0 \nLibs: -L${libdir} -lplayergstinterface \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libplayergstinterface.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: subtec \nDescription: subtec library \nVersion: 1.0 \nLibs: -L${libdir} -lsubtec \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libsubtec.pc

        INSTALL_STATUS_ARR+=("middleware was successfully installed.")
    fi
}
