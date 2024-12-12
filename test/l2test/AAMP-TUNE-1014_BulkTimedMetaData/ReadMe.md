
# AAMP L2  test
<p> Test used to check/verify bulkTimedMetadata in Live</p>

<RDKAAMP-3532> Charter] [CTSB] no timedMetadata from tunetime manifest.

## Pre-requisites to run this test

1. Streams for this test will be downloaded from https://cpetestutility.stb.r53.xcal.tv
   but location can be overridden by specifying TEST_1014_STREAM_PATH if required.

    a. When running inside docker: -

         echo "TEST_1014_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz" >> .env

    b. When running on ubuntu (outside a docker container): -

        export TEST_1014_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz

2. Archive file containing manifest data has been downloaded ( URL: from location where the file is stored )
   and extracted into directory 'testdata'
   
3.  AAMP installed using install-aamp.sh script


## Run l2test using script:

From the *test/l2test/ folder run:

./run_l2_aamp.py -t 1014

## Details
* By default aamp-cli runs without a video window to output A/V so it can be run via Jenkins

* When running manually from a terminal then A/V output might be useful. This can be achieved by "run_l2_aamp.py -v"

Automated test setup when run_l2_aamp.py is invoked:

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

