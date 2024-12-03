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

#include <cjson/cJSON.h>
#include <gtest/gtest.h>

#include "IPLicnsStatistics.h"
#include "StatsDefine.h"

bool g_ForPartnerApps = true;

class IPLicnsStatisticsTest : public ::testing::Test
{
protected:
    CLicenseStatistics *mCLicenseStatistics;

    class TestLicenseStatistics : public CLicenseStatistics
    {
        public:
            TestLicenseStatistics() : CLicenseStatistics(){}

            void CallIncrementCount(VideoStatCountType type)
            {
                TestLicenseStatistics::isInitialized = true;
                IncrementCount(type);
            }

            void CallRecord_License_EncryptionStat(bool  isEncypted, bool isKeyChanged)
            {
                TestLicenseStatistics::isInitialized = true;
                TestLicenseStatistics::mbEncypted = true;
                Record_License_EncryptionStat(isEncypted, isKeyChanged);
            }

            void CallRecord_License_EncryptionStat1(bool  isEncypted, bool isKeyChanged)
            {
                TestLicenseStatistics::isInitialized = true;
                TestLicenseStatistics::mbEncypted = false;
                Record_License_EncryptionStat(isEncypted, isKeyChanged);
            }

            void CallRecord_License_EncryptionStat2(bool  isEncypted, bool isKeyChanged)
            {
                TestLicenseStatistics::isInitialized = false;
                Record_License_EncryptionStat(isEncypted, isKeyChanged);
            }

            cJSON *CallToJson()
            {
                TestLicenseStatistics::mTotalClearToEncrypted = 2;
                cJSON *value = ToJson();
                TestLicenseStatistics::mTotalEncryptedToClear = 2;
                value = ToJson();
                return value;
            }
    };

    void SetUp() override {
        mCLicenseStatistics = new CLicenseStatistics();
        mLicenseStat = new TestLicenseStatistics();
    }
    void TearDown() override {
        delete mCLicenseStatistics;
        mCLicenseStatistics = nullptr;
        delete mLicenseStat;
        mLicenseStat = nullptr;
    }

    TestLicenseStatistics *mLicenseStat;
};

TEST_F(IPLicnsStatisticsTest, IncrementCountTest)
{
    VideoStatCountType type[4] = {
        COUNT_UNKNOWN,
	    COUNT_LIC_TOTAL,
	    COUNT_LIC_ENC_TO_CLR,
	    COUNT_LIC_CLR_TO_ENC,
        };
    for(int i=0;i<4;i++)
    {
        mLicenseStat->CallIncrementCount(type[i]);
    }
}

TEST_F(IPLicnsStatisticsTest, Record_License_EncryptionStat)
{
    bool isEncypted = false;
    bool isKeyChanged = true;
    mLicenseStat->CallRecord_License_EncryptionStat(isEncypted, isKeyChanged);
}

TEST_F(IPLicnsStatisticsTest, Record_License_EncryptionStat1)
{
    bool isEncypted = true;
    bool isKeyChanged = true;
    mLicenseStat->CallRecord_License_EncryptionStat1(isEncypted, isKeyChanged);
}

TEST_F(IPLicnsStatisticsTest, Record_License_EncryptionStat2)
{
    bool isEncypted = false;
    bool isKeyChanged = true;
    mLicenseStat->CallRecord_License_EncryptionStat2(isEncypted, isKeyChanged);
}

TEST_F(IPLicnsStatisticsTest, ToJsonTest)
{
    mLicenseStat->CallToJson();
}


