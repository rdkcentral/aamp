# AAMP TSB Session Manager tests
This set of l2 tests aims to verify basic functionality of newly added TSBSessionManager created as part of RDK-48051 User Story.



## Test Files
+ test_5002.py  Iterates through test cases to verify various functionalities related to TSB Session Manager and basic functionalities of Data Manager


## Pre-requisites to run this test

1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```


## Run l2test using script:

Expect around 80 seconds for test to complete./run_l2_aamp.py -t 5002

From the *test/l2test/ folder run:
```
./run_l2_aamp.py -t 5002
```

## Details
* TESTDATA0 ------> Test Session Manager initialization with config true
* TESTDATA1 ------> Test Session Manager not initialized with config false
* TESTDATA2 ------> Test CullSegments() is working to cull old segments
* TESTDATA3 ------> Test TSB Data Manager basic logs
* TESTDATA4 ------> Test for TSB Store logs
* TESTDATA5 ------> Test the read API
* TESTDATA6 ------> Test writing to TSB
* TESTDATA7 ------> Test seek
* TESTDATA8 ------> Test pause on live
* TESTDATA9 ------> Test pause and resume
* TESTDATA10 -----> Test trick modes (rewind and fast forward)
* TESTDATA11 -----> Test pause and trick modes
* TESTDATA12 -----> Test AAMP Local TSB with VoD content
