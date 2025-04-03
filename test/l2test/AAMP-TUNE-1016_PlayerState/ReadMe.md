
# AAMP L2  test
<p> Test used to exercise changes in player state</p>

## Pre-requisites to run this test

1. Depends on hosted stream availability:
    c. https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

2.  AAMP installed using install-aamp.sh script

## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_aamp.py -t 1016

## Details
* By default aamp-cli runs without a video window to output A/V so it can be run via Jenkins

* When running manually from a terminal then A/V output might be useful. This can be achieved by "run_l2_aamp.py -v"
