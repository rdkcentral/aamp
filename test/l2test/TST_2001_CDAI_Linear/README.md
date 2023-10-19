# AAMP L2 Linear CDAI test
<p> Test used to check/verify the linear CDAI behaviour and APIs</p>

## Test Scenarios

### Test 0  (alternate ads)
    Main | Ad | Main | Ad | Main | Ad | Main

### Test 1  (back to back ads)
    Main |  Ad | Ad | Ad | Ad | Ad | Main

## Test Files
+ simlinear.py  Webserver for serving manifest data.

+ run_test.py  Causes aamp-cli to play manifest and calling SetAlternateContents API to place the ads.
               Checks log messages output from aamp are as expected. For each test gives PASS/FAIL result.


## Pre-requisites to run this test

1. python3 and required modules installed.

         python3 -m pip install pexpect requests flask webargs aiohttp

2. Streams for this test will be downloaded from https://cpetestutility.stb.r53.xcal.tv
   but location can be overridden by specifying TEST_2001_STREAM_PATH if required.

    a. When running inside docker: -

         echo "TEST_2001_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz" >> .env

    b. When running on ubuntu (outside a docker container): -

        export TEST_2001_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz

3. Archive file containing manifest data has been downloaded ( URL: from location where the file is stored )
   and extracted into directory 'testdata'

# aamp repository downloaded and aamp-cli built
This will result in the following directory structure:
```
$ ls -l
drwxrwxr-x aamp            <-- aamp repository containing a built aamp-cli
drwxrwxr-x testdata        <-- manifest test data obtained from artifactory or other location where it is kept
```
## Run the test
```
$ ./aamp/test/l2test/TST_2001_CDAI_Linear/run_test.py
```

Or when running manually while viewing the video.
```
$ ./aamp/test/l2test/TST_2001_CDAI_Linear/run_test.py -v
```

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
