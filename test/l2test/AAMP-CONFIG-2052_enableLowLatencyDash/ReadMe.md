# AAMP enableLowLatencyDash config L2 test
<p> Test case to validate enableLowLatencyDash config</p>
 
<p>Streaming URL : </p>
<p>https://akamaibroadcasteruseast.akamaized.net/cmaf/live/657078/akasource/out.mpd</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-3713</p>


# Run l2test using script:

From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2052

# Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2052

