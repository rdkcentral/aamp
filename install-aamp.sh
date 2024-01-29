#!/bin/bash
# This script will setup basic environment and fetch aamp code
# for a vanilla Big Sur/Monterey system to be ready for development

#######################Default Values##################
aamposxinstallerver="0.11"
defaultbuilddir=aamp-devenv-$(date +"%Y-%m-%d-%H-%M")
defaultcodebranch="dev_sprint_24_1"
defaultchannellistfile="$HOME/aampcli.csv"
defaultopensslversion="openssl@1.1"
defaultlibdashversion="libdash = 3.0"
googletestreference="tags/release-1.11.0"

processtorun="aamp"
subtecoption=""
dontrunaampcli=false
installed_pkgconfig="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"


# pull in general utility finctions
source install-script-utilities.sh

arch=$(uname -m)
echo "Architecture is +$(arch)+"
if [[ $arch == "x86_64" ]]; then
    defaultgstversion="1.18.6"
elif [[ $arch == "arm64" ]]; then
    defaultgstversion="1.20.3" #1.19.90
else
    echo "Achitecture $arch is unsupported"
    exit
fi

######################################################## 
#Disable coverage tests by default
COVERAGE=OFF

######################################################## 
# Declare a status array to collect full summary to be printed by the end of execution
declare -a  arr_install_status

find_or_install_pkgs() {
    # Check if brew package $1 is installed
    # http://stackoverflow.com/a/20802425/1573477
    for pkg in "$@"; 
    do  
    	if brew ls --versions $pkg > /dev/null; then        
            echo "${pkg} is already installed."
            arr_install_status+=("${pkg} is already installed.")
        else
            echo "Installing ${pkg}"
            brew install $pkg
            #update summery
            if brew ls --versions $pkg > /dev/null; then
                #The package is successfully installed 
                arr_install_status+=("The package was ${pkg} was successfully installed.")
            
            else
                #The package is failed to be installed
                arr_install_status+=("The package ${pkg} was FAILED to be installed.")
            fi
        fi
        #if pkg is openssl and its successfully installed every time ensure to symlink to the latest version 
        if [ $pkg =  "${defaultopensslversion}" ]; then
            OPENSSL_PATH=$(brew info $defaultopensslversion|sed '4q;d'|cut -d " " -f1)
            sudo rm -f /usr/local/ssl
            sudo ln -s $OPENSSL_PATH /usr/local/ssl
            export PKG_CONFIG_PATH="$OPENSSL_PATH/lib/pkgconfig" 
        fi
        pkgdir="`brew --prefix ${pkg}`/lib/pkgconfig:" 
        installed_pkgconfig=$pkgdir$installed_pkgconfig
    done
}


install_system_packages() {

    #Install XCode Command Line Tools
    base_macOSver=10.15
    ver=$(sw_vers | grep ProductVersion | cut -d':' -f2 | tr -d ' ')
    if [ $(echo -e $base_macOSver"\n"$ver | sort -V | tail -1) == "$base_macOSver" ];then
        echo "Install XCode Command Line Tools"
        xcode-select --install
        sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_$ver.pkg -target /

    else
        echo "New version of Xcode detected - XCode Command Line Tools Install not required"
        xcrun --sdk macosx --show-sdk-path
        ### added fix for  https://stackoverflow.com/questions/17980759/xcode-select-active-developer-directory-error
        sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
    fi

    #Check/Install base packages needed by aamp env
    echo "Check/Install aamp development environment base packages"
    find_or_install_pkgs json-glib cmake $defaultopensslversion libxml2 ossp-uuid cjson gnu-sed jpeg-turbo taglib speex mpg123 meson ninja pkg-config flac lcov gcovr

    # ORC causes compile errors on x86_64 Mac, but not on ARM64
    if [[ $arch == "x86_64" ]]; then
        echo "Checking/removing ORC package which cause compile errors with gst-plugins-good"
        brew list | grep -i orc
        if [ $? -eq 0 ]; then
            read -p "Found ORC, remove ORC package (Y/N)" remove_orc
            case $remove_orc in
                [Yy]* ) brew remove -f --ignore-dependencies orc 
                    ;;
                * ) echo "Exiting without removal ..."
                    exit
                    ;;
            esac
        fi
    elif [[ $arch == "arm64" ]]; then
        find_or_install_pkgs orc
    fi
     
    echo "Checking for gtest installation"
    pkg-config --exists gtest
    if [ $? != 0 ]; then
        do_clone https://github.com/google/googletest
        cd googletest
        git checkout $googletestreference
        mkdir build

        cd build
        cmake ../
        make
        sudo make install
        cd ../../
    else
        echo "google test is already installed."
    fi


    pkg-config --exists libcjson
    if [ $? != 0 ]; then
        do_clone https://github.com/DaveGamble/cJSON.git
        cd cJSON
        mkdir build
        cd build
        cmake ../
        make
        sudo make install
        cd ../../
    else
        echo "libcjson is already installed."
    fi

    #Install Gstreamer and plugins if not installed
    if [ -f  /Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 ];then
        if [ $(/Library/Frameworks/GStreamer.framework/Versions/1.0/bin/gst-launch-1.0 --version | head -n1 |cut -d " " -f 3) == $defaultgstversion ] ; then
            echo "gstreamer ver $defaultgstversion installed"
            return
        fi
    fi
    echo "Installing GStreamer packages..."
    
    if [[ $arch == "x86_64" ]]; then
        curl -o gstreamer-1.0-$defaultgstversion-x86_64.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-$defaultgstversion-x86_64.pkg
        sudo installer -pkg gstreamer-1.0-$defaultgstversion-x86_64.pkg -target /
        rm gstreamer-1.0-$defaultgstversion-x86_64.pkg
        curl -o gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg
        sudo installer -pkg gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg  -target /
        rm gstreamer-1.0-devel-$defaultgstversion-x86_64.pkg
    elif [[ $arch == "arm64" ]]; then
        curl -o gstreamer-1.0-$defaultgstversion-universal.pkg  https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-$defaultgstversion-universal.pkg
        sudo installer -pkg gstreamer-1.0-$defaultgstversion-universal.pkg -target /
        rm gstreamer-1.0-$defaultgstversion-universal.pkg
        curl -o gstreamer-1.0-devel-$defaultgstversion-universal.pkg https://gstreamer.freedesktop.org/data/pkg/osx/$defaultgstversion/gstreamer-1.0-devel-$defaultgstversion-universal.pkg
        sudo installer -pkg gstreamer-1.0-devel-$defaultgstversion-universal.pkg  -target /
        rm gstreamer-1.0-devel-$defaultgstversion-universal.pkg
        # Gstreamer has a broken .pc prefix that can't be worked around with meson
        grep non_existent_on_purpose /Library/Frameworks/GStreamer.framework/Libraries/pkgconfig/*.pc
        retVal=$?
        if [ $retVal -eq 0 ]; then
            echo "Fixing Gstreamer.framework .pc files."
            sudo sed -i '.bak'  's#prefix=.*#prefix=/Library/Frameworks/GStreamer.framework/Versions/1.0#' /Library/Frameworks/GStreamer.framework/Libraries/pkgconfig/*
        fi
    fi
}

install_subtec() {
    echo "Cloning subtec-app..."
    do_clone "https://code.rdkcentral.com/r/components/generic/subtec-app"

    echo
    echo "Cloning websocket-ipplayer2-utils..."
    do_clone https://code.rdkcentral.com/r/components/generic/websocket-ipplayer2-utils subtec-app/websocket-ipplayer2-utils

    if [ ! -d glib ]; then
        echo "Installing glib..."
        do_clone https://gitlab.gnome.org/GNOME/glib.git -b 2.78.0
    fi
    cd glib
    meson build && cd build
    meson compile
    cd ../../
    
    sed -i '' 's:COMMAND gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code SubtitleDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/SubtitleDbusInterface.xml:COMMAND '"$PWD"'/glib/build/gio/gdbus-2.0/codegen/gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code SubtitleDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/SubtitleDbusInterface.xml:g' subtec-app/subttxrend-dbus/CMakeLists.txt
    
    sed -i '' 's:COMMAND gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code TeletextDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/TeletextDbusInterface.xml:COMMAND '"$PWD"'/glib/build/gio/gdbus-2.0/codegen/gdbus-codegen --interface-prefix com.libertyglobal.rdk --generate-c-code TeletextDbusInterface ${CMAKE_CURRENT_SOURCE_DIR}/api/dbus/TeletextDbusInterface.xml:g' subtec-app/subttxrend-dbus/CMakeLists.txt
    
    echo "************************"
    echo "Subtec-App successfully installed!"
    echo "************************"
}

create_subtec_run_script()
{
    # Create a subtec run script in the build dir
    # This will contain all the paths to the subtec build so wherever aamp-cli is
    # run from it can run subtec
if [[ "$OSTYPE" == "darwin"* ]]; then    
    subtecrunscript=$builddir/build/Debug/aampcli-run-subtec.sh
elif [[ "$OSTYPE" == "linux"* ]]; then
    subtecrunscript=$builddir/build/aampcli-run-subtec.sh
else
    echo "WARNING - unrecognised platform!"
    subtecrunscript=$builddir/aampcli-run-subtec.sh
fi    

    echo '#!/bin/bash' > $subtecrunscript

if [[ "$OSTYPE" == "darwin"* ]]; then
    cat <<AAMPCLI_RUN_SUBTEC >> $subtecrunscript
# Verify the Unix domain socket size settings to support large sidecar subtitle
# files.
SYSCTL_MAXDGRAM=\`sysctl net.local.dgram.maxdgram\`
MIN_MAXDGRAM=102400
if [[ "\${SYSCTL_MAXDGRAM}" =~ net.local.dgram.maxdgram:\ ([0-9]+) ]]
then
    MAXDGRAM=\${BASH_REMATCH[1]}
    if (( MAXDGRAM < MIN_MAXDGRAM ))
    then
        echo "To support the loading of sidecar subtitle files, increase the Unix domain"
        echo "socket size settings. For example:"
        echo "    sudo sysctl net.local.dgram.maxdgram=\$((MIN_MAXDGRAM))"
        echo "    sudo sysctl net.local.dgram.recvspace=\$((MIN_MAXDGRAM * 2))"
    fi
fi

AAMPCLI_RUN_SUBTEC
fi

if [[ "$OSTYPE" == "linux"* ]]; then
    # start a weston window to display subtitles
    echo 'export XDG_RUNTIME_DIR=/tmp/subtec' >> $subtecrunscript
    echo 'mkdir -p /tmp/subtec' >> $subtecrunscript
    echo 'weston &' >> $subtecrunscript
    echo 'sleep 5' >> $subtecrunscript
fi    

    echo 'cd '$builddir'/build/subtec-app' >> $subtecrunscript
    echo 'THIS_DIR=$PWD' >> $subtecrunscript
    echo 'MSP=${MSP:-/tmp/pes_data_main}' >> $subtecrunscript
    echo 'INSTALL_DIR=$PWD/build/install' >> $subtecrunscript
    echo 'LD_LIBRARY_PATH=$INSTALL_DIR/usr/local/lib $INSTALL_DIR/usr/local/bin/subttxrend-app -msp=$MSP -cfp=$THIS_DIR/config.ini' >> $subtecrunscript
}

install_and_build_subtec() {
    pushd $builddir
    echo "Install and build subtec-app..."

    if [[ ! -d "subtec-app" ]]; then
        install_subtec
    fi
    
    if [ ! -d "subtec-app/subttxrend-app/x86_builder/" ]; then
        echo "Subtec-app is not correctly installed."
    else
        echo "Patching subtec-app..."
        git apply OSX/patches/subttxrend-app-packet.patch --directory subtec-app

        cd subtec-app/subttxrend-app/x86_builder/
        PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig:$PKG_CONFIG_PATH ./build.sh fast

        if [ -f ./build/install/usr/local/bin/subttxrend-app ]; then
            if [ ! -f "$builddir/build/subtec-app" ]; then
                ln -s $PWD $builddir/build/subtec-app
            fi
        fi
    fi

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Set size limits on Unix domain sockets to enable the loading of large
        # sidecar subtitle files using the set textTrack <filename> command. These
        # settings are large enough to load the files generated in the directory
        # test/VideoTestStream/text. Repeat this if your Mac is restarted.
        if [ "$(sysctl -n net.local.dgram.maxdgram)" -lt 102400 ]; then
            echo "To support the loading of sidecar subtitle files, increase the Unix domain maxdgram"
            sudo sysctl net.local.dgram.maxdgram=102400
        fi

        if [ "$(sysctl -n net.local.dgram.recvspace)" -lt 204800 ]; then
            echo "To support the loading of sidecar subtitle files, increase the Unix domain recvspace"
            sudo sysctl net.local.dgram.recvspace=204800
        fi
    fi

    popd
}


#main/start


# Parse subtec options
if  [[ $1 = "subtec" ]]; then
    processtorun="subtec"
    shift
    if  [[ $1 = "clean" ]]; then
        subtecoption="clean"
        shift
    fi
fi

echo "Ver=$aamposxinstallerver"

#Optional Command-line support for -b <aamp code branch> and -d <build directory> 
while getopts ":d:b:cf:n" opt; do
  case ${opt} in
    d ) # process option d install base directory name
	builddir=${OPTARG}
	echo "${OPTARG}"
      ;;
    b ) # process option b code branch name
	codebranch=${OPTARG}
      ;;
    c ) # process option c coverage
        COVERAGE=ON
	echo coverage "${COVERAGE}"
      ;;
    f )# process option f to get compiler flags
        buildargs=${OPTARG}
        echo "${OPTARG}"
        ;;
    n )# process option n not to run aamp-cli on MAC
        dontrunaampcli=true
        echo "Skip AAMPCli : ${dontrunaampcli}"
        ;;
    * ) echo "Usage: $0 [subtec [clean]] [-b aamp branch name] [-d local setup directory name] [-f compiler flags] [-n]"
        echo
        echo "Note:  Subtec is built by default but can be rebuilt separately with the subtec option "
        echo "       ('clean' will delete the subtec source and reinstall before building):"
        echo "             ./install-aamp [subtec [clean]] [-b branch] [-d directory]"
        exit
      ;;
  esac
done

if [[ "$buildargs" == "" ]]; then
    echo "No additional build configs specified"
else
    #add flags to build to cmake by splitting buildargs with separator ','
    echo "Additional build configs specified"
    echo "${buildargs}"
fi

#Check and if needed setup default aamp code branch name and local environment directory name
if [[ $codebranch == "" ]]; then
	codebranch=${defaultcodebranch}
	echo "using default code branch: $defaultcodebranch"	
fi 

if [[ "$builddir" == "" ]]; then
    builddir=$defaultbuilddir
    if [ -d "../aamp" ]; then
        abs_path="$(cd "../aamp" && pwd -P)"
        while true; do
        read -p '[!Alert!] Install script identified that the aamp folder already exists @ ../aamp.
        Press Y, if you want to use same aamp folder (../aamp) for your simulator build.
        Press N, If you want to use separate build folder for aamp simulator. Press (Y/N)'  yn
                case $yn in
                     [Yy]* ) builddir=$abs_path; echo "using following aamp build directory $builddir"; break;;
                     [Nn]* ) builddir=${defaultbuilddir} ; echo "using following aamp build directory $PWD/$builddir"; break ;;
                     * ) echo "Please answer yes or no.";;
                esac
        done
    fi
fi

if [[ "$OSTYPE" == "darwin"* ]]; then

    #Check/Install brew
    which -s brew
    if [[ $? != 0 ]] ; then
        echo "Installing Homebrew, as its not available"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo "Homebrew is installed now just updating it"
        brew update
    fi
    
    brew install git
    brew install cmake

elif [[ "$OSTYPE" == "linux"* ]]; then
    sudo apt install git cmake
fi

if [[ ! -d "$builddir" ]]; then
    echo "Creating aamp build directory under $builddir";
    mkdir $builddir
    cd $builddir
    
    do_clone -b $codebranch https://code.rdkcentral.com/r/rdk/components/generic/aamp
    cd aamp
else
    cd $builddir
fi

builddir=$PWD
echo "Builddir: $builddir"
echo "Code Branch: $codebranch"


#
# Check for subtec process commands
#
if  [[ $processtorun = "subtec" ]]; then
    if  [[ $subtecoption = "clean" ]]; then
        echo "Deleting subtec-app ..."
        rm -rf subtec-app
        rm -rf glib
    fi
    
    install_and_build_subtec
    exit 0
fi



#
# Install aamp
#

#build all aamp supporting packages under lib folder 
mkdir -p build


#Check OS=macOS
if [[ "$OSTYPE" == "darwin"* ]]; then

    echo $OSTYPE

    #Check if Xcode is installed
    if xpath=$( xcode-select --print-path ) &&
      test -d "${xpath}" && test -x "${xpath}" ; then
      echo "Xcode already installed. Skipping."

    else
      echo "Installing Xcode…"
      xcode-select --install
      if xpath=$( xcode-select --print-path ) &&
            test -d "${xpath}" && test -x "${xpath}" ; then
            echo "Xcode installed now."
       else
            #... isn't correctly installed
            echo "Xcode installation not detected. Exiting $0 script."
            echo "Xcode installation is mandatory to use this AAMP installation automation script."
            echo "please check your app store for Xcode or check at https://developer.apple.com/download/"
            exit 0
       
       fi
    fi




    #cleanup old libs builds
    if [ -d "./.libs" ]; then
	    sudo rm -rf ./.libs
    fi
    
    mkdir .libs
    cd .libs
	
    install_system_packages
     
    #Build aamp dependent modules
    echo "git clone and install aamp dependencies"
	
    echo "Fetch,aamp custom patch(qtdemux),build and install gst-plugins-good-$defaultgstversion.tar.xz ..." 
    pwd
    curl -o gst-plugins-good-$defaultgstversion.tar.xz https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-$defaultgstversion.tar.xz
    tar -xvzf gst-plugins-good-$defaultgstversion.tar.xz
    cd gst-plugins-good-$defaultgstversion
    pwd
    patch -p1 < ../../OSx/patches/0009-qtdemux-aamp-tm_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0013-qtdemux-remove-override-segment-event_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0014-qtdemux-clear-crypto-info-on-trak-switch_gst-1.16.patch
    patch -p1 < ../../OSx/patches/0021-qtdemux-aamp-tm-multiperiod_gst-1.16.patch
    sed -in 's/gstglproto_dep\x27], required: true/gstglproto_dep\x27], required: false/g' meson.build

    echo "Building gst-plugins-good with --pkg-config path $installed_pkgconfig..."
    meson --pkg-config-path=$installed_pkgconfig build 
    ninja -C build
    ninja -C build install

    # ARM vs x86 have different installation directories
    if [ -d /usr/local/lib/gstreamer-1.0 ]; then
        sudo cp  /usr/local/lib/gstreamer-1.0/libgstisomp4.dylib /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0/libgstisomp4.dylib
    else
        sudo cp  /opt/homebrew/lib/gstreamer-1.0/libgstisomp4.dylib /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0/libgstisomp4.dylib
    fi
    retVal=$?
    if [ $retVal -ne 0 ]; then
        echo "Error gst-plugins-good build failed."
        exit
    fi
    pwd
    cd ../

    echo "Checking for libdash installation"
    pkg-config --exists $defaultlibdashversion
    if [ $? != 0 ]; then
        # This cleanup is a NOP, but useful for reference
        sudo rm -rf /usr/local/include/libdash
        sudo rm -f /usr/local/lib/pkgconfig/libdash.pc
        mkdir temp
        cp ../install_libdash.sh ./temp
        cd temp

        sudo bash ./install_libdash.sh
        if [ $? != 0 ]; then
            echo "libdash installation FAILED"
            exit 0
        fi
        cd ..
    else
        echo "$defaultlibdashversion is already installed."
    fi

    pwd
    do_clone_rdk_repo $codebranch aampabr
    do_clone_rdk_repo $codebranch gst-plugins-rdk-aamp
    do_clone_rdk_repo $codebranch aampmetrics

    #Build aamp components
    echo "Building following aamp components"
 	 
    #Build aampabr
    echo "Building aampabr..."
    pwd
    cd aampabr
    mkdir -p build
    cd build
    cmake ..
    make
    sudo make install
    cd ../..

    #Build aampmetrics
    echo "Building aampmetrics..."
    cd aampmetrics
    mkdir -p build
    cd build
    cmake .. 
    make
    sudo make install
    cd ../..

    cd ../
    
    echo "Installing packages..."
    brew install coreutils
    brew install websocketpp
    brew install boost
    brew install jansson
    brew install libxkbcommon
    brew install cppunit
    brew install gnu-sed
    
    brew install fontconfig
    brew install doxygen
    brew install graphviz
    
    #Build aamp-cli
    echo "Build aamp-cli"
    pwd
    
    #clean existing build folder if exists
    if [ -d "./build" ]; then
       sudo rm -rf ./build
    fi

    mkdir -p build
    # allow XCode to clean build folder
    xattr -w com.apple.xcode.CreatedByBuildSystem true build
   
    # Would be nice to use $installed_pkfconfig here, but it results in link error, not finding libapp-1.0
    cd build && PKG_CONFIG_PATH=/usr/local/opt/libffi/lib/pkgconfig:/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/ssl/lib/pkgconfig:/usr/local/opt/curl/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CUSTOM_QTDEMUX_PLUGIN_ENABLED=TRUE -DCOVERAGE_ENABLED=${COVERAGE} -DSMOKETEST_ENABLED=ON -DUTEST_ENABLED=ON -G Xcode ../

    # the cmake Xcode generator can not set this scheme property (Debug -> Options -> Console -> Use Terminal
    patch ./AAMP.xcodeproj/xcshareddata/xcschemes/aamp-cli.xcscheme < ../OSX/patches/aamp-cli.xscheme.patch 

    echo "Please Start XCode, open aamp/build/AAMP.xcodeproj project file"
	
    ##Create default channel ~/aampcli.csv – supports local configuration overrides
    echo "If not present, Create $HOME/aampcli.csv to suport virtual channel list of test assets that could be loaded in aamp-cli"

    if [ -f "$defaultchannellistfile" ]; then
    	echo "$defaultchannellistfile exists."
    else 
    	echo "$defaultchannellistfile does not exist. adding default test version"
	cp ../OSX/aampcli.csv $defaultchannellistfile
    fi	

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

    (sleep 20 ; open AAMP.xcodeproj) &
    
    echo "Now Building aamp-cli" 
    xcodebuild -scheme aamp-cli  build

    if [  -f "./Debug/aamp-cli" ]; then 
        echo "OSX AAMP Build PASSED"
        arr_install_status+=("OSX AAMP Build PASSED")
        
        create_subtec_run_script	# after build/Debug directory created by xcodebuild
    else
        echo "OSX AAMP Build FAILED"
        arr_install_status+=("OSX AAMP Build FAILED")
	fi
   
    echo ""   
    echo "********AAMP install summary start************"

    for item in "${!arr_install_status[@]}"; 
    do
        printf "$item ${arr_install_status[$item]} \n"
    done   
    echo ""
    echo "********AAMP install summary end*************"

    echo "Installing subtec..."
    install_and_build_subtec

    if [ $dontrunaampcli = false ];then
        #Launching aamp-cli

        otool -L ./Debug/aamp-cli

        ./Debug/aamp-cli https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

    fi

    exit 0

    #print final status of the script
elif [[ "$OSTYPE" == "linux"* ]]; then
    
    cd Linux

    #cat ../../../aampmetrics.patch > patches/aampmetrics.patch
    source install-linux-deps.sh
    source install-linux.sh -b $codebranch -g $googletestreference

    cd ../
    echo "Building aamp-cli..."
    if [ -d "./build" ]; then
       sudo rm -rf ./build
    fi

    mkdir -p build
    create_subtec_run_script
    
    PKG_CONFIG_PATH=$PWD/Linux/lib/pkgconfig /usr/bin/cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/Linux -DCMAKE_PLATFORM_UBUNTU=1 -DCMAKE_LIBRARY_PATH=$PWD/Linux/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DSMOKETEST_ENABLED=ON -DCOVERAGE_ENABLED=${COVERAGE} -DUTEST_ENABLED=ON -DCMAKE_CUSTOM_QTDEMUX_PLUGIN_ENABLED=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S$PWD -B$PWD/build -G "Unix Makefiles"
    
    echo "Making aamp-cli..."
    cd build
    make
    sudo make install

    
    if [  -f "./aamp-cli" ]; then
        echo "****Linux AAMP Build PASSED****"
        ldd ./aamp-cli
        arr_install_status+=("Linux AAMP Build PASSED")
        
        echo "Installing subtec..."
        install_and_build_subtec
        
        echo "Installing VSCode..."
        sudo snap install --classic code
	    
        echo "Installing VSCode Dependencies..."
        code --install-extension ms-vscode.cmake-tools
		
        echo "Openning VSCode Workspace..."
        code ../ubuntu-aamp-cli.code-workspace
    else
        echo "****Linux AAMP Build FAILED****"
        arr_install_status+=("Linux AAMP Build FAILED")
    fi
    
    
else

        #abort the script if its not macOS
        echo "Aborting unsupported OS detected"
        echo $OSTYPE
        exit 1
fi
