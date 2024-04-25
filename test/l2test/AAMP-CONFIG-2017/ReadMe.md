# AAMP liveoffset test

<p> Test case to validate setliveOffset() API</p>
 
<p>Streaming URL : https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/aamptest/streams/simlinear/SkyWitness/30t-after-fix/skywitness-30t-after-fix.zip</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-2432</p>


## Run l2test using script:

From the *test/l2test* folder run:

 ./run_l2_aamp.py -v -t 2017

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
     ./run_l2_aamp.py -v -t 2017
