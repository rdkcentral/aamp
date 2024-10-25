# AAMP useMatchingBaseUrl config L2 test
<p> Test case to validate useMatchingBaseUrl config</p>
 
<p>Streaming URL : </p>
<p>https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/matchingUrlDump.tar.xz</p>
 
<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-3379</p>


# Run l2test using script:

From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2042

# Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2042
