/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file FragmentCollectorMpdTestsMain.cpp
 * @brief L1 Test Runner for StreamAbstractionAAMP_MPD 
 * @details Following updated L1 testing instructions from .github/copilot_instructions/
 *          - Uses fake infrastructure properly (links fakes library)
 *          - Tests focus on component behavior, not mock/fake behavior
 *          - Proper build integration with master test script
 */

#include <gtest/gtest.h>

/**
 * @brief Main test runner
 * @details Standard GoogleTest main function for L1 tests
 */
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
