# AAMP TSB Session Manager tests
L2 tests to verify AAMP Local TSB configuration and common code for LLD and SLD content.

## Test Files
+ test_5004.py  Iterates through test cases to verify AAMP Local TSB configuration and basic functionality

## Pre-requisites to run this test
1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```

## Run l2test using script:
From the *test/l2test/* folder run:
```
./run_l2_aamp.py -t 5004
```

## Details
* TESTDATA0 ------> Test AAMP Local TSB initialization with config true
* TESTDATA1 ------> Test AAMP Local TSB not initialized with config false
* TESTDATA2 ------> Test AAMP Local TSB not initialized when PTS restamping is disabled
* TESTDATA3 ------> Test CullSegments() is working to cull old segments
* TESTDATA4 ------> Test TSB Data Manager basic logs
* TESTDATA5 ------> Test for TSB Store logs
* TESTDATA6 ------> Test the read API
* TESTDATA7 ------> Test writing to AAMP Local TSB
* TESTDATA8 ------> Test AAMP Local TSB not initialized for VoD content
