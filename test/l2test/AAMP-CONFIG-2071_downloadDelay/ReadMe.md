# AAMP downloadDelay config L2 test
<p> Test case to validate downloadDelay config</p>
<p> downloadDelay : Delay for downloads to simulate network latency.Default: 0</p>
 
<p>Streaming URL : </p>
<p>https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-3858</p>


# Run l2test using script:

From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2071

# Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2071

