#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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
import copy
import pytest

# live Manifest - expected frequent manifest updates

TESTS=[{"cmd":'setconfig {"info":true,"progress":true}'}
]

# Create instances
playback = {}

@pytest.fixture(params=TESTS)
def test_data(request):
    '''
    This returns a test data of different configurations
    '''
    return request.param

@pytest.mark.ci_test_set
def test_1012(aamp_setup_teardown,test_data):
    #Dictionary to store basic test stream and sequence of commands that we need to verify
    BASIC_TEST_DATA = {
        "title": "TEST MANIFEST PREPROCESS",
        "max_test_time_seconds": 20,
        "expect_list": [
            {"cmd": test_data['cmd']},
            {"cmd": "tunedata https://d24rwxnt7vw9qb.cloudfront.net/v1/dash/e6d234965645b411ad572802b6c9d5a10799c9c1/All_Reference_Streams/6ba06d17f65b4e1cbd1238eaa05c02c1/index.mpd"},
            {"expect": "Previous preprocessed manifest is not read"},
            {"expect": "PreProcessed Manifest not available send Need Manifest data event"},
	    {"expect": "AAMP_EVENT_NEED_MANIFEST_DATA"},
            {"expect": "updateManifest"},
            {"cmd": "sleep 2000"},
            {"cmd": "stop"},

            {"cmd": "tunedata https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel-tiled-thumbnails-numbered.ism/.mpd"},
            {"expect": "PreProcessed manifest provided"},
            {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
            {"expect": "aamp pos"},
            {"cmd": "sleep 2000"},
            {"cmd": "stop"},
        ]
    }

    # Update playback for each test
    playback = copy.deepcopy(BASIC_TEST_DATA)
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    #Passing each test sequence to aampcli through run_expect_a API
    aamp.run_expect_a(playback)
