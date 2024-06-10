# AAMP liveoffset4k test

<p> Test case to validate setliveOffset4k() API</p>
 
<p>Streaming URL : https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/sky/skywitness-4klive-8M.tar.gz</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-2433</p>


## Run l2test using script:

From the *test/l2test* folder run:

 ./run_l2_aamp.py -v -t 2018

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
     ./run_l2_aamp.py -v -t 2018
