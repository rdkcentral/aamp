# AAMP Multi profile test

This python3 L2 test verifies Rewind functionality with different speed

<RDKAAMP-2586> L2 Test script generation: Test Rewind functionality

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the below stream to get the Rewind functionality info

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd


## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    python run_l2_aamp.py -v -t 4006 
