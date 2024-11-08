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

#include "IPHTTPStatistics.h"

bool g_ForPartnerApps = true;

class CHTTPStatisticsTest : public ::testing::Test
{
protected:
    CHTTPStatistics *mCHTTPStatistics;


    class TestHTTPStatistics : public CHTTPStatistics
    {
        public:
            TestHTTPStatistics() : CHTTPStatistics(){}

            void CallmLatencyReport()
            {
                CLatencyReport* mLatencyReport;
                mLatencyReport = new CLatencyReport();
                GetLatencyReport();
                ToJson();

                delete mLatencyReport;
            }

            void CallmSessionSummary()
            {
                CSessionSummary * mSessionSummary;
                mSessionSummary = new CSessionSummary();
                GetSessionSummary();
                ToJson();

                delete mSessionSummary;
            }

            void CallmManifestGenericStats()
            {
                ManifestGenericStats * mManifestGenericStats;
                mManifestGenericStats = new ManifestGenericStats();
                GetManGenStatsInstance();
                ToJson();

                delete mManifestGenericStats;
            }
    };

    void SetUp() override {
        mCHTTPStatistics = new CHTTPStatistics();
        mHTTPStat = new TestHTTPStatistics();
    }
    void TearDown() override {
        delete mCHTTPStatistics;
        mCHTTPStatistics = nullptr;
        delete mHTTPStat;
        mHTTPStat = nullptr;
    }

    TestHTTPStatistics *mHTTPStat;
};

TEST_F(CHTTPStatisticsTest, IncrementCountTest)
{
    long downloadTimeMs = 500000;
    int responseCode = 3;
    bool connectivity = true;
    ManifestData *manifestData;
    mCHTTPStatistics->IncrementCount(downloadTimeMs, responseCode, connectivity, manifestData);
}

TEST_F(CHTTPStatisticsTest, IncrementCountTest1)
{
    long downloadTimeMs = 500000;
    int responseCode = 200;
    bool connectivity = true;
    ManifestData *manifestData;
    mCHTTPStatistics->IncrementCount(downloadTimeMs, responseCode, connectivity, manifestData);
}

TEST_F(CHTTPStatisticsTest, IncrementCountTest2)
{
    long downloadTimeMs = 500000;
    int responseCode = 200;
    bool connectivity = true;

    ManifestData * manifestData;
    size_t size;
    long parseTimeMs;
    size_t periodCount;
    manifestData = new ManifestData(downloadTimeMs, size, parseTimeMs, periodCount);
    manifestData->mDownloadTimeMs=downloadTimeMs;
    manifestData->mParseTimeMs = 100;
    manifestData->mSize = 5000;
    manifestData->mPeriodCount = 20;

    mCHTTPStatistics->IncrementCount(downloadTimeMs, responseCode, connectivity, manifestData);
}

TEST_F(CHTTPStatisticsTest, cJSON_test)
{
    mHTTPStat->CallmLatencyReport();
}

TEST_F(CHTTPStatisticsTest, cJSON_test1)
{
    mHTTPStat->CallmSessionSummary();
}

TEST_F(CHTTPStatisticsTest, cJSON_test2)
{
    mHTTPStat->CallmManifestGenericStats();
}
