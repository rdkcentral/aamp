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
#include <cjson/cJSON.h>
#include "StatsDefine.h"
#include "IPSessionSummary.h"

class CSessionSummaryTest : public ::testing::Test
{
protected:
    CSessionSummary *sessionSummary;

    void SetUp() override
    {
        ptr = new SessionSummaryP();
        sessionSummary = new CSessionSummary();
    }

    void TearDown() override
    {
        delete sessionSummary;
        sessionSummary = nullptr;
        delete ptr;
        ptr = nullptr;
    }

    class SessionSummaryP : public CSessionSummary
    {

    public:
        SessionSummaryP() : CSessionSummary() {}

        void Set_IsInitialised()
        {
            SessionSummaryP::isInitialized = true;
            ToJson();
        }
        void SetInitialised()
        {
            SessionSummaryP::isInitialized = true;
            SessionSummaryP::mSessionSummaryMap["ABCD"] = 3;
            ToJson();
        }
        void Setfunc(int x, bool m)
        {
            SessionSummaryP::mSessionSummaryMap["20"] = 20;
            UpdateSummary(x, m);
        }
    };
    SessionSummaryP *ptr;
};

TEST_F(CSessionSummaryTest, UpdateSummaryTest1)
{
    int response = 28;
    bool connectivity = true;
    sessionSummary->UpdateSummary(response, connectivity);
}

TEST_F(CSessionSummaryTest, UpdateSummaryTest4)
{
    ptr->Setfunc(20, true);
}
TEST_F(CSessionSummaryTest, IncrementCountTest1)
{
    std::string str = "abc123";
    sessionSummary->IncrementCount(str);
    ASSERT_NO_THROW(sessionSummary->IncrementCount(str));
}

TEST_F(CSessionSummaryTest, ToJsonTest1)
{
    ptr->Set_IsInitialised();
    cJSON *json = ptr->ToJson();
    ASSERT_EQ(json, nullptr);
    cJSON_Delete(json);
}
TEST_F(CSessionSummaryTest, IncrementCountTest2)
{
    ptr->SetInitialised();
}

