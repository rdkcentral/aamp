# AAMP Multi profile test

This python3 L2 test verifies "AVAILABLE TEXT TRACKS" logline with Type field  is  present 

<RDKAAMP-3255> L1 and/or LE test for DELIA-66159 Type field is not populated for Inband CC

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/InbandCCHarvest/vod2-s8-prd.top.xs.xumo.com/v2/bmff/cenc/t/ipvod20/UPTV0000000008373881/movie/1698194067914/manifest.mpd

and verifies "AVAILBALE TEXT TRACKS' logline with Type field  is  present  as per the fix of DELIA-66159

## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 1011
