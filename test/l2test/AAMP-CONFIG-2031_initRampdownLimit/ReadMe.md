<p> Test case to validate initRampdownLimit config</p>
 
<p>Streaming URL : </p>
<p>https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/VideoTestStream_HLS.tar.xz
</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-3026</p>


## Run l2test using script:

From the *test/l2test* folder run:

 ./run_l2_aamp.py -v -t 2031

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
     ./run_l2_aamp.py -v -t 2031
