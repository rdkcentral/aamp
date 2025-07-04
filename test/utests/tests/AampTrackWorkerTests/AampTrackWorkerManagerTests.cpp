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
#include "AampTrackWorkerManager.hpp"
#include "MediaSegmentDownloadJob.hpp"
#include "priv_aamp.h"

using namespace aamp;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class AampTrackWorkerManagerTest : public ::testing::Test
{
protected:
	std::shared_ptr<PrivateInstanceAAMP> mPrivateInstanceAAMP;
	std::shared_ptr<AampTrackWorkerManager> mTrackWorkerManager;

	void SetUp() override
	{
		mPrivateInstanceAAMP = std::make_shared<PrivateInstanceAAMP>(nullptr);
		mTrackWorkerManager = std::make_shared<AampTrackWorkerManager>();
	}

	void TearDown() override
	{
		mTrackWorkerManager->StopWorkers();
		mTrackWorkerManager->RemoveWorkers();
	}
};

TEST_F(AampTrackWorkerManagerTest, CreateWorkerSuccessfully)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	EXPECT_NE(worker, nullptr);
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_VIDEO), worker);
}

TEST_F(AampTrackWorkerManagerTest, CreateWorkerReturnsSameInstance)
{
	auto worker1 = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_AUDIO);
	auto worker2 = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_AUDIO);
	EXPECT_EQ(worker1, worker2); // Should return the same instance
}

TEST_F(AampTrackWorkerManagerTest, GetWorkerReturnsNullIfNotExists)
{
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_AUX_AUDIO), nullptr);
}

TEST_F(AampTrackWorkerManagerTest, RemoveWorkersClearsAllWorkers)
{
	mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_AUDIO);
	mTrackWorkerManager->StartWorkers();

	mTrackWorkerManager->StopWorkers();
	mTrackWorkerManager->RemoveWorkers();
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_VIDEO), nullptr);
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_AUDIO), nullptr);
}

TEST_F(AampTrackWorkerManagerTest, StopWorkersPausesAndStopsAllWorkers)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	mTrackWorkerManager->StartWorkers();
	EXPECT_NE(worker, nullptr);

	mTrackWorkerManager->StopWorkers();
	EXPECT_TRUE(worker->IsStopped());
}

TEST_F(AampTrackWorkerManagerTest, WaitForCompletionWorks)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	EXPECT_NE(worker, nullptr);

	// Submit a job that will take some time to complete
	auto job = std::make_shared<MediaSegmentDownloadJob>(nullptr, []() {
		std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Simulate a long-running job
	});

	// Submit the job to the worker
	auto future = worker->SubmitJob(job, false);
	EXPECT_TRUE(future.valid());

	bool timeoutOccurred = false;
	// Wait for completion with a timeout
	mTrackWorkerManager->StartWorkers();
	mTrackWorkerManager->WaitForCompletionWithTimeout(50, [&]() { timeoutOccurred = true; mTrackWorkerManager->StopWorkers(); });
	EXPECT_TRUE(timeoutOccurred);
	EXPECT_TRUE(worker->IsStopped());
}
