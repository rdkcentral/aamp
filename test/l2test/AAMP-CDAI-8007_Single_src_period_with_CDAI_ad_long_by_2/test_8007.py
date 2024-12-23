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
import re
from l2test_pts_restamp import PtsRestampUtils
from l2test_window_server import WindowServer
###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

pts_restamp_utils = PtsRestampUtils()


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
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        # Adding a 32-second ad for the ad break in Period 1
        "advert add http://localhost:8080/content/ad_32s.mpd 32",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":300, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=32", "min": 0, "max": 50},
        # Indication that the ad break is starting for Period 1
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # State change indicating the start of ad playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        # Expectation for the period ID change due to ad insertion
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 60},
        # State change indicating the start of ad playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 10, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 70},
        # State change indicating the transition back to outside of ad break after playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 80},
        # Expectation for the period ID change due to ad completion
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 20, "max": 60},
        # Expectation for playing last (extra) 2 sec from ad - 16th ad fragment(Full ad)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/1080p_016.m4s", "min": 0, "max": 150},
        #End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test":True},
    ]
}

# Test Case 4.2: Back to back source period with CDAI ad long by <= 2 sec
# Description: This test case verifies the handling of a back to back period with an ad break where the ad duration is slightly longer than the specified SCTE duration.
# Scenario:
# - The BackToBack.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 30 seconds, but the actual ad duration is 32 seconds, making the ad longer by 2 seconds.
#   - Period 2: 10 seconds long, containig a 10 sec ad break - susbtituted with 12 sec ad content
#   - Period 2: 30 seconds long, with no ads.
#   - The expectation is to play the full ad and handle the extra 2+2-second difference, which would add latency to the live buffer.
TESTDATA2= {
    "title": "Back to Back source period with CDAI ad long by <= 2 sec",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/BackToBackAd.mpd?live=true",
    "cmdlist": [
        "adtesting",
        # Adding a 32-second ad for the first 30 sec ad break in Period 1
        "advert add http://localhost:8080/content/ad_32s.mpd 32 0",
        # Adding a 12-second ad for the second 10 sec ad break in Period 2
        "advert add http://localhost:8080/content/ad_12s.mpd 12 1"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":400, "callback" : pts_restamp_utils.check_restamp},
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
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[2\] at Offset\[0.000000\]", "min": 0, "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:30000 adjustEndPeriodOffset:1", "min": 0, "max": 200},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 30, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:2 end period offset:10000 adjustEndPeriodOffset:1", "min": 0, "max": 200},        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 20, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 10000, endPeriodId: 4, endPeriodOffset: 0, \#Ads: 1", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[4\] at Offset\[0.000000\]", "min": 30, "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},        {"expect": r"Adbreak ended early. Terminating Ad playback", "min": 0, "max": 150},
        # Expectation for playing the last 2 seconds from the ad segment
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/1080p_016.m4s", "min": 0, "max": 200},
        # Ensuring the last additional 2 seconds from the 12-second ad is played
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/1080p_011.m4s", "min": 0, "max": 200, "count": 2},
        # Confirming the last segment fetched belongs to Period 2, indicating end of the test
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8007(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()
