# AAMP Multi profile test

This python3 L2 test verifies initialBitrate4k

<RDKAAMP-2436> L2 Test script generation: Test initialBitrate4k

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the below stream to get the initialBitrate4k info

https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd


## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    python run_l2_aamp.py -v -t 2008 
