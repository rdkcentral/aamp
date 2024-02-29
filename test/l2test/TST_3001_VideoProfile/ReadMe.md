# AAMP Multi profile test

This python3 L2 test verifies that all video profiles in an asset can be played successfully

<RDK-2099> L2 Test script generation: Test all video profiles in stream

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8
https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

and verifies that all video profiles are played correctly for 30s each.

## Run l2test using script:

From the *test/l2test/TST_3001_MultiVideoProfile* folder run:

./run_test.py

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/TST_3001_MultiVideoProfile
    ./run_test.py
