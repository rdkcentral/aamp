#!/usr/bin/env bash


function subtec_install_fn() {

    # Need PLAYER_DIR passed in so we can find patch files
    if [ -z "${1}" ] ; then
        echo "Middleware directory parameter is empty, can not find patch files."
        return 1
    fi
    # Need LOCAL_DEPS_BUILD_DIR passed in so we can patch the location of glib
    if [ -z "${2}" ] ; then
        echo "Dependency directory parameter is empty, can not patch subtec-app CMakeLists.txt"
        return 1
    fi

    echo "Cloning subtec-app..."
    do_clone_fn "https://code.rdkcentral.com/r/components/generic/subtec-app"
    git -C subtec-app checkout a95f7591fff3fb8777781dfdc76d95fc0a1c382b

    echo
    echo "Cloning websocket-ipplayer2-utils..."
    do_clone_fn https://code.rdkcentral.com/r/components/generic/websocket-ipplayer2-utils subtec-app/websocket-ipplayer2-utils
    git -C subtec-app/websocket-ipplayer2-utils checkout 2287fea4d1af0a632aed5f1b8bfba8babbdade1f


    pushd subtec-app
    echo "Patching subtec-app from ${1}"
    git apply -p1 ${1}/OSX/patches/subttxrend-app-xkbcommon.patch
    git apply -p1 ${1}/OSX/patches/subttxrend-app-packet.patch
    git apply -p1 ${1}/OSX/patches/websocket-ipplayer2-link.patch --directory websocket-ipplayer2-utils
    git apply -p1 ${1}/OSX/patches/websocket-ipplayer2-typescpp.patch --directory websocket-ipplayer2-utils
    cp ${1}/OSX/patches/RDKLogoBlack.png subttxrend-gfx/quartzcpp/assets/RDKLogo.png
    git apply -p1 ${1}/OSX/patches/subttxrend-app-ubuntu_24_04_build.patch
    git apply -p1 ${1}/OSX/patches/websocket-ipplayer2-ubuntu_24_04_build.patch --directory websocket-ipplayer2-utils


    echo "Patching subtec-app CMakeLists.txt with '$2'"
    if [[ "$OSTYPE" == "darwin"* ]] ; then
        SED_ARG="''"     # MacOS -i has different -i argument
    fi
    sed -i ${SED_ARG} 's:COMMAND gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code SubtitleDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/SubtitleDbusInterface.xml:COMMAND '"${2}"'/glib/build/gio/gdbus-2.0/codegen/gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code SubtitleDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/SubtitleDbusInterface.xml:g' subttxrend-dbus/CMakeLists.txt

    sed -i ${SED_ARG} 's:COMMAND gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code TeletextDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/TeletextDbusInterface.xml:COMMAND '"${2}"'/glib/build/gio/gdbus-2.0/codegen/gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code TeletextDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/TeletextDbusInterface.xml:g' subttxrend-dbus/CMakeLists.txt
    
    echo "Update asio::service to asio::context"
    sed -i ${SED_ARG} 's/asio::io_service/asio::io_context/g'  websocket-ipplayer2-utils/src/ipp2/servers/SimpleWebServer.cpp
    sed -i ${SED_ARG} 's/asio::ip::address::from_string/asio::ip::make_address/g'  websocket-ipplayer2-utils/src/ipp2/servers/SimpleWebServer.cpp
    sed -i ${SED_ARG} 's/\([a-zA-Z0-9_]*\)->reset()/\1->restart()/g'  websocket-ipplayer2-utils/src/ipp2/servers/SimpleWebServer.cpp
    sed -i ${SED_ARG} 's/\([a-zA-Z0-9_]*\)->post(/asio::post(*\1, /g'  websocket-ipplayer2-utils/src/ipp2/servers/SimpleWebServer.cpp
    

    echo "subtec-app source prepared"
    popd
}

function subtec_install_build_fn() {

    cd $LOCAL_DEPS_BUILD_DIR

    # OPTION_CLEAN == true
    if [ $1 = true ] ; then
        echo "subtec clean"
        rm -rf subtec-app
    fi


    # Install
    if [ -d "subtec-app" ]; then
        echo "subtec-app is already installed"
        INSTALL_STATUS_ARR+=("subtec-app was already installed.")
    else
        # Tell installer where DEPs are so cmake can be patched
        subtec_install_fn ${PLAYER_DIR} ${LOCAL_DEPS_BUILD_DIR}
    fi

    # Build
    cd subtec-app/subttxrend-app/x86_builder/

    if [ ! -d build/install ] ; then
        PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH ./build.sh fast

        if [ -f ./build/install/usr/local/bin/subttxrend-app ]; then
            echo "subtec-app has been built."
            INSTALL_STATUS_ARR+=("subtec-app has been built.")
        else
            echo "subtec-app build has failed."
            return 1
        fi
    fi
}
