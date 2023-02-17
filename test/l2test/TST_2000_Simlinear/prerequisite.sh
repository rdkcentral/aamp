#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

if [ -z $(which curl) ]
then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        brew install curl
    else
        sudo apt-get update 
        sudo apt install -y curl
    fi
else
    echo "Curl already installed"
fi

if (curl -O  -X GET $TEST_STREAM_PATH);then
    echo "Test referring :" $TEST_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_STREAM_PATH' can be reached. 
    If the URL is incorrect, set the environmental variable TEST_STREAM_PATH to the right value in .env file under l2test folder"
fi

for x in $(pwd)/*.tar.gz
do
  echo "extracting $x"
  tar -xvf "$x" -C "$(pwd)/testdata"
done

