# AAMP Multi codec test

This python3 L2 test verifies codec change in audio does not cause looping issue when pts restamping enabled



## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the below stream to test multi codec playback.

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/multi-period/multi-audio-codec/codec.mpd

This stream was custom made with audio codec changing after each period. It switches back and forth from AAC to EC3.



## Run l2test using script:

```
cd aamp/test/l2test/
source l2venv/bin/activate
./run_l2_aamp.py -t 13005
```
