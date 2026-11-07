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

#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>
#include <cjson/cJSON.h>

#include "priv_aamp.h"

#include "AampDRMLicPreFetcher.h"
#include "DrmSession.h"
#include "AampCurlDownloader.h"
#include "DrmSessionManager.h"
#define private public
#include "AampDRMLicManager.h"
#undef private

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "aampMocks.h"
#include "curlMocks.h"
#include "DrmTestUtils.h"

using ::testing::An;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::_;

class AampLegacyDrmSessionTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mAamp = nullptr;
	AampLogManager mLogging;
	TestUtilDrm *mUtils = nullptr;

	void SetUp() override
	{
		MockAampReset();
		MockCurlReset();

		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
		mUtils = new TestUtilDrm(mAamp);

	}

	void TearDown() override
	{
		delete mUtils;
		mUtils = nullptr;

		delete mAamp;
		mAamp = nullptr;

		MockAampReset();
		MockCurlReset();
	}

public:
	bool EnqueueWatermarkTaskForTest(std::function<void()> task)
	{
		return mUtils->getSessionManager()->EnqueueWatermarkTask(std::move(task));
	}

	void StopWatermarkWorkerForTest()
	{
		mUtils->getSessionManager()->StopWatermarkWorker();
	}

	bool WaitForWatermarkWorkerToStop()
	{
		auto *manager = mUtils->getSessionManager();
		std::unique_lock<std::mutex> lock(manager->mWatermarkTaskMutex);
		return manager->mWatermarkTaskCondition.wait_for(lock, std::chrono::seconds(5), [manager]() {
			return manager->mWatermarkWorkerStopping;
		});
	}
};

TEST_F(AampLegacyDrmSessionTests, WatermarkTasksRunOnWorkerInFifoOrder)
{
	struct TaskResults
	{
		std::mutex mutex;
		std::vector<int> order;
		std::vector<std::thread::id> threads;
		std::promise<void> complete;
	};
	auto results = std::make_shared<TaskResults>();
	const auto callerThread = std::this_thread::get_id();
	auto tasksCompleteFuture = results->complete.get_future();

	for (int taskId = 1; taskId <= 3; ++taskId)
	{
		ASSERT_TRUE(EnqueueWatermarkTaskForTest([results, taskId]() {
			{
				std::lock_guard<std::mutex> lock(results->mutex);
				results->order.push_back(taskId);
				results->threads.push_back(std::this_thread::get_id());
			}
			if (taskId == 3)
			{
				results->complete.set_value();
			}
		}));
	}

	ASSERT_EQ(tasksCompleteFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	EXPECT_EQ(results->order, (std::vector<int>{1, 2, 3}));
	ASSERT_EQ(results->threads.size(), 3u);
	for (const auto& taskThread : results->threads)
	{
		EXPECT_NE(taskThread, callerThread);
	}
}

TEST_F(AampLegacyDrmSessionTests, WatermarkWorkerDrainsQueuedTasksWhenStopped)
{
	auto *manager = mUtils->getSessionManager();
	std::promise<void> taskStarted;
	std::promise<void> releaseTask;
	auto releaseTaskFuture = releaseTask.get_future().share();
	std::promise<void> queuedTaskComplete;
	auto queuedTaskCompleteFuture = queuedTaskComplete.get_future();

	if (!EnqueueWatermarkTaskForTest([&]() {
		taskStarted.set_value();
		releaseTaskFuture.wait();
	}))
	{
		FAIL() << "Could not enqueue initial watermark task";
		return;
	}
	if (taskStarted.get_future().wait_for(std::chrono::seconds(5)) != std::future_status::ready)
	{
		releaseTask.set_value();
		StopWatermarkWorkerForTest();
		FAIL() << "Watermark worker did not start the initial task";
		return;
	}
	if (!EnqueueWatermarkTaskForTest([&]() { queuedTaskComplete.set_value(); }))
	{
		releaseTask.set_value();
		StopWatermarkWorkerForTest();
		FAIL() << "Could not enqueue pending watermark task";
		return;
	}

	std::thread stopper([this]() { StopWatermarkWorkerForTest(); });
	const bool stopRequested = WaitForWatermarkWorkerToStop();
	releaseTask.set_value();
	stopper.join();

	EXPECT_TRUE(stopRequested);
	EXPECT_EQ(queuedTaskCompleteFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	EXPECT_FALSE(EnqueueWatermarkTaskForTest([]() {}));

	manager->setPlaybackSpeedState(false, 0.0, false, 0.0, 7, 42.0, true);
	EXPECT_EQ(manager->mDrmSessionManager->mCurrentSpeed.load(), 7);
	EXPECT_TRUE(manager->mDrmSessionManager->mFirstFrameSeen.load());
}

TEST_F(AampLegacyDrmSessionTests, PublicWatermarkUpdatesAreProcessedInQueueOrder)
{
	auto *manager = mUtils->getSessionManager();
	auto queueFlushed = std::make_shared<std::promise<void>>();
	auto queueFlushedFuture = queueFlushed->get_future();

	manager->setPlaybackSpeedState(false, 0.0, false, 0.0, 4, 12.5, true);
	manager->setVideoMute(false, 0.0, false, 0.0, true, 12.5);
	manager->setVideoWindowSize(1920, 1080);
	manager->hideWatermarkOnDetach();
	ASSERT_TRUE(EnqueueWatermarkTaskForTest([queueFlushed]() { queueFlushed->set_value(); }));

	ASSERT_EQ(queueFlushedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	EXPECT_EQ(manager->mDrmSessionManager->mCurrentSpeed.load(), 4);
	EXPECT_TRUE(manager->mDrmSessionManager->mIsVideoOnMute.load());
	EXPECT_FALSE(manager->mDrmSessionManager->mFirstFrameSeen.load());
}

TEST_F(AampLegacyDrmSessionTests, TestCreateClearkeySession)
{
	AampDRMLicenseManager *aampDRMLicManager = mUtils->getSessionManager();

	cJSON *keysObj = cJSON_CreateObject();
	cJSON *keyInstanceObj = cJSON_CreateObject();
	cJSON_AddStringToObject(keyInstanceObj, "alg", "cbc");
	cJSON_AddStringToObject(keyInstanceObj, "k", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON_AddStringToObject(keyInstanceObj, "kid", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON *keysArr = cJSON_AddArrayToObject(keysObj, "keys");
	cJSON_AddItemToArray(keysArr, keyInstanceObj);

	char *keyResponse = cJSON_PrintUnformatted(keysObj);
	mUtils->setupCurlPerformResponse(keyResponse);
	cJSON_free(keyResponse);
	cJSON_Delete(keysObj);

	const unsigned char initData[] = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10,
		0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2,
		0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01, 0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad,
		0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d, 0x00, 0x00, 0x00, 0x00};

	DrmMetaDataEventPtr aampEvent =
		std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, "");

	// Setting a ClearKey license server URL in the global config.
	// This should get used to request the license.
	std::string ckLicenseServerURL = "http://licenseserver.example/license";
	gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_CKLicenseServerUrl,
								   ckLicenseServerURL);

	const char*systemId = "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b";
	void *ptr= static_cast<void*>(&aampEvent);

	DrmSession *drmSession = aampDRMLicManager->createDrmSession(reinterpret_cast<const char *>(systemId), 
															  eMEDIAFORMAT_DASH, reinterpret_cast<const unsigned char *>(initData), static_cast<uint16_t>(sizeof(initData)),(int)eMEDIATYPE_VIDEO, (DrmCallbacks*)mAamp, aampEvent, (const unsigned char*)nullptr,true);
}
