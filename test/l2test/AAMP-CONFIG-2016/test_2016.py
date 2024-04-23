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

bitrate = 800000
UHD_stream = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/RDKAAMP-1540/4k/h265/SegmentTimeline.mpd"

TESTDATA1 = {
    "title": "Test case to validate abr",
    "logfile": "aamp_abr_false.txt",
    "max_test_time_seconds": 20,
    # Set initial bitrate to the lowest available profile. Using initialBitrate & initialBItrate4K so that as Video progresses, it should switch to higher profiles.
    # Since ABR is disable player will not switch playback from lowest profile to other profiles. We can check the bitrate of video profile after every ~3 seconds of playback and should be same as we configured in initialBitrate. 
    "aamp_cfg": f"info=true\ntrace=true\nabr=false\ninitialBitrate={bitrate}\ninitialBitrate4K={bitrate}",
    "expect_list": [
        # Verify aamp config using getconfig
        {"cmd": f"getconfig"},
        {"expect": re.escape(f'"abr":false')},
        # Start playback
        {"cmd": UHD_stream},

        # Check Video Bitrate
        {"cmd": f"get videoBitrate"},
        {"expect": re.escape(f"CURRENT VIDEO PROFILE BITRATE = {bitrate}")},
        # Check Playback position it should be between 1 second to 4 seconds.
        {"expect": r"Returning Position as [1-5]\d{3}"},
        {"cmd": f"get videoBitrate"},
        {"expect": re.escape(f"CURRENT VIDEO PROFILE BITRATE = {bitrate}")},

        # Check Playback position it should be between 4 second to 7 seconds. It also confirms playback progressing
        {"expect": r"Returning Position as [5-9]\d{3}"},
        {"cmd": f"get videoBitrate"},
        {"expect": re.escape(f"CURRENT VIDEO PROFILE BITRATE = {bitrate}")},
        
        # Check Playback position it should be between 7 second to 10 seconds. It also confirms playback progressing
        {"expect": r"Returning Position as 1[0-5]\d{3}"},
        {"cmd": f"getconfig"},
        {"expect": re.escape(f'"abr":false')},
    ]
}


TESTDATA2 = {
    "title": "Test case to validate abr",
    "logfile": "aamp_abr_true.txt",
    "max_test_time_seconds": 60,
    # Set initial bitrate to the lowest available profile. Using initialBitrate & initialBItrate4K so that as Video progresses, it should switch to higher profiles.
    # Since ABR is enable player will switch playback from lowest profile to other profiles. 
    "aamp_cfg": f"info=true\ntrace=true\nabr=true\ninitialBitrate={bitrate}\ninitialBitrate4K={bitrate}",
    "url": f"{UHD_stream}",
    "expect_list": [
        # Indicates player switch from one video profile to another
        {"expect": r"AAMPLogABRInfo : switching to '(high|low)er' profile", "min":0, "max": 60},
        {"expect": r"ABRProfileChanged", "min":0, "max": 60},
        {"expect": r"StreamAbstractionAAMP_MPD:\ ABR", "min":0, "max": 60},
        
        # Checking the playback progress
        {"expect": r"Returning Position as [1-4]\d{3}", "min":0, "max": 60},
        {"expect": r"Returning Position as [5-9]\d{3}", "min":0, "max": 60, "end_of_test": True},
    ]
}



############################################################

def test_2016_0(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA1)

def test_2016_1(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA2)


