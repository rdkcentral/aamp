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


# Test Case5.1: Single source period with SCTE duration smaller than period duration
# Description: This test case verifies the handling  an ad break where the ad and scte duration is lesser than period duration
# Scenario:
# - The TestCase5.2.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
#   - Period 2: 10 sec duration with 5 seconds scte 
#   - Period 3: 10 seconds long, with no ads.
# - The expectation is period1 and 2 should play the remaining content from source .

TESTDATA1 = {
    "title": "SCTE and AD smaller than period duration",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TestCase5_2.mpd?live=true",
    "cmdlist": [
        # Adding a 10-second ad for the first 10 sec ad break in Period 1
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":300, "callback" : pts_restamp_utils.check_restamp},
        #Expectation for two ad break
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 50},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "max": 200},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:10000 adjustEndPeriodOffset:1", "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\]", "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "max": 200},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "max": 200},
        # Expectation for playing remaining content from period 1 ,after playing 15 secs of ad
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_024.m4s", "max": 200,"end_of_test": True},
    ]
}

# Test Case5.2: Back to back source period with SCTE and AD smaller than period duration
# Description: This test case verifies the handling of a back to back period with an ad break where the ad and scte duration is lesser than period duration
# Scenario:
# - The TestCase5.2.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
#   - Period 2: 10 sec duration with 5 seconds scte ,substituted with 5 sec ad  -
#   - Period 3: 10 seconds long, with no ads.
# - The expectation is period1 and 2 should play the remaining content from source .

TESTDATA2 = {
    "title": "Back to back scenario :SCTE and AD smaller than period duration",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TestCase5_2.mpd?live=true",
    "cmdlist": [
        "adtesting",
        # Adding a 10-second ad for the first 10 sec ad break in Period 1
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
        # Adding a 5-second ad for the second 5 sec ad break in Period 2 (10 sec period duration)
        "advert map 2 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":300, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]", "max": 150},
        # Detection of the second ad break in Period 2 with a duration of 5 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[5000\] isDAIEvent\[1\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 150},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1", "max": 200},
        # Expectation for the second ad (5 seconds) in the second ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "max": 200},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20, "max": 200},
        
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\]", "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:10000 adjustEndPeriodOffset:1", "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\].", "max": 200},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 20, "max": 200},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\]", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[OUTSIDE_ADBREAK_WAIT4ADS\].", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK_WAIT4ADS\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        # Starting Ad break 2
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "max": 200},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:2 end period offset:5000 adjustEndPeriodOffset:1", "max": 200},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 20, "max": 200},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 5000, endPeriodId: 2, endPeriodOffset: 5000, \#Ads: 1", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 30, "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[2\] at Offset\[5.000000\].", "max": 200},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 200},
        {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(360|480|720|1080)p_005.m4s", "max":200,"count":2},
        # Expectation for playing remaning content from period 
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_033.m4s", "max": 200},
        # Ensuring the remaning content played from period 2
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_045.m4s", "max": 200, "count": 2, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8008(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20
 
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()

