# How to run L2 test suites

Generic script run_l2_aamp.py is available in aamp/test/l2test folder.

**Python 3 installation is required for the AAMP L2 framework** 

To install required packages, run the below script for first time

    ./l2framework_testenv.sh

Activate python virtual environment "l2venv" that will have been created.

    source l2venv/bin/activate

# How to run L2 test suites

**Tests that use environment variables**

For tests that use environment variables, export them before running the test. For example: -

    export TEST_STREAM_PATH=https://artifactory.host.com/artifactory/streams.tar.gz


**To run all test suites without building, use the following command from aamp/test/l2test directory**

    ./run_l2_aamp.py

Tests that require a custom build will get skipped without the -b option.

**To run all test suites displaying video without building, use the following run command**

    ./run_l2_aamp.py -v

**To build and run all test suites, use the following command**

    ./run_l2_aamp.py -b

**To build and run specified test suites, use the following command**

    ./run_l2_aamp.py -b -t 1000 2000

**To run specified test suites without making build, use the following command**

    ./run_l2_aamp.py -t 1000 2000

Tests that are being run with the current build may fail if the given build is incompatible with the test.

**To run all test suites but exclude those specified, use the following command**

    ./run_l2_aamp.py -e 1000 2000

**To list test suites available with short description, use the following command**

    ./run_l2_aamp.py -l

**To get help about script, use the following command**

    ./run_l2_aamp.py -h


# How to run single L2 test suite for debug purposes

From the chosen test suite directory aamp/test/l2test/TST_xxxx  
Note if you have run the test with ./run_l2_aamp.py script already once you can skip steps 2 and 3.

1.  Create and activate a Python virtual environment.

    ```
    python3 -m venv l2venv
    source l2venv/bin/activate
    ```

2.  Find and install any required python packages. The following is used by run_l2_aamp.py, but can be done manually.

    ```
    python3 -m pip install pipreqs  (or pip3 install pipreqs)
    pipreqs --mode gt .
    python3 -m pip install -r requirements.txt  (or pip3 install -r requirements.txt)
    ```

3.  Run any prerequisite scripts for the test.

    ```
    ./prepare_testenv.sh
    ./prerequisite.sh
    ```

4.  Then run the test and this step can be repeated as needed.

    ```
    ./run_test.py
    ```
    Some of the tests may accept options `./run_test.py -h` to list them.


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

- Create new test suite folder under ***aamp/test/l2test***

- The name of the test suite folder should begin with ***"TST_,"*** followed by the 4 digit number and feature/suite name. ***e.g. TST_1000_Webvtt*** 

- ***run_test.py*** This is the python script that will be executed to run the test.

- If a feature requires any build configuration, include the ***build_config.json*** file, which contains the necessary build configuration.
e.g. ***{ "CXXFLAGS" : "MAKE_SUBTEC_SIMULATOR_ENABLED"}*** the values from the parsed json file will be sent to the build_script.

- If the test suite needs to build up a test environment, include the install ***prepare_testenv.sh*** file, which specifies the additional infrastructure needed to perform the tests.

- If the test suite has any prerequisites, include a ***prerequisite.sh***. It specifies actions to be taken before starting a test run.


