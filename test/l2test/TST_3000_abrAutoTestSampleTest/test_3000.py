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

#Starts simlinear, a web server for serving ABR test streams
#Starts aamp-cli and initiates playback by giving it a stream URL
#verifys aamp log output against expected list of events
#
#Also see README.txt

import os
import sys
from inspect import getsourcefile
import pytest
import requests

if "RUNNING_IN_DOCKER" in os.environ:
    sys.path.append("../tools/abrTestingProxy/")
    import proxy_ctrl
    proxy = proxy_ctrl.ProxyCtrl()


def set_rate(match,rate):
    proxy.SetRate(rate)


def add_rule(match,rule):
    proxy.ErrorReply(rule[0], rule[1], rule[2], rule[3])


def remove_rule(match,rule):
    proxy.RemoveRule(rule)

###############################################################################

TESTDATA1= {
"title": "Canned live HLS playback. Adjust available bandwidth",
"logfile": "testdata1.txt",
"url": "http://simlinear:8085/testdata/m3u8s/manifest.1.m3u8",
"aamp_cfg": "info=true\ntrace=true\nnetworkProxy=http://abrtestproxy:8080/",
"max_test_time_seconds": 300,
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "http://simlinear:8085/testdata/m3u8s/discontinuity_test_video_1080_4800000.m3u8",
     "min": 0, "max": 4}, # Ramp up to the highest bitrate within 3 segment duration (~6 seconds)
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_10.ts",
     "min": 0, "max": 8, "callback": set_rate, "callback_arg": 700},      # First few segment requests should be settled faster than real time and the speed ramped up
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/180_250000/hls/segment_15.ts",
     "min": 0, "max": 36,  "callback": set_rate, "callback_arg": 1000},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/270_400000/hls/segment_22.ts",
     "min": 50, "max": 60, "callback": set_rate, "callback_arg": 2100},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/360_800000/hls/segment_27.ts",
     "min": 65, "max": 80, "callback": set_rate, "callback_arg": 20000},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_32.ts",
     "min": 90, "max": 100, "callback": add_rule, "callback_arg": ["token1", ".*video/1080_4800000.*_33.ts", "404", "10"]},
    {"expect": "msg:CDN:VIDEO,HTTP-404 url:http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_33.ts",
     "min": 90, "max": 104, "callback": remove_rule, "callback_arg":"token1"},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/720_2400000/hls/segment_33.ts",
     "min": 90, "max": 104, "callback": add_rule, "callback_arg": ["token2", ".*video/.*_35.ts", "404", "2"]},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/540_1200000/hls/segment_35.ts",
     "min": 90, "max": 112},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_40.ts",
     "min": 100, "max": 135, "callback": add_rule,"callback_arg": ["token3", ".*video/.*_41.ts", "55", "1"]},
    {"expect": "Anomaly evt:1 msg:CDN:VIDEO,Curl-55 url:http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_41.ts",
     "min": 100, "max": 143},
    {"expect": "Buffer is running low",
     "min": 0, "max": 200, "not_expected" : True},
    {"expect": "fragment injector done. track video",
     "min": 150, "max": 250, "end_of_test": True }
    ]
}


def simlinear_cmd(cmd_list):
    """
    Takes list of tuples, feeds then into simlinear
    [ ( url, {json}) , (url, [json]) ...]
    """
    for url,json in cmd_list:
        response = requests.post(url, json=json)
        print("{} {} {}".format(url, json,response.status_code))
        if response.status_code != 200:
            return response.status_code
    return response.status_code


@pytest.mark.skipif("RUNNING_IN_DOCKER" not in os.environ, reason="Runs in abr setup only")
def test_3000(aamp_setup_teardown):
    aamp = aamp_setup_teardown

    # Start the simlinear that is running in another container
    cmd = [("http://simlinear:5000/sim/start", {"port": 8085, "type": 'HLS'})]
    assert simlinear_cmd(cmd) == 200, "simlinear_cmd failed"

    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA1)

