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

# Starts simlinear, a web server for serving ABR test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
# Also see README.md

import os
import sys
from inspect import getsourcefile
import pytest
import subprocess
from l2test_pts_restamp import PtsRestampUtils

pts_restamp_utils = PtsRestampUtils()

httpserver_process = None
TEST_PATH = os.path.abspath(os.path.join('.'))
HTTP_PORT = os.environ["L2_HTTP_PORT"] if "L2_HTTP_PORT" in os.environ else '8080'
HTTPSERVER_DATA_PATH = os.path.join(TEST_PATH, 'httpdata')
HTTPSERVER_CMD = ['python3', '-m', 'http.server', HTTP_PORT, '-d', HTTPSERVER_DATA_PATH]
use_local_httpserver = True

if "TEST_2001_LOCAL_ADS" in os.environ and os.environ["TEST_2001_LOCAL_ADS"] == 'true':
    AD_HOST = "http://localhost:" + HTTP_PORT
    print("Local Ads")
else:
    if "TEST_2001_ADS_PATH" in os.environ:
        AD_HOST = os.environ["TEST_2001_ADS_PATH"]
    else:
        AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"

    use_local_httpserver = False
    print("Remote Ads")


AD_URLS = [
    # skip placement
    "file://skip",

    # 40sec - ad1 (telecoms)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd",

    # 60sec - ad2 (lifeboat)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd",

    # 30sec - ad3 (bet)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd",

    # 10sec - ad4 (movie)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd",

    # 20sec - ad5 (witness)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad5/hsar1195-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad2/d14dff37-36d1-4850-aa9d-7d948cbf1fc6/1628318436178/AD/HD/manifest.mpd",

    # 25sec - ad6 (one)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad6/hsar1103-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD/manifest.mpd",

    # 5sec - ad7 (golf)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd",
    ]

test_time = 300

def start_httpserver():
    if use_local_httpserver:
        global httpserver_process
        if not os.path.exists(HTTPSERVER_DATA_PATH):
            print("ERROR File does not exist {} Check setup.".format(HTTPSERVER_DATA_PATH))
            sys.exit(os.EX_SOFTWARE)
        try:
                httpserver_process = subprocess.Popen(HTTPSERVER_CMD, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception as e:
            print("Failed to start http server {}, Check for instance already running".format(HTTPSERVER_CMD, e))
            sys.exit(os.EX_SOFTWARE)   # Return non-zero

def stop_httpserver():
    global httpserver_process

    if httpserver_process:
        print("Terminating httpserver")
        httpserver_process.terminate()
        httpserver_process = None


TESTDATA0 = {
"title": "Linear CDAI TESTDATA0 alternating",
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": test_time,
"aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\nenablePTSReStamp=true\n",
"cmdlist": [
    "advert add " + AD_URLS[3] + " 30",
    "advert add " + AD_URLS[0] + " 20",  # Not played
    "advert add " + AD_URLS[5] + " 20",
    "advert add " + AD_URLS[0] + " 10",  # Not played
    "advert add " + AD_URLS[3] + " 30",  # repeating ad
    "advert add " + AD_URLS[0],
    "advert list",
    ],
"expect_list": [
    # ( string, min time seconds, max time seconds)

    # Period 881036617 - 30sec
    {"expect": r"Found CDAI events for period 881036617"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036617\] Duration\[test_time00\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036617 added\[Id=ad0,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad0\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036617\] AdIdx\[0\] Found at Period\[881036617\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036617\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad0\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036617\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036617\] to Queue"},

    # Period 881036618 - 20sec
    {"expect": r"Found CDAI events for period 881036618"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036618\] Duration\[20000\] isDAIEvent[1]"},

    # Period 881036619 - 20sec
    {"expect": r"Found CDAI events for period 881036619"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036619\] Duration\[20000\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036619 added\[Id=ad2,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036619\] AdIdx\[0\] Found at Period\[881036619\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036619\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad2\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036619\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036619\] to Queue"},

    # Period 881036620 - 10sec
    {"expect": r"Found CDAI events for period 881036620"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036620\] Duration\[10000\] isDAIEvent[1]"},

    # Period 881036621 - 30sec
    {"expect": r"Found CDAI events for period 881036621"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036621\] Duration\[test_time00\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036621 added\[Id=ad4,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad4\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036621\] AdIdx\[0\] Found at Period\[881036621\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036621\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad4\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036621\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036621\] to Queue"},


    {"expect": r"\[CDAI\] Removing the period\[881036617\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036618\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036619\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036620\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036621\] from mAdBreaks"},
    {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
    {"expect": r"GetFile.*?periodid-881036623-", "end_of_test":True} # Finish when we start fetching for this period

    ]
}


TESTDATA1 = {
"title": "Linear CDAI TESTDATA1 back2back",
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": test_time,
"aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\nenablePTSReStamp=true\n",
"cmdlist": [
    "advert add " + AD_URLS[3] + " 30",
    "advert add " + AD_URLS[5] + " 20",
    "advert add " + AD_URLS[5] + " 20",
    "advert add " + AD_URLS[4] + " 10",
    "advert add " + AD_URLS[3] + " 30",
    "advert add " + AD_URLS[0],
    "advert list",
    ],
"expect_list": [

    # Period 881036617 - 30sec
    {"expect": r"Found CDAI events for period 881036617"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036617\] Duration\[test_time00\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036617 added\[Id=ad0,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad0\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036617\] AdIdx\[0\] Found at Period\[881036617\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036617\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad0\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036617\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036617\] to Queue"},

    # Period 881036618 - 20sec
    {"expect": r"Found CDAI events for period 881036618"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036618\] Duration\[20000\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036618 added\[Id=ad1,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad1\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036618\] AdIdx\[0\] Found at Period\[881036618\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036618\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad1\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036618\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036618\] to Queue"},


    # Period 881036619 - 20sec
    {"expect": r"Found CDAI events for period 881036619"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036619\] Duration\[20000\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036619 added\[Id=ad2,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036619\] AdIdx\[0\] Found at Period\[881036619\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036619\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad2\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036619\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036619\] to Queue"},

    # Period 881036620 - 10sec
    {"expect": r"Found CDAI events for period 881036620"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036620\] Duration\[10000\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036620 added\[Id=ad3,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad3\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036620\] AdIdx\[0\] Found at Period\[881036620\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036620\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad3\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036620\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036620\] to Queue"},

    # Period 881036621 - 30sec
    {"expect": r"Found CDAI events for period 881036621"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036621\] Duration\[test_time00\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036621 added\[Id=ad4,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad4\]"},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036621\] AdIdx\[0\] Found at Period\[881036621\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036621\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad4\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036621\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036621\] to Queue"},


    {"expect": r"\[CDAI\] Removing the period\[881036617\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036618\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036619\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036620\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036621\] from mAdBreaks"},

    {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
    {"expect": r"GetFile.*?periodid-881036622-", "end_of_test":True} # Finish when we start fetching for this period
    ]
}

#TestCase 2.2 - Back to back Source Periods with Multiple CDAI Ad Substitutions: Refer to TC - AAMP Client-side Dynamic Ad Use Cases
#Period 881036617: 30-second ad replaced with 20-second and 10-second ads
#Period 881036618: 20-second ad replaced with two 10-second ads

TESTDATA2 = {
"title": "TC:2.2-Back to back source period with multiple CDAI ad substitution",
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": test_time,
"aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\nenablePTSReStamp=true\n",
"cmdlist": [

    "advert add " + AD_URLS[5] + " 20",
    "advert add " + AD_URLS[4] + " 10",
    "advert add " + AD_URLS[5] + " 10",
    "advert add " + AD_URLS[5] + " 10",
    "advert add " + AD_URLS[0],
    "advert list",
    ],
"expect_list": [

    # Period 881036617 - 30sec - Substitited with 20sec and 10sec ad
    {"expect": r"Found CDAI events for period 881036617"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036617\] Duration\[test_time00\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036617 added\[Id=ad1,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad1\]"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]"},
    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036617\] AdIdx\[0\] Found at Period\[881036617\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036617\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad1\] to Queue."},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad2\] to Queue."},
    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036617\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036617\] to Queue"},

    # Period 881036618 - 20sec - Substituted with two 10 sec ads
    {"expect": r"Found CDAI events for period 881036618"},
    {"expect": r"\[CDAI\] Found Adbreak on period\[881036618\] Duration\[20000\] isDAIEvent[1]"},
    {"expect": r"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036618 added\[Id=ad1,"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad1\]"},
    {"expect": r"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]"},
    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036618\] AdIdx\[0\] Found at Period\[881036618\]"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036618\] to Queue"},
    {"expect": r"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad1\] to Queue."},

    {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036618\] FINISHED"},
    {"expect": r"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036618\] to Queue"},


    {"expect": r"\[CDAI\] Removing the period\[881036617\] from mAdBreaks"},
    {"expect": r"\[CDAI\] Removing the period\[881036618\] from mAdBreaks"},

    {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
    {"expect": r"GetFile.*?periodid-881036622-", "end_of_test":True} # Finish when we start fetching for this period
    ]
}
TESTLIST = [TESTDATA0, TESTDATA1, TESTDATA2]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

@pytest.mark.ci_test_set
def test_2001(aamp_setup_teardown, test_data):
    global pts_restamp_utils

    start_httpserver() # Only starts if being used

    pts_restamp_utils.reset()

    # At least this many segs must get restamped. A check to make sure that the restamp is getting checked
    pts_restamp_utils.max_segment_cnt = 100

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()

    stop_httpserver()