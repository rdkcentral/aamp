# AAMP L2 sample automated ABR test
Test used to check / verify the AAMP behavior when the network bandwidth and request responses are manipulated.

The test uses 2 other services in docker containers:
simlinear - plays back the canned streams (Instructions to build the container are in tools/simlinear/Dockerfile)
abr-test-proxy - proxy server allowing the bitrate and HTTP responce manipulation (Instructions to build the container are in tools/abrTestingProxy/README.txt)


run_test.py  Causes aamp-cli to play HLS content. Checks log messages output from AAMP are as expected. For each test gives PASS/FAIL result

## Pre-requisites to run this test
The automated test is designed to run in docker containers orchestrated by docker compose. (see docker-compose.yml in the l2test) which requires an env file

Setting up the .env file:
'''
echo -e “TEST_3000_STREAM_PATH=https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/aamptest/streams/simlinear/streams09-Jan2023.tar.gz" >> .env
echo -e "DOCKER_REPO=aamp-docker.artifactory.comcast.com//" >> .env
echo -e "RUNNING_IN_DOCKER=true" >> .env
echo -e "TEST_STREAM_PATH=./" >> .env
'''

## Run the test
'''
docker compose -p $PROJECT_NAME -f docker-compose.yml up --abort-on-container-exit --remove-orphans --force-recreate
'''

