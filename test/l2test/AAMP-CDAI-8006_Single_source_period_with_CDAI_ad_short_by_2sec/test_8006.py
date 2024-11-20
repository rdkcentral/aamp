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


# Test Case3.1: Ad Short by <= 2 Seconds
# Description: This test case verifies ad insertion behavior when the actual ad duration is slightly shorter than the SCTE break duration.
# Scenario:
# - The TC1.mpd file consists of 3 periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break.
#     - The SCTE break duration is 30 seconds, but the actual ad duration is 28.8 seconds.
#     - A 30-second ad is substituted with a 28.8-second ad.
#   - Period 2: 30 seconds long, with no ads.

TESTDATA1 = {
    "title": "Ad Short by <= 2 sec",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        #To add a 28 -second ad to the specified ad break in the content
        "advert add http://localhost:8080/content/ad_28s.mpd 28 0",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":300, "callback" : pts_restamp_utils.check_restamp},
        # Confirmation of finding an ad break in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=28", "min": 0, "max": 50},
        # State change indicating the transition into ad playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        # Confirmation that the ad break has started at Period 1
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Period ID change from base content to ad content
        {"expect": re.escape("Period ID changed from '0' to '0-1' [BasePeriodId='1']"), "min": 20, "max": 60},
        # After ad completion transition back to normal playback
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 80},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 28800, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 70},
        #After completing the ad, confirm the transition back to Period 2 (base content)
	{"expect": re.escape("Period ID changed from '0-1' to '2' [BasePeriodId='2']"), "min": 20, "max": 180},
        #Ensure upto last segment of ad is fetched or not
	{"expect":r"aamp url:0,0,0,1.920000,https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest/track-video-repid-LE1-tc--frag-14.mp4","min":20,"max":180},
        #Ensure fragments are not fetched from base period 1
	{"expect":r"aamp url:0,0,0,1.920000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_016.m4s","min":20,"max":180,"not_expected" : True},
	#Ensure fragments starts downloaded from base period 2
	{"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_031.m4s","min":20,"max":180},
         #End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test":True},
    ]
}


# Test Case3.2: Back-to-Back Source Periods with CDAI Ad Short by <= 2 Seconds
# Description: This test case verifies the handling of back-to-back ad breaks with slightly shorter ad durations than the defined SCTE break durations.
# Scenario:
# - Periods used for testing:
# - Period 0 :30 sec long .No ad
# - Period 1: 30 seconds long. A 30-second ad is substituted with a 28.8-second ad.
# - Period 2: 10 seconds long. A 10-second ad is substituted with a 9.2-second ad.
# - Period 3: 30 seconds long.No
# - The expectation is to confirm the correct handling of CDAI ad breaks for consecutive periods.
########### Test Failed : After playing 28 sec and 9 sec ad ,play state is looping
TESTDATA2= {
    "title": "Back to Back source period with CDAI ad short by <= 2 sec",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/BackToBackAd.mpd?live=true",
    "cmdlist": [
        # Adding a 32-second ad for the first 30 sec ad break in Period 1
        "advert add http://localhost:8080/content/ad_28s.mpd 28 0",
        # Adding a 12-second ad for the second 10 sec ad break in Period 2
        "advert add http://localhost:8080/content/ad_9s.mpd 9 1"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":300, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 150},
        # Detection of the second ad break in Period 2 with a duration of 10 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[10000\]", "min": 0, "max": 150},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        # Expectation for the first ad (32 seconds) in the first ad break
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=28", "min": 0, "max": 200},
        # Expectation for the second ad (12 seconds) in the second ad break
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 duration\=9", "min": 0, "max": 200},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 200},
        {"expect": re.escape("Period ID changed from '0' to '0-P1' [BasePeriodId='1']"), "min": 20, "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-P1' to '2' [BasePeriodId='2']"), "min": 20, "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 32000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 0, "max": 200},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 0, "max": 200},
        # Confirming the last segment fetched belongs to Period 2, indicating end of the test
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 200, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8006(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()
