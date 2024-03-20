#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

if [ "$TEST_1004_STREAM_PATH" == "" ]; then
    TEST_1004_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
fi

#Stop multiple fetching of data if we already have it
file=$(basename ${TEST_1004_STREAM_PATH})
if [ -f $file ]; then
    echo "$file exists, not fetching again"
    exit 0
fi

echo "TEST_1004_STREAM_PATH=$TEST_1004_STREAM_PATH"

if (curl -O -L -C - $TEST_1004_STREAM_PATH);then
    echo "Test referring:" $TEST_1004_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_1004_STREAM_PATH' can be reached.
    If the URL is incorrect, set the environmental variable TEST_1004_STREAM_PATH to the right value in .env file under l2test folder"
fi

for x in $(pwd)/*.{tar.gz,tgz}
do
  if [[ -f $x ]]; then
    echo "extracting $x"
    tar -xf "$x" -C "$(pwd)/testdata"
  fi
done

for x in $(pwd)/*.zip
do
  if [[ -f $x ]]; then
    echo "extracting $x"
    unzip -u "$x" -d "$(pwd)/testdata"
  fi
done
