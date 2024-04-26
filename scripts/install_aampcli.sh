#!/usr/bin/env bash

function aampcli_install_postbuild_fn()
{
    cd $AAMP_DIR/build

    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo ""
        if [ $OPTION_DONT_RUN_AAMPCLI = false ];then
            # Launch Xcode
            (open AAMP.xcodeproj) &
    
            #Launching aamp-cli
            otool -L ./Debug/aamp-cli
            ./Debug/aamp-cli https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
        else
            echo "To use Xcode, open aamp/build/AAMP.xcodeproj project file"
        fi      
    else
        if [ $OPTION_DONT_RUN_AAMPCLI = false ];then
            echo "Opening VSCode Workspace..."
            code ../ubuntu-aamp-cli.code-workspace
        fi
    fi
}

function aampcli_install_prebuild_fn()
{
    cd $AAMP_DIR

    # $OPTION_CLEAN == true
    if [ ${1} == true ] ; then
        echo "aampcli clean"
        if [ -d build ] ; then
            rm -rf build
            # uninstall?
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock.a
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgmock_main.a
            #rm $LOCAL_DEPS_BUILD_DIR/lib/libgtest.a
        fi
    fi


    if [ -d "build" ]; then
        echo "aamp-cli is already installed"
        INSTALL_STATUS_ARR+=("aamp-cli prebuild was already installed.")
    else
        mkdir -p build
        touch build/install_manifest.txt
        if [[ "$OSTYPE" == "darwin"* ]]; then
            mkdir -p build/Debug
            # allow XCode to clean build folder
            xattr -w com.apple.xcode.CreatedByBuildSystem true build
        fi
    fi

    #Create default channel ~/aampcli.csv – supports local configuration overrides
    if [ -f "${HOME}/aampcli.csv" ]; then
        echo "${HOME}/aampcli.csv exists."
    else
        echo "Creating default channel list file ${HOME}/aampcli.csv"
        cp ./OSX/aampcli.csv ${HOME}/aampcli.csv
    fi 
}

function aampcli_install_build_darwin_fn()
{

    echo "Build aamp-cli"

    cd $AAMP_DIR


    # Local built dependencies
    PKG_CONFIG="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"

    # MacOS using a gstreamer framework
    PKG_CONFIG="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:${PKG_CONFIG}"
    if [[ $ARCH == "x86_64" ]]; then
        PKG_CONFIG="${PKG_CONFIG}:/usr/local/lib/pkgconfig"
    elif [[ $ARCH == "arm64" ]]; then
        PKG_CONFIG="${PKG_CONFIG}:/opt/homebrew/lib/pkgconfig"
    fi
    # MacOS provides a curl installation, but we'd like a newer version where was it installed?
    PKG_CONFIG_CURL=$(install_pkgs_pkgconfig_darwin_fn curl)
    if [ ! -z ${PKG_CONFIG_CURL} ] ; then
        PKG_CONFIG="${PKG_CONFIG_CURL}:${PKG_CONFIG}"
    fi

    cd build && PKG_CONFIG_PATH=${PKG_CONFIG}:${PKG_CONFIG_PATH} cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CUSTOM_QTDEMUX_PLUGIN_ENABLED=TRUE -DSMOKETEST_ENABLED=OFF -DCOVERAGE_ENABLED=${OPTION_COVERAGE} -DUTEST_ENABLED=ON -G Xcode ../

    # the cmake Xcode generator can not set this scheme property (Debug -> Options -> Console -> Use Terminal
    patch ./AAMP.xcodeproj/xcshareddata/xcschemes/aamp-cli.xcscheme < ../OSX/patches/aamp-cli.xscheme.patch


    if [ -d "AAMP.xcodeproj" ]; then
        echo "AAMP Environment Sucessfully Installed."
        arr_install_status+=("AAMP Environment Sucessfully Installed.")
    else
        echo "AAMP Environment FAILED to Install."
        arr_install_status+=("AAMP Environment FAILED to Install.")
    fi
   
    echo "Starting Xcode, open aamp/build/AAMP.xcodeproj project file OR Execute ./aamp-cli or /playbintest <url> binaries"
    echo "Opening AAMP project in Xcode..."
    # Changed "\-bash" as that signifies login shell, running ./install-aamp.sh (as opposed to source install-aamp.sh) and that would not be the case
    if ps -o comm= $$ | grep -q "bash"; then
        echo "Running in bash"
    else
        echo "Changing login shell to bash"
        chsh -s /bin/bash
    fi

   
    echo "Now Building aamp-cli"
    xcodebuild -scheme aamp-cli  build

    if [  -f "./Debug/aamp-cli" ]; then
        echo "OSX AAMP Build PASSED"
        arr_install_status+=("OSX AAMP Build PASSED")
        
        subtec_install_run_script_fn   # after build/Debug directory created by xcodebuild
    else
        echo "OSX AAMP Build FAILED"
        arr_install_status+=("OSX AAMP Build FAILED")
        return 1
    fi
    
}

function aampcli_install_build_linux_fn
{
    echo "Build aamp-cli"

    cd $AAMP_DIR

    # Local built dependencies
    PKG_CONFIG="${LOCAL_DEPS_BUILD_DIR}/lib/pkgconfig"

    PKG_CONFIG_PATH="${PKG_CONFIG}" cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=${LOCAL_DEPS_BUILD_DIR} -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH="${LOCAL_DEPS_BUILD_DIR}/lib" -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCOVERAGE_ENABLED=${OPTION_COVERAGE} -DUTEST_ENABLED=ON -DCMAKE_CUSTOM_QTDEMUX_PLUGIN_ENABLED=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S$PWD -B"${AAMP_DIR}/build" -G "Unix Makefiles"

   echo "Making aamp-cli..."
   cd build
   make aamp-cli
   make install
   if [  -f "./aamp-cli" ]; then
        echo "****Linux AAMP Build PASSED****"
        ldd ./aamp-cli
        arr_install_status+=("Linux AAMP Build PASSED")
    else
        echo "****Linux AAMP Build FAILED****"
        arr_install_status+=("Linux AAMP Build FAILED")
	return 1
    fi
}

function aampcli_install_build_fn()
{
    if [[ "$OSTYPE" == "darwin"* ]]; then
	    aampcli_install_build_darwin_fn
    else
	    aampcli_install_build_linux_fn
    fi
}

