# AAMP mapM3U8 config L2 test

Testcase to validate mapM3U8 config

**Streaming URL :** 

https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8
https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

**Jira :**

https://ccp.sys.comcast.net/browse/RDKAAMP-3638

## Run l2test using script:
From the *test/l2test* folder run:

    ./run_l2_aamp.py -v -t 2046

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test
    ./l2framework_testenv.sh
    source l2venv/bin/activate
    ./run_l2_aamp.py -v -t 2046
