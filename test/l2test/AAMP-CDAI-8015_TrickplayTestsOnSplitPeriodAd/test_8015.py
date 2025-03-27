#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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
import re
import pytest
from l2test_window_server import WindowServer

###############################################################################
aamp = None
# Callbacks used by the tests
def send_command(match, command):
        aamp.sendline(command)  # Send the command

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

# Test case for seeking on spot-level split period ads
# Restamp checks are purposefully not used as they seek will cause a jump in pts and thus cause the test to fail
TESTDATA1 = {
    "title": "Seek on CDAI split period ad with short ad",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  'period 0' => 30s no ads,
    #  'period 1' => 20 seconds with 20 second ad
    #  'period 2' => 10 seconds -> 15 second ad (split period)
    #  'period 3' => 10 seconds -> no ad,
    #  'period 4' => 20 seconds -> 20 second ad,
    #  'period 5' => no ads till end of content
    #  Behaviour :
    #  1. After starting Adbreak '4', from the 'period 4', seek to "60" ('period 2' end), so it jumps to 6th fragment of 'period 2',
    #  as first 5 fragments from the 'period 2' is getting seeked
    #  2. Again seeked to "65", middle of 'period 3', seek to 33rd fragment from the base content

    "max_test_time_seconds": 100,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split2.mpd?live=true",
    "cmdlist": [
        "adtesting",
        "advert map 1 http://localhost:8080/content/ad_20s.mpd",
        "advert map 2 http://localhost:8080/content/ad_15s.mpd",
        "advert map 4 http://localhost:8080/content/ad_20s.mpd",
        "advert list",
        ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split2.mpd","min":0,"max":3},
        # Starting adbreak in 'period 1'
        {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 1 added\[Id\=adId1, url\=http://localhost:8080/content/ad_20s.mpd, durationMs\=20000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\].","min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"),"min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=0\] \[period=1\]"},
        # Placement of current ad in the 'period 1' completed
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1,"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[2\] at Offset\[0.000000\]."},
        # Starting adbreak in 'period 2'
        {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 2 added\[Id\=adId2, url\=http://localhost:8080/content/ad_15s.mpd, durationMs\=15000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\].","min": 30, "max": 50},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='2']"),"min": 30, "max": 50},
        # Ensure that Start and end fragments of Ad Id-2 downloaded
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(1080|720|480|360)p_001.m4s"},
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(1080|720|480|360)p_008.m4s"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=0\] \[period=2\]"},
        {"expect": r"Detected split period.","min":50,"max":70},
        # Ads started in 'period 2', overlapped in 'period 3', Ad started at 'period 2', ends in 'period 3'
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 15000, endPeriodId: 3, endPeriodOffset: 5000, \#Ads: 1,"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[3\] at Offset\[5.000000\]."},
        {"expect": re.escape("Period ID changed from '1-114' to '3' [BasePeriodId='3']"),"min":50,"max":70},
        # Check for ad execution from the 'period 4'
        {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 4 added\[Id\=adId3, url\=http://localhost:8080/content/ad_20s.mpd, durationMs\=20000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[4\] AdIdx\[0\] Found at Period\[4\]."},
        {"expect": re.escape("Period ID changed from '3' to '2-114' [BasePeriodId='4']"),"min":50,"max":70},
        # Seek operation to test playback from the shared ad across periods
        # Navigate to timestamp 60 seconds (Period 2nd end).
        {"expect": r"aamp pos: \[\d+\..6[5-9]..\d+\..*\]", "callback_once": send_command, "callback_arg": "seek 60", "min": 65, "max": 70},
        {"expect": r"aamp_Seek\(60.000000\)"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[3\]","min": 65, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=> \[IN_ADBREAK_AD_PLAYING\]","min": 65, "max": 70},
        # Period 2 => 10secs, so while seeking it jumps to download the 6th fragment(1st five fragments position is seeked)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(1080|720|480|360)p_006.m4s","min": 65, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[3\] at Offset\[5.000000\].","min": 65, "max": 70},
        {"expect": re.escape("Period ID changed from '1-114' to '3' [BasePeriodId='3']"),"min": 65, "max": 70},
        # Continue to download fragment from 'period 3'
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s\?live=true","min": 65, "max": 70},
        # Command: Seek to timestamp 65 seconds (middle of Period 3)
        {"expect": r"aamp pos: \[\d+\..7[5-9]..\d+\..*\]", "callback_once": send_command, "callback_arg": "seek 65", "min": 70, "max":100},   
        {"expect": r"aamp_Seek\(65.000000\)"},
        # Verify playback of base content in Period 3 (33rd fragment) (seek to 65 = 33 th fragment from base content)
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s","min": 70, "max":100},
        # Starting adbreak in 'period 4'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[4\] AdIdx\[0\] Found at Period\[4\].","min": 70, "max":100},
        {"expect": re.escape("Period ID changed from '3' to '2-114' [BasePeriodId='4']"),"min": 70, "max":100},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 4, duration: 20000, endPeriodId: 5, endPeriodOffset: 0, \#Ads: 1","min": 70, "max":100},
        {"expect": re.escape("Period ID changed from '2-114' to '5' [BasePeriodId='5']"),"min": 70, "max":100},
        # Starting to playback basePeriod 5,
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_047.m4s","min": 70,"end_of_test": True},
        ]
    }

#Test case for trickplay functionality, particularly performing trickplay within split periods with ads, covers rewind and the fast forward operations
# Restamp checks are purposefully not used as they seek will cause a jump in pts and thus cause the test to fail
TESTDATA2 = {
    "title": "Trickplay on CDAI Split period ad with short ad",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  'period 0' => 30s no ads,
    #  'period 1' => 20 seconds with 20 second ad
    #  'period 2' => 10 seconds -> 15 second ad (split period)
    #  'period 3' => 10 seconds -> no ad,
    #  'period 4' => 20 seconds -> 20 second ad,
    #  'period 5' => no ads till end of content
    #  expected behaviour : after playing 20 second ad in the 4th period, performing 'rew 2', and once it reaches beginning of the stream from the tsb,
    #  then performing 'ff 2' operation.
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    # Set livewindow to 90 seconds to have a longer manifest for trickplay
    "url": "http://localhost:8080/content/split.mpd?live=true&livewindow=120",
    "cmdlist": [
        "adtesting",
        "advert map 1 http://localhost:8080/content/ad_20s.mpd",
        "advert map 2 http://localhost:8080/content/ad_15s.mpd",
        "advert map 4 http://localhost:8080/content/ad_20s.mpd",
        "advert list",
        ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split.mpd"},
        #Starting adbreak in 'period 1'
        {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 1 added\[Id\=adId1, url\=http://localhost:8080/content/ad_20s.mpd, durationMs\=20000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]."},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx\=0\] \[period\=1\]"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1"},
        #Starting adbreak in 'period 2'
        {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 2 added\[Id\=adId2, url\=http://localhost:8080/content/ad_15s.mpd, durationMs\=16000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]."},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='2']")},
        {"expect": r"Detected split period"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 16000, endPeriodId: 3, endPeriodOffset: 6000, \#Ads: 1,"},
        {"expect": re.escape("Period ID changed from '1-114' to '3' [BasePeriodId='3']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[4\] AdIdx\[0\] Found at Period\[4\]."},
        {"expect": re.escape("Period ID changed from '3' to '2-114' [BasePeriodId='4']")},
        #Trigerring rewind once the second ad completed
        {"expect": r"aamp pos: \[0..7[1-5]..*..1.00","callback_once": send_command, "callback_arg": "rew 2"},
        {"expect": r"aamp_SetRate rate\(1.000000\)->\(-2.000000\)"},
        # rewinds ad in 'period 4'
        {"expect": re.escape("Period ID changed from '2-114' to '3' [BasePeriodId='3']"),"min": 50, "max": 100},
        # rewinding source period in 'period 3'
        {"expect": re.escape("Period ID changed from '3' to '1-114' [BasePeriodId='3'"),"min": 50, "max": 100},
        # rewinding 15s ad in 'period 1'
        {"expect": re.escape("Period ID changed from '1-114' to '0-114' [BasePeriodId='1']"),"min": 50, "max": 100},
        {"expect": re.escape("Period ID changed from '0-114' to '0' [BasePeriodId='0']"),"min": 50, "max": 100},
        # rewinding source content 'period 0'
        # Reached the beginning of the Tsb
        {"expect": r"BOS"},
        # Starting to play from the start 
        {"expect": r"aamp pos: \[0..1[4-8]..*..1.00","callback_once": send_command, "callback_arg": "ff 2","min": 120, "max": 130},
        {"expect": r"aamp_SetRate rate\(1.000000\)->\(2.000000\)"},
        # forwarding ad break in 'period 1'
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"),"min": 120, "max": 140},
        # forwarding ad break in 'period 2' overlapped in 'period 3'
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='2']"),"min": 120, "max": 140},
        {"expect": re.escape("Period ID changed from '1-114' to '3' [BasePeriodId='3']"),"min": 120, "max": 140},
        # forwarding ad break in 'period 4'
        {"expect": re.escape("Period ID changed from '3' to '2-114' [BasePeriodId='4']"),"min": 120, "max": 140},
        {"expect": re.escape("Period ID changed from '2-114' to '5' [BasePeriodId='5']"),"min": 120, "max": 140},
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_096.m4s\?live=true","end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8015(aamp_setup_teardown, test_data):
    global aamp
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
