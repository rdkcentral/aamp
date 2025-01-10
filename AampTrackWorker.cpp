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

#include "AampTrackWorker.h"
#include <iostream>

namespace aamp
{

	/**
	 * @brief Constructs an AampTrackWorker object.
	 *
	 * Initializes the worker thread and sets the initial state of the worker.
	 *
	 * @param[in] _aamp The PrivateInstanceAAMP instance.
	 * @param[in] _mediaType The media type of the track.
	 *
	 */
	AampTrackWorker::AampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType)
		: aamp(_aamp), mMediaType(_mediaType), mJobAvailable(false), mStop(false), mWorkerThread(), mJob(), mMutex(), mCondVar(), mCompletionVar()
	{
		if (_aamp == nullptr)
		{
			AAMPLOG_ERR("AampTrackWorker constructor received null aamp");
			mStop = true;
			return;
		}

		try
		{
			mWorkerThread = std::thread(&AampTrackWorker::ProcessJob, this);
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception caught in AampTrackWorker constructor: %s", e.what());
			mStop = true;
		}
		catch (...)
		{
			AAMPLOG_ERR("Unknown exception caught in AampTrackWorker constructor");
			mStop = true;
		}
	}

	/**
	 * @brief Destructs the AampTrackWorker object.
	 *
	 * Signals the worker thread to stop, waits for it to finish, and cleans up resources.
	 *
	 * @return void
	 */
	AampTrackWorker::~AampTrackWorker()
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mStop = true;
			mJobAvailable = true; // Wake up thread to exit
		}
		mCondVar.notify_one();
		if (mWorkerThread.joinable())
		{
			mWorkerThread.join();
		}
		mCompletionVar.notify_one();
	}

	/**
	 * @brief Submits a job to the worker thread.
	 *
	 * The job is a function that will be executed by the worker thread.
	 *
	 * @param[in] job The job to be executed by the worker thread.
	 *
	 * @return void
	 */
	void AampTrackWorker::SubmitJob(std::function<void()> job)
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			this->mJob = job;
			mJobAvailable = true;
		}
		AAMPLOG_DEBUG("Job submitted for media type %s", GetMediaTypeName(mMediaType));
		mCondVar.notify_one();
	}

	/**
	 * @brief Waits for the current job to complete.
	 *
	 * Blocks the calling thread until the current job has been processed by the worker thread.
	 *
	 * @return void
	 */
	void AampTrackWorker::WaitForCompletion()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mCompletionVar.wait(lock, [this]() { return !mJobAvailable; });
		AAMPLOG_DEBUG("Job wait completed for media type %s", GetMediaTypeName(mMediaType));
	}

	/**
	 * @brief The main function executed by the worker thread.
	 *
	 * Waits for jobs to be submitted, processes them, and signals their completion.
	 * The function runs in a loop until the worker is signaled to stop.
	 *
	 * @return void
	 */
	void AampTrackWorker::ProcessJob()
	{
		UsingPlayerId playerId(aamp->mPlayerId);
		AAMPLOG_INFO("Process Job for media type %s", GetMediaTypeName(mMediaType));

		// Main loop
		while (true)
		{
			std::function<void()> currentJob;
			{
				std::unique_lock<std::mutex> lock(mMutex);
				mCondVar.wait(lock, [this]() { return mJobAvailable || mStop; });
				if (mStop)
				{
					break;
				}
				currentJob = mJob;

				// Execute the job
				if (!mStop && currentJob)
				{
					AAMPLOG_DEBUG("Executing Job for media type %s Job: %p", GetMediaTypeName(mMediaType), &currentJob);
					lock.unlock();
					try
					{
						currentJob();
					}
					catch (const std::exception &e)
					{
						AAMPLOG_ERR("Exception caught while executing job for media type %s: %s", GetMediaTypeName(mMediaType), e.what());
					}
					catch (...)
					{
						AAMPLOG_ERR("Unknown exception caught while executing job for media type %s", GetMediaTypeName(mMediaType));
					}
					lock.lock();
				}

				AAMPLOG_DEBUG("Job completed for media type %s", GetMediaTypeName(mMediaType));
				mJobAvailable = false;
				mCompletionVar.notify_one();
			}
		}

		AAMPLOG_INFO("Exiting Process Job for media type %s", GetMediaTypeName(mMediaType));
	}
} // namespace aamp
