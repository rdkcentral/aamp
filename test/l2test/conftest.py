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
import pytest
import framework

"""
A good starting point
https://stackoverflow.com/questions/34466027/what-is-conftest-py-for-in-pytest
"""
def pytest_addoption(parser):
    """
    Add some options to pytest that can then be passed
    onto framework.py that is used for testing aamp
    """
    parser.addoption("--aamp_video", action="store_true", help="run AAMP with video window")
    parser.addoption("--aamp_log", action="store_true", help="AAMP logging to stdout rather than file")
    parser.addoption("--sim_log", action="store_true", help="AAMP logging to stdout rather than file")


@pytest.fixture
def aamp_setup_teardown(request):
    """
    The following creates an aamp instance before the test starts
    and ensures that any running aamp or simlinear processes are
    stopped when the test finishes
    """
    aamp = framework.Aamp(request)
    yield aamp
    print("Cleanup")
    aamp.exit_aamp()
