# AAMP TSB tests
L2 tests to verify audio track change with Local AAMP TSB enabled.

## Test Files
+ test_5005.py  Iterates through test cases.

## Pre-requisites to run this test
1. setup l2test environment as detailed in /test/l2test/README.md (one-time setup)
2. activate venv (each time when l2 tests need to be run)
```
source l2venv/bin/activate
```

## Run l2test using script:
From the *test/l2test/* folder run:
```
./run_l2_aamp.py -t 5005
```

