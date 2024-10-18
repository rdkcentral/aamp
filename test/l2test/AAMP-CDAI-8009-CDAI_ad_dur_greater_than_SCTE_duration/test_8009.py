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
server_path = os.path.join(os.getcwd(), "AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/server.py")

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

# Test Case 6.1: Single period with CDAI ad duration > SCTE35 duration
# Description: This test case verifies the handling of a single period with an ad break where the ad duration is  longer than the specified SCTE duration.
# Scenario:
# - The TestCase1.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 30 seconds, but the actual ad duration is 40 seconds.
#   - Period 2: 30 seconds long, with no ads.
# - The expectation is to play the full ad.

TESTDATA1 = {
    "title": "Single period with CDAI ad duration > SCTE35 duration",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        # Adding a 40-second ad for the ad break in Period 1
        "advert add http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/ad_40s.mpd 40",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=40", "min": 0, "max": 50},
        # State change indicating the start of ad playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        # Indication that the ad break is starting for Period 1
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Expectation for the period ID change due to ad insertion
        {"expect": re.escape("Period ID changed from '0' to '0-112' [BasePeriodId='1']"), "min": 20, "max": 60},
        # State change indicating the transition back to outside of ad break after playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 80},
	# Expectation for the period ID change due to ad completion
        {"expect": re.escape("Period ID changed from '0-112' to '2' [BasePeriodId='2']"), "min": 20, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 70},
	#Expectation for truncate happening - it should not play excess ad duration
	{"expect": r"HttpRequestEnd.?ad_40/en_001.mp3", "min":0, "max":180},
        {"expect": r"HttpRequestEnd.?ad_40/en_0015.mp3", "min":0, "max":180},
        {"expect": r"HttpRequestEnd.*?ad_40/en_0016.mp3", "min":0, "max":180, "not_expected": True},
        #End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test":True},
    ]
}

# Test Case6.2: Single period with CDAI ad duration > SCTE35 duration - Case2
# Description: This test case verifies the handling of a single period with an ad break
# where the ad duration is longer than the specified SCTE duration.
# Scenario:
# - The TC1.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break.
#     The SCTE break duration is set to 30 seconds, but the substitution is two 20 sec ads.
#   - Period 2: 30 seconds long, with no ads.
# - The expectation is to play the first ad and play the remaining content from the source period.

TESTDATA2 = {
    "title": "Single period with CDAI ad duration > SCTE35 duration- With 2 ads substitution",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        # Adding two 20-second ads for the ad break in Period 1
        "advert add http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/ad_20s.mpd 20",
        "advert add http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/ad_20s.mpd 20",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 200},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 200},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=20", "min": 0, "max": 200},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId2 duration\=20", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 200},
        # Expectation for the period ID change due to ad insertion
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        # Expectation for the period ID change due to ad completion
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='1']"), "min": 20, "max": 200},
        {"expect": re.escape("Period ID changed from '1-114' to '2' [BasePeriodId='2']"), "min": 20, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 200},
        #Ensure ad1 ends properly 
        {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/ad_20/1080p_010.m4s","min":10,"max":200},
        #Ensure fragments download by both ads
        {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8009-CDAI_ad_dur_greater_than_SCTE_duration/testdata/content/ad_20/1080p_006.m4s", "min":10,"max":200,"count":2},
        {"expect": r"HttpRequestEnd.*?/dash/en_031.mp", "min":0, "max":180},
        # End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 200, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8009(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    start_server()
    aamp.run_expect_b(test_data)
    stop_server()
