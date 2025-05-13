#!/usr/bin/env bash


function install_build_entos_player_firebolt_interface_fn()
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
        git checkout feature/RDKEMW-4040
        mkdir -p build
        cd build
        cmake .. -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR}
        make
        make install

        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: EntosPlayerFireboltInterface \nDescription: iarm rfc interfaces library \nVersion: 1.0 \nLibs: -L${libdir} -lEntosPlayerFireboltInterface \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libEntosPlayerFireboltInterface.pc
        echo -e 'prefix='$LOCAL_DEPS_BUILD_DIR'/lib \nexec_prefix='$LOCAL_DEPS_BUILD_DIR' \nlibdir='$LOCAL_DEPS_BUILD_DIR'/lib \nincludedir='$LOCAL_DEPS_BUILD_DIR'/include \n \nName: BaseConversion \nDescription: base 16 and 64 conversion library \nVersion: 1.0 \nLibs: -L${libdir} -lBaseConversion \nCflags: -I${includedir}' > $LOCAL_DEPS_BUILD_DIR/lib/pkgconfig/libBaseConversion.pc
        
        INSTALL_STATUS_ARR+=("middleware was successfully installed.")
    fi
}