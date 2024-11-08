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
#include "IPLatencyReport.h"
#include "StatsDefine.h"

bool g_ForPartnerApps = true;

class CLatencyReportTest : public ::testing::Test {
protected:
    // Member variable to hold the instance of CMCDHeaders
    CLatencyReport *LatencyReport;

    class TestLatencyReport : public CLatencyReport
    {
        public:
            TestLatencyReport() : CLatencyReport(){}

            cJSON *CallToJson()
            {
                TestLatencyReport::isInitialized = true;
                TestLatencyReport::mLatencyReportMap["First"] = 3;
                cJSON *value = ToJson();
                return value;
            }

            cJSON *CallToJson1()
            {
                TestLatencyReport::isInitialized = true;
                cJSON *value = ToJson();
                return value;
            }

            void CallRecordLatency(long timeMs)
            {
                TestLatencyReport::mLatencyReportMap["0"]=0;
                RecordLatency(timeMs);
            }
    };

    // SetUp() will be called before each test case
    void SetUp() override {
        mLatencyReport = new TestLatencyReport(); // obj for inherhit the pretected var..
        LatencyReport = new CLatencyReport(); // obj for inherit the public var..
    }

    // TearDown() will be called after each test case
    void TearDown() override {
        delete LatencyReport;
        LatencyReport = nullptr;
        delete mLatencyReport;
        mLatencyReport = nullptr;
    }
    TestLatencyReport *mLatencyReport;
};

TEST_F(CLatencyReportTest, ToJsonTest) 
{
    mLatencyReport->CallToJson();   
}

TEST_F(CLatencyReportTest, ToJsonTest1) 
{
    mLatencyReport->CallToJson1();   
}

TEST_F(CLatencyReportTest, RecordLatencyTest){
    int Report1= 1234;
    LatencyReport->RecordLatency(Report1);
   
}

TEST_F(CLatencyReportTest, RecordLatencyTest1)
{
    int timeMs= 0;
    mLatencyReport->CallRecordLatency(timeMs);   
}

