#!/bin/bash
set -x

mkdir -p /aamp/test/l2test/testdata

if [ "$TEST_3000_STREAM_PATH" == "" ]; then
    TEST_3000_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/aamptest/streams/simlinear/streams09-Jan2023.tar.gz"
fi

if (curl -O  -X GET $TEST_3000_STREAM_PATH);then
    echo "Test referring :" $TEST_3000_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_3000_STREAM_PATH' can be reached.
    If the URL is incorrect, set the environmental variable TEST_3000_STREAM_PATH to the right value in .env file under l2test folder"
fi

for x in $(pwd)/*.tar.gz
do
  echo "extracting $x"
  tar -xvf "$x" -C "/aamp/test/l2test/testdata"
done

