# AAMP TSB Session Manager tests
L2 tests to verify AAMP Local TSB for SLD content.

## Test Files
+ test_5002.py  Iterates through test cases to verify AAMP Local TSB behaviour for SLD (Standard Latency DASH) content

## Pre-requisites to run this test
1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```

## Run l2test using script:
From the *test/l2test/* folder run:
```
./run_l2_aamp.py -t 5002
```

## Details
* TESTDATA0 -----> Test seek
* TESTDATA1 -----> Test pause on live
* TESTDATA2 -----> Test pause and resume
* TESTDATA3 -----> Test trick modes (rewind and fast forward)
* TESTDATA4 -----> Test pause and trick modes
