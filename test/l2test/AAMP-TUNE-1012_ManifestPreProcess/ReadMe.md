# AAMP preprocessed Manifest updates test

This python3 L2 test verifies the Dash playback with prefetched/updated manifest data
Manifest PreProcessing support is only available for dash streams, Hence this test is valid only for dash.

<RDKAAMP-2827> Implement L2 test for RDKAAMP-2671 [Manifest call back for pre-processing]

## Test Files
simlinear.py  Webserver for serving manifest data.

run_test.py  Causes aamp-cli to play manifest and check logs for tuning related info.
               Checks log messages output from aamp are as expected. For the test gives PASS/FAIL result.

## Pre-requisites to L2 tests:

Install AAMP using install-aamp.sh script.

This test plays the following streams:
1 .Stream for this test will be downloaded from https://cpetestutility.stb.r53.xcal.tv
2. https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/foxtel-10/single-period.mpd

Archive file containing manifest data has been downloaded ( URL: from location where the file is stored )
   and extracted into directory 'testdata'

## Run l2test using script:
## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 1012
