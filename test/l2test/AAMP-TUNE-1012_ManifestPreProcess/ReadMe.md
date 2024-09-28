# AAMP preprocessed Manifest updates test

This python3 L2 test verifies the Dash playback with prefetched/updated manifest data
Manifest PreProcessing support is only available for dash streams, Hence this test is valid only for dash.

<RDKAAMP-2827> Implement L2 test for RDKAAMP-2671 [Manifest call back for pre-processing]

## Pre-requisites to L2 tests:

Install AAMP using install-aamp.sh script.

This test plays the following streams:
"https://d24rwxnt7vw9qb.cloudfront.net/v1/dash/e6d234965645b411ad572802b6c9d5a10799c9c1/All_Reference_Streams/6ba06d17f65b4e1cbd1238eaa05c02c1/index.mpd"
"https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel-tiled-thumbnails-numbered.ism/.mpd"

## Run l2test using script:
## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    ./run_l2_aamp.py -t 1012
