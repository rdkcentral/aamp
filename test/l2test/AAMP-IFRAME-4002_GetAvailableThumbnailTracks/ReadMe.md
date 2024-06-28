# AAMP Multi profile test

This python3 L2 test verifies that the thumbnails are correctly extracted from the test assets

<RDK-2262> L2 Test script generation : Test getAvailableThumbnailTracks

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

1 # https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock1/mpeg_2sec/manifest.m3u8
2 # https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock2/mpeg_2sec/manifest.m3u8

and verifies that all video profiles are played correctly for 30s each.

## Run l2test using script:

From the *test/l2test* folder run:

./run_l2_aamp.py -t 4002

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 4002
