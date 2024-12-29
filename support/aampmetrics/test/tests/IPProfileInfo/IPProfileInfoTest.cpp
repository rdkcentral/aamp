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

#include <gtest/gtest.h>
#include "IPProfileInfo.h"
#include "StatsDefine.h"
#include "CMCDHeaders.h"
#include <cjson/cJSON.h>

#include "IPHTTPStatistics.h"
#include "IPLatencyReport.h"
#include "IPSessionSummary.h"
#include "ManifestGenericStats.h"

bool g_ForPartnerApps = true;

class CProfileInfoTest : public ::testing::Test
{
protected:
    CProfileInfo* newObj;
    void SetUp() override
    {
        cProfileInfo = new TestableCProfileInfo();
        newObj = new CProfileInfo();
    }

    void TearDown() override
    {
        delete cProfileInfo;
        cProfileInfo = nullptr;

        delete newObj;
        newObj = nullptr;
    }

    class TestableCProfileInfo : public CProfileInfo
    {
    public:
        TestableCProfileInfo() : CProfileInfo() {}

        void CallmpManifestStat()
        {
            CHTTPStatistics* mpManifestStat; 
            mpManifestStat = new CHTTPStatistics();
            GetManifestStat();

            ToJson();
            delete mpManifestStat;
        }
        void CallmpFragmentStat()
        {
            CFragmentStatistics* mpFragmentStat; 
            mpFragmentStat = new CFragmentStatistics();
            GetFragmentStat();

            

            ToJson();
            delete mpFragmentStat;
        }
        void CallJsonObjNotNullmWidth(int value)
        {
            
            mpManifestStat = new CHTTPStatistics();
            GetManifestStat();

            TestableCProfileInfo::mWidth = value;  

            ToJson();
        }
        void CallJsonObjNotNullmHeight(int value)
        {
            mpManifestStat = new CHTTPStatistics();
            GetManifestStat();

            TestableCProfileInfo::mHeight = value;  

            ToJson();
        }
        
    };
    TestableCProfileInfo *cProfileInfo;
};

TEST_F(CProfileInfoTest, ToJsonTest_0)
{
    // Test case 1: when mpManifestStat is not null
    cProfileInfo->CallmpManifestStat();
}


TEST_F(CProfileInfoTest, ToJsonTest_1)
{
    // Test case 2: when mpFragmentStat is not null
    cProfileInfo->CallmpFragmentStat();
}

TEST_F(CProfileInfoTest, ToJsonTest_2)
{
    // Test case 3: when jsonObj is not null
    int value = 1;
    // cProfileInfo->CallmpManifestStat();
    cProfileInfo->CallJsonObjNotNullmWidth(value);
}
TEST_F(CProfileInfoTest, ToJsonTest_2_1)
{
    //testing if condition with 0 value
    int value = 0;
    cProfileInfo->CallJsonObjNotNullmWidth(value);
}
TEST_F(CProfileInfoTest, ToJsonTest_3)
{
    // Test case 3: when jsonObj is not null
    int value = 5;
    cProfileInfo->CallJsonObjNotNullmHeight(value);
}
TEST_F(CProfileInfoTest, ToJsonTest_4)
{
    // Test case 3: when jsonObj is not null
    //testing if condition with 0 value
    int value = 0;

    cProfileInfo->CallJsonObjNotNullmHeight(value);
}

