#!/bin/bash

version() { echo "$@" | awk -F. '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }'; }

# audio generation requires following installs:
#Support functions
mac_find_or_install_pkgs() {
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
    done
}

linux_find_or_install_pkgs() {

        for pkg in "$@";
        do
            if which $pkg > /dev/null; then
                echo "${pkg} already installed."
            else
                echo "Installing ${pkg}"
                sudo apt -y install $pkg
                if which $pkg > /dev/null; then
                        echo "${pkg} successfully installed."
                else
                        echo "FYI:${pkg} install failed!"
                fi
            fi

        done

}

#check and install basic packages
if [[ "$OSTYPE" == "darwin"* ]]; then
    mac_find_or_install_pkgs python ffmpeg
elif [[ "$OSTYPE" == "linux"* ]]; then
    linux_find_or_install_pkgs python3 python3-pip ffmpeg
fi

# pip3 install gtts
gtts_version="1.0"
if which gtts-cli > /dev/null; then
   echo "gtts already installed, checking version."
   gtts_version=`gtts-cli --version | cut -d " " -f 3`
else
   echo "Installing gtts, as its not available"
fi

if [ $(version $gtts_version) -ge $(version "2.3.2") ]; then
    echo "gtts $gtts_version is up to date"
else
    if [[ "$OSTYPE" == "darwin"* ]]; then
       pip3 install "gtts >=2.3.2" --break-system-packages # Required for system wide download
    elif [[ "$OSTYPE" == "linux"* ]]; then
       sudo pip3 install "gtts>=2.3.2"
    fi

   gtts_version=`gtts-cli --version | cut -d " " -f 3`
   if [ $(version $gtts_version) -ge $(version "2.3.2") ]; then
      echo "gtts successfully installed."
   else
      echo "gtts install failed check your \$PATH variable!"
   fi
fi

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    export LC_ALL=C.UTF-8
    export LANG=C.UTF-8
fi
