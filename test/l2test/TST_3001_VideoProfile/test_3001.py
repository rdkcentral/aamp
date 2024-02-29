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

full_test_data = {
    "title": "Test multi video profile",
    "max_test_time_seconds": 90,
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n"
}

def test_3001(aamp_setup_teardown):

    with open("TST_3001_VideoProfile/commands.json") as f:
        commands = json.loads(f.read())

    failed_indices = []
    failed_test = False

    for idx, test_sequence in enumerate(commands):

        full_test_data["expect_list"] = test_sequence
        full_test_data["logfile"] = "multiprofile_" + str(idx) + ".log",

        aamp = aamp_setup_teardown
        aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
        aamp.create_aamp_cfg(full_test_data.get('aamp_cfg'))

        if aamp.run_expect_a(full_test_data) is False:
            failed_test = True
            failed_indices.append(idx)

    if failed_test:
        err_str = ""
        for idx in failed_indices:
            err_str = err_str + " " + str(idx)
        print("\nFailed profiles: {}".format(err_str))
        sys.exit(os.EX_SOFTWARE)   #Return non-zero


