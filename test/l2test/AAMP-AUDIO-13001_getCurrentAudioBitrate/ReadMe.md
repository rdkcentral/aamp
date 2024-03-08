# AAMP Multi profile test

This python3 L2 test verifies audio bitrate of the current playback

<RDKAAMP-2245> L2 Test script generation: Test getCurrentAudioBitrate

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8
https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd



## Run l2test using script:


## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 13001 --aamp_video
