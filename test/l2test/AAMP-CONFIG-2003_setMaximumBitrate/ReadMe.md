# AAMP Multi Profile test

This python3 L2 test verifies that MaximumBitrate config is working successfully

<RDK-2393> L2 Test - Test case to validate SetMaximumBitrate() API

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8
https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

and verifies that proper bitrate profiles are played correctly for 20s each.

## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 2003
