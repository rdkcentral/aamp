# AAMP Multi profile test

This python3 L2 test verifies that the thumbnails are correctly extracted from the test assets

<RDK-2262> L2 Test script generation : Test getAvailableThumbnailTracks

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

1 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x16/cmaf/mpeg_2sec/master_cmaf.m3u8
2 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x10/cmaf/mpeg_2sec/master_cmaf.m3u8
3 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x6/cmaf/mpeg_2sec/master_cmaf.m3u8
4 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x16/cmaf/mpeg_6sec/master_cmaf.m3u8
5 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x10/cmaf/mpeg_6sec/master_cmaf.m3u8
6 # https://g004-vod-us-cmaf-stg-ak.cdn.peacocktv.com/pub/global/mPX/ylU/PCK_1606787164542_01_thumb_5x6/cmaf/mpeg_6sec/master_cmaf.m3u8


and verifies that all video profiles are played correctly for 30s each.

## Run l2test using script:

From the *test/l2test* folder run:

./run_test.py -t 4002

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_test.py -t 4002
