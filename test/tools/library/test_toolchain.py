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
#
import sys
import os
import subprocess

HELP = """

Used to test Video test streams.
    1. generate-hls-dash.sh is executed from test/VideoTestStream to harvest manifests.
    2. Simlinear server is started to host required URLs.
    3. URLs are run one-by-one in aamp-cli to test the playback.

"""
failedTests = []

def test_VideoTestStream():

    global exitStatus
    print("Starting VideoTestStreams")
    os.chdir('../../VideoTestStream')

    print("Harvesting...")
    os.system('./generate-hls-dash.sh')

    print("Starting simlinear server...")
    server_process = subprocess.Popen(['./startserver.sh'], shell=True)

    os.chdir('../tools/library')
    for idx, url in enumerate(VIDEO_TEST_STREAM_URLS):
        test_url = url["URL"]
        test_dir = "VideoTestStream_test{}".format(idx)
        os.makedirs(test_dir, exist_ok=True)
        if run_aamp(test_dir, test_url): 
            print("PASSED {} \n\n".format(test_dir))

        else:
            print("FAILED {} \n\n".format(test_dir))
            failedTests.append(test_dir)
            exitStatus = 1

    print("Stopping simlinear server")
    server_process.terminate()
    server_process.kill()

# VideoTestStream
"""
These are kept separate from rest of the URLs because the testing flow is different.
(No harvest and transcode is needed)
"""
VIDEO_TEST_STREAM_URLS = [
    {
        "URL": "http://127.0.0.1:8080/main.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main.mpd",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mp4.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mp4.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mux.m3u3",
    },
]

###################################


if __name__ == "__main__":


    # Run VideoTestStream tests
    test_VideoTestStream()

    if failedTests:
        print("FAILED TESTS:")
        for f in failedTests:
            print(f)
    else:
        print("ALL TESTS PASSED")

    sys.exit(exitStatus)