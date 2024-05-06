#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

if [ "$TEST_0001_STREAM_PATH" == "" ]; then
    TEST_0001_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/aamptest/streams/simlinear/SkyWitness/30t-after-fix/skywitness-30t-after-fix.zip"
fi

#Stop multiple fetching of data if we already have it
file=$(basename ${TEST_0001_STREAM_PATH})
if [ -f $file ]; then
    echo "$file exists, not fetching again"
    exit 0
fi
if (curl -O  -X GET $TEST_0001_STREAM_PATH);then
    echo "Test referring :" $TEST_0001_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_0001_STREAM_PATH' can be reached.
    If the URL is incorrect, set the environmental variable TEST_0001_STREAM_PATH to the right value in .env file under l2test folder"
fi

for x in $(pwd)/*.zip
do
  echo "extracting $x"
  unzip "$x" -d "$(pwd)/testdata"
done

