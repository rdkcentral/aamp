# AAMP Multi profile test

This python3 L2 test verifies "Skipping Fetchfragment" logline is not present and EOS happens at 26 s of playback

<DELIA-65821> L2 Test - Testcase to cover DELIA-65057 [Charter] [Field] [QS025] intermittent freezes on live /VOD content

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/DELIA-65821_Multiperiod/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd

and verifies "Skipping Fetchfragment" logline is not present and EOS happens at 26 s of playback as per the fix of DELIA-65057

## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 1007 --aamp-video
