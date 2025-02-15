#  !/usr/bin/env python3
#  If not stated otherwise in this file or this component's LICENSE file the
#  following copyright and licenses apply:
#  Copyright 2024 RDK Management
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  http://www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from inspect import getsourcefile
import os
import pytest
import re

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CONFIG-2073/hls_test_audio.tar.xz"

aamp = None


# Callbacks used by the tests
def send_command(match, command):
    print(f"command: {command}")
    aamp.sendline(command)


# Define test data configurations
TESTDATA1 = {
    "title": "Set ID3 as true",
    "logfile": "ID3_true.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\nid3=true\n",
    "max_test_time_seconds": 60,
    "archive_url": archive_url,
    "url": "hls_test_audio/cdn.theoplayer.com/video/indexcom/manifest.m3u8.1",
    "simlinear_type": "HLS",
    "expect_list": [
        {"expect": r"mManifestUrl: http://localhost:8085/hls_test_audio/cdn.theoplayer.com/video/indexcom/manifest.m3u8.1","min": 0,"max": 5},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING","min": 0,"max": 5,"callback": send_command,"callback_arg": "sleep 1000"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING", "max": 5},
        { "expect": r"\[ID3MetadataHandler\]\[(\d+)\] ID3 tag # timestamp: (\d+) - PTS: (\d+\.\d+) (\d+) \[\d+\] \|\| data: ID3v40 hdr:"},
        {"expect": r"\[SendId3MetadataEvent\]\[(\d+)\]ID3 tag length: (\d+) payload: ID3"},
        {"expect": r"\[SendId3MetadataEvent\]\[(\d+)\]\{schemeIdUri:\"[^\"]*\",value:\"[^\"]*\",presentationTime:(\d+),timeScale:(\d+),eventDuration:(\d+),id:(\d+),timestampOffset:(\d+)\}"},
        {"expect": "sleep complete", "end_of_test": True},
    ],
}

TESTDATA2 = {
    "title": "ID3",
    "logfile": "ID3_false.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\nid3=false\n",
    "max_test_time_seconds": 60,
    "archive_url": archive_url,
    "url": "hls_test_audio/cdn.theoplayer.com/video/indexcom/manifest.m3u8.1",
    "simlinear_type": "HLS",
    "expect_list": [
        {"expect": r"mManifestUrl: http://localhost:8085/hls_test_audio/cdn.theoplayer.com/video/indexcom/manifest.m3u8.1","min": 0,"max": 5},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING","min": 0,"max": 5,"callback": send_command,"callback_arg": "sleep 1000"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING", "max": 5},
        {"expect": r"\[ID3MetadataHandler\]\[(\d+)\] ID3 tag # timestamp: (\d+) - PTS: (\d+\.\d+) (\d+) \[\d+\] \|\| data: ID3v40 hdr:"},
        {"expect": r"\[SendId3MetadataEvent\]\[(\d+)\]ID3 tag length: (\d+) payload: ID3","not_expected": True},
        {"expect": r"\[SendId3MetadataEvent\]\[(\d+)\]\{schemeIdUri:\"[^\"]*\",value:\"[^\"]*\",presentationTime:(\d+),timeScale:(\d+),eventDuration:(\d+),id:(\d+),timestampOffset:(\d+)\}","not_expected": True},
        {"expect": "sleep complete", "end_of_test": True},
    ],
}

############################################################
TESTLIST = [TESTDATA1, TESTDATA2]


@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param


def test_2073(aamp_setup_teardown, test_data):
    global aamp
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
