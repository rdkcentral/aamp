# AAMP VOD CDAI Single Pipeline L2 test

This python3 L2 test verifies Httprequestend prints correctly prints the current video bitrate in vod and linear content.
In particular it was introduced to verify the following feature:

RDKAAMP-3194: [AAMP] Incorrect HttpRequestEnd bitrate debug

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.


## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_test.py -t 1010
