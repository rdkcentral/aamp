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

		bool GetJobAvailableFlag()
		{
			return mJobAvailable;
		}

		bool GetStopFlag()
		{
			return mStop;
		}

		void SetStopFlag(bool stop)
		{
			mStop = stop;
		}

		void SetJobAvailableFlag(bool jobAvailable)
		{
			mJobAvailable = jobAvailable;
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
	EXPECT_FALSE(mTestableAampTrackWorker->GetJobAvailableFlag());
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
TEST_F(FunctionalTests, SubmitJobExecutesSuccessfully)
{
	bool jobExecuted = false;
	mTestableAampTrackWorker->SubmitJob([&]()
										{ jobExecuted = true; });
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
	mTestableAampTrackWorker->SubmitJob([&]()
										{ counter += 1; });
	mTestableAampTrackWorker->WaitForCompletion();

	mTestableAampTrackWorker->SubmitJob([&]()
										{ counter += 2; });
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
	mTestableAampTrackWorker->SubmitJob([&]() {}); // Dummy job
	mTestableAampTrackWorker->WaitForCompletion();

	// Simulate stop signal
	{
		std::lock_guard<std::mutex> lock(mTestableAampTrackWorker->mMutex);
		mTestableAampTrackWorker->SetStopFlag(true);
		mTestableAampTrackWorker->SetJobAvailableFlag(true);
	}
	mTestableAampTrackWorker->NotifyConditionVariable();

	// Wait for thread to join in destructor
	try
	{
		delete mTestableAampTrackWorker;
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
		mTestableAampTrackWorker->SubmitJob([&]()
											{ throw ex; });
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
