# AAMP Multi profile test

This python3 L2 test verifies disable4K

<RDKAAMP-2440> L2 Test script generation: Test Disable4K

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the below stream disabling the 4k video to non4k

https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd


## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2009