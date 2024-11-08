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
#include "IPFragmentStatistics.h"
#include <cjson/cJSON.h>

bool g_ForPartnerApps = true;

class IPFragmentStatisticsTest : public ::testing::Test {
protected:
    // Member variable to hold the instance of IPFragmentStatisticsTest
    CFragmentStatistics *IPFragment;
    class TestFragment : public CFragmentStatistics
    {
public: 
 TestFragment() : CFragmentStatistics(){}

    void callpNormalFragmentStat()
    {
       TestFragment:: pNormalFragmentStat = new CHTTPStatistics();
        ToJson();
    }

    void callpInitFragmentStat()
    {
        TestFragment::pNormalFragmentStat = new CHTTPStatistics();
        ToJson();
    }

    void Setm_url()
    {
        TestFragment::m_url="abc";
        ToJson();
    }
    void pInitFragmentStatTest(){
		pInitFragmentStat  = new CHTTPStatistics();
		ToJson();
	}

    };
    // SetUp() will be called before each test case
    void SetUp() override {
        // Create a new instance of IPFragmentStatisticsTest
        mIPFragment = new TestFragment();
        IPFragment = new CFragmentStatistics(); 
    }

    // TearDown() will be called after each test case
    void TearDown() override {
        // Delete the instance of IPFragmentStatisticsTest
        delete IPFragment;
        IPFragment = nullptr;
        delete mIPFragment;
        mIPFragment =nullptr;
    }
    TestFragment *mIPFragment;
};


// Test case for SetSessionId and GetSessionId
TEST_F(IPFragmentStatisticsTest, ToJsontest) {

    IPFragment->ToJson();

}

TEST_F(IPFragmentStatisticsTest, ToJsontest3) {

    mIPFragment->callpNormalFragmentStat();

}
TEST_F(IPFragmentStatisticsTest, ToJsontest5) {

    mIPFragment->Setm_url();

}

TEST_F(IPFragmentStatisticsTest, ToJsontest2) {

    mIPFragment->callpInitFragmentStat();

}

TEST_F(IPFragmentStatisticsTest, pInitFragmentStatTest) {

    mIPFragment->pInitFragmentStatTest();

}

TEST_F(IPFragmentStatisticsTest, SetUrltest2) {

    std::string url = "abc";
    IPFragment->SetUrl(url);
    IPFragment->ToJson();

}

TEST_F(IPFragmentStatisticsTest, GetNormalFragmentStattest) {
    CHTTPStatistics *pInitFragmentStat;
    pInitFragmentStat = IPFragment->GetNormalFragmentStat();

}

TEST_F(IPFragmentStatisticsTest, GetInitFragmentStattest) {
    CHTTPStatistics *pInitFragmentStat;
    pInitFragmentStat = IPFragment->GetInitFragmentStat();

}