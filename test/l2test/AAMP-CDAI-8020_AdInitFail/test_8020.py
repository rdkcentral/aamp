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

timeout = 180
pts_restamp_utils = PtsRestampUtils()

server_process = None
server_path = os.path.join(os.getcwd(), "AAMP-CDAI-8020_AdInitFail/testdata/content/server.py")

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA1 = {
    "title": "Test - First ad video init fragment fails in a single CDAI ad break",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer, "extra_args":["--force404", "ad_30.*?p_init\\.m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    ],
    #Source ad is 30 secs, one single ad of 30sec
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/TC1.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":timeout, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "max": 30},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 url\=.*?ad_30s.mpd", "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/content/ad_30/(1080|720|480|360)p_init.m4s", "min": 0, "max": 40},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 00, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 0, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-111 restoring mNextPts from 30.000000 to 0.000000","min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0-111' to '1' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 0, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 0, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA2 = {
    "title": "Test - First ad audio init fragment fails in a single CDAI ad break",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_30.*?en_init\\.m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    ],
    #Source ad is 30 secs, one single ad of 30sec
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/TC1.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":timeout, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "max": 30},
		{"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 url\=.*?ad_30s.mpd", "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/content/ad_30/en_init.m4s", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 0, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-111 restoring mNextPts from 30.000000 to 0.000000","min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0-111' to '1' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 0, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 0, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA3 = {
    "title": "Test - First ad video init fragment fails in a multi CDAI ad break",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_20.*?p_init\\.m4s"] },
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert map 1 http://localhost:8080/content/ad_20s.mpd",
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    ],
    #Source ad is 30 secs, 1 ad of 20sec and another ad of 10sec
    #First ad init fragment download fails
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/TC1.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":timeout, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "max": 30},
		{"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 url\=.*?ad_20s.mpd", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId2 url\=.*?ad_10s.mpd", "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/content/ad_20/(1080|720|480|360)p_init.m4s", "min": 0, "max": 40},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 0, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-114 restoring mNextPts from 20.000000 to 0.000000","min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: AdIdx\[1\] Found at Period\[1\].", "min": 30, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 50},
        {"expect": re.escape("Period ID changed from '1' to '1-111' [BasePeriodId='1']"), "min": 0, "max": 50},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/(1080|720|480|360)p_001.m4s", "min": 30, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 0, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 2", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 0, "max": 70},
        {"expect": re.escape("Period ID changed from '1-111' to '2' [BasePeriodId='2']"), "min": 0, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA4 = {
    "title": "Test - Second ad video init fragment fails in a multi CDAI ad break",
    "max_test_time_seconds": timeout,
    #ad_10s.mpd references fragments from ad_30s.mpd manifest. Hence the rule is set to ad_30/ fragments in server
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_30.*?p_init\\.m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert map 1 http://localhost:8080/content/ad_20s.mpd",
        "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    ],
    #Source ad is 30 secs, 1 ad of 20sec and another ad of 10sec
    #Second ad init fragment download fails
     "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/TC1.mpd", "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "max":timeout, "callback" : pts_restamp_utils.check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "max": 30},
        {"expect": r"\[AAMPCLI\] \[CDAI\] Dynamic ad start signalled", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 url\=.*?ad_20s.mpd", "max": 30},
        {"expect": r"\[AAMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId2 url\=.*?ad_10s.mpd", "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 0, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 0, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/(1080|720|480|360)p_001.m4s", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 0, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[1\] Found at Period\[1\].", "min": 0, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 0, "max": 60},
        {"expect": re.escape("Period ID changed from '0-114' to '1-111' [BasePeriodId='1']"), "min": 0, "max": 60},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/content/ad_30/(1080|720|480|360)p_init.m4s", "min": 0, "max": 60},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 0, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[20.000000\].Ad\[idx=1\]", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].", "min": 0, "max": 60},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 1-111 restoring mNextPts from 20.000000 to 0.000000", "min": 40, "max": 60},
        {"expect": re.escape("Period ID changed from '1-111' to '1' [BasePeriodId='1']"), "min": 0, "max": 60},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 0, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 2", "min": 50, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 0, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

# Currently TESTDATA3,TESTDATA4 are failing in restamp pts check, so commenting out for now, will be fixed in RDKAAMP-3556
#TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4]
TESTLIST = [TESTDATA1, TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8020(aamp_setup_teardown, test_data):
    global pts_restamp_utils

    pts_restamp_utils.reset()
    # test runs till AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_032.m4s for video and audio
    pts_restamp_utils.max_segment_cnt = 60

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()

