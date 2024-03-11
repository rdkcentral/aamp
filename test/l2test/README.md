# How to run L2 test suites


**Python 3 installation is required for the AAMP L2 framework** 

To install required packages, run the below script for first time

    ./l2framework_testenv.sh

Activate python virtual environment "l2venv" that will have been created.

    source l2venv/bin/activate

# What run_l2_aamp.py does
1. Installs required python modules when run for first time
2. Builds aamp and gstreamer subtec plugin (optional -b)
3. Selects list of tests to run from command line options
4. Runs pytest on that list of tests
5. Generates results.json file from results.xml produced by pytest

# How to run L2 test suites run_l2_aamp.py

**Tests that use environment variables**

For tests that use environment variables, export them before running the test. For example: -

    export TEST_STREAM_PATH=https://artifactory.host.com/artifactory/streams.tar.gz


**To run all test suites without building, use the following command from aamp/test/l2test directory**

    ./run_l2_aamp.py


**To run all test suites displaying video without building, use the following run command**

    ./run_l2_aamp.py -v

**To build and then run all test suites, use the following command**

    ./run_l2_aamp.py -b

**To build and run specified test suites, use the following command**

    ./run_l2_aamp.py -b -t 1000 2000

**To run specified test suites without making build, use the following command**

    ./run_l2_aamp.py -t 1000 2000

**To run all test suites but exclude those specified, use the following command**

    ./run_l2_aamp.py -e 1000 2000

**To run the subset of tests chosen to be used in ci**

Command line arguments other than -e -t -v -b get passed into pytest. Consult pytest documentation for these. 
The option -t -e select an intial set of tests and then pytest options may further restrict that set.

    ./run_l2_aamp.py -m ci_test_set

**To get help about script, use the following command**

    ./run_l2_aamp.py -h
    
## Test output log files

Each test will output log files to a subdirectory 'output' E.G

    .../l2test/TST_1000_Webvtt/output

## Invoking pytest directly
Once required python modules have been installed (E.G via an invocation of ./run_l2_aamp.py) 
then pytest can be used to run individual tests, or all tests. No .xml or .json results file will be 
produced when invoked this way.

    pytest TST_1000_Webvtt

# To run with a python virtual environment

Create the venv, activate the env, run the tests. Note: The name of the env 'l2venv' is 
important as run_ls_aamp.py is configured to avoid this directory.


    python3 -m venv l2venv
    source l2venv/bin/activate
    ./run_l2_aamp.py ...


# To run using Ubuntu Docker

### Environment setup

From ubuntu install docker and docker compose.  
https://docs.docker.com/engine/install/ubuntu/

Ubuntu 20.04: https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-on-ubuntu-20-04

For creating docker image, build and run test suites, execute the below command from the directory.
From aamp/test/l2test  
Note: if docker is not in the sudo group you may need to prepend sudo to the commands or add it to the group.

**To create a Docker image for the first time, simply run the command below**

    docker compose -f docker-compose_buildimage.yml up

**To list the docker images available**

    docker images


**Tests that use environment variables**
Set environment variables in the aamp/test/l2test/.env file for docker runs. If a stream path needs to be given in the .env file before starting the test by performing the following command.

    echo "TEST_STREAM_PATH=https://artifactory.host.com/artifactory/streams.tar.gz" >> .env

**To run all test suites without building, use the following command**

    docker compose -f docker-compose.yml up

**To build and run all test suites, use the following command**

    optargs="-b" docker compose -f docker-compose.yml up

**To build and run specified test suites, use the following command**

    optargs="-b -t 1000 2000" docker compose -f docker-compose.yml up

Tests that are being run with the current build may fail if the given build is incompatible with the test.
See documentation above for more script options.

`./run_l2_aamp.py $optargs` will accept the option argument from docker-compose.yml as an input.

**After testing, run this to stop container**

    docker compose -f docker-compose.yml down

**To force docker to recreate the container add option below**

    docker compose -f docker-compose.yml up --force-recreate


# How to add new test suite

- Update the shared .xls to allocate a new test suite name
  
- Create new test suite folder under ***aamp/test/l2test***

- The name of the test suite folder should correspond to the entry in the .xsl and contain a unique 4+ digit number. ***e.g. AAMP-TUNE-1000_Webvtt*** 

- Copy in needed files from some other test as a starting point.

- Note that test_xxxx.py will have to be named so that the number matches that of the containing directory.

- If the test suite has any prerequisites, include a ***prerequisite.sh***. It specifies actions to be taken before starting a test run.


