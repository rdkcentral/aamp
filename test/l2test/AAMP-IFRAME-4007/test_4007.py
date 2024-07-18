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

TESTDATA1 = {
    "title": f"Mpd Trick Play - Pause & Play",
    "logfile": f"pause_play.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        # Start Playback
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": re.escape("PAUSED -> PLAYING")},
        # Check playback rate
        {"cmd": "get playbackRate"},
        {"expect": re.escape("PLAYBACK RATE = 1")},
        # Pause the playback this will change playback rate to 0
        {"cmd": "pause"},
        {"expect": r"aamp_SetRate\ \(0\.000000\)"},
        {"expect": r"aamp_SetRate rate\(1\.000000\)->\(0\.000000\)"},
        {"expect": "AAMPGstPlayerPipeline state set to PAUSED"}, 
        {"expect": "AAMP_EVENT_STATE_CHANGED: PAUSED"},
        {"expect": "AAMP_EVENT_SPEED_CHANGED current rate=0"},
        # Expected Current playback position as a 3 digit number. i.e less than 999. 
        {"expect": r"Returning Position as (\d{3}) "},
        {"cmd": "sleep 3000"},
        {"expect": "sleeping for 3.000000 seconds"},
        # Expected Current playback position as a 3 digit number even though we passed sleep for 3 seconds. This confirms playback position is same and video is in pause state. 
        {"expect": r"Returning Position as (\d{3}) "},
        # Check playback rate using AAMP's get option to confirm video playback is at 0 confirms stream is in pause state
        {"cmd": "get playbackRate"},
        {"expect": "PLAYBACK RATE = 0"},

        # Start/Resume playback
        {"cmd": "play"}, 
        {"expect": r"aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"aamp_SetRate rate\(1\.000000\)->\(1\.000000\)"},
        # Resumes playback from 3 digit number. The position where player was paused.
        {"expect": r"Resuming Playback at Position '(\d{3})'."},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": "AAMP_EVENT_SPEED_CHANGED current rate=1"},
        # Playback position is now 4 digit number. Confirms playback progressed.
        {"expect": r"Returning Position as (\d{4}) "},
        {"cmd": "sleep 3000"},
        {"expect": "sleeping for 3.000000 seconds"},
        # Playback position is now 5 digit number after wait of 3 seconds. Confirms playback progressed.
        {"expect": r"Returning Position as (\d{5}) "},
        # Check playback rate using AAMP's get option.
        {"cmd": "get playbackRate"},
        {"expect": "PLAYBACK RATE = 1"},
        # Correct Media segments are getting downloaded.
        {"expect": r"(1080|720|480|360)p.*m4s"},
    ]
}

ff_speed_4 = 4
ff_speed_8 = 8

TESTDATA2 = {
    "title": f"Mpd Trick Play - FF & Resume",
    "logfile": f"ff_resume.txt",
    "max_test_time_seconds": 40,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\nprogressReportingInterval=0.25",
    "url": f"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        # Start Playback
        # {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": re.escape("PAUSED -> PLAYING")},
        # Check playback rate
        {"cmd": "get playbackRate"},
        {"expect": re.escape("PLAYBACK RATE = 1")},

        # Fast forward the playback 
        {"cmd": f"ff {ff_speed_4}"},
        {"expect": r"aamp_SetRate rate\((1+(?:\.\d+)?)\)->\((" + str(ff_speed_4) + r"(?:\.\d+)?)\)"},
        # Wait for aamp to move its playback position more than 59 seconds
        {"expect": r"aamp pos: \[0\.\.[5-9](\d{1})"},
        {"cmd": "get playbackPosition"},
        {"expect": r"PLAYBACK POSITION = [5-9](\d{1})"},
    
        # Start/Resume playback
        {"cmd": "play"}, 
        {"expect": r"aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
        {"expect": "AAMP_EVENT_SPEED_CHANGED current rate=1"}, #3394
        # Playback position is now 6 digit number. Could be between 51000 54999. Confirms playback progressed and did not started from 0 after transition of speed from 4 to 1 
        {"expect": r"Returning Position as 5[1-4](\d{3}) "},
        # Playback position is now 6 digit number. Could be between 55000 59999. Confirms playback progressed and did not started from 0 after transition of speed from 4 to 1.
        {"expect": r"Returning Position as 5[5-9](\d{3}) "},
        # Check playback rate using AAMP's get option.
        {"cmd": "get playbackRate"},
        {"expect": "PLAYBACK RATE = 1"},
        # Correct Media segments are getting downloaded.
        {"expect": r"(1080|720|480|360)p.*m4s"},
    ]
}

TESTDATA3 = {
    "title": f"Mpd Trick Play - FF Ramp Up & Resume ",
    "logfile": f"ff_ramp_up_resume.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "url": f"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        # Start Playback
        # {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": re.escape("PAUSED -> PLAYING")},
        # Check playback rate
        {"cmd": "get playbackRate"},
        {"expect": re.escape("PLAYBACK RATE = 1")},

        # Fast forward the playback 
        {"cmd": f"ff {ff_speed_4}"},
        {"expect": r"aamp_SetRate rate\((1+(?:\.\d+)?)\)->\((" + str(ff_speed_4) + r"(?:\.\d+)?)\)"},
        # Wait for aamp to move its playback position more than 59 seconds
        {"expect": r"aamp pos: \[0\.\.[5-9](\d{1})"},
        {"cmd": "get playbackPosition"},
        {"expect": r"PLAYBACK POSITION = [5-9](\d{1})"},
        
        # Ramp up Fast forward 
        {"cmd": f"ff {ff_speed_8}"},
        {"expect": r"aamp_SetRate rate\((" + str(ff_speed_4) + r"+(?:\.\d+)?)\)->\((" + str(ff_speed_8) + r"(?:\.\d+)?)\)"},
        # Wait for aamp to move its playback position more than 120 seconds
        {"expect": r"aamp pos: \[0\.\.1[2-5](\d{1})"},
        {"cmd": "get playbackPosition"},
        {"expect": r"PLAYBACK POSITION = 1[2-5](\d{1})"},
    
        # Start/Resume playback
        {"cmd": "play"}, 
        {"expect": r"aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"aamp_SetRate rate\(" + str(ff_speed_8) + r"\.000000\)->\(1\.000000\)"},
        {"expect": "AAMP_EVENT_SPEED_CHANGED current rate=1"}, #3394
        # # Playback position is now 6 digit number. Could be between 15000 19999. Confirms playback progressed and did not started from 0 after transition of speed from 4 to 1 
        {"expect": r"Returning Position as 1[5-9](\d{4}) "},
        # Check playback rate using AAMP's get option.
        {"cmd": "get playbackRate"},
        {"expect": "PLAYBACK RATE = 1"},
        # Correct Media segments are getting downloaded.
        {"expect": r"(1080|720|480|360)p.*m4s"},
    ]
}

TESTDATA4 = {
    "title": f"Mpd Trick Play - FF Ramp Down & Resume ",
    "logfile": f"ff_ramp_down_resume.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "url": f"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        # Start Playback
        # {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": re.escape("PAUSED -> PLAYING")},
        # Check playback rate
        {"cmd": "get playbackRate"},
        {"expect": re.escape("PLAYBACK RATE = 1")},

        # Fast forward the playback 
        {"cmd": f"ff {ff_speed_8}"},
        {"expect": r"aamp_SetRate rate\((1+(?:\.\d+)?)\)->\((" + str(ff_speed_8) + r"(?:\.\d+)?)\)"},
        # Wait for aamp to move its playback position more than 59 seconds
        {"expect": r"aamp pos: \[0\.\.[5-9](\d{1})"},
        {"cmd": "get playbackPosition"},
        {"expect": r"PLAYBACK POSITION = [5-9](\d{1})"},
        
        # Ramp Down Fast forward 
        {"cmd": f"ff {ff_speed_4}"},
        {"expect": r"aamp_SetRate rate\((" + str(ff_speed_8) + r"+(?:\.\d+)?)\)->\((" + str(ff_speed_4) + r"(?:\.\d+)?)\)"},
        # Wait for aamp to move its playback position more than 120 seconds
        {"expect": r"aamp pos: \[0\.\.1[2-5](\d{1})"},
        {"cmd": "get playbackPosition"},
        {"expect": r"PLAYBACK POSITION = 1[2-5](\d{1})"},
    
        # Start/Resume playback
        {"cmd": "play"}, 
        {"expect": r"aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"aamp_SetRate rate\(" + str(ff_speed_4) + r"\.000000\)->\(1\.000000\)"},
        {"expect": "AAMP_EVENT_SPEED_CHANGED current rate=1"}, #3394
        # # Playback position is now 6 digit number. Could be between 11000 14999. Confirms playback progressed and did not started from 0 after transition of speed from 4 to 1 
        {"expect": r"Returning Position as 1[1-4](\d{4}) "},
        # Check playback rate using AAMP's get option.
        {"cmd": "get playbackRate"},
        {"expect": "PLAYBACK RATE = 1"},
        # Correct Media segments are getting downloaded.
        {"expect": r"(1080|720|480|360)p.*m4s"},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_4007(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)

