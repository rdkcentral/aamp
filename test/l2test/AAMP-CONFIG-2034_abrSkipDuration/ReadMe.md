<p> Test case to validate abrSkipDuration API</p>
 
<p>Streaming URL : https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/AAMP-IFRAME-4007/VideoTestStream.tar.xz</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-2992</p>


## Run l2test using script:

From the *test/l2test* folder run:

 ./run_l2_aamp.py -v -t 2034

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
     ./run_l2_aamp.py -v -t 2034
