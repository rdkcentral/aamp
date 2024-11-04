#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
server_path = os.path.join(os.getcwd(), "AAMP-CDAI-8004_ShortAd/testdata/content/server.py")

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

TESTDATA1 = {
    "title": "Test1 Current ad duration same as source ad duration",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/main.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad_30s.mpd 30",
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad_40s.mpd 40",
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad-30s.mpd 30",
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad_20s.mpd 20",
    ],
    #Source ad is 120 secs, all ads will sum up to 120 secs
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/main.mpd", "min": 0, "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[120000\]", "min": 0, "max": 150},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 duration\=40 url\=.*?ad_40s.mpd", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId3 duration\=30 url\=.*?ad-30s.mpd", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId4 duration\=20 url\=.*?ad_20s.mpd", "min": 0, "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 100, "max": 160},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 100, "max": 160},
        {"expect": re.escape("Period ID changed from '1' to '0-111' [BasePeriodId='2']"), "min": 100, "max": 180},
        {"expect": re.escape("Period ID changed from '0-111' to '1-112' [BasePeriodId='2']"), "min": 100, "max": 200},
        {"expect": re.escape("Period ID changed from '1-112' to '2-113' [BasePeriodId='2']"), "min": 150, "max": 220},
        {"expect": re.escape("Period ID changed from '2-113' to '3-114' [BasePeriodId='2']"), "min": 150, "max": 250},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 120000, endPeriodId: 3, endPeriodOffset: 0, \#Ads: 4", "min": 240, "max": 260},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 240, "max": 260},
        {"expect": re.escape("Period ID changed from '3-114' to '3' [BasePeriodId='3']"), "min": 240, "max": 270},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/dash/(1080|720|480|360)p_132.m4s\?live=true", "min": 250, "max": 270},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/dash/(1080|720|480|360)p_133.m4s\?live=true", "min": 250, "max": 270, "end_of_test":True},
    ]
}

TESTDATA2 = {
    "title": "Test2 Present ad duration less than source ad duration",
    "max_test_time_seconds": 400,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/main.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad_30s.mpd 30",
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad_40s.mpd 40",
        "advert add http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/ad-30s.mpd 30",
    ],
    #Source ad is 120 secs but all ads will sum up to 100 secs
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8004_ShortAd/testdata/content/main.mpd", "min": 0, "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[120000\]", "min": 0, "max": 150},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 duration\=40 url\=.*?ad_40s.mpd", "min": 0, "max": 150},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId3 duration\=30 url\=.*?ad-30s.mpd", "min": 0, "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 100, "max": 160},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 100, "max": 160},
        {"expect": re.escape("Period ID changed from '1' to '0-111' [BasePeriodId='2']"), "min": 100, "max": 180},
        {"expect": re.escape("Period ID changed from '0-111' to '1-112' [BasePeriodId='2']"), "min": 100, "max": 200},
        {"expect": re.escape("Period ID changed from '1-112' to '2-113' [BasePeriodId='2']"), "min": 150, "max": 220},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 220, "max": 240},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 100000, endPeriodId: 3, endPeriodOffset: 0, \#Ads: 3", "min": 220, "max": 260},
        #After ad3 period 113 should not switch to ad1 period 111
        {"expect": re.escape("Period ID changed from '2-113' to '0-111' [BasePeriodId='2']"), "min": 200, "max": 250, "not_expected":True},
        #After ad3 should switch to main content period 2
        {"expect": re.escape("Period ID changed from '2-113' to '2' [BasePeriodId='2']"), "min": 200, "max": 260},
        # Make sure we log these segments so restamp values can be checked
        # The ptsoffset calculated value on returning to period 2
        {"expect": r"UpdatePtsOffset.*?Id 2 mPTSOffsetSec 0.000000 mNextPts -70.000000 timelineStartSec 142.000000", "min": 220, "max": 400},
        # First seg from base period 2 after returning from ad should be 122
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_121.m4s\?live=true", "min": 220, "max": 400, "not_expected":True},
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_122.m4s\?live=true", "min": 220, "max": 400},
        # Last seg from base period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_131.m4s\?live=true", "min": 220, "max": 400},
        # first seg from base period 3
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_132.m4s\?live=true", "min": 220, "max": 400},
        # End the test
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_133.m4s\?live=true", "min": 220, "max": 400, "end_of_test":True},
    ]

}

TESTLIST = [TESTDATA1,TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8004(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    start_server()
    try:
        aamp.run_expect_b(test_data)
        stop_server()
    finally:
        stop_server()

