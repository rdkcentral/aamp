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
		using AampTrackWorker::mMutex; // Expose protected member for testing

		TestableAampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType)
			: aamp::AampTrackWorker(_aamp, _mediaType)
		{
		}

		bool GetStopFlag()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return mStop;
		}

		bool HasPendingTasks()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return !mJobQueue.empty();
		}

		void SetStopFlag(bool stop)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mStop = stop;
			if (stop) {
				// Clear queue and reject all pending tasks
				std::queue<std::function<void()>> empty;
				mJobQueue.swap(empty);
			}
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
	};
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	AampMediaType mMediaType = AampMediaType::eMEDIATYPE_VIDEO;
	TestableAampTrackWorker *mTestableAampTrackWorker;

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		mTestableAampTrackWorker = new TestableAampTrackWorker(mPrivateInstanceAAMP, mMediaType);
	}

	void TearDown() override
	{
		delete mTestableAampTrackWorker;
		mTestableAampTrackWorker = nullptr;

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
	EXPECT_FALSE(mTestableAampTrackWorker->GetStopFlag());
	EXPECT_FALSE(mTestableAampTrackWorker->HasPendingTasks());
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
		delete mTestableAampTrackWorker;	// Explicit delete to check thread join
		mTestableAampTrackWorker = nullptr; // Avoid double free
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
TEST_F(FunctionalTests, SubmitTaskExecutesSuccessfully)
{
	bool taskExecuted = false;
	auto future = mTestableAampTrackWorker->Submit([&]()
												{ taskExecuted = true; });
	future.get(); // Wait for completion and check for exceptions
	EXPECT_TRUE(taskExecuted);
}

/**
 * @test FunctionalTests::MultipleJobsExecution
 * @brief Functional tests for AampTrackWorker with multiple jobs
 *
 * The tests verify the job submission and completion of the worker thread
 */
TEST_F(FunctionalTests, MultipleTasksExecution)
{
	int counter = 0;
	auto future1 = mTestableAampTrackWorker->Submit([&]()
												{ counter += 1; });
	auto future2 = mTestableAampTrackWorker->Submit([&]()
												{ counter += 2; });
	
	future1.get(); // Wait for first task
	future2.get(); // Wait for second task

	EXPECT_EQ(counter, 3);
}

/**
 * @test FunctionalTests::ProcessJobExitsGracefully
 * @brief Functional tests for AampTrackWorker with stop signal
 *
 * The tests verify the worker thread exits gracefully when stop signal is set
 */
TEST_F(FunctionalTests, TaskWorkerExitsGracefully)
{
	// Submit a task and wait for it to complete
	auto future1 = mTestableAampTrackWorker->Submit([]() {});
	future1.get();

	// Submit another task but stop before it executes
	bool taskExecuted = false;
	auto future2 = mTestableAampTrackWorker->Submit([&]() { taskExecuted = true; });

	// Stop the worker
	mTestableAampTrackWorker->SetStopFlag(true);
	mTestableAampTrackWorker->NotifyConditionVariable();

	try
	{
		future2.get();
		FAIL() << "Task should not execute after stop";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_STREQ(e.what(), "Worker is stopped");
		EXPECT_FALSE(taskExecuted);
	}

	// Clean up should succeed
	try
	{
		delete mTestableAampTrackWorker;
		mTestableAampTrackWorker = nullptr;
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Exception caught in destructor: " << e.what();
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
TEST_F(FunctionalTests, ProcessTaskHandlesNullTask)
{
	try
	{
		auto future = mTestableAampTrackWorker->Submit(std::function<void()>(nullptr));
		EXPECT_THROW(future.get(), std::exception); // Expect exception for null task
		SUCCEED();
	}
	catch (const std::exception &e)
	{
		FAIL() << "Unexpected exception: " << e.what();
	}
}
/**
 * @test FunctionalTests::SubmitJobHandlesExceptions
 * @brief Functional tests for AampTrackWorker with job throwing exception
 *
 * The tests verify the worker thread handles exceptions thrown by jobs gracefully
 */
TEST_F(FunctionalTests, SubmitTaskHandlesExceptions)
{
	std::runtime_error ex("test exception");
	auto future = mTestableAampTrackWorker->Submit([&]()
												{ throw ex; });
	try
	{
		future.get();
		FAIL() << "Expected exception not thrown";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_STREQ(e.what(), "test exception");
		SUCCEED();
	}
	catch (...)
	{
		FAIL() << "Wrong exception type caught";
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
 * @test FunctionalTests::RejectsTasksWhenStopped
 * @brief Functional tests for AampTrackWorker task rejection when stopped
 *
 * The tests verify that tasks are rejected with an exception when the worker is stopped
 */
TEST_F(FunctionalTests, RejectsTasksWhenStopped)
{
	// Stop the worker
	mTestableAampTrackWorker->SetStopFlag(true);

	// Try to submit a new task
	bool taskExecuted = false;
	auto future = mTestableAampTrackWorker->Submit([&]() { taskExecuted = true; });

	try
	{
		future.get();
		FAIL() << "Task should be rejected when worker is stopped";
	}
	catch (const std::runtime_error& e)
	{
		EXPECT_STREQ(e.what(), "Worker is stopped");
		EXPECT_FALSE(taskExecuted);
	}
}
