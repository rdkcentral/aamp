# AAMP Multi profile test

This python3 L2 test verifies UriParameter configuration

<RDKAAMP-2445> L2 Test script generation: Test UriParameter

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the below stream to get the UriParameter info

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd


## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    python run_l2_aamp.py -v -t 2015 
