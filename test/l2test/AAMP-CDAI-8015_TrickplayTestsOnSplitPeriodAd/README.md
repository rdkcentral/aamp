# AAMP L2 Tune time test
Test used to replicate the Split period CDAI scenario under RDK-53685 [AAMP][ITV] CDAI Split period support


## Test Files
+ test_8015.py  Causes aamp-cli to play manifest and check logs for cdai related logs.
               Checks log messages output from aamp are as expected. For the test gives PASS/FAIL result.


## Pre-requisites to run this test

1. Streams for this test will be downloaded from https://cpetestutility.stb.r53.xcal.tv
   but location can be overridden by specifying TEST_8015_STREAM_PATH if required.

    a. When running inside docker: -

         echo "TEST_8015_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz" >> .env

    b. When running on ubuntu (outside a docker container): -

        export TEST_8015_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz

2. Archive file containing manifest data has been downloaded ( URL: from location where the file is stored )
   and extracted into directory 'testdata'


## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_aamp.py -t 8015

## Details
* By default aamp-cli runs without a video window to output A/V so it can be run via Jenkins

* When running manually from a terminal then A/V output might be useful. This can be achieved by "run_test.py -v"

Automated test setup when run_test.py is invoked:

                                   testdata
                                      |
                                      V
                                -----------------
                    |--launch-> | simlinear.py  |
    ------------    |           -----------------
    run_test .py|-> |                | http fetch
    ------------    |                V
        ^           |           -----------
        |           |-launch-> | aamp-cli  |--------------> |
        |                       -----------                 |
        |                                                   |
        |                                                   |
        |                                                   |
        | -----------------log messages --<-----------------|
