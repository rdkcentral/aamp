#!/bin/bash -e
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
#
# This script will build and run TSB L1 and L2 tests.

cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE_ENABLED=ON .
make VERBOSE=1
ctest --output-on-failure
output=Test_Coverage/index.html
# To produce a more useful measure of test coverage, exclude log macros
# (which contain branches) and compiler-generated exception branches.
#
# Exception branches are generated for all C++ standard library calls
# that are not declared "noexcept" - even for the variants that TSB
# uses that return errors via output parameter or fail() method call.
# TSB wouldn't be expected to have exception handling code for these,
# or for construction of types like std::string or filesystem::path -
# which might in theory throw exceptions like "bad_alloc".
#
# For more info, refer to gcov documentation.
mkdir -p $(dirname $output) && gcovr --html $output --html-details --filter src --exclude-lines-by-pattern ".*TSB_LOG_.*" --exclude-throw-branches
echo "Coverage written to $output"
