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

/**
 * @brief Test fixture for AampTrackWorkerManager tests.
 */
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

/**
 * @brief Test creating a worker successfully.
 */
TEST_F(AampTrackWorkerManagerTest, CreateWorkerSuccessfully)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	EXPECT_NE(worker, nullptr);
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_VIDEO), worker);
}

/**
 * @brief Test that creating a worker with the same media type returns the existing instance.
 */
TEST_F(AampTrackWorkerManagerTest, CreateWorkerReturnsSameInstance)
{
	auto worker1 = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_AUDIO);
	auto worker2 = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_AUDIO);
	EXPECT_EQ(worker1, worker2); // Should return the same instance
}

/**
 * @brief Test getting a worker that does not exist returns nullptr.
 */
TEST_F(AampTrackWorkerManagerTest, GetWorkerReturnsNullIfNotExists)
{
	EXPECT_EQ(mTrackWorkerManager->GetWorker(AampMediaType::eMEDIATYPE_AUDIO), nullptr);
}

/**
 * @brief Test RemoveWorkers API clears all workers.
 */
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

/**
 * @brief Test StopWorkers pauses and stops all workers.
 */
TEST_F(AampTrackWorkerManagerTest, StopWorkersPausesAndStopsAllWorkers)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	mTrackWorkerManager->StartWorkers();
	EXPECT_NE(worker, nullptr);

	mTrackWorkerManager->StopWorkers();
	EXPECT_TRUE(worker->IsStopped());
}

/**
 * @brief Test WaitForCompletionWithTimeout works as expected.
 */
TEST_F(AampTrackWorkerManagerTest, WaitForCompletionWorks)
{
	auto worker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	EXPECT_NE(worker, nullptr);

	// Submit a job that will take some time to complete
	auto job = std::make_shared<MediaSegmentDownloadJob>(nullptr, []() {
		std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Simulate a long-running job
	});

	bool timeoutOccurred = false;
	mTrackWorkerManager->StartWorkers();
	auto future = worker->SubmitJob(job, false); 
	EXPECT_TRUE(future.valid());
	// Wait for completion with a timeout of 50ms, which should trigger the timeout as job takes 200ms
	mTrackWorkerManager->WaitForCompletionWithTimeout(50, [&]() { timeoutOccurred = true; mTrackWorkerManager->StopWorkers(); });
	EXPECT_TRUE(timeoutOccurred);
	EXPECT_TRUE(worker->IsStopped());
}

/**
 * @brief Test WaitForCompletionWithTimeout skips non-critical workers.
 */
TEST_F(AampTrackWorkerManagerTest, WaitForCompletionSkipsNonCriticalWorkers)
{
	auto videoWorker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_VIDEO);
	auto textWorker = mTrackWorkerManager->CreateWorker(mPrivateInstanceAAMP.get(), AampMediaType::eMEDIATYPE_SUBTITLE);
	EXPECT_NE(videoWorker, nullptr);
	EXPECT_NE(textWorker, nullptr);
	// Submit a job that will take some time to complete
	auto videoJob = std::make_shared<MediaSegmentDownloadJob>(nullptr, []() {
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate
	});
	auto textJob = std::make_shared<MediaSegmentDownloadJob>(nullptr, []() {
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate
	});

	bool timeoutOccurred = false;
	mTrackWorkerManager->StartWorkers();
	auto videoFuture = videoWorker->SubmitJob(videoJob, false);
	auto textFuture = textWorker->SubmitJob(textJob, false);
	EXPECT_TRUE(videoFuture.valid());
	EXPECT_TRUE(textFuture.valid());

	// Wait for completion with a timeout of 50ms, which should trigger the timeout as jobs take 100ms
	mTrackWorkerManager->WaitForCompletionWithTimeout(50, [&]() { timeoutOccurred = true; mTrackWorkerManager->StopWorkers(); });
	EXPECT_TRUE(timeoutOccurred);
	EXPECT_TRUE(videoWorker->IsStopped());
	EXPECT_TRUE(textWorker->IsStopped()); // Text worker should be stopped but was skipped in wait
}
