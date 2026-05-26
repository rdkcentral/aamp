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
 * @file StreamAbstractionAAMP_MPD_Tests.cpp
 * @brief Unit tests for StreamAbstractionAAMP_MPD class
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fragmentcollector_mpd.h"
#include "priv_aamp.h"
#include "AampConfig.h"
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
/**
 * @brief Test fixture for StreamAbstractionAAMP_MPD tests
 */
class StreamAbstractionAAMP_MPD_TestFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize test objects
        aamp = std::make_shared<PrivateInstanceAAMP>();
        mpd = new StreamAbstractionAAMP_MPD(aamp.get(), 1.0, eMEDIATYPE_VIDEO);
    }
    void TearDown() override
    {
        // Cleanup
        if (mpd)
        {
            delete mpd;
            mpd = nullptr;
        }
        aamp.reset();
    }
    std::shared_ptr<PrivateInstanceAAMP> aamp;
    StreamAbstractionAAMP_MPD* mpd;
};
/**
 * @brief Basic test to verify test fixture setup
 */
TEST_F(StreamAbstractionAAMP_MPD_TestFixture, BasicSetup)
{
    EXPECT_NE(mpd, nullptr);
    EXPECT_NE(aamp, nullptr);
}
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
