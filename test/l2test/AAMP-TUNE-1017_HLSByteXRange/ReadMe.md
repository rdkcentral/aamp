
# AAMP L2  test
<p> Test used to validate HLS #EXT-X-BYTERANGE parsed incorrectly </p>

    RDKAAMP-4220 HLS #EXT-X-BYTERANGE parsed incorrectly 

## Pre-requisites to run this test

1. Depends on hosted stream availability:
    c. https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/byterange-test-stream.zip

2.  AAMP installed using install-aamp.sh script

## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_aamp.py -t 1017


