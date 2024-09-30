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


# VODTrickplay setting to 2 and ff is 4
TESTDATA1 = {
        "title": "Setting VODTrickplay Configuration",
        "logfile": "test1.txt",
        "max_test_time_seconds": 60 ,
        "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
        "expect_list": [
            {"cmd": "set vodTrickplayFps 2"},
            {"expect": " Matched Command VodTrickplayFps - set vodTrickplayFps 2"},
            {"expect": "vodTrickPlayFps New Owner"},
            {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
            {"cmd": "ff 4"},
            
            {"cmd": "sleep 3000"},
            {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=4\.000000"},
            {"expect": r"fragmentRepeatCount 6 fragmentTime 6\.000000 skipTime 2\.000000 segNumber 7"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_009\.m4s"},
            {"expect": r"fragmentRepeatCount 9 fragmentTime 9\.000000 skipTime 2\.000000 segNumber 10"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_012\.m4s"},
            {"expect": r"fragmentRepeatCount 12 fragmentTime 12\.000000 skipTime 2\.000000 segNumber 13"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_015\.m4s"},
            
            {"expect": r"Returning Position as 4"},

            # Expected aamp pos: [0..0..900..-1..-1.00..0.00....800000..800000..4.00] here 4.00 at the end confirms playback speed(ff) is 4
            {"expect": r"aamp pos: \[(.*)" + f"{4}" + r".00]"},
            

        ]
    }

# VODTrickplay setting to 2 and ff is 8
TESTDATA2 = {
        "title": "Setting VODTrickplay Configuration",
        "logfile": "test2.txt",
        "max_test_time_seconds": 60 ,
        "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
        "expect_list": [
            {"cmd": "set vodTrickplayFps 2"},
            {"expect": " Matched Command VodTrickplayFps - set vodTrickplayFps 2"},
            {"expect": "vodTrickPlayFps New Owner"},
            {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
            {"cmd": "ff 8"},
            {"cmd": "sleep 3000"},
            {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=8\.000000"},
            
            {"expect": r"fragmentRepeatCount 5 fragmentTime 5\.000000 skipTime 4\.000000 segNumber 6"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_010\.m4s"},
            {"expect": r"fragmentRepeatCount 10 fragmentTime 10\.000000 skipTime 4\.000000 segNumber 11"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_015\.m4s"},
            {"expect": r"fragmentRepeatCount 15 fragmentTime 15\.000000 skipTime 4\.000000 segNumber 16"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_020\.m4s"},
            
            {"expect": r"Returning Position as 24"},

            # Expected aamp pos: [0..0..900..-1..-1.00..0.00....800000..800000..8.00] here 8.00 at the end confirms playback speed(ff) is 8
            {"expect": r"aamp pos: \[(.*)" + f"{8}" + r".00]"},
            

        ]
    }

# VODTrickplay setting to 4 and ff is 32
TESTDATA3 = {
        "title": "Setting VODTrickplay Configuration",
        "logfile": "test3.txt",
        "max_test_time_seconds": 60 ,
        "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
        "expect_list": [
            {"cmd": "set vodTrickplayFps 4"},
            {"expect": " Matched Command VodTrickplayFps - set vodTrickplayFps 4"},
            {"expect": "vodTrickPlayFps New Owner"},
            {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
            {"cmd": "ff 32"},
            {"cmd": "sleep 3000"},
            {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=32\.000000"},
            {"expect": r"fragmentRepeatCount 9 fragmentTime 9\.000000 skipTime 8\.000000 segNumber 10"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_018\.m4s"},
            
            {"expect": r"fragmentRepeatCount 18 fragmentTime 18\.000000 skipTime 8\.000000 segNumber 19"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_027\.m4s"},
            {"expect": r"fragmentRepeatCount 27 fragmentTime 27\.000000 skipTime 8\.000000 segNumber 28"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_036\.m4s"},
            
            {"expect": r"Returning Position as 32"},

            # Expected aamp pos: [0..0..900..-1..-1.00..0.00....800000..800000..32.00] here 32.00 at the end confirms playback speed(ff) is 32
            {"expect": r"aamp pos: \[(.*)" + f"{32}" + r".00]"},
            

        ]
    }
# VODTrickplay setting to 2 and rewind is 8
TESTDATA4 = {
        "title": "Setting VODTrickplay Configuration",
        "logfile": "test4.txt",
        "max_test_time_seconds": 60 ,
        "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
        "expect_list": [
            {"cmd": "set vodTrickplayFps 2"},
            {"expect": " Matched Command VodTrickplayFps - set vodTrickplayFps 2"},
            {"expect": "vodTrickPlayFps New Owner"},
            {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},

            {"cmd": "seek 300"},
            {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
            {"cmd": "rew 8"},
            {"cmd": "sleep 5000"},
            {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-8\.000000"},
    
            {"expect": r"fragmentRepeatCount 295 fragmentTime 295\.000000 skipTime -4\.000000 segNumber 296"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_292\.m4s"},
            {"expect": r"fragmentRepeatCount 290 fragmentTime 290\.000000 skipTime -4\.000000 segNumber 291"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_287\.m4s"},
            {"expect": r"fragmentRepeatCount 285 fragmentTime 285\.000000 skipTime -4\.000000 segNumber 286"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_282\.m4s"},
            
            {"expect": r"Returning Position as 2(\d{5})"},
            

            # Expected aamp pos: [0..299..900..-1..-1.00..0.00....800000..800000..-8.00] here -8.00 at the end confirms playback speed(rewind) is 8
            {"expect": r"aamp pos: \[(.*)" + f"{-8}" + r".00]"},
            

        ]
    }

# VODTrickplay setting to 4 and rewind is 32
TESTDATA5 = {
        "title": "Setting VODTrickplay Configuration",
        "logfile": "test5.txt",
        "max_test_time_seconds": 60 ,
        "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
        "expect_list": [
            {"cmd": "set vodTrickplayFps 4"},
            {"expect": " Matched Command VodTrickplayFps - set vodTrickplayFps 4"},
            {"expect": "vodTrickPlayFps New Owner"},
            {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},

            {"cmd": "seek 300"},
            {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
            {"cmd": "rew 32"},
            {"cmd": "sleep 5000"},
            {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-32\.000000"},
    
            
            {"expect": r"fragmentRepeatCount 291 fragmentTime 291\.000000 skipTime -8\.000000 segNumber 292"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_284\.m4s"},
            {"expect": r"fragmentRepeatCount 282 fragmentTime 282\.000000 skipTime -8\.000000 segNumber 283"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_275\.m4s"},

            {"expect": r"fragmentRepeatCount 273 fragmentTime 273\.000000 skipTime -8\.000000 segNumber 274"},
            {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_266\.m4s"},
            

            # Expected aamp pos: [0..299..900..-1..-1.00..0.00....800000..800000..-32.00] here -32.00 at the end confirms playback speed(rewind) is 32
            {"expect": r"aamp pos: \[(.*)" + f"{-32}" + r".00]"},
            

        ]
    }

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2025(aamp_setup_teardown,test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


