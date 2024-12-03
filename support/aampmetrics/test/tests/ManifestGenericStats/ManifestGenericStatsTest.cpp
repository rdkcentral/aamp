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
#include "ManifestGenericStats.h"
#include "CMCDHeaders.h"
#include <cjson/cJSON.h>



class ManifestGenericStatsTest : public ::testing::Test {
protected:

    void SetUp() override {
        
        manifestGenericStats = new TestableManifestGenericStats();
    }
    
    void TearDown() override {
        
        delete manifestGenericStats;
        manifestGenericStats = nullptr;

    }
    class TestableManifestGenericStats : public ManifestGenericStats
    {
    public:
        TestableManifestGenericStats() : ManifestGenericStats()
        {
        }

        cJSON * CallToJson()
        {
            TestableManifestGenericStats::isInitialized = true;

            cJSON *jsonObj = ToJson();
            EXPECT_TRUE(isInitialized);
            return jsonObj;
        }
    };
    TestableManifestGenericStats* manifestGenericStats; 

};

TEST_F(ManifestGenericStatsTest, ToJsonTest_1) 
{ 
    cJSON *jsonObj = manifestGenericStats->CallToJson();
    EXPECT_TRUE(jsonObj);
    
}
TEST_F(ManifestGenericStatsTest, UpdateManifestDataTest_2) {
    // Allocate memory for manifestData
    ManifestData* manifestData = new ManifestData(100, 500, 50, 3);
    manifestData->mDownloadTimeMs = 100;
    manifestData->mSize = 500;
    manifestData->mParseTimeMs = 50;
    manifestData->mPeriodCount = 3;
    manifestGenericStats->UpdateManifestData(manifestData);
    delete manifestData;
    EXPECT_TRUE(manifestData);
}

TEST_F(ManifestGenericStatsTest, ToJsonTest) 
{ 
    cJSON *jsonObj =  manifestGenericStats->ToJson();
    ASSERT_TRUE(jsonObj != nullptr);
}

