#!/bin/bash

function linux_install_packages() {
    sudo apt-get update
    sudo apt-get --no-install-recommends install -y $@
}

function brew_install_packages() {
    brew update --auto-update
    brew install $@
}

echo "Installing required packages"
if [[ "$OSTYPE" == "darwin"* ]]; then
    if [[ $(command -v brew) == "" ]] ; then
        echo "Installing Homebrew, as its not available"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        echo "Installed Homebrew, run the commands to add it to your environment path, then re-run this script."
        exit 0
    else
        echo "Homebrew is installed now just updating it"
    fi

    packages="curl wget"
    brew_install_packages $packages

    python3 -m ensurepip
    python3 -m pip install --upgrade pip
    python3 -m pip install virtualenv

else
    packages="python3-pip python3-venv gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly curl wget unzip"
    linux_install_packages $packages
fi

# log python3 info
which python3
python3 --version
python3 -m pip -V
which pip3

# check pip3 in path looks right
if [[ $(python3 -m pip -V) != $(pip3 -V) ]]; then
    pip3 -V
    echo ""
    echo "pip3 in path doesn't seem to be using the same installation/version as python3"
    echo "fix or use \"python3 -m pip\" instead of pip3 when installing packages"
fi

# linux
if [[ "$OSTYPE" != "darwin"* ]]; then
    # ensure $HOME/.local/bin exists for python pip user installed packages and is in PATH
    if [ "$EUID" -ne 0 ]; then
        if [ ! -d "$HOME/.local/bin" ]; then
            mkdir -p "$HOME/.local/bin"
            if [ ! -d "$HOME/.local/bin" ]; then
                echo "Error $HOME/.local/bin dir not present."
            fi
        fi

        if [ ! $(echo "$PATH" | grep "$HOME/.local/bin") ]; then
            echo ""
            echo "Add dir to the PATH, with \"export PATH=\$HOME/.local/bin:\$PATH\""
            echo "Or logout and back in to get \$HOME/.local/bin added to PATH"
            echo "then re-run this script."
            exit 0
        fi
    fi
fi

# check if a l2venv is already in use and if not create it
if [[ "$VIRTUAL_ENV" != "$(pwd)/l2venv" ]]; then
    python3 -m venv l2venv
    echo ""
    echo "Created a Python virtual environment \"l2venv\""
    echo "Activate venv with \"source l2venv/bin/activate\"."
    echo "venv can be deactivated with the \"deactivate\" command."
fi

