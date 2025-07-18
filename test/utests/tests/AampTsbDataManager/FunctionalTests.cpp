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

    std::string url1 = "http://example.com/fragment1";
    std::string url2 = "http://example.com/fragment2";
    std::string url3 = "http://example.com/fragment3";
    std::string url = url1;
    std::string period1 = "period1";
    std::string period2 = "period2";
    std::string period3 = "period3";
    std::string period = period1;
    double absPosition1 = 1005.0;
    double absPosition2 = 2005.0;
    double absPosition3 = 3005.0;
    double absPosition = absPosition1;
    AampTsbDataManager *mDataManager;
    StreamInfo streamInfo;
    std::shared_ptr<CachedFragment> cachedFragment;
    TSBWriteData writeData;

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }
        mDataManager = new AampTsbDataManager();
        cachedFragment = std::make_shared<CachedFragment>();
        cachedFragment->type = eMEDIATYPE_VIDEO;
        cachedFragment->position = 105.0;
        cachedFragment->absPosition = absPosition;
        cachedFragment->duration = 1.0;
        cachedFragment->timeScale = 240000;
        cachedFragment->PTSOffsetSec = 0.0;
        writeData.url = url;
        writeData.cachedFragment = cachedFragment;
        writeData.pts = 0.0;
        writeData.periodId = period;
    }

    void TearDown() override
    {
        delete mDataManager;
        mDataManager = nullptr;
    }
};


TEST_F(FunctionalTests, GetNearestFragment_SingleFragment)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);
    auto fragmentAt = mDataManager->GetNearestFragment(1005.0);
    auto fragmentBefore = mDataManager->GetNearestFragment(1004.0);
    auto fragmentAfter = mDataManager->GetNearestFragment(1006.0);
    EXPECT_NE(fragmentAt, nullptr);
    EXPECT_EQ(fragmentAt, fragmentBefore);
    EXPECT_EQ(fragmentAt, fragmentAfter);
}

TEST_F(FunctionalTests, GetNearestFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto fragmentBefore = mDataManager->GetNearestFragment(1004.0);
    auto fragmentAt5 = mDataManager->GetNearestFragment(1005.0);
    auto fragmentBetween = mDataManager->GetNearestFragment(1007.5);
    auto fragmentAt15 = mDataManager->GetNearestFragment(1015.0);
    auto fragmentAfter = mDataManager->GetNearestFragment(1020.0);

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
    double position = 120.0;
    double absPosition = 1020.0;
    double duration = 5.0;
    double pts = 120.0;
    bool discont = false;
    std::string periodId = "period2";

    writeData.url = url;
    writeData.pts = pts;
    writeData.periodId = periodId;
    writeData.cachedFragment->position = position;
    writeData.cachedFragment->absPosition = absPosition;
    writeData.cachedFragment->duration = duration;

    EXPECT_FALSE(mDataManager->AddFragment(writeData, media, discont));
}

TEST_F(FunctionalTests, TestAddFragment_WithDiscontinuity)
{
    std::string url = "http://example.com/fragment3";
    AampMediaType media = eMEDIATYPE_VIDEO;
    AampTime position = 130.0;
    AampTime absPosition = 1030.0;
    AampTime duration = 5.0;
    AampTime pts = 30.0;
    bool discont = true; // Discontinuity set to true
    std::string periodId = "period3";
    uint32_t timeScale = 240000;
    AampTime PTSOffsetSec = 123.4;

    writeData.url = url;
    writeData.pts = pts.inSeconds();
    writeData.periodId = periodId;
    writeData.cachedFragment->position = position.inSeconds();
    writeData.cachedFragment->absPosition = absPosition.inSeconds();
    writeData.cachedFragment->duration = duration.inSeconds();
    writeData.cachedFragment->timeScale = timeScale;
    writeData.cachedFragment->PTSOffsetSec = PTSOffsetSec.inSeconds();

    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition.inSeconds());
    EXPECT_TRUE(mDataManager->AddFragment(writeData, media, discont));
    EXPECT_STREQ(mDataManager->GetLastFragment()->GetUrl().c_str(), url.c_str());
    EXPECT_EQ(mDataManager->GetLastFragment()->GetMediaType(), media);
    EXPECT_EQ(mDataManager->GetLastFragment()->GetAbsolutePosition(), absPosition);
    EXPECT_EQ(mDataManager->GetLastFragment()->GetDuration(), duration);
    EXPECT_EQ(mDataManager->GetLastFragment()->GetPTS(), pts);
    EXPECT_TRUE(mDataManager->GetLastFragment()->IsDiscontinuous());
    EXPECT_EQ(mDataManager->GetLastFragment()->GetPeriodId(), periodId);
    EXPECT_EQ(mDataManager->GetLastFragment()->GetTimeScale(), timeScale);
    EXPECT_EQ(mDataManager->GetLastFragment()->GetPTSOffsetSec(), PTSOffsetSec);
}

TEST_F(FunctionalTests, GetLastFragmentPosition_EmptyList)
{
    double position = mDataManager->GetLastFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 0.0);
}

TEST_F(FunctionalTests, GetLastFragmentPosition_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    double position = mDataManager->GetLastFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 1015.0);
}

TEST_F(FunctionalTests, GetLastFragment_EmptyList)
{
    auto lastFragment = mDataManager->GetLastFragment();
    EXPECT_EQ(lastFragment, nullptr);
}

TEST_F(FunctionalTests, GetLastFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto lastFragment = mDataManager->GetLastFragment();
    auto fragmentRequested = mDataManager->GetNearestFragment(1017.0);
    EXPECT_EQ(lastFragment, fragmentRequested);
}

TEST_F(FunctionalTests, GetFirstFragment_EmptyList)
{
    auto firstFragment = mDataManager->GetFirstFragment();
    EXPECT_EQ(firstFragment, nullptr);
}

TEST_F(FunctionalTests, GetFirstFragment_MultipleFragments)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto firstFragment = mDataManager->GetFirstFragment();
    auto fragmentRequested = mDataManager->GetNearestFragment(1007.0);
    EXPECT_EQ(firstFragment, fragmentRequested);
}

TEST_F(FunctionalTests, GetFirstFragmentPosition)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    double position = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(position, 1005.0);
}

TEST_F(FunctionalTests, GetFragmentTests)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    bool eos=false;
    auto fragment = mDataManager->GetFragment(1005.0, eos);
    auto fragmentAt5 = mDataManager->GetNearestFragment(1007.0);
    auto fragment3 = mDataManager->GetFragment(1005.0001, eos);
    auto fragment2 = mDataManager->GetFragment(1006, eos);

    EXPECT_EQ(fragment, fragmentAt5);
    EXPECT_EQ(fragment2, nullptr);
    EXPECT_EQ(fragment3, nullptr);
}

TEST_F(FunctionalTests, RemoveFragmentsBeforePosition)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto removedFragments = mDataManager->RemoveFragments(1010.0);
    EXPECT_EQ(removedFragments.size(), 1);
    double firstFragmentPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstFragmentPositionAfterRemoval, 1010.0);
}

TEST_F(FunctionalTests, RemoveFragmentsAll)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto removedFragments = mDataManager->RemoveFragments(1020.0);
    EXPECT_EQ(removedFragments.size(), 3);
    bool isFragmentPresent = mDataManager->IsFragmentPresent(1005.0);
    EXPECT_FALSE(isFragmentPresent);
}

TEST_F(FunctionalTests, RemoveFragmentsNone)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    auto removedFragments = mDataManager->RemoveFragments(1005.0);
    EXPECT_TRUE(removedFragments.empty());
    double firstFragmentPosition = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstFragmentPosition, 1005.0);
}

TEST_F(FunctionalTests, RemoveFragment_EmptyList)
{
    bool initDeleted = false;
    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment(initDeleted);
    EXPECT_EQ(initDeleted,false);
    EXPECT_EQ(removedFragment, nullptr);
}

TEST_F(FunctionalTests, RemoveFragment_SingleElement)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);
    bool initDeleted = false;

    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment(initDeleted);
    EXPECT_EQ(initDeleted,true);
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_EQ(removedFragment->GetAbsolutePosition(), static_cast<AampTime>(1005.0));
    double absPosition = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(absPosition, 0.0); // Expecting list to be empty after removal
}

TEST_F(FunctionalTests, RemoveFragment_MultipleElements)
{
    bool initDeleted = false;
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    TsbFragmentDataPtr removedFragment = mDataManager->RemoveFragment(initDeleted);
    EXPECT_EQ(initDeleted,false);
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_EQ(removedFragment->GetAbsolutePosition(), static_cast<AampTime>(1005.0));

    double firstPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstPositionAfterRemoval, 1010.0);

    removedFragment = mDataManager->RemoveFragment(initDeleted);
    EXPECT_EQ(initDeleted,false);
    EXPECT_NE(removedFragment, nullptr);
    EXPECT_EQ(removedFragment->GetAbsolutePosition(), static_cast<AampTime>(1010.0));

    firstPositionAfterRemoval = mDataManager->GetFirstFragmentPosition();
    EXPECT_DOUBLE_EQ(firstPositionAfterRemoval, 1015.0);
}

TEST_F(FunctionalTests, GetNextDiscFragmentForwardSearch)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, true);   // Discontinuous fragment

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    writeData.cachedFragment->discontinuity = false;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    TsbFragmentDataPtr fragment1 = mDataManager->GetNextDiscFragment(1005.0, false);
    ASSERT_NE(fragment1, nullptr);
    EXPECT_EQ(fragment1->GetAbsolutePosition(), static_cast<AampTime>(1010.0));

    TsbFragmentDataPtr fragment2 = mDataManager->GetNextDiscFragment(1010.0, false); // Exact match
    ASSERT_NE(fragment2, nullptr);
    EXPECT_EQ(fragment2->GetAbsolutePosition(), static_cast<AampTime>(1010.0));
}

TEST_F(FunctionalTests, GetNextDiscFragmentBackwardSearch)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    writeData.cachedFragment->position = 105.0;
    writeData.cachedFragment->absPosition = 1005.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, true);   // Discontinuous fragment

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, true);   // Discontinuous fragment

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 116.0;
    writeData.cachedFragment->absPosition = 1016.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    TsbFragmentDataPtr fragment1 = mDataManager->GetNextDiscFragment(1016.0, true);
    ASSERT_NE(fragment1, nullptr);
    EXPECT_EQ(fragment1->GetAbsolutePosition(), static_cast<AampTime>(1010.0));

    TsbFragmentDataPtr fragment2 = mDataManager->GetNextDiscFragment(1010.0, true);
    ASSERT_NE(fragment2, nullptr);
    EXPECT_EQ(fragment2->GetAbsolutePosition(), static_cast<AampTime>(1010.0));
}

TEST_F(FunctionalTests, GetNextDiscFragmentNoDiscontinuity)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    writeData.cachedFragment->position = 105.0;
    writeData.cachedFragment->absPosition = 1005.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 116.0;
    writeData.cachedFragment->absPosition = 1016.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    TsbFragmentDataPtr fragment = mDataManager->GetNextDiscFragment(1005.0, false);
    ASSERT_EQ(fragment, nullptr);
}

TEST_F(FunctionalTests, IsFragmentPresentTests)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    EXPECT_TRUE(mDataManager->IsFragmentPresent(1005.0));
    EXPECT_TRUE(mDataManager->IsFragmentPresent(1010.0));
    EXPECT_TRUE(mDataManager->IsFragmentPresent(1015.0));
    EXPECT_FALSE(mDataManager->IsFragmentPresent(0.0));
    EXPECT_FALSE(mDataManager->IsFragmentPresent(1020.0));
}

TEST_F(FunctionalTests, GetNearestFragment_EmptyData)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);
    auto fragment = mDataManager->GetNearestFragment(10.0);
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(FunctionalTests, TestFlush)
{
    mDataManager->AddInitFragment(url, eMEDIATYPE_VIDEO, streamInfo, period, absPosition);

    writeData.url = url1;
    writeData.periodId = period1;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url2;
    writeData.periodId = period2;
    writeData.cachedFragment->position = 110.0;
    writeData.cachedFragment->absPosition = 1010.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    writeData.url = url3;
    writeData.periodId = period3;
    writeData.cachedFragment->position = 115.0;
    writeData.cachedFragment->absPosition = 1015.0;
    mDataManager->AddFragment(writeData, eMEDIATYPE_VIDEO, false);

    mDataManager->Flush();

    EXPECT_EQ(mDataManager->GetFirstFragment(), nullptr);
    EXPECT_FALSE(mDataManager->IsFragmentPresent(1005.0)); // Confirm Init Fragments have been removed
}
