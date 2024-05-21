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

def verify_video_header(matched):
    video_header = "v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163/track-video-periodid-879813569-repid-root_video0-tc-0-header.mp4"
    
    test_dir = os.path.dirname(__file__)
    
    subp = subprocess.Popen(
        [f"grep '{video_header}' {test_dir}/aamp.log | wc -l"],
        shell=True,
        stdout=subprocess.PIPE,
    )

    stdout, stderr = subp.communicate()
    video_header_count = int(stdout.decode('utf-8'))

    print(f"{video_header_count = } | {matched = } | {test_dir = }")
    subp.wait()
    if video_header_count > 0:
        assert 0, f"Downloaded video header {video_header_count} times"

TESTDATA1 = {
    "title": "Test case to validate AudioOnlyPlayBack Configuration",
    "max_test_time_seconds": 30,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\naudioOnlyPlayback=true",
    "expect_list": [
        {"expect": r"No valid adaptation set found for Media\[video\]"},
        {"expect": r"Media\[video\] disabled"},
        
        {"cmd": "sleep 7000"},
        {"expect": r"Media\[audio\] enabled"},

        #audio Segments
        {"expect": r"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163\-eac3/track\-audio\-periodid\-879813569\-repid\-root_audio110\-tc\-0\-frag\-879825325\.mp4"},

        {"expect": r"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163\-eac3/track\-audio\-periodid\-879813569\-repid\-root_audio110\-tc\-0\-frag\-879825327\.mp4"},

        {"expect": r"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163\-eac3/track\-audio\-periodid\-879813569\-repid\-root_audio110\-tc\-0\-frag\-879825330\.mp4"},
        
        {"expect": r"sleep complete", "callback":verify_video_header},
        
    ]
}

TESTLIST = [TESTDATA1]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2020(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)

