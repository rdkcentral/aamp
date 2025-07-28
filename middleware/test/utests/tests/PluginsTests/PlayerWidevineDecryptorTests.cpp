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

#include "gstwidevinedecryptor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class PluginsTests : public ::testing::Test
{
protected:
	//GstAampwidevinedecryptor *aampwidevinedecryptor;
	void SetUp() override
	{
		//aampwidevinedecryptor = new GstAampwidevinedecryptor();
	}

	void TearDown() override
	{	
		//delete aampwidevinedecryptor;
	}
public:
};

TEST_F(PluginsTests, TestWidevineDecryptor_Init)
{
	//GstAampwidevinedecryptor *aampwidevinedecryptor = NULL;
	//gst_aampwidevinedecryptor_init(aampwidevinedecryptor);
}
