/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>

#include "AampGstUtils.h"
#include "MockAampGstUtils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class AampGstUtilsTests : public ::testing::Test
{
protected:

    void SetUp() override
    {
        g_mockAampGstUtils = new MockAampGstUtils();
    }

    void TearDown() override
    {
        delete g_mockAampGstUtils;
        g_mockAampGstUtils = nullptr;
    }

public:

};

TEST_F(AampGstUtilsTests, esMP3test)
{
    GstCaps dummycaps;
    GstCaps *caps{&dummycaps};
    EXPECT_CALL(*g_mockAampGstUtils,gst_caps_new_simple(StrEq("audio/mpeg"),StrEq("mpegversion"), G_TYPE_INT, 1, NULL)).WillOnce(Return(caps));

    EXPECT_TRUE(GetGstCaps(FORMAT_AUDIO_ES_MP3)==caps);
}
