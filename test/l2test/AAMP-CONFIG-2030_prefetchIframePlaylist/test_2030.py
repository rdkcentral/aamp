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


from inspect import getsourcefile
import os
import pytest
import re 
import subprocess

def find_in_logs(text):
    path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "aamp.log")
    cmd = f'grep "{text}" {path} | wc -l'
    subp = subprocess.Popen([cmd],
            shell=True,
            stdout=subprocess.PIPE,
        )
    output = int(subp.stdout.read().decode())
    return output


def check_iframe_download(match):
    if find_in_logs("Downloading iframe playlist") > 0 or find_in_logs("Cached iframe playlist") > 0:
        assert 0, "ERROR - Prefetched Iframe Playlist" 

TESTDATA1 = {
"title": "Set preFetchIframePlaylist as true",
"logfile": "testdata1.txt",
"max_test_time_seconds": 15,
"aamp_cfg": f"info=true\ntrace=true\nprogress=true\npreFetchIframePlaylist=true\n",
"expect_list": [
	{"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
    {"expect":r"StreamAbstractionAAMP_HLS:: Downloading iframe playlist"},
    {"expect":r"StreamAbstractionAAMP_HLS:: Cached iframe playlist"},
    {"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
    {"expect":r"Returning Position as [1-3](\d{3})"}, 
    {"expect":r"Returning Position as [4-8](\d{3})"}, 
  ]
}

TESTDATA2 = {
"title": "Set preFetchIframePlaylist as false",
"logfile": "testdata2.txt",
"max_test_time_seconds": 15,
"aamp_cfg": f"info=true\ntrace=true\nprogress=true\npreFetchIframePlaylist=false\n",
"expect_list": [
    {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
    {"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING", "callback":check_iframe_download},
    {"expect":r"Returning Position as [1-3](\d{3})"}, 
    {"expect":r"Returning Position as [4-8](\d{3})"}, 
  ]
}
TESTLIST = [TESTDATA1, TESTDATA2]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2050(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
