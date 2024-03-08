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
import sys
import json

from inspect import getsourcefile

#Test stream
DASH_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"
DASH_BITRATE = 288000

#Dictionary to store test stream and its respective bitrate that we need to verify
testStreamDic = { DASH_TEST_STREAM:DASH_BITRATE}
fullTestData = {
    "title": "TEST GETCURRENRAUDIOBITRATE API",
    "max_test_time_seconds": 15,
    "aamp_cfg": "info=true\nabr=false\n"
}

#The API is employed to create a test sequence,
#which must be executed a specified number of times corresponding
#to the total number of test streams.
def getTestSequence(streamUrl,bitRate,sleepInms):
    testCommandlist =[
        {"cmd": streamUrl},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"cmd": "set audioTrack 0"},
        {"expect": "Matched Command AudioTrack"},
        {"cmd": "get audioBitrate"},
        {"expect": "AUDIO BITRATE = {}".format(bitRate)},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState"},
    ]
    return testCommandlist


def test_13001(aamp_setup_teardown):
    testSequenceList = []
    for stream,audioBitrate in testStreamDic.items():
        #Generating test sequence list for all the given stream.
        testSequenceList.append(getTestSequence(stream,audioBitrate,2000))

    for idx, testSequence in enumerate(testSequenceList):

        fullTestData["expect_list"] = testSequence
        fullTestData["logfile"] = "CurrentAudioBitrate_" + str(idx) + ".log",

        aamp = aamp_setup_teardown
        aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
        aamp.create_aamp_cfg(fullTestData.get('aamp_cfg'))
        #Passing each test sequence to aampcli through run_expect_a API
        aamp.run_expect_a(fullTestData)
