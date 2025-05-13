/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include <chrono>
#include <atomic>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampConfig.h"

class IsPeriodChangeMarkedTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
        mUnblocked = false;
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }

public:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};
    std::atomic<bool> mUnblocked;
};

TEST_F(IsPeriodChangeMarkedTests, GetAndSet)
{
    EXPECT_FALSE(mPrivateInstanceAAMP->GetIsPeriodChangeMarked());

    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);
    EXPECT_TRUE(mPrivateInstanceAAMP->GetIsPeriodChangeMarked());

    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(false);
    EXPECT_FALSE(mPrivateInstanceAAMP->GetIsPeriodChangeMarked());
}

TEST_F(IsPeriodChangeMarkedTests, WaitForDiscontinuityProcessToComplete)
{
    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);

    EXPECT_FALSE(mUnblocked);

    // Spawn thread to perform wait.
    std::thread t([this]{
        this->mPrivateInstanceAAMP->WaitForDiscontinuityProcessToComplete();
        this->mUnblocked = true;
    });

    // Sleep a bit to let the thread run.
    const std::chrono::duration<int, std::milli> delay(100);
    std::this_thread::sleep_for(delay);

    EXPECT_FALSE(mUnblocked);

    // Signal the thread.
    mPrivateInstanceAAMP->UnblockWaitForDiscontinuityProcessToComplete();

    t.join();

    EXPECT_TRUE(mUnblocked);
}

TEST_F(IsPeriodChangeMarkedTests, ClearToUnblock)
{
    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(true);

    EXPECT_FALSE(mUnblocked);

    // Spawn thread to perform wait.
    std::thread t([this]{
        this->mPrivateInstanceAAMP->WaitForDiscontinuityProcessToComplete();
        this->mUnblocked = true;
    });

    // Sleep a bit to let the thread run.
    const std::chrono::duration<int, std::milli> delay(100);
    std::this_thread::sleep_for(delay);

    EXPECT_FALSE(mUnblocked);

    // Clearing the flag will unblock the thread.
    mPrivateInstanceAAMP->SetIsPeriodChangeMarked(false);

    t.join();

    EXPECT_TRUE(mUnblocked);
}
