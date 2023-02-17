#!/bin/bash

echo "Installing pip3 and pipreqs"
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Homebrew is installed now just updating it"
    if [[ $(command -v brew) == "" ]] ; then
        echo "Installing Homebrew, as its not available"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        echo "Homebrew is installed now just updating it"
        brew update
    fi
    brew install python3-pip
    pip3 install pipreqs
else
    sudo apt-get install python3-pip
    sudo pip3 install pipreqs
fi