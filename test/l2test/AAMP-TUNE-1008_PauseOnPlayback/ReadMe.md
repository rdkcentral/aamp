# AAMP Pause On Playback L2 test

This python3 L2 test verifies Pause On Playback functionality.
In particular it was introduced to verify the following feature:

<CPESP-4677> Cloud Review buffer support for DTT Channels with IP Switchover

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_test.py -t 1008