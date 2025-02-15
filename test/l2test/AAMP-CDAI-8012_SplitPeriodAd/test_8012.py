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
from l2test_window_server import WindowServer

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"



TESTDATA1 = {
    "title": "Test1 Split period ad",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 20 seconds -> 20 second ad, 2nd period 10 seconds -> 20 second ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds -> 20 second ad, 5th period no ads till end of content
    #  current behavior : after playing 20 second ad in 2nd period we see 3rd period (base content 01.00 to 01.10 timestamp) playback for 10 seconds .
    #  expected behavior : after playing 20 second ad in 2nd period we should see 4th period ad playback for 20 seconds
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_20s.mpd",
    "advert map 3 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] CDAIPeriod\[adId2 - 20000\]", "max": 150},
        #Expect not to download any fragments from 3rd period
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_061.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "max": 150, "end_of_test":True},
    ]
}

TESTDATA2 = {
    "title": "Test2 Split period ad with short ad",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 20 seconds -> 20 second ad, 2nd period 10 seconds -> 15 second ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds -> 20 second ad, 5th period no ads till end of content
    #  current behavior : after playing 15 second ad in 2nd period we see 3rd period (base content 01.00 to 01.10 timestamp) playback for 10 seconds .
    #  expected behavior : after playing 15 second ad in 2nd period we should see 5 seconds of base content of 3rd period then 4th period ad playback for 20 seconds
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split2.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_15s.mpd",
    "advert map 3 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split2.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  \d+\] CDAIPeriod\[adId2 - 15000\]", "max": 150},
        #Expect not to download any fragments from 3rd period
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_061.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "max": 150, "end_of_test":True},
    ]
}

#Split period single ad across multiple periods
TESTDATA3 = {
    "title": "Test3 Split period ad across multiple periods ",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 10 seconds -> 30 second ad, 2nd period 10 seconds ->no ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds ->  no ad, 5th period no ads till end of content
    #  Expected behavior : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] BasePeriod\[1 -  10000\] CDAIPeriod\[adId1 - 30000\]", "max": 150},
        #Expect not to download fragments from 3rd period beginning
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true", "not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50, "max": 150,"end_of_test":True},
    ]
}

#Split period multiple ads across multiple periods
TESTDATA4 = {
    "title": "Test3 Split period ad across multiple periods ",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 10 seconds -> 20 second and 10 second ad, 2nd period 10 seconds ->no ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds ->  no ad, 5th period no ads till end of content
    #  Expected behavior : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] BasePeriod\[1 -  10000\] CDAIPeriod\[adId1 - 20000\] CDAIPeriod\[adId2 - 10000\]", "max": 150},
        #Expect not to download fragments from 3rd period beginning
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true","not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50, "max": 150,"end_of_test":True},
    ]
}

#Split period multiple ads across multiple periods with no restamp
TESTDATA5 = {
    "title": "Test5 Split period ad without pts restamp",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 20 seconds -> 20 second ad, 2nd period 10 seconds -> 20 second ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds -> 20 second ad, 5th period no ads till end of content
    #  current behaviour : after playing 20 second ad in 2nd period we see 3rd period (base content 01.00 to 01.10 timestamp) playback for 10 seconds .
    #  expected behaviour : after playing 20 second ad in 2nd period we should see 4th period ad playback for 20 seconds
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=false\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_20s.mpd",
    "advert map 3 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] CDAIPeriod\[adId2 - 20000\]", "max": 150},
        #Expect not to download any fragments from 3rd period
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_061.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "max": 150, "end_of_test":True},
    ]
}

TESTDATA6 = {
    "title": "Test6 Split period ad with short ad without pts restamp",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 20 seconds -> 20 second ad, 2nd period 10 seconds -> 15 second ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds -> 20 second ad, 5th period no ads till end of content
    #  current behaviour : after playing 15 second ad in 2nd period we see 3rd period (base content 01.00 to 01.10 timestamp) playback for 10 seconds .
    #  expected behaviour : after playing 15 second ad in 2nd period we should see 5 seconds of base content of 3rd period then 4th period ad playback for 20 seconds
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=false\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split2.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_15s.mpd",
    "advert map 3 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split2.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  \d+\] CDAIPeriod\[adId2 - 15000\]", "max": 150},
        #Expect not to download any fragments from 3rd period
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_061.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "max": 150, "end_of_test":True},
    ]
}

#Split period ad across multiple periods
TESTDATA7 = {
    "title": "Test7 Split period ad across multiple periods without pts restamp ",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 10 seconds -> 30 second ad, 2nd period 10 seconds ->no ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds ->  no ad, 5th period no ads till end of content
    #  Expected behaviour : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=false\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] BasePeriod\[1 -  10000\] CDAIPeriod\[adId1 - 30000\]", "max": 150},
        #Expect not to download fragments from 3rd period beginning
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true", "not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50, "max": 150,"end_of_test":True},
    ]
}

#Split period ad across multiple periods
TESTDATA8 = {
    "title": "Test8 Split period ad across multiple periods without pts restamp",
    # Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
    #  0th period 30s no ads, 1st period 10 seconds -> 30 second ad, 2nd period 10 seconds ->no ad, 3rd period 10 seconds -> no ad, 4th period 20 seconds ->  no ad, 5th period no ads till end of content
    #  Expected behaviour : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
    "max_test_time_seconds": 300,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\nenablePTSReStamp=false\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    "advert list",
    ],
    # needs to be expanded.
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd", "max": 3},
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[\d\] Duration\[\d+\]", "max": 150},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[\w+\] \=\> \[\w+\].", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period. BasePeriod\[2 -  10000\] BasePeriod\[3 -  10000\] BasePeriod\[1 -  10000\] CDAIPeriod\[adId1 - 20000\] CDAIPeriod\[adId2 - 10000\]", "max": 150},
        #Expect not to download fragments from 3rd period beginning
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true", "not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_033.m4s\?live=true", "min": 50, "max": 150},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50, "max": 150,"end_of_test":True},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5,TESTDATA6,TESTDATA7,TESTDATA8]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8012(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)


