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

aamp = None
# Callbacks used by the tests
def send_command(match, command):
        aamp.sendline(command)  # Send the command

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

pts_restamp_utils = PtsRestampUtils()


# Test Case5.1: Single source period with SCTE duration smaller than period duration
# Description: This test case verifies the handling  an ad break where the ad and scte duration is lesser than period duration
# Scenario:
# - The TestCase5.2.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
#   - Period 2: 10 sec duration with 5 seconds scte
#   - Period 3: 30 seconds long, with no ads.
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
        {"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},
        #Expectation for two ad break
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 50},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:10000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\]"},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\]."},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']")},
        # Expectation for playing remaining content from period 1 ,after playing 15 secs of ad
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_024.m4s","end_of_test": True},
    ]
}

# Test Case5.2: Back to back source period with SCTE and AD smaller than period duration
# Description: This test case verifies the handling of a back to back period with an ad break where the ad and scte duration is lesser than period duration
# Scenario:
# - The TestCase5.2.mpd file contains three periods:
#   - Period 0: 30 seconds long, with no ads.
#   - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
#   - Period 2: 10 sec duration with 5 seconds scte ,substituted with 5 sec ad  -
#   - Period 3: 30 seconds long, with no ads.
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
        {"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]", "max": 150},
        # Detection of the second ad break in Period 2 with a duration of 5 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[5000\] isDAIEvent\[1\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 150},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1"},
        # Expectation for the second ad (5 seconds) in the second ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20,},

        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\]"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:10000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\]."},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 20},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\]"},
        # Starting Ad break 2
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:2 end period offset:5000 adjustEndPeriodOffset:1"},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 20},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 5000, endPeriodId: 2, endPeriodOffset: 5000, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[2\] at Offset\[5.000000\]."},
        # Transition back to outside ad break state after playback completion
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30},
        {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(360|480|720|1080)p_005.m4s","count":2},
        # Expectation for playing remaning content from period
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_033.m4s"},
        # Ensuring the remaning content played from period 2
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/1080p_045.m4s", "count": 2, "end_of_test": True},
    ]
}

#  TestCase3 : Test case for seeking on Back to back scenario :SCTE and AD smaller than period duration
#  Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# - The TestCase5.2.mpd file contains three periods:
# - Period 0: 30 seconds long, with no ads.
# - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
# - Period 2: 10 sec duration with 5 seconds scte ,substituted with 5 sec ad  -
# - Period 3: 30 seconds long, with no ads.
TESTDATA3 = {
    "title": "Seeking in Back to back scenario :SCTE and AD smaller than period duration",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TestCase5_2.mpd?live=true&livewindow=120",
    "cmdlist": [
        "adtesting",
        # Adding a 10-second ad for the first 10 sec ad break in Period 1
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
        # Adding a 5-second ad for the second 5 sec ad break in Period 2 (10 sec period duration)
        "advert map 2 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "max": 3},
        {"expect": pts_restamp_utils.LOG_LINE, "max":40, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]", "max": 150},
        # Detection of the second ad break in Period 2 with a duration of 5 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[5000\] isDAIEvent\[1\]", "max": 150},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 150},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1"},
        # Expectation for the second ad (5 seconds) in the second ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:10000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\]."},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 20},
        # Starting Ad break 2
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]"},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 20},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 5000, endPeriodId: 2, endPeriodOffset: 5000, \#Ads: 1"},
        # Seek to pos 30,
        {"expect": r"aamp pos: \[0..4[3-7]..*..1.00\]","callback_once": send_command, "callback_arg": "seek 30"},
        {"expect": r"aamp_Seek\(30.000000\)"},
        # Seek to 'period 1', started to download ad fragment 1 from 'period 1'
        {"expect": r"HttpRequestEnd: .*?http://localhost:8080/content/ad_20/(1080|720|480|360)p_001.m4s","min": 40, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 40, "max": 90},
        # Seek to pos 50,
        {"expect": r"aamp pos: \[0..6[3-7]..*..1.00\]","callback_once": send_command, "callback_arg": "seek 50","min": 80,"max": 100},
        {"expect": r"aamp_Seek\(50.000000\)"},
        # Seeked to the mid of 'period 1', started to download 26th fragment, 55 = 26th fragment download from 'period 1'
        {"expect": r"HttpRequestEnd: .*?http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s","min": 80, "max": 110},
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 80, "max": 110},
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_043.m4s", "min": 90, "end_of_test": True},
    ]
}

# TestCase4 : Test case for trickplay functionality, particularly performing trickplay within CDAI ads, and source period, covers rewind and the fast forward operations
# Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# - Period 0: 30 seconds long, with no ads.
# - Period 1: 30 seconds long, containing a single ad break. The SCTE break duration is set to 10 seconds, ad duration is 10 seconds
# - Period 2: 10 sec duration with 5 seconds scte ,substituted with 5 sec ad  -
# - Period 3: 30 seconds long, with no ads.
TESTDATA4 = {
    "title": "Back to back scenario :SCTE and AD smaller than period duration",
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\ndebug=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TestCase5_2.mpd?live=true&livewindow=120",
    "cmdlist": [
        "adtesting",
        # Adding a 10-second ad for the first 10 sec ad break in Period 1
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
        # Adding a 5-second ad for the second 5 sec ad break in Period 2 (10 sec period duration)
        "advert map 2 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd"
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "max": 3},
        {"expect": pts_restamp_utils.LOG_LINE, "max":70, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[10000\] isDAIEvent\[1\]"},
        # Detection of the second ad break in Period 2 with a duration of 5 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[5000\] isDAIEvent\[1\]"},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled"},
        # Expectation for the first ad (15 seconds) in the first ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1"},
        # Expectation for the second ad (5 seconds) in the second ad break
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 30},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 10, "max": 30},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 10000, endPeriodId: 1, endPeriodOffset: 10000, \#Ads: 1","min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[10.000000\].", "min": 20, "max": 40},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 20, "max": 40},
        # Starting Ad break 2
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "max": 200},
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 20, "max": 200},
        # Indicating successful placement of the second ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 5000, endPeriodId: 2, endPeriodOffset: 5000, \#Ads: 1", "max": 200},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[2\] at Offset\[5.000000\].", "min": 20, "max": 200},
        {"expect": re.escape("Period ID changed from '1-1' to '2' [BasePeriodId='2']"), "min": 20, "max": 200},
        #Trigerring rewind once the second ad completed.. happens at around 80s
        {"expect": r"aamp pos: \[0..7[2-6]..*..1.00\]","callback_once": send_command, "callback_arg": "rew 2"},
        {"expect": re.escape("Period ID changed from '3' to '2' [BasePeriodId='2']"), "min": 70, "max": 110},
        {"expect": re.escape("Period ID changed from '2' to '1-1' [BasePeriodId='2']"), "min": 70, "max": 110},
        #TODO: To be removed after iframes are fixed in 1-1 CDAI ad
        {"expect": re.escape("Period ID changed from '1-1' to '1' [BasePeriodId='1']"), "min": 70, "max": 110},
        {"expect": re.escape("Period ID changed from '1' to '0-114' [BasePeriodId='1']"), "min": 70, "max": 110},
        {"expect": re.escape("Period ID changed from '0-114' to '0' [BasePeriodId='0']"), "min": 70, "max": 110},
        #To ensure the 'playrate -2'
        {"expect": r"aamp pos: \[0..[3-4][0-9]...*..-2.00\]"},
        # Reach end of rewind
        {"expect": r"on BOS"},
        #Let the playback resume and reach 10 seconds before starting ff2
        {"expect": r"aamp pos: \[0..1[0-9]...*..1.00\]", "callback_once": send_command, "callback_arg": "ff 2", "min": 100, "max": 140},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 120, "max": 160},
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 120, "max": 160},
        # Uncomment once we fix the iframes on 1-1 ads
        {"expect": re.escape("Period ID changed from '1' to '1-1' [BasePeriodId='2']"), "min": 120, "max": 160},
        {"expect": re.escape("Period ID changed from '1-1' to '2' [BasePeriodId='2']"), "min": 120, "max": 160},
        {"expect": re.escape("Period ID changed from '2' to '3' [BasePeriodId='3']"), "min": 120, "max": 160},
        # End test once the playback reaches normal speed
        {"expect": r"aamp pos: \[0..*..1.00\]", "min": 160, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8008(aamp_setup_teardown, test_data):
    global pts_restamp_utils,aamp

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
    pts_restamp_utils.check_num_segments()


