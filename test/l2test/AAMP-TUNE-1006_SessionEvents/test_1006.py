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
import re
import pytest

import uuid

from inspect import getsourcefile


sleep_time=5000

base_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/"

stream_urls = [
    base_url + "main.m3u8",
    base_url + "main.mpd",
    base_url + "main_mp4.m3u8",
]

stream_configuration=[
    {"url": stream_urls[0]},
    {"url": stream_urls[1]},
]

def GenerateUUIDs(count):
    '''Generates the required number of UUIDs.'''

    assert count > 0, "Number of generated UUIDs must be greater than 0."
    
    uuids = []
    for idx in range (count):
        uuids.append(str(uuid.uuid4()))

    return uuids


@pytest.fixture(params = stream_configuration)
def test_data(request):
    return request.param


def test_1006_singleplayer(aamp_setup_teardown, test_data):
    '''Tests that the main events related to a tune request carry the expected Session ID.'''

    def Generate_ExpectList(url, sid):
        '''Generates the commands list to be used in this test.'''
        
        data = [
            {"cmd": "progress"},
            {"cmd": "sessionid {}".format(sid)},
            {"expect": r"SessionId - 0 # {}".format(sid)},

            {"cmd": url},

            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=1\)\(session_id={}\)'.format(sid)},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid)},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid)},
            {"expect": r'\[SendEventSync\].*\(type=1\)\(session_id={}\)'.format(sid)},
            {"expect": r'AAMP_EVENT_TUNED'},
            {"expect": r'AAMP_EVENT_PROGRESS'},
            {"expect": r'\tsessionId=\'{}\''.format(sid)},
            {"cmd": "sleep {}".format(sleep_time)},

            {"cmd": "pause"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=6\)\(session_id={}\)'.format(sid)},
            {"expect": r'\[SendEventSync\].*\(type=3\)\(session_id={}\)'.format(sid)},
            {"cmd": "sleep {}".format(sleep_time)},

            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid)},
            {"expect": r'\[SendEventSync\].*\(type=3\)\(session_id={}\)'.format(sid)},
            {"cmd": "sleep {}".format(sleep_time)},

            {"cmd": "ff 2"},
            {"expect": r'\[SendEventSync\].*\(type=3\)\(session_id={}\)'.format(sid)},

            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=8"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"}
	]
        return data

    single_test_data = {
        "title": "Test Single Player SessionID",
        "max_test_time_seconds": 20,
        "aamp_cfg": "info=true\nprogress=true\n",
        "expect_list": Generate_ExpectList(test_data["url"], GenerateUUIDs(1)[0]),
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(single_test_data)


def test_1006_cli(aamp_setup_teardown):
    '''Test of the multi player control in the simulator.
    We create a gap in the collection of players and switch between the players before and after the gap
    to verify that the UUID assignment logic works as expected.
    '''

    def Generate_ExpectList(url, sid):
        ''' Generates the commands list to be used in this test.'''
        
        data = [

            {"cmd": "sessionid {}".format(sid[0])},
            {"expect": r"SessionId - 0 # {}".format(sid[0])},

            {"cmd": "new player_1"},
            {"cmd": "sessionid {}".format(sid[1])},
            {"expect": r"SessionId - 1 # {}".format(sid[1])},

            {"cmd": "new player_2"},
            {"cmd": "sessionid {}".format(sid[2])},
            {"expect": r"SessionId - 2 # {}".format(sid[2])},

            {"cmd": "release player_1"},
            {"expect": r'DeleteStreamSink for PLAYER\[1\]'},

            {"cmd": url[1]},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[2])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=8"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"},
	    {"cmd": "new player_3"},
            {"cmd": "sessionid {}".format(sid[3])},
            {"expect": r"SessionId - 3 # {}".format(sid[3])},

            {"cmd": "select 0"},
            {"cmd": "autoplay"},

            {"cmd": url[0]},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid[0])},
            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[0])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=8"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"},
            {"cmd": "select player_3"},
            {"cmd": "release 0"},
            {"expect": r'DeleteStreamSink for PLAYER\[0\]'},
            {"cmd": "release player_2"},
            {"expect": r'DeleteStreamSink for PLAYER\[2\]'},

            # 
            {"cmd": "autoplay"},
            {"cmd": url[1]},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid[3])},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[3])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "pause"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=6\)\(session_id={}\)'.format(sid[3])},

            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=6"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"}
	]
        return data

    single_test_data = {
        "title": "Test SessionID API",
        "max_test_time_seconds": 30,
        "aamp_cfg": "info=true\nprogress=true\n",
        "expect_list": Generate_ExpectList(stream_urls, GenerateUUIDs(4)),
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(single_test_data)


def test_1006_multiplayer(aamp_setup_teardown):
    '''Test that in a multiplayer configuration the events generated by each player contain the correct Session ID.
    Creates 3 players and configures them with different assets, then switches between the players and starts/stops the playback.
    '''

    def Generate_ExpectList(url, sid):
        '''Generates the commands list to be used in this test.'''
        
        data = [
            {"cmd": "progress"},
            {"cmd": "autoplay"},
            
            {"cmd": "sessionid {}".format(sid[0])},
            {"expect": r"SessionId - 0 # {}".format(sid[0])},
            {"cmd": url[2]},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid[0])},

            {"cmd": "new player_1"},
            {"cmd": "sessionid {}".format(sid[1])},
            {"expect": r"SessionId - 1 # {}".format(sid[1])},
            {"cmd": url[1]},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid[1])},

            {"cmd": "new player_2"},
            {"cmd": "sessionid {}".format(sid[2])},
            {"expect": r"SessionId - 2 # {}".format(sid[2])},
            {"cmd": url[0]},
            {"expect": r'\[SendEventSync\].*\(type=9\)\(session_id={}\)'.format(sid[2])},

            {"cmd": "select 0"},
            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[0])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "pause"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=6\)\(session_id={}\)'.format(sid[0])},

            {"cmd": "select 1"},
            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[1])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=8"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"},
            {"cmd": "select 2"},
            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[2])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=8"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"},
            {"cmd": "release 1"},
            {"expect": r'DeleteStreamSink for PLAYER\[1\]'},

            {"cmd": "select 0"},
            {"cmd": "play"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=8\)\(session_id={}\)'.format(sid[0])},
            {"cmd": "sleep {}".format(sleep_time)},
            {"cmd": "pause"},
            {"expect": r'\[SendEventSync\].*\(type=14\)\(state=6\)\(session_id={}\)'.format(sid[0])},
            {"cmd": "stop"},
            { "expect": "aamp_stop PlayerState=6"},
            { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"}
        ]
        return data

    single_test_data = {
        "title": "Test Multiplayer SessionID",
        "max_test_time_seconds": 20,
        "aamp_cfg": "info=true\nprogress=true\n",
        "expect_list": Generate_ExpectList(stream_urls, GenerateUUIDs(3)),
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(single_test_data)

