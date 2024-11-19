#!/usr/bin/env python3
#
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

# Utility class to verify PTS restamping in L2 tests.
###############################################################################

class PtsRestampUtils:
    restamp_values:dict[str, float] = {}
    segment_cnt = 0
    max_segment_cnt = 0
    # Minimum tolerance for restamped PTS checks. Can be set by the test.
    tolerance_min = 0
    segment_cnt_reached = None

    def reset(self):
        self.restamp_values = {}
        self.segment_cnt = 0
        self.max_segment_cnt = 0
        self.segment_cnt_reached = None

    def restart(self):
        self.restamp_values = {}
        self.segment_cnt = 0

    def check_restamp(self,match,arg):
        # Get the fields from the log line
        mediaTrack = match.group(1)
        timeScale = float(match.group(2))
        before = float(match.group(3))
        after = float(match.group(4))
        duration = float(match.group(5))
        url = match.group(6)

        self.segment_cnt += 1
        print(f"PTS check ({self.segment_cnt}) mediaTrack {mediaTrack} timeScale {timeScale:.0f} before {before:.0f} after {after:.0f} duration {duration:.0f} url {url}")

        # The actual duration in the provided segments may not match that from the manifest.
        # This can be seen in https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd
        # We allow the pts value after restamp to differ by 5% of the segment duration.
        duration_sec = duration / timeScale
        tolerance_sec = duration_sec * 0.05
        tolerance_sec = max(self.tolerance_min, tolerance_sec)
        after_sec = after / timeScale

        # Skip check of restamp if this is the first segment because we cannot do comparision with previous
        if mediaTrack in self.restamp_values:
            expected_restamp = self.restamp_values.get(mediaTrack)
            diff_sec = abs(after_sec - expected_restamp)
            str = f"PTS: actual {after_sec:.3f}s, expected {expected_restamp:.3f}s, diff {diff_sec:.3f}s, tol {tolerance_sec:.3f}s"
            print(str)
            assert diff_sec <= tolerance_sec, str

        # Save what we are expecting for the next value
        self.restamp_values[mediaTrack] = after_sec + duration_sec

        # Call function provided if enough segments have been restamped
        if self.segment_cnt_reached != None and self.max_segment_cnt != 0 and self.segment_cnt > self.max_segment_cnt:
            self.segment_cnt_reached()
            self.segment_cnt_reached = None     # Clear the function provided so it is not called a second time

    def check_num_segments(self):
        str = f"Number of segments: restamped {self.segment_cnt}, minimum expected {self.max_segment_cnt}"
        print(str)
        assert self.segment_cnt >= self.max_segment_cnt, str

class TrickModesPtsRestampUtils:
    LOG_LINE = r'\[TrickModePtsRestamp\]\[\d+\]rate (-?\d+\.\d+), initFragment ([01]), discontinuity [01], position \d+\.\d+s, duration \d+\.\d+s, restamped position (\d+\.\d+)s, duration (\d+\.\d+)s'
    expected_position = 0.0
    segment_cnt = 0
    max_segment_cnt = 0
    # Minimum tolerance for floating point calculation.
    tolerance = 0.001
    segment_cnt_reached = None

    def reset(self):
        self.restamp_pos = 0.0
        self.segment_cnt = 0
        self.max_segment_cnt = 0
        self.segment_cnt_reached = None

    def check_restamp(self,match,arg):
        # Get the fields from the log line
        rate = float(match.group(1))
        initFragment = match.group(2)
        restampedPosition = float(match.group(3))
        restampedDuration = float(match.group(4))

        self.segment_cnt += 1
        print(f"Trick modes PTS check ({self.segment_cnt}) rate {rate} initFragment {initFragment} restampedPosition {restampedPosition}s restampedDuration {restampedDuration}s")

        # Set the expected restamped postion to 0 for each init fragment
        if initFragment == '1':
            self.expected_position = 0.0
        else:
            diff_sec = abs(restampedPosition - self.restamp_pos)
            str = f"Trick modes PTS: rate {rate:.1f}, actual {restampedPosition:.3f}s, expected {self.restamp_pos:.3f}s, diff {diff_sec:.3f}s"
            print(str)
            assert diff_sec <= self.tolerance, str

        # Save what we are expecting for the next value
        self.restamp_pos = restampedPosition + restampedDuration

        # Call function provided if enough segments have been restamped
        if self.segment_cnt_reached != None and self.max_segment_cnt != 0 and self.segment_cnt > self.max_segment_cnt:
            self.segment_cnt_reached()
            self.segment_cnt_reached = None     # Clear the function provided so it is not called a second time

    def check_num_segments(self):
        str = f"Trick modes number of segments: restamped {self.segment_cnt}, minimum expected {self.max_segment_cnt}"
        print(str)
        assert self.segment_cnt >= self.max_segment_cnt, str
