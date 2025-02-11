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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events

import os
import pytest

from inspect import getsourcefile

base_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/"

url_info = [
    {"url": base_url + "main.m3u8", "fct": lambda fmt : f'Inserted. url ' + base_url + f'hls/{fmt}.m3u8'},
    {"url": base_url + "main.mpd", "fct": lambda fmt : f'Inserted init url ' + base_url + f'dash/{fmt}_init.m4s'},
]

bitrate_info = [
    {"bitrate":800000, "format":"360p"},
    {"bitrate":1400000, "format":"480p"},
    {"bitrate":2800000, "format":"720p"},
    {"bitrate":5000000, "format":"1080p"},
]

@pytest.fixture(params=bitrate_info)
def test_data(request):
    return request.param

def test_3001(aamp_setup_teardown, test_data):
    '''Tests the playback of a specific video profile.'''
    
    def Generate_ExpectList(info):
        ''' Generates the commands list to be used in this test.'''
    
        bitrate = info.get("bitrate", 0)
        data_fmt = info.get("format", "")
        
        assert bitrate > 0, "Bitrate data missing from configuration."
        assert not data_fmt == "", "Packet format data missing from configuration."

        data = [
            {"cmd": "bps {}".format(bitrate)},
            {"expect": "Set video bitrate"},
        ]

        def Generate_AssetList(index, d, br, fmt):
            '''Generates the commands list specific for asset at index #'''
            
            d.append({"cmd": url_info[index]["url"]})
            d.append({"expect": "{}".format(url_info[index]["fct"](fmt))})
            d.append({"expect": r"\[SendTuneMetricsEvent\].*\"vfb\":{}".format(br)})
            d.append({"cmd": "sleep 30000"})
            d.append({"cmd": "stop"})
            d.append({"expect": r'aamp_stop PlayerState=8'})

        Generate_AssetList(0, data, bitrate, data_fmt)
        Generate_AssetList(1, data, bitrate, data_fmt)
        
        return data

    single_test_data = {
        "title": "Test playback of Video Profile at {} bps".format(test_data["bitrate"]),
        "max_test_time_seconds": 90,
        "aamp_cfg": "abr=false\ninfo=true\n",
        "expect_list": Generate_ExpectList(test_data),
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(single_test_data)
