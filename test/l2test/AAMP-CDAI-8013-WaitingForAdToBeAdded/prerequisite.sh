#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

if [ "$TEST_8013_STREAM_PATH" == "" ]; then
    TEST_8013_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
fi

#Stop multiple fetching of data if we already have it
file=$(basename ${TEST_8013_STREAM_PATH})
if [ -f $file ]; then
    echo "$file exists, not fetching again"
    exit 0
fi

if [ "$TEST_8013_ADS_PATH" == "" ]; then
    TEST_8013_ADS_PATH="https://cpetestutility.stb.r53.xcal.tv"
fi

echo "TEST_8013_STREAM_PATH=$TEST_8013_STREAM_PATH"
echo "TEST_8013_ADS_PATH=$TEST_8013_ADS_PATH"
echo "TEST_8013_LOCAL_ADS=$TEST_8013_LOCAL_ADS"

if (curl -O -L -C - $TEST_8013_STREAM_PATH);then
    echo "Test referring:" $TEST_8013_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_8013_STREAM_PATH' can be reached.
    If the URL is incorrect, set the environmental variable TEST_8013_STREAM_PATH to the right value in .env file under l2test folder"
fi


if [ "$TEST_8013_LOCAL_ADS" == "true" ]; then
    echo "Downloading Ad..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_8013_ADS_PATH/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD" 2>&1 | grep -i "failed\|error"
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
