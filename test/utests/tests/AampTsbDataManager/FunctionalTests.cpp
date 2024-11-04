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
#include <gmock/gmock.h>
#include "AampTsbDataManager.h"
#include "AampConfig.h"
#include "AampLogManager.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::SetArgPointee;

AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:

    std::string url = "http://example.com/fragment1";
    std::string url1 = "http://example.com/fragment1";
    std::string url2 = "http://example.com/fragment2";
    std::string url3 = "http://example.com/fragment3";
    std::string period = "period1";
    std::string period1 = "period1";
    std::string period2 = "period2";
    std::string period3 = "period3";
    AampTsbDataManager *mDataManager;
    StreamInfo streamInfo;

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }
        mDataManager = new AampTsbDataManager();
    }

    void TearDown() override
    {
        delete mDataManager;
        mDataManager = nullptr;
    }
};


TEST_F(FunctionalTests, GetNearestFragment_SingleFragment)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period);
    auto fragmentAt = mDataManager->GetNearestFragment(5.0);
    auto fragmentBefore = mDataManager->GetNearestFragment(4.0);
    auto fragmentAfter = mDataManager->GetNearestFragment(6.0);
    EXPECT_NE(fragmentAt, nullptr);
    EXPECT_EQ(fragmentAt, fragmentBefore);
    EXPECT_EQ(fragmentAt, fragmentAfter);
}

TEST_F(FunctionalTests, GetNearestFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);

    auto fragmentBefore = mDataManager->GetNearestFragment(4.0);
    auto fragmentAt5 = mDataManager->GetNearestFragment(5.0);
    auto fragmentBetween = mDataManager->GetNearestFragment(7.5);
    auto fragmentAt15 = mDataManager->GetNearestFragment(15.0);
    auto fragmentAfter = mDataManager->GetNearestFragment(20.0);

    // Assertions to verify that the correct fragment is returned for each position
    EXPECT_NE(fragmentBefore, nullptr);
    EXPECT_EQ(fragmentBefore, fragmentAt5);
    EXPECT_NE(fragmentBetween, nullptr);
    EXPECT_NE(fragmentAt15, nullptr);
    EXPECT_EQ(fragmentAt15, fragmentAfter);
}

TEST_F(FunctionalTests, TestAddFragment_MissingInitHeader)
{
    std::string url = "http://example.com/fragment2";
    AampMediaType media = eMEDIATYPE_AUDIO;
    double position = 20.0;
    double duration = 5.0;
    double pts = 20.0;
    bool discont = false;
    std::string periodId = "period2";
    EXPECT_FALSE(mDataManager->AddFragment(url, media, position, duration, pts, discont, periodId));
}

TEST_F(FunctionalTests, TestAddFragment_WithDiscontinuity)
{
    std::string url = "http://example.com/fragment3";
    AampMediaType media = eMEDIATYPE_VIDEO;
    double position = 30.0;
    double duration = 5.0;
    double pts = 30.0;
    bool discont = true; // Discontinuity set to true
    std::string periodId = "period3";
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    EXPECT_TRUE(mDataManager->AddFragment(url, media, position, duration, pts, discont, periodId));
}

TEST_F(FunctionalTests, GetLastFragmentPosition_EmptyList)
{
    double position = mDataManager->GetLastFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 0.0);
}

TEST_F(FunctionalTests, GetLastFragmentPosition_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);
    double position = mDataManager->GetLastFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 15.0);
}

TEST_F(FunctionalTests, GetLastFragment_EmptyList)
{
    auto lastFragment = mDataManager->GetLastFragment();
    EXPECT_EQ(lastFragment, nullptr);
}

TEST_F(FunctionalTests, GetLastFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);
    auto lastFragment = mDataManager->GetLastFragment();

    auto fragmentAt15 = mDataManager->GetNearestFragment(17.0);
    EXPECT_EQ(lastFragment, fragmentAt15);
}

TEST_F(FunctionalTests, GetFirstFragment_EmptyList)
{
    auto firstFragment = mDataManager->GetFirstFragment();
    EXPECT_EQ(firstFragment, nullptr);
}

TEST_F(FunctionalTests, GetFirstFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);
    auto firstFragment = mDataManager->GetFirstFragment();

    auto fragmentAt5 = mDataManager->GetNearestFragment(7.0);
    EXPECT_EQ(firstFragment, fragmentAt5);
}

TEST_F(FunctionalTests, GetFirstFragmentPosition)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);
    double position = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 5.0);
}

TEST_F(FunctionalTests, GetFragmentTests)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);

    bool eos=false;
    auto fragment = mDataManager->GetFragment(5,eos);
    auto fragmentAt5 = mDataManager->GetNearestFragment(7.0);
    auto fragment3 = mDataManager->GetFragment(5.0001,eos);
    auto fragment2 = mDataManager->GetFragment(6,eos);

    EXPECT_EQ(fragment, fragmentAt5);
    EXPECT_EQ(fragment2, nullptr);
    EXPECT_EQ(fragment3, nullptr);
}


TEST_F(FunctionalTests, RemoveFragmentsBeforePosition)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false, period3);
    auto removedFragments = mDataManager->RemoveFragments(10.0);
    EXPECT_EQ(removedFragments.size(), 1);
    double firstFragmentPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstFragmentPositionAfterRemoval, 10.0);
}

TEST_F(FunctionalTests, RemoveFragmentsAll)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false, period3);
    auto removedFragments = mDataManager->RemoveFragments(20.0);
    EXPECT_EQ(removedFragments.size(), 3);
    bool isFragmentPresent = mDataManager->IsFragmentPresent(5.0);
    EXPECT_FALSE(isFragmentPresent);
}

TEST_F(FunctionalTests, RemoveFragmentsNone)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false, period3);
    auto removedFragments = mDataManager->RemoveFragments(5.0);
    EXPECT_TRUE(removedFragments.empty());
    double firstFragmentPosition = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstFragmentPosition, 5.0);
}

TEST_F(FunctionalTests, RemoveFragment_EmptyList)
{
    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment();
    EXPECT_EQ(removedFragment, nullptr);
}

TEST_F(FunctionalTests, RemoveFragment_SingleElement)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment();
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_DOUBLE_EQ(removedFragment->GetPosition(), 5.0);
    double position = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 0.0); // Expecting list to be empty after removal
}

TEST_F(FunctionalTests, RemoveFragment_MultipleElements)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false, period3);
    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment();
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_DOUBLE_EQ(removedFragment->GetPosition(), 5.0);
    double firstPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstPositionAfterRemoval, 10.0);
    removedFragment = mDataManager->RemoveFragment();
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_DOUBLE_EQ(removedFragment->GetPosition(), 10.0);
    firstPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstPositionAfterRemoval, 15.0);
}

TEST_F(FunctionalTests, GetNextDiscFragmentForwardSearch)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, true, period2); // Discontinuous fragment
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false, period3);
    TsbFragmentDataPtr fragment1 = mDataManager->GetNextDiscFragment(5.0, false);
    ASSERT_NE(fragment1, nullptr);
    EXPECT_DOUBLE_EQ(fragment1->GetPosition(), 10.0);
    TsbFragmentDataPtr fragment2 = mDataManager->GetNextDiscFragment(10.0, false); //exacct maatch
    ASSERT_NE(fragment2, nullptr);
    EXPECT_DOUBLE_EQ(fragment2->GetPosition(), 10.0);
}

TEST_F(FunctionalTests, GetNextDiscFragmentBackwardSearch)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, true, period1); // Discontinuous fragment
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, true, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 16.0, 1.0, 0.0, false, period3);
    TsbFragmentDataPtr fragment1 = mDataManager->GetNextDiscFragment(16.0, true);
    ASSERT_NE(fragment1, nullptr);
    EXPECT_DOUBLE_EQ(fragment1->GetPosition(), 10.0);
    TsbFragmentDataPtr fragment2 = mDataManager->GetNextDiscFragment(10.0, true);
    ASSERT_NE(fragment2, nullptr);
    EXPECT_DOUBLE_EQ(fragment2->GetPosition(), 10.0);
}

TEST_F(FunctionalTests, GetNextDiscFragmentNoDiscontinuity)
{ 
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false, period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 16.0, 1.0, 0.0, false, period3);

    TsbFragmentDataPtr fragment = mDataManager->GetNextDiscFragment(5.0, false);
    ASSERT_EQ(fragment, nullptr);
}

TEST_F(FunctionalTests, IsFragmentPresentTests)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);

    EXPECT_TRUE(mDataManager->IsFragmentPresent(5.0));
    EXPECT_TRUE(mDataManager->IsFragmentPresent(10.0));
    EXPECT_TRUE(mDataManager->IsFragmentPresent(15.0));
    EXPECT_FALSE(mDataManager->IsFragmentPresent(0.0));
    EXPECT_FALSE(mDataManager->IsFragmentPresent(20.0));
}


TEST_F(FunctionalTests, GetNearestFragment_EmptyData)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    auto fragment = mDataManager->GetNearestFragment(10.0);
    EXPECT_EQ(fragment, nullptr);
}


TEST_F(FunctionalTests, TestFlush)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period);
    mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1);
    mDataManager->AddFragment(url2, eMEDIATYPE_VIDEO, 10.0, 1.0, 0.0, false,period2);
    mDataManager->AddFragment(url3, eMEDIATYPE_VIDEO, 15.0, 1.0, 0.0, false,period3);
    mDataManager->Flush();

    EXPECT_EQ(mDataManager->GetFirstFragment(), nullptr);
    EXPECT_FALSE(mDataManager->AddFragment(url1, eMEDIATYPE_VIDEO, 5.0, 1.0, 0.0, false, period1)); // Confirm Init Fragments have been removed
}
