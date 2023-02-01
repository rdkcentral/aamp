# AAMP WebVTT L2 test

This python3 L2 test verifies WebVTT functionality. In particular it was
introduced to verify the following feature:
<RDK-37719> [UVE] support ability to override WebVTT caption styling

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following stream:
https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

If that stream is not available, the test will fail at the beginning with a
timeout waiting for the event AAMP_EVENT_TUNED. Any other stream with no
subtitles can be used to run this test.

## Run l2test using script:

From the *test/l2test/webvtt* folder run:

./run_test.py

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/webvtt
    ./run_test.py
