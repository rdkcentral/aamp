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

httpserver_process = None
TEST_PATH = os.path.abspath(os.path.join('.'))
HTTP_PORT = os.environ["L2_HTTP_PORT"] if "L2_HTTP_PORT" in os.environ else '8080'
HTTPSERVER_DATA_PATH = os.path.join(TEST_PATH, 'httpdata')
HTTPSERVER_CMD = ['python3', '-m', 'http.server', HTTP_PORT, '-d', HTTPSERVER_DATA_PATH]
use_local_httpserver = True

if "TEST_8013_LOCAL_ADS" in os.environ and os.environ["TEST_8013_LOCAL_ADS"] == 'true':
    AD_HOST = "http://localhost:" + HTTP_PORT
    print("Local Ads")
else:
    if "AAMP-CDAI-8013-WaitingForAdToBeAdded" in os.environ:
        AD_HOST = os.environ["AAMP-CDAI-8013-WaitingForAdToBeAdded"]
    else:
        AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"

    use_local_httpserver = False
    print("Remote Ads")


AD_URLS = [
    # skip placement
    "file://skip",

    # 30sec - ad3 (bet)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd",
    ]


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


###############################################################################


@pytest.fixture
def http_setup_teardown():
    start_httpserver()
    yield
    print("Cleanup")
    stop_httpserver()

@pytest.mark.ci_test_set
def test_8013(http_setup_teardown, aamp_setup_teardown):
    '''Tests the OUTSIDE_ADBREAK_WAIT4ADS state'''
    # This test runs a stream with period 881036616 with 20sec duration
    # It then adds an ad placement to period 881036617 with 30sec duration
    # But the ad is only added when download head reaches baseperiod 881036617 with a delay
    BASIC_TEST_DATA = {
        "title": "Linear CDAI TESTDATA OUTSIDE_ADBREAK_WAIT4ADS",
        "url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
        'simlinear_type': 'DASH',
        "max_test_time_seconds": 60,
        "aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\n",
        "cmdlist": [
                "advert add " + AD_URLS[0],
                "advert list",
            ],
        "expect_list": [
                {"expect": r"Found CDAI events for period 881036617"},
                {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\] Got adIdx\[-1\] for adBreakId\[881036617\] but adBreak object exist"},
                {"expect": r"\[WaitForNextAdResolved\]\[\d+\]Waiting for next ad placement in 881036617 to complete with timeout \d+ ms."},
                {"cmd": "advert clear"},
                {"cmd": "set alternateContents 881036617 ad1 " + AD_URLS[1]},
                {"expect": r"State changed from \[OUTSIDE_ADBREAK\] => \[OUTSIDE_ADBREAK_WAIT4ADS\]"},
                {"expect": r"State changed from \[OUTSIDE_ADBREAK_WAIT4ADS\] => \[IN_ADBREAK_AD_PLAYING\]"},
                {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '881036616' to '0-1' \[BasePeriodId='881036617'\]"},
                {"expect": r"HttpRequestEnd: 2,7,.*--header.mp4"},
                {"expect": r"HttpRequestEnd: 0,0,.*frag-0.mp4"},
                {"expect": "Found CDAI events for period 881036618"},
                {"cmd": "stop"},
        ]
    }
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(BASIC_TEST_DATA)


