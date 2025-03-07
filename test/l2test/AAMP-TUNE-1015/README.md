## AAMP L2 LLD Tuning tests
+ This set of l2 tests aims to verify basic functionality of LLD streaming


## Test Files
+ test_1015.py  Iterates through test cases to verify various functionalities related to tuning to LLD


## Pre-requisites to run this test

1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```


## Run l2test using script:

Expect around 80 seconds for test to complete./run_l2_aamp.py -t 1015

From the *test/l2test/ folder run:
```
./run_l2_aamp.py -t 1015
```

## Details
* TESTDATA0 ------> LLD Playback With Position Verification



