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
#include <thread>
#include <chrono>
#include "AampTrackWorker.h"
#include "MediaSegmentDownloadJob.hpp"
#include "priv_aamp.h"
#include "AampUtils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief Functional tests for AampTrackWorker class
 */
class FunctionalTests : public ::testing::Test
{
protected:
	class TestableAampTrackWorker : public aamp::AampTrackWorker
	{
	public:
		using AampTrackWorker::mQueueMutex; // Expose protected member for testing

		TestableAampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType)
			: aamp::AampTrackWorker(_aamp, _mediaType)
		{
		}

		void SetStopFlag(bool stop)
		{
			mStop.store(stop);
		}

		PrivateInstanceAAMP *GetAampInstance()
		{
			return aamp;
		}

		AampMediaType GetMediaType()
		{
			return mMediaType;
		}

		void NotifyConditionVariable()
		{
			mCondVar.notify_one();
		}

		size_t GetJobQueueSize()
		{
			return mJobQueue.size();
		}

		bool IsPaused()
		{
			return mPaused.load();
		}
	};
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	AampMediaType mMediaType = AampMediaType::eMEDIATYPE_VIDEO;
	std::shared_ptr<TestableAampTrackWorker> mTestableAampTrackWorker;

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		mTestableAampTrackWorker = std::make_shared<TestableAampTrackWorker>(mPrivateInstanceAAMP, mMediaType);
		mTestableAampTrackWorker->StartWorker();
	}

	void TearDown() override
	{
		if(mTestableAampTrackWorker)
		{
			// Stop worker thread
			mTestableAampTrackWorker->StopWorker();
			mTestableAampTrackWorker = nullptr;
		}

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		if (gpGlobalConfig)
		{
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
		}
	}
};

/**
 * @test FunctionalTests::ConstructorInitializesFields
 * @brief Functional tests for AampTrackWorker constructor
 *
 * The tests verify the initialization of the worker flags in constructor
 */
TEST_F(FunctionalTests, ConstructorInitializesFields)
{
	EXPECT_FALSE(mTestableAampTrackWorker->IsStopped());
	EXPECT_EQ(mTestableAampTrackWorker->GetJobQueueSize(), 0);
	EXPECT_EQ(mTestableAampTrackWorker->GetAampInstance(), mPrivateInstanceAAMP);
	EXPECT_EQ(mTestableAampTrackWorker->GetMediaType(), mMediaType);
}

/**
 * @test FunctionalTests::DestructorCleansUpResources
 * @brief Functional tests for AampTrackWorker destructor
 *
 * The tests verify the destructor cleans up resources and exits gracefully
 */
TEST_F(FunctionalTests, DestructorCleansUpResources)
{
	try
	{
		mTestableAampTrackWorker->StopWorker(); // Ensure worker thread is stopped
		mTestableAampTrackWorker.reset(); // This will call the destructor
		mTestableAampTrackWorker = nullptr;
		// No exceptions or undefined behavior should occur
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker destructor: " << e.what();
	}
}

/**
 * @test FunctionalTests::SubmitJobExecutesSuccessfully
 * @brief Functional tests for AampTrackWorker with single job submission
 *
 * The tests verify the job submission and completion of the worker thread
 */
TEST_F(FunctionalTests, SubmitJobExecutesSuccessfully)
{
	bool jobExecuted = false;
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			jobExecuted = true;
		});
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	mTestableAampTrackWorker->WaitForCompletion();
	EXPECT_TRUE(jobExecuted);
}

/**
 * @test FunctionalTests::MultipleJobsExecution
 * @brief Functional tests for AampTrackWorker with multiple jobs
 *
 * The tests verify the job submission and completion of the worker thread
 */
TEST_F(FunctionalTests, MultipleJobsExecution)
{
	int counter = 0;
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			counter += 1;
		});
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	mTestableAampTrackWorker->WaitForCompletion();
	EXPECT_EQ(counter, 3);
}

/**
 * @test FunctionalTests::ProcessJobExitsGracefully
 * @brief Functional tests for AampTrackWorker with stop signal
 *
 * The tests verify the worker thread exits gracefully when stop signal is set
 */
TEST_F(FunctionalTests, ProcessJobExitsGracefully)
{
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){});
	mTestableAampTrackWorker->SubmitJob(downloadJob); // Dummy job
	mTestableAampTrackWorker->WaitForCompletion();
	mTestableAampTrackWorker->StopWorker();

	// Wait for thread to join in destructor
	try
	{
		mTestableAampTrackWorker.reset();
		mTestableAampTrackWorker = nullptr;
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker destructor: " << e.what();
	}
}

/**
 * @test FunctionalTests::ConstructorHandlesExceptionsGracefully
 * @brief Functional tests for AampTrackWorker constructor exception handling
 *
 * The tests check if the constructor handles exceptions gracefully
 */
TEST_F(FunctionalTests, ConstructorHandlesExceptionsGracefully)
{
	try
	{
		PrivateInstanceAAMP mAAMP;
		aamp::AampTrackWorker audioWorker(&mAAMP, AampMediaType::eMEDIATYPE_AUDIO);
		aamp::AampTrackWorker auxAudioWorker(&mAAMP, AampMediaType::eMEDIATYPE_AUX_AUDIO);
		aamp::AampTrackWorker subtitleWorker(&mAAMP, AampMediaType::eMEDIATYPE_SUBTITLE);
		SUCCEED();
	}
	catch (...)
	{
		FAIL() << "Constructor threw an unexpected exception";
	}
}

/**
 * @test FunctionalTests::ProcessJobHandlesNullJobs
 * @brief Functional tests for AampTrackWorker with null job submission
 *
 * The tests verify the worker thread does not crash or behave unexpectedly with null job
 */
TEST_F(FunctionalTests, ProcessJobHandlesNullJobs)
{
	try
	{
		mTestableAampTrackWorker->SubmitJob(nullptr); // Submit an invalid/null job
		mTestableAampTrackWorker->WaitForCompletion();
		SUCCEED(); // No crashes or unexpected behavior
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker ProcessJob: " << e.what();
	}
}
/**
 * @test FunctionalTests::SubmitJobHandlesExceptions
 * @brief Functional tests for AampTrackWorker with job throwing exception
 *
 * The tests verify the worker thread handles exceptions thrown by jobs gracefully
 */
TEST_F(FunctionalTests, SubmitJobHandlesExceptions)
{
	std::exception ex;
	try
	{
		auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){throw ex;});
		mTestableAampTrackWorker->SubmitJob(downloadJob);
		mTestableAampTrackWorker->WaitForCompletion();
		SUCCEED(); // No crashes or unexpected behavior
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker job: " << e.what();
	}
}

/**
 * @test FunctionalTests::ConstructorHandlesNullAamp
 * @brief Functional tests for AampTrackWorker constructor with null aamp
 *
 * The tests verify the constructor handles null aamp parameter gracefully
 */
TEST_F(FunctionalTests, ConstructorHandlesNullAamp)
{
	try
	{
		aamp::AampTrackWorker nullAampWorker(nullptr, AampMediaType::eMEDIATYPE_VIDEO);
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker constructor with null aamp: " << e.what();
	}
}

/**
 * @test FunctionalTests::PauseAndResumeWorker
 * @brief Functional tests for AampTrackWorker Pause and Resume
 *
 * The tests verify the worker thread can be paused and resumed successfully
 */
TEST_F(FunctionalTests, PauseAndResumeWorker)
{
	mTestableAampTrackWorker->Pause();
	EXPECT_TRUE(mTestableAampTrackWorker->IsPaused());

	mTestableAampTrackWorker->Resume();
	EXPECT_FALSE(mTestableAampTrackWorker->IsPaused());
}

/**
 * @test FunctionalTests::ClearJobsTest
 * @brief Functional tests for AampTrackWorker ClearJobs
 *
 * The tests verify the job queue is cleared successfully
 */
TEST_F(FunctionalTests, ClearJobsTest)
{
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [](){});
	mTestableAampTrackWorker->Pause();
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	EXPECT_EQ(mTestableAampTrackWorker->GetJobQueueSize(), 1);

	mTestableAampTrackWorker->ClearJobs();
	EXPECT_EQ(mTestableAampTrackWorker->GetJobQueueSize(), 0);
}

/**
 * @test FunctionalTests::RescheduleActiveJobTest
 * @brief Functional tests for AampTrackWorker RescheduleActiveJob
 *
 * The tests verify the active job is rescheduled successfully
 */
TEST_F(FunctionalTests, RescheduleActiveJobTest)
{
	int counter = 0;
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			if(counter < 1)
			{
				mTestableAampTrackWorker->RescheduleActiveJob();
			}
			counter += 1;
		});
	mTestableAampTrackWorker->Pause();
	mTestableAampTrackWorker->SubmitJob(downloadJob);
	mTestableAampTrackWorker->Resume();
	mTestableAampTrackWorker->WaitForCompletion();
	EXPECT_EQ(counter, 2);
}

/**
 * @test FunctionalTests::SubmitJobPushToFront
 * @brief Functional tests for AampTrackWorker SubmitJob with push to front
 *
 * The tests verify that the job is pushed to the front of the queue when the second argument is true
 */
TEST_F(FunctionalTests, SubmitJobPushToFront)
{
	std::string result;
	auto firstJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			result += "string";
		});
	auto secondJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			result += "test";
		});
	mTestableAampTrackWorker->Pause();
	mTestableAampTrackWorker->SubmitJob(firstJob);
	mTestableAampTrackWorker->SubmitJob(secondJob, true); // Push to front
	mTestableAampTrackWorker->Resume();
	mTestableAampTrackWorker->WaitForCompletion();

	EXPECT_EQ(result, "teststring"); // secondJob should execute before firstJob
}

/**
 * @test FunctionalTests::SubmitJobPushToFrontAndReschedule
 * @brief Functional tests for AampTrackWorker SubmitJob with push to front and reschedule
 *
 * The tests verify that the job is pushed to the front of the queue and rescheduled successfully
 */
TEST_F(FunctionalTests, SubmitJobPushToFrontAndReschedule)
{
	int counter = 0;
	std::string result;
	auto downloadJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			if(counter < 1)
			{
				mTestableAampTrackWorker->RescheduleActiveJob();
			}
			result += "test";
			counter += 1;
		});
	auto anotherJob = std::make_shared<aamp::MediaSegmentDownloadJob>(nullptr, [&](){
			result += "string";
		});
	mTestableAampTrackWorker->Pause();
	mTestableAampTrackWorker->SubmitJob(anotherJob);
	mTestableAampTrackWorker->SubmitJob(downloadJob, true); // Push to front
	mTestableAampTrackWorker->Resume();
	mTestableAampTrackWorker->WaitForCompletion();

	EXPECT_EQ(counter, 2);
	EXPECT_EQ(result, "testteststring"); // downloadJob should execute twice before anotherJob
}

/**
 * @test FunctionalTests::StartWorkerTest
 * @brief Functional tests for AampTrackWorker StartWorker
 *
 * The tests verify that the worker thread starts successfully
 */
TEST_F(FunctionalTests, StartWorkerTest)
{
	try
	{
		mTestableAampTrackWorker->StopWorker(); // Ensure worker is stopped
		mTestableAampTrackWorker->StartWorker(); // Start worker again
		EXPECT_FALSE(mTestableAampTrackWorker->IsStopped());
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker StartWorker: " << e.what();
	}
}

/**
 * @test FunctionalTests::StopWorkerTest
 * @brief Functional tests for AampTrackWorker StopWorker
 *
 * The tests verify that the worker thread stops successfully
 */
TEST_F(FunctionalTests, StopWorkerTest)
{
	try
	{
		mTestableAampTrackWorker->StopWorker(); // Stop worker
		EXPECT_TRUE(mTestableAampTrackWorker->IsStopped());
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in AampTrackWorker StopWorker: " << e.what();
	}
}
