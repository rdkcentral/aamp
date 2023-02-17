# How to run L2 testsuites

<p>Generic script run_l2_aamp.py is available in  aamp/test/l2test folder.</p>

***Python 3 installation is required for the AAMP L2 framework*** 

<p>To install python3, run the below script for first time</p>

> ./l2framework_testenv.sh

# To run on Ubuntu docker

# Environment setup

<p> from ubuntu install docker and docker compose </p>

https://docs.docker.com/engine/install/ubuntu/

https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-on-ubuntu-20-04

<p> For creating docker image ,build and run test suites , from aamp/test/l2test path execute the below command. </p>

***To create a Docker image for the first time, simply run the command below***

>  sudo docker compose -f docker-compose_buildimage.yml up

***To check docker images created newly***

> sudo docker images

***Tests that require environment variables***

> TST_2000: requires TEST_STREAM_PATH set to the URL containing the test stream.

***To append the teststream artifactory path in .env file***

<p> For the first time, the artifactory path needs to be given in the.env file before starting the test by performing the following command. </p>

> echo "TEST_STREAM_PATH=https://artifactory.host.com/artifactory/streams.tar.gz" >> .env

<p> If require to override the environment variables in .env file please use the below command</p>

> echo "TEST_STREAM_PATH=https://artifactory.host.com/artifactory/streams.tar.gz" > .env

***URL:specify the location where teststream stored***

***To run all suite without making build,run the below command from l2test directory***

> sudo docker compose -f docker-compose.yml up

***To run all suite with build,run the below command from l2test directory***

> sudo optargs="-b" docker compose -f docker-compose.yml up

***To make build and run the specified suites ,use the following run command***

> sudo optargs="-b -t 1000" docker compose -f docker-compose.yml up

***To make build and run mulitple suites ,use the following run command***

> sudo optargs="-b -t 1000 2000" docker compose -f docker-compose.yml up

***To run specified suite without making build ,use the following run command***

> sudo optargs="-t 1000" docker compose -f docker-compose.yml up

***To run mulitple suites without making build,use the following run command***

> sudo optargs="-t 1000 2000" docker compose -f docker-compose.yml up

<p> Tests that are being run with the current build may fail if the given build is incompatible with the test. <p>

***To make build and run testsuites with exclusion tests specified***

> sudo optargs="-b -dt 1000 2000" docker compose -f docker-compose.yml up

***To run testsuites with exclusion tests specified***

> sudo optargs="-dt 1000 2000" docker compose -f docker-compose.yml up

***To list test suites with short description ,use the following run command***

> sudo optargs="-lt" docker compose -f docker-compose.yml up 

***To get help about script ,use the following run command***

> sudo optargs="-h" docker compose -f docker-compose.yml up

<p> ./run_l2_aamp.py $optargs will accept the option argument from docker-compose.yml as an input. <p>

# How to add new test suite

- Create new Testsuite folder under ***aamp/test/l2test***

- The name of the testsuite folder should begin with ***"TST_,"*** followed by the 4digit number and feature name.

>>>***ex*** :TST_1000_Webvtt

- ***run_test.py*** This is the python script that will be executed to run the test.

- If a feature requires any build configuration, include the ***build config.json*** file, which contains the necessary build configuration.

>>> ex: ***{ "CXXFLAGS" : "MAKE_SUBTEC_SIMULATOR_ENABLED"}*** the values from the parsed json file will be sent to the build_script.

- If the testsuite needs to build up a test environment, include the install test prepare_testenv.sh file, which specifies the additional infrastructure needed to perform the tests.

>>> ***ex*** :  sudo pip3 install pipreqs

- If the testsuite has any prerequisites,include ***prerequisite.sh***.It specifies actions to be taken before starting a test run.


# How to run L2 testsuites on MAC

> TST_2000: requires TEST_STREAM_PATH set to the URL containing the test stream.

***To export the teststream artifactory path***

<p> For new session, the artifactory path needs to be specified before starting the test by performing the following command. </p>

> export TEST_STREAM_PATH==https://artifactory.host.com/artifactory/streams.tar.gz

***URL:specify the location where teststream stored***

***To run all suite without making build,run the below command from l2test directory***

> ./run_l2_aamp.py

***To run all suite with build,run the below command from l2test directory***

> ./run_l2_aamp.py -b

***To make build and run the specified suites ,use the following run command***

> ./run_l2_aamp.py -b -t 1000

***To make build and run mulitple suites ,use the following run command***

> ./run_l2_aamp.py -b -t 1000 2000

***To run specified suite without making build ,use the following run command***

> ./run_l2_aamp.py -t 1000

***To run mulitple suites without making build,use the following run command***

> ./run_l2_aamp.py -t 1000 2000

<p> Tests that are being run with the current build may fail if the given build is incompatible with the test. <p>

***To make build and run testsuites with exclusion tests specified***

> ./run_l2_aamp.py -b -dt 1000 2000

***To run testsuites with exclusion tests specified***

> ./run_l2_aamp.py -dt 1000 2000

***To list test suites with short description ,use the following run command***

> ./run_l2_aamp.py -lt

***To get help about script ,use the following run command***

> ./run_l2_aamp.py -h

# How to add new test suite

- Create new Testsuite folder under ***aamp/test/l2test***

- The name of the testsuite folder should begin with ***"TST_,"*** followed by the 4digit number and feature name.

>>>***ex*** :TST_1000_Webvtt

- ***run_test.py*** This is the python script that will be executed to run the test.

- If a feature requires any build configuration, include the ***build config.json*** file, which contains the necessary build configuration.

>>> ex: ***{ "CXXFLAGS" : "MAKE_SUBTEC_SIMULATOR_ENABLED"}*** the values from the parsed json file will be sent to the build_script.

- If the testsuite needs to build up a test environment, include the install test prepare_testenv.sh file, which specifies the additional infrastructure needed to perform the tests.

>>> ***ex*** :  brew install pipreqs

- If the testsuite has any prerequisites,include ***prerequisite.sh***.It specifies actions to be taken before starting a test run.


