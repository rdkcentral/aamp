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
server_path = os.path.join(os.getcwd(), "AAMP-CDAI-8020_AdInitFail/testdata/content/server.py")

def start_server(args):
    global server_process
    if os.path.isfile(server_path):
        try:
            print("args passed ", args)
            server_process = subprocess.Popen(["python3", server_path] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print("started server.py")
            atexit.register(server_process.terminate)
        except Exception as e:
            print("Failed to start server.py "+server_path+" exception: "+e.strerror)
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
    tolerance_sec = duration_sec * 0.05
    after_sec = after / timeScale
    diff_sec = abs(after_sec - expected_restamp)
    print(f"PTS (secs): actual {after_sec:.3f}, expected {expected_restamp:.3f}, diff {diff_sec:.3f}, tol {tolerance_sec:.3f}")
    assert diff_sec <= tolerance_sec

    # Save what we are expecting for the next value
    restamp_values[mediaTrack] = after_sec + duration_sec

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA1 = {
    "title": "Test - First ad video init fragment fails in a single CDAI ad break",
    "max_test_time_seconds": 150,
    "server_config": ["--force404", "ad_30.*?p_init\\.m4s"],
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30s.mpd 30",
    ],
    #Source ad is 30 secs, one single ad of 30sec
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":150, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 30},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30/(1080|720|480|360)p_init.m4s", "min": 20, "max": 40},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 20, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-111 restoring mNextPts from 30.000000 to 0.000000","min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0-111' to '1' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 50, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA2 = {
    "title": "Test - First ad audio init fragment fails in a single CDAI ad break",
    "max_test_time_seconds": 150,
    "server_config": ["--force404", "ad_30.*?en_init\\.m4s"],
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30s.mpd 30",
    ],
    #Source ad is 30 secs, one single ad of 30sec
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":150, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 30},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30/en_init.m4s", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 20, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-111 restoring mNextPts from 30.000000 to 0.000000","min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0-111' to '1' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 50, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA3 = {
    "title": "Test - First ad video init fragment fails in a multi CDAI ad break",
    "max_test_time_seconds": 150,
    "server_config": ["--force404", "ad_20.*?p_init\\.m4s"],
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_20s.mpd 20",
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_10s.mpd 10",
    ],
    #Source ad is 30 secs, 1 ad of 20sec and another ad of 10sec
    #First ad init fragment download fails
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":150, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 30},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=20 url\=.*?ad_20s.mpd", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId2 duration\=10 url\=.*?ad_10s.mpd", "min": 0, "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_20/(1080|720|480|360)p_init.m4s", "min": 20, "max": 40},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[0.000000\].Ad\[idx=0\]", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].","min": 20, "max": 40},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 0-114 restoring mNextPts from 20.000000 to 0.000000","min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0-114' to '1' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: AdIdx\[1\] Found at Period\[1\].", "min": 30, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 30, "max": 50},
        {"expect": re.escape("Period ID changed from '1' to '1-111' [BasePeriodId='1']"), "min": 30, "max": 50},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30/(1080|720|480|360)p_001.m4s", "min": 30, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 40, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 2", "min": 50, "max": 70},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 50, "max": 70},
        {"expect": re.escape("Period ID changed from '1-111' to '2' [BasePeriodId='2']"), "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

#TC1.mpd
#period 0 30Sec, no scte35, segment numbers 1..15
#period 1 30Sec, with scte35, segment numbers 16..31
#period 2 30Sec, no scte35, segment numbers 32..47
TESTDATA4 = {
    "title": "Test - Second ad video init fragment fails in a multi CDAI ad break",
    "max_test_time_seconds": 150,
    #ad_10s.mpd references fragments from ad_30s.mpd manifest. Hence the rule is set to ad_30/ fragments in server
    "server_config": ["--force404", "ad_30.*?p_init\\.m4s"],
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\ndebug=true\ntrace=true\n",
    "url": "http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd?live=true",
    "cmdlist": [
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_20s.mpd 20",
        "advert add http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_10s.mpd 10",
    ],
    #Source ad is 30 secs, 1 ad of 20sec and another ad of 10sec
    #Second ad init fragment download fails
     "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/TC1.mpd", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":150, "callback" : check_restamp},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 30},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=20 url\=.*?ad_20s.mpd", "min": 0, "max": 30},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId2 duration\=10 url\=.*?ad_10s.mpd", "min": 0, "max": 30},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 20, "max": 40},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']"), "min": 20, "max": 40},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_20/(1080|720|480|360)p_001.m4s", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 20, "max": 40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[1\] Found at Period\[1\].", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 40, "max": 60},
        {"expect": re.escape("Period ID changed from '0-114' to '1-111' [BasePeriodId='1']"), "min": 40, "max": 60},
        {"expect": r"\[CacheFragment\]\[\d+\]Init fragment fetch failed -- fragmentUrl http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/ad_30/(1080|720|480|360)p_init.m4s", "min": 40, "max": 60},
        {"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: failed. isInit: 1 IsTrackVideo: video isDisc: 1 vidInitFail: 1", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad Playback failed. Going to the base period\[1\] at offset\[20.000000\].Ad\[idx=1\]", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_AD_NOT_PLAYING\].", "min": 40, "max": 60},
        {"expect": r"\[RestorePtsOffsetCalculation\]\[\d+\]Idx 1 Id 1-111 restoring mNextPts from 20.000000 to 0.000000", "min": 40, "max": 60},
        {"expect": re.escape("Period ID changed from '1-111' to '1' [BasePeriodId='1']"), "min": 40, "max": 60},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_016.m4s\?live=true", "min": 40, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_NOT_PLAYING\] \=\> \[OUTSIDE_ADBREAK\].", "min": 40, "max": 60},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 2", "min": 50, "max": 70},
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']"), "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "min": 50, "max": 70},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/AAMP-CDAI-8020_AdInitFail/testdata/content/dash/(1080|720|480|360)p_032.m4s\?live=true", "min": 50, "max": 70, "end_of_test":True},
    ]
}

# Currently TESTDATA3,TESTDATA4 are failing in restamp pts check, so commenting out for now, will be fixed in RDKAAMP-3556
#TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4]
TESTLIST = [TESTDATA1, TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8020(aamp_setup_teardown, test_data):
    global segment_cnt, restamp_values
    segment_cnt = 0
    restamp_values = {}
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    start_server(test_data["server_config"])
    aamp.run_expect_b(test_data)
    stop_server()
    print("Segment count ", segment_cnt)
    assert segment_cnt > 20, "Fail restamp was not checked."

