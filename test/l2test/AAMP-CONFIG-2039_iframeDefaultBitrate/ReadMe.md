# AAMP iframeDefaultBitrate config L2 test
<p> Test case to validate iframeDefaultBitrate config</p>
 
<p>Streaming URL: </p>
<p>https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/multiIframeTracks.tar.xz</p>
 
<p>Jira:https://ccp.sys.comcast.net/browse/RDKAAMP-3375</p>


# Run l2test using script:

From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2039

# Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2039
