# AAMP L2 Tune time test
Test used to check/verify the linear CDAI behaviour with Single Source Period with Single CDAI Ad having Ad fragment download failures


## Test Files
+ test_8022.py  Causes aamp-cli to play manifest and check logs for tuning related info.
               Checks log messages output from aamp are as expected. For the test gives PASS/FAIL result.


## Pre-requisites to run this test

1. Streams for this test will be downloaded from https://cpetestutility.stb.r53.xcal.tv
   but location can be overridden by specifying TEST_8022_STREAM_PATH if required.

    a. When running inside docker: -

         echo "TEST_8022_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz" >> .env

    b. When running on ubuntu (outside a docker container): -

        export TEST_8022_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz

2. Archive file containing manifest data has been downloaded ( URL: from location where the file is stored )
   and extracted into directory download_archive outside this folder


## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_test.py -t 8022

## Details
* By default aamp-cli runs without a video window to output A/V so it can be run via Jenkins

* When running manually from a terminal then A/V output might be useful. This can be achieved by "run_test.py -v"

Automated test setup when run_test.py is invoked:

                                   testdata
                                      |
                                      V
                                -----------------
                    |--launch-> | webserver.py  |
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
