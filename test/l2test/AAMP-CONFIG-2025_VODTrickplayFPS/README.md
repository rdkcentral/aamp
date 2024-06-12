# AAMP Simlinear L2 test
<p> Test case to validate VODTrickplayFps Configuration.</p>

<p>Streaming URL : https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd</p>

<p>Jira : https://ccp.sys.comcast.net/browse/RDKAAMP-2904</p>

## Run l2test using script:

From the *test/l2test/ folder run:

- git clone
- cd aamp
- ./install-aamp.sh -c
- cd aamp/test/l2test/
./l2framework_testenv.sh
- source l2venv/bin/activate
- python run_l2_aamp.py -t 2025
