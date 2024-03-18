# AAMP Multi profile test

This python3 L2 test verifies audio bitrate of the current playback with different configurations

<RDKAAMP-1654> L2 Test script generation: Test GetPlaybackPosition

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

https://livesim.dashif.org/livesim/segtimeline_1/testpic_2s/Manifest.mpd


## Run l2test using script:


## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 1005 --aamp_video
