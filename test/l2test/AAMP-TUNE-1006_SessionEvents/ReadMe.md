# AAMP Multi profile test

This python3 L2 test verifies that the newly added SessionID field is correctly added to the Events emitted.

<RDK-2265> L2 Test script generation : Test SessionID

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

1 # https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated//main.m3u8
2 # https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated//main.mpd
3 # https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated//main_mp4.m3u8

and verifies that SessionID field is correctly added to the Events emitted.

## Run l2test using script:

From the *test/l2test* folder run:

./run_test.py -t 1006

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_test.py -t 1006
