/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "AampMPDUtils.h"

AampConfig *gpGlobalConfig{nullptr};

class AampMPDUtils : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};


TEST(AampMPDUtils, IsCompatibleMimeTypeTest1)
{
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
    eMEDIATYPE_AUDIO,
    eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
    eMEDIATYPE_MANIFEST,
    eMEDIATYPE_LICENCE,
    eMEDIATYPE_IFRAME,
    eMEDIATYPE_INIT_VIDEO,
    eMEDIATYPE_INIT_AUDIO,
    eMEDIATYPE_INIT_SUBTITLE,
    eMEDIATYPE_INIT_AUX_AUDIO,
    eMEDIATYPE_PLAYLIST_VIDEO,
    eMEDIATYPE_PLAYLIST_AUDIO,
    eMEDIATYPE_PLAYLIST_SUBTITLE,
    eMEDIATYPE_PLAYLIST_AUX_AUDIO,
    eMEDIATYPE_PLAYLIST_IFRAME,
    eMEDIATYPE_INIT_IFRAME,
    eMEDIATYPE_DSM_CC,
    eMEDIATYPE_IMAGE,
    eMEDIATYPE_DEFAULT
    };
    std::string mimeType = "test";
    for(int i=0; i<21; i++){
    bool minetype = IsCompatibleMimeType(mimeType,mediaType[i]);
	EXPECT_FALSE(minetype);
    }
}

TEST(AampMPDUtils, IsCompatibleMimeTypeTest2)
{
    AampMediaType mediaType = eMEDIATYPE_AUX_AUDIO;
    std::string mimeType = "audio/webm";
    bool result = IsCompatibleMimeType(mimeType,mediaType);
	EXPECT_TRUE(result);
}
TEST(AampMPDUtils, IsCompatibleMimeTypeTest3)
{
    AampMediaType mediaType = eMEDIATYPE_SUBTITLE;
    std::string mimeType = "application/ttml+xml";
    bool minetype = IsCompatibleMimeType(mimeType,mediaType);
	EXPECT_TRUE(minetype);
}
TEST(AampMPDUtils, ComputeFragmentDurationTest1)
{
	uint32_t duration = 0;
	uint32_t timeScale = 50;

	double result = ComputeFragmentDuration(duration,timeScale);
	EXPECT_DOUBLE_EQ(result,2.0);
}
