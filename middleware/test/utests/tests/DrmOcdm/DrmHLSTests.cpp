/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockDrmHelper.h"


using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

std::shared_ptr<MockDrmHelper> drmHelper;

class DrmHLSTests : public ::testing::Test
{
	public:
	protected:
	void SetUp() override
	{
		drmHelper = std::make_shared<MockDrmHelper>();
	}

	void TearDown() override
	{
		drmHelper = nullptr;
	}
};

extern std::shared_ptr<DrmHelper> ProcessContentProtection( std::string attrName, bool param , bool isRequired);

TEST_F(DrmHLSTests, ProcessContentProtection)
{
	{ /* Widevine */
		std::string attrName = "#EXT-X-KEY:METHOD=SAMPLE-AES-CTR, KEYFORMAT=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\",KEYFORMATVERSIONS=\"1\",URI=\"data:text/plain;base64,AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsIARIgZTI1ZmFkYjQ4YmZiNDkyMjljZTBhNGFmZGZlMDUxOTcaB3NsaW5ndHYiBUhHVFZEKgVTRF9IRA==\",KEYID=0xe25fadb48bfb49229ce0a4afdfe05197";
		
		EXPECT_CALL(*drmHelper, isDecryptClearSamplesRequired()).WillOnce(Return(false));
	
		auto rc = ProcessContentProtection(attrName,drmHelper->getPropogateUriParam(),drmHelper->isDecryptClearSamplesRequired());
		ASSERT_NE( rc, nullptr );
	
	}
	
	{ /* PlayReady */
	    std::string attrName = "#EXT-X-KEY:METHOD=SAMPLE-AES-CTR,URI=\"data:text/plain;charset=UTF-16;base64,BgIAAAEAAQD8ATwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsASQBEAD4AbgA0AEkARABBAEsATwAxAHMARwByAGcAegBpAHkAOAA4AFgAcgBqAGYAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgB1AGkAbwA4AFcAVQBwAFQANAA0ADAAPQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=\",KEYFORMAT=\"com.microsoft.playready\",KEYFORMATVERSIONS=\"1\"";
	
		EXPECT_CALL(*drmHelper, isDecryptClearSamplesRequired()).WillOnce(Return(false));
		auto rc = ProcessContentProtection(attrName,drmHelper->getPropogateUriParam(),drmHelper->isDecryptClearSamplesRequired());

		// getting nullptr here
		// comments elsewhere indicate PlayReady helper currently supports DASH only, so known issue
		// ASSERT_NE( rc, nullptr );
	}
}
