# AAMP Multi profile test

This python3 L2 test verifies audio bitrates the available audio bitrates in the stream 

<RDKAAMP-2244> L2 Test script generation: Test getAudioBitrates

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the below stream to get the audio bitrates

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/foxtel-10/single-period.mpd



## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 13002 --aamp_video
