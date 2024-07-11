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
import shutil


def corrupt_fragments(filename, recover=False):
    """
    Function to corrupt / recover segment.
    """
    test_dir = os.path.dirname(__file__)
    filename = f"{test_dir}/testdata/{filename}"
    filename_og = f"{filename}.og"
    if recover == True:
        print(f"Recovery {filename_og = }, {filename = }")
        shutil.copyfile(filename_og, filename)
        os.remove(filename_og)
        return True
    
    if os.path.isfile(filename_og):
        os.remove(filename)
        shutil.copyfile(filename_og,filename)
        os.remove(filename_og)
    shutil.copyfile(filename,filename_og)
    with open(filename, 'rb') as file:
        # Read the entire file as bytes
        file_content = file.read()
        # Convert bytes to hexadecimal representation
        hex_representation = file_content.hex()
    byte_data = bytes.fromhex(hex_representation[80:])
    os.remove(filename)
    with open(filename, 'wb') as file:
        file.write(byte_data)
    print(f"Corrupted {filename_og = }, {filename = }")




segmentInjectFailThreshold = 2
TESTDATA1 = {
    "title": "segmentInjectFailThreshold config",
    "logfile": f"segmentInjectFailThreshold_{segmentInjectFailThreshold}.txt",
    "max_test_time_seconds": 10,
    "aamp_cfg": f"info=true\ntrace=true\nabr=false\nsegmentInjectFailThreshold={segmentInjectFailThreshold}\n",
    "url":f"VideoTestStream_HLS/main.m3u8",
    "simlinear_type": "HLS",
    "expect_list": [
        # Check AAMP Status changed to "INITIALIZING"  
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING", "min":0, "max":3},
        # Verify download attempt for 1st corrupted segment"  
        {"expect": "Got next fragment url http://localhost:8085/VideoTestStream_HLS/hls/480p_001.ts", "min":0, "max":3},
        # Confirmation - Downloaded segment is corrupt hence it is discarded"  
        {"expect": "Segment doesn't starts with valid TS packet, discarding", "min":0, "max":3},
        {"expect": "Not updating totalInjectedDuration since fragment is Discarded", "min":0, "max":3},
        # Verify download attempt for 2nd corrupted segment"  
        {"expect": "Got next fragment url http://localhost:8085/VideoTestStream_HLS/hls/480p_002.ts", "min":0, "max":3},
        # Confirmation - Downloaded segment is corrupt hence it is discarded"  
        {"expect": "Segment doesn't starts with valid TS packet, discarding", "min":0, "max":3},
        {"expect": "Not updating totalInjectedDuration since fragment is Discarded", "min":0, "max":3},
        # Verify segment injection failed count reached to threshold value"  
        {"expect": "Reached max inject failure count: 2, stopping playback", "min":0, "max":3},
        # AAMP playback failed"  
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: Playback failed due to PTS error", "min":0, "max":3, "end_of_test":True},
    ],
    "corrupt_fragments_names": ['VideoTestStream_HLS/hls/480p_001.ts', 'VideoTestStream_HLS/hls/480p_002.ts']
}

segmentInjectFailThreshold = 3
TESTDATA2 = {
    "title": "segmentInjectFailThreshold config",
    "logfile": f"segmentInjectFailThreshold_{segmentInjectFailThreshold}.txt",
    "max_test_time_seconds": 10,
    "aamp_cfg": f"info=true\ntrace=true\nabr=false\ninitialBitrate=5000000\nsegmentInjectFailThreshold={segmentInjectFailThreshold}\n",
    "url":f"VideoTestStream_HLS/main.m3u8",
    "simlinear_type": "HLS",
    "expect_list": [ 
        # Check AAMP Status changed to "INITIALIZING"  
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING", "min":0, "max":3},
        # Verify download attempt for 1st corrupted segment"  
        {"expect": "Got next fragment url http://localhost:8085/VideoTestStream_HLS/hls/1080p_003.ts", "min":0, "max":3},
        # Confirmation - Downloaded segment is corrupt hence it is discarded"  
        {"expect": "Segment doesn't starts with valid TS packet, discarding", "min":0, "max":3},
        {"expect": "Not updating totalInjectedDuration since fragment is Discarded", "min":0, "max":3},
        # Verify download attempt for 2nd corrupted segment"  
        {"expect": "Got next fragment url http://localhost:8085/VideoTestStream_HLS/hls/1080p_004.ts", "min":0, "max":3},
        # Confirmation - Downloaded segment is corrupt hence it is discarded"  
        {"expect": "Segment doesn't starts with valid TS packet, discarding", "min":0, "max":3},
        {"expect": "Not updating totalInjectedDuration since fragment is Discarded", "min":0, "max":3},
        # Verify download attempt for 3rd corrupted segment"  
        {"expect": "Got next fragment url http://localhost:8085/VideoTestStream_HLS/hls/1080p_005.ts", "min":0, "max":3},
        # Confirmation - Downloaded segment is corrupt hence it is discarded"  
        {"expect": "Segment doesn't starts with valid TS packet, discarding", "min":0, "max":3},
        {"expect": "Not updating totalInjectedDuration since fragment is Discarded", "min":0, "max":3},
        # Verify segment injection failed count reached to threshold value"  
        {"expect": "Reached max inject failure count: 3, stopping playback", "min":0, "max":3},
        # AAMP playback failed"  
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: Playback failed due to PTS error", "min":0, "max":3, "end_of_test":True},
    ],
    "corrupt_fragments_names": ['VideoTestStream_HLS/hls/1080p_003.ts', 'VideoTestStream_HLS/hls/1080p_004.ts', 'VideoTestStream_HLS/hls/1080p_005.ts']
}



TESTLIST = [TESTDATA1, TESTDATA2]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2028(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    [corrupt_fragments(f) for f in test_data["corrupt_fragments_names"]]
    aamp.run_expect_b(test_data)
    [corrupt_fragments(f, recover=True) for f in test_data["corrupt_fragments_names"]]

