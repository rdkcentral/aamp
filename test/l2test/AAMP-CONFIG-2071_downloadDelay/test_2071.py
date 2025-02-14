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


amp_url_timestamp = None


# callback used in testcases
def verify_download_delay(match, download_delay_configured):
    global aamp_url_timestamp

    timestamp = float(match.group(1))  # Timestamp for aamp url or HttpRequestEnd

    # Convert downloadDelay from milliseconds to seconds
    download_delay_configured_sec = float(download_delay_configured) / 1000.0 # The expected download delay in secs
    print(f"download_delay_configured (in sec): {download_delay_configured_sec}")

    if "aamp url" in match.group(0):
        aamp_url_timestamp = timestamp  # Store the timestamp for aamp url when found

    elif "HttpRequestEnd" in match.group(0):
        # Assert that aamp_url_timestamp is not None before proceeding
        assert aamp_url_timestamp is not None, "Test failed: 'aamp_url_timestamp' is None. 'aamp url' was not found before 'HttpRequestEnd'."

        time_difference = round(timestamp - aamp_url_timestamp, 2)
        lower_bound = (download_delay_configured_sec - 0.5)  # lower bound in accordance with minor latency variations
        upper_bound = (download_delay_configured_sec + 2.5)  # upper bound in accordance with larger network delays
        print(f"Valid range: {lower_bound} to {upper_bound} seconds")
        print(f"Time difference: {time_difference} seconds")

        assert (lower_bound <= time_difference <= upper_bound), f"Test failed: Time difference of {time_difference} exceeds the valid range of {lower_bound} to {upper_bound} seconds."

        # If the assert passes output message
        print("Time difference is within the expected range.")

        # Reset the timestamp to None to avoid impacting the next test
        aamp_url_timestamp = None


download_delay = 5000
TESTDATA0 = {
    "title": "Set downloadDelay to 5000",
    "logfile": "downloadDelay_5000_ms.log",
    "aamp_cfg": f"trace=true\ninfo=true\nprogress=true\nabr=false\ndownloadDelay={download_delay}\n",
    "max_test_time_seconds": 60,
    "url": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list": [
        {"expect": r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd","max": 10},
        {"expect": r"([0-9,\.]+): ([\S]+)aamp url:(\d+),(\d+),(\d+),(\d+\.\d+),https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_init.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9\,\.]+): ([\S]+)HttpRequestEnd: [0-9\,\.]+https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_init.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9,\.]+): ([\S]+)aamp url:(\d+),(\d+),(\d+),(\d+\.\d+),https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_001.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9\,\.]+): ([\S]+)HttpRequestEnd: [0-9\,\.]+https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_001.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"Returning Position as [1-3](\d{3})","min": 30,"end_of_test": True},
    ],
}

download_delay = 10000
TESTDATA1 = {
    "title": "Set downloadDelay to 10000",
    "logfile": "downloadDelay_10000_ms.log",
    "aamp_cfg": f"trace=true\ninfo=true\nprogress=true\nabr=false\ndownloadDelay={download_delay}\n",
    "max_test_time_seconds": 60,
    "url": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list": [
        {"expect": r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd","max": 10},
        {"expect": r"([0-9,\.]+): ([\S]+)aamp url:(\d+),(\d+),(\d+),(\d+\.\d+),https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_init.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9\,\.]+): ([\S]+)HttpRequestEnd: [0-9\,\.]+https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_init.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9,\.]+): ([\S]+)aamp url:(\d+),(\d+),(\d+),(\d+\.\d+),https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_001.m4s","max": 30,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"([0-9\,\.]+): ([\S]+)HttpRequestEnd: [0-9\,\.]+https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(360|480|720|1080)p_001.m4s","max": 50,"callback": verify_download_delay,"callback_arg": download_delay},
        {"expect": r"Returning Position as [1-3](\d{3})","min": 30,"end_of_test": True},
    ],
}

############################################################
TESTLIST = [TESTDATA0, TESTDATA1]


@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param


def test_2071(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
