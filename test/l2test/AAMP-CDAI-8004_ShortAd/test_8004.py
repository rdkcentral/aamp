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
from inspect import getsourcefile
import pytest
import re
from l2test_pts_restamp import PtsRestampUtils
from l2test_window_server import WindowServer

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

pts_restamp_utils = PtsRestampUtils()

#Test Case 2.1: Single Source Period with Multiple CDAI Ad Replacements : Refer TC :https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
#Period 2: Contains a 120-second ad, replaced by multiple ads of 30, 40, 30, and 20 seconds.
TESTDATA1 = {
    "title": "Test1 Current ad duration same as source ad duration",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ntrace=true\nprogress=true\n",
    "archive_url": archive_url,
    "archive_server": { 'server_class': WindowServer},
    "url": "http://localhost:8080/content/main.mpd?live=true",
    "cmdlist": [
        "advert map 2 http://localhost:8080/content/ad_30s.mpd",
        "advert map 2 http://localhost:8080/content/ad_40s.mpd",
        "advert map 2 http://localhost:8080/content/ad-30s.mpd",
        "advert map 2 http://localhost:8080/content/ad_20s.mpd"
    ],
    #Source ad is 120 secs, all ads will sum up to 120 secs
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/main.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":300, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[120000\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId1 url\=.*?ad_30s.mpd", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 url\=.*?ad_40s.mpd", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId3 url\=.*?ad-30s.mpd", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId4 url\=.*?ad_20s.mpd", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] => \[IN_ADBREAK_AD_PLAYING\].", "min": 100, "max": 160},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 100, "max": 160},
        {"expect": re.escape("Period ID changed from '1' to '0-111' [BasePeriodId='2']"), "min": 100, "max": 180},
        {"expect": re.escape("Period ID changed from '0-111' to '1-112' [BasePeriodId='2']"), "min": 100, "max": 200},
        {"expect": re.escape("Period ID changed from '1-112' to '2-113' [BasePeriodId='2']"), "min": 150, "max": 220},
        {"expect": re.escape("Period ID changed from '2-113' to '3-114' [BasePeriodId='2']"), "min": 150, "max": 250},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 120000, endPeriodId: 3, endPeriodOffset: 0, \#Ads: 4", "min": 240, "max": 260},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] => \[OUTSIDE_ADBREAK\].", "min": 240, "max": 260},
        {"expect": re.escape("Period ID changed from '3-114' to '3' [BasePeriodId='3']"), "min": 240, "max": 270},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_132.m4s\?live=true", "min": 240, "max": 270},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_133.m4s\?live=true", "min": 240, "max": 270, "end_of_test":True},
    ]
}

TESTDATA2 = {
    "title": "Test2 Present ad duration less than source ad duration",
    "max_test_time_seconds": 400,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ntrace=true\nprogress=true\n",
    "archive_url": archive_url,
    "archive_server": { 'server_class': WindowServer},
    "url": "http://localhost:8080/content/main.mpd?live=true",
    "cmdlist": [
        "advert map 2 http://localhost:8080/content/ad_30s.mpd",
        "advert map 2 http://localhost:8080/content/ad_40s.mpd",
        "advert map 2 http://localhost:8080/content/ad-30s.mpd",
    ],
    #Source ad is 120 secs but all ads will sum up to 100 secs
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/main.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":400, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[120000\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId1 url\=.*?ad_30s.mpd", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 url\=.*?ad_40s.mpd", "max": 150},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId3 url\=.*?ad-30s.mpd", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 100, "max": 160},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 100, "max": 160},
        {"expect": re.escape("Period ID changed from '1' to '0-111' [BasePeriodId='2']"), "min": 100, "max": 180},
        {"expect": re.escape("Period ID changed from '0-111' to '1-112' [BasePeriodId='2']"), "min": 100, "max": 200},
        {"expect": re.escape("Period ID changed from '1-112' to '2-113' [BasePeriodId='2']"), "min": 150, "max": 220},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 220, "max": 240},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 100000, endPeriodId: 2, endPeriodOffset: 100000, \#Ads: 3", "min": 220, "max": 260},
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
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20
    
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))

    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()

