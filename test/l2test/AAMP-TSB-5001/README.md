# AAMP TSB Session Manager tests
This set of l2 tests aims to verufy basic functionality of newly added TSBSessionManager created as part of RDK-48051 User Story.



## Test Files
+ test_5001.py  Iterates through test cases to verify various functionalities related to TSB Session Manager and basic functionalities of Data Manager


## Pre-requisites to run this test

1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```


## Run l2test using script:

Expect around 80 seconds for test to complete./run_l2_aamp.py -t 5001

From the *test/l2test/ folder run:
```
./run_l2_aamp.py -t 5001
```

## Details
* TESTDATA0 ------> Test Session Manager initialization with config true
* TESTDATA1 ------> Test Session Manager not initialized with config false
* TESTDATA2 ------> Test AAMP Local TSB with PTS restamping disabled
* TESTDATA3 ------> Test CullSegments() is working to cull old fragments
* TESTDATA4 ------> Test TSB Data Manager basic logs
* TESTDATA5 ------> Test for TSB Store logs
* TESTDATA6 ------> Test if Playback starts with chunked word passed along with Non LLD URL
* TESTDATA7 ------> Test File Read logs are coming after seek back to test Read API
* TESTDATA8 ------> Test writing to TSB
* TESTDATA9 ------> Test seek
* TESTDATA10 -----> Test pause on live
* TESTDATA11 -----> Test pause and resume
* TESTDATA12 -----> Test trick modes (rewind and fast forward)
* TESTDATA13 -----> Test pause and trick modes
