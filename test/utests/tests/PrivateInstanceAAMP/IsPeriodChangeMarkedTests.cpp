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

#include "main_aamp.h"

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

        mPlayerInstanceAAMP = new PlayerInstanceAAMP(gpGlobalConfig);
        mUnblocked = false;
    }

    void TearDown() override
    {
        delete mPlayerInstanceAAMP;
        mPlayerInstanceAAMP = nullptr;

//        delete gpGlobalConfig;
//        gpGlobalConfig = nullptr;
    }

public:
    PlayerInstanceAAMP *mPlayerInstanceAAMP{};
    std::atomic<bool> mUnblocked;
};

TEST_F(IsPeriodChangeMarkedTests, GetAndSet)
{
    EXPECT_FALSE(mPlayerInstanceAAMP->GetIsPeriodChangeMarked());

    mPlayerInstanceAAMP->SetIsPeriodChangeMarked(true);
    EXPECT_TRUE(mPlayerInstanceAAMP->GetIsPeriodChangeMarked());

    mPlayerInstanceAAMP->SetIsPeriodChangeMarked(false);
    EXPECT_FALSE(mPlayerInstanceAAMP->GetIsPeriodChangeMarked());
}

TEST_F(IsPeriodChangeMarkedTests, WaitForDiscontinuityProcessToComplete)
{
    mPlayerInstanceAAMP->SetIsPeriodChangeMarked(true);

    EXPECT_FALSE(mUnblocked);

    // Spawn thread to perform wait.
    std::thread t([this]{
        this->mPlayerInstanceAAMP->WaitForDiscontinuityProcessToComplete();
        this->mUnblocked = true;
    });

    // Sleep a bit to let the thread run.
    const std::chrono::duration<int, std::milli> delay(100);
    std::this_thread::sleep_for(delay);

    EXPECT_FALSE(mUnblocked);

    // Signal the thread.
    mPlayerInstanceAAMP->UnblockWaitForDiscontinuityProcessToComplete();

    t.join();

    EXPECT_TRUE(mUnblocked);
}

TEST_F(IsPeriodChangeMarkedTests, ClearToUnblock)
{
    mPlayerInstanceAAMP->SetIsPeriodChangeMarked(true);

    EXPECT_FALSE(mUnblocked);

    // Spawn thread to perform wait.
    std::thread t([this]{
        this->mPlayerInstanceAAMP->WaitForDiscontinuityProcessToComplete();
        this->mUnblocked = true;
    });

    // Sleep a bit to let the thread run.
    const std::chrono::duration<int, std::milli> delay(100);
    std::this_thread::sleep_for(delay);

    EXPECT_FALSE(mUnblocked);

    // Clearing the flag will unblock the thread.
    mPlayerInstanceAAMP->SetIsPeriodChangeMarked(false);

    t.join();

    EXPECT_TRUE(mUnblocked);
}
