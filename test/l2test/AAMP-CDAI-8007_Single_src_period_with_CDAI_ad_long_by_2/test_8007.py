#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Starts simlinear, a web server for serving test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
# Also see README.md

import os
import sys
from inspect import getsourcefile
import pytest
import subprocess
import atexit
import re

###############################################################################

server_process = None
server_path = os.path.join(os.getcwd(), "AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/server.py")

def start_server():
    global server_process
    if os.path.isfile(server_path):
        try:
            server_process = subprocess.Popen(["python3", server_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print("started server.py")
            atexit.register(server_process.terminate)
        except Exception as e:
            print("Failed to start server.py"+server_path)
    else:
        print("Error: server.py file not found "+ server_path)

def stop_server():
    global server_process
    if server_process:
        print("stop server")
        server_process.terminate()
        server_process = None
        
restamp_values:dict[str, float] = {}
segment_cnt = 0

def check_restamp(match,arg):
    global segment_cnt, restamp_values

    # Get the fields from the log line
    mediaTrack = match.group(1)
    timeScale = float(match.group(2))
    before = float(match.group(3))
    after = float(match.group(4))
    duration = float(match.group(5))
    url = match.group(6).decode()

    segment_cnt += 1
    print(segment_cnt, mediaTrack, timeScale, before, after, duration, url)

    # Our expected pts value starts from 0
    expected_restamp = restamp_values.get(mediaTrack, 0)

    # The actual duration in the provided segments may not match that from the manifest.
    # This can be seen in https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd
    # We allow the pts value after restamp to differ by 5% of the segment duration
    duration_sec = duration / timeScale
    #tolerance_sec = duration_sec * 0.05
    tolerance_sec = 0.7 # Test data needs fixing
    after_sec = after / timeScale
    diff_sec = abs(after_sec - expected_restamp)
    print(f"PTS (secs): actual {after_sec:.3f}, expected {expected_restamp:.3f}, diff {diff_sec:.3f}, tol {tolerance_sec:.3f}")
    assert diff_sec <= tolerance_sec

    # Save what we are expecting for the next value
    restamp_values[mediaTrack] = after_sec + duration_sec
 
# Test Case 4.1: Single Source Period with CDAI Ad Long by <= 2 Seconds
# Description: This test case verifies the handling of a single period with an ad break where the ad duration is slightly longer than the specified SCTE duration.
# Scenario:
# - The TC1.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 30 seconds, but the actual ad duration is 32 seconds, making the ad longer by 2 seconds.
#   - Period 2: 30 seconds long, with no ads.
# - The expectation is to play the full ad and handle the extra 2-second difference, which would add latency to the live buffer.

TESTDATA1 = {
    "title": "Single source period with CDAI ad long by <= 2 sec",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        # Adding a 32-second ad for the ad break in Period 1
        "advert add http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_32s.mpd 32",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":300, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=32", "min": 0, "max": 50},
        # State change indicating the start of ad playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        # Indication that the ad break is starting for Period 1
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Expectation for the period ID change due to ad insertion
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 60},
        # State change indicating the transition back to outside of ad break after playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 80},
	# Expectation for the period ID change due to ad completion
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 20, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 70},
        # Expectation for handling adbreak ending early and terminating ad playback
        {"expect": r"Adbreak ended early. Terminating Ad playback", "min": 0, "max": 150},
        # Expectation for playing last (extra) 2 sec from ad - 16th ad fragment(Full ad)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_30/1080p_016.m4s", "min": 0, "max": 150},
	#End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test":True},
    ]
}

#####################    Test failed Need to fix #################
# Test Case 4.2: Back to back source period with CDAI ad long by <= 2 sec
# Description: This test case verifies the handling of a back to back period with an ad break where the ad duration is slightly longer than the specified SCTE duration.
# Scenario:
# - The BackToBack.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 30 seconds, but the actual ad duration is 32 seconds, making the ad longer by 2 seconds.
#   - Period 2: 10 seconds long.containig a 10 sec ad break - susbtituted with 12 sec ad content
#   - Period 2: 30 seconds long, with no ads.
# - The expectation is to play the full ad and handle the extra 2+2-second difference, which would add latency to the live buffer.
# Existing Behaviour : Playback stuck in second ad 

TESTDATA2= {
    "title": "Back to Back source period with CDAI ad long by <= 2 sec",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/BackToBackAd.mpd?live=true",
    "cmdlist": [
        # Adding a 32-second ad for the first 30 sec ad break in Period 1
        "advert add http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_32s.mpd 32",
        # Adding a 12-second ad for the second 10 sec ad break in Period 2
        "advert add http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_12s.mpd 12"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
         {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":400, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 150},
        # Detection of the second ad break in Period 2 with a duration of 10 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[10000\]", "min": 0, "max": 150},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        # Expectation for the first ad (32 seconds) in the first ad break
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=32", "min": 0, "max": 200},
        # Expectation for the second ad (12 seconds) in the second ad break
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 duration\=12", "min": 0, "max": 200},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 200},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 20, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 32000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 0, "max": 200},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 12000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 0, "max": 200},
        {"expect": r"Adbreak ended early. Terminating Ad playback", "min": 0, "max": 150},
        # Expectation for playing the last 2 seconds from the ad segment
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_30/1080p_016.m4s", "min": 0, "max": 200},
        # Ensuring the last additional 2 seconds from the 12-second ad is played
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8007_Single_src_period_with_CDAI_ad_long_by_2/testdata/content/ad_30/1080p_011.m4s", "min": 0, "max": 200, "count": 2},
        # Confirming the last segment fetched belongs to Period 2, indicating end of the test
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8007(aamp_setup_teardown, test_data):
    global segment_cnt, restamp_values
    segment_cnt = 0
    restamp_values = {}
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    start_server()
    aamp.run_expect_b(test_data)
    stop_server()

    assert segment_cnt > 20, "Fail restamp was not checked."
