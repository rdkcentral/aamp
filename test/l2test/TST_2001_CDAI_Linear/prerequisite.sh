#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

if [ "$TEST_2001_STREAM_PATH" == "" ]; then
    TEST_2001_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
fi

#Stop multiple fetching of data if we already have it
file=$(basename ${TEST_2001_STREAM_PATH})
if [ -f $file ]; then
    echo "$file exists, not fetching again"
    exit 0
fi

if [ "$TEST_2001_ADS_PATH" == "" ]; then
    TEST_2001_ADS_PATH="https://cpetestutility.stb.r53.xcal.tv"
fi

echo "TEST_2001_STREAM_PATH=$TEST_2001_STREAM_PATH"
echo "TEST_2001_ADS_PATH=$TEST_2001_ADS_PATH"
echo "TEST_2001_LOCAL_ADS=$TEST_2001_LOCAL_ADS"

if (curl -O -L -C - $TEST_2001_STREAM_PATH);then
    echo "Test referring:" $TEST_2001_STREAM_PATH
else
    echo "Downloading the stream failed. Check that the URL '$TEST_2001_STREAM_PATH' can be reached.
    If the URL is incorrect, set the environmental variable TEST_2001_STREAM_PATH to the right value in .env file under l2test folder"
fi


if [ "$TEST_2001_LOCAL_ADS" == "true" ]; then
    echo "Downloading Ad1..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad2..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad3..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad4..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad5..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad5/hsar1195-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad2/d14dff37-36d1-4850-aa9d-7d948cbf1fc6/1628318436178/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad6..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad6/hsar1103-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD" 2>&1 | grep -i "failed\|error"
    echo "Downloading Ad7..."
    wget -P $(pwd)/httpdata -r -np -nH -nc -R "index.html*" -e robots=off "$TEST_2001_ADS_PATH/aamptest/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD" 2>&1 | grep -i "failed\|error"
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
