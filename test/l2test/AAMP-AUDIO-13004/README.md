# AAMP Multi profile test

This python3 L2 test verifies validate getAvailableAudioTracks() for Audio track.

<RDKAAMP-2391> L2 Test script generation: validate getAvailableAudioTracks() functionality

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd



## Run l2test using script:


## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 13004 --aamp_video
