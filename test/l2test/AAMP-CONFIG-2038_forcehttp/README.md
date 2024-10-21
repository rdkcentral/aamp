# AAMP forceHttp config L2 test
<p> Test case to validate forcehttp config</p>
 
<p>Streaming URL : </p>
<p>https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-3395</p>


## Run l2test using script:

From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2038

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2038
