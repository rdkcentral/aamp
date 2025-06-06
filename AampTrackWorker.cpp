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
		: aamp(_aamp), mMediaType(_mediaType), mStop(false), mPaused(false), mActiveJob(nullptr), mWorkerThread(), mJobQueue(), mQueueMutex(), mCondVar(), mCompletionVar()
	{
		if (_aamp == nullptr)
		{
			AAMPLOG_ERR("AampTrackWorker constructor received null aamp");
			mStop.store(true);
			return;
		}
		AAMPLOG_DEBUG("AampTrackWorker constructor for media type %s", GetMediaTypeName(mMediaType));
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
		StopWorker();
		ClearJobs();
		AAMPLOG_DEBUG("AampTrackWorker destructor for media type %s", GetMediaTypeName(mMediaType));
	}

	/**
	 * @brief Starts the worker thread.
	 *
	 * Creates the worker thread and starts it.
	 *
	 * @return void
	 */
	void AampTrackWorker::StartWorker()
	{
		try
		{
			mWorkerThread = std::thread(&AampTrackWorker::ProcessJob, shared_from_this(), std::weak_ptr<AampTrackWorker>(shared_from_this()));
			mStop.store(false);
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception caught in AampTrackWorker constructor: %s", e.what());
			mStop.store(true);
		}
		catch (...)
		{
			AAMPLOG_ERR("Unknown exception caught in AampTrackWorker constructor");
			mStop.store(true);
		}
	}

	/**
	 * @brief Stops the worker thread.
	 *
	 * Signals the worker thread to stop and waits for it to finish.
	 *
	 * @return void
	 */
	void AampTrackWorker::StopWorker()
	{
		{
			std::lock_guard<std::mutex> lock(mQueueMutex);
			mStop.store(true);
			AAMPLOG_DEBUG("Stopping worker thread for media type %s", GetMediaTypeName(mMediaType));
		}
		mCondVar.notify_one();
		mCompletionVar.notify_all();
		if (mWorkerThread.joinable())
		{
			mWorkerThread.join();
		}
	}

	/**
	 * @brief Submits a job to the worker thread.
	 *
	 * The job is a function that will be executed by the worker thread.
	 *
	 * @param[in] job The job to be executed by the worker thread.
	 * @param[in] highPriority Flag to indicate if the job should be executed
	 *
	 * @return void
	 */
	void AampTrackWorker::SubmitJob(AampTrackWorkerJobPtr job, bool highPriority)
	{
		{
			std::lock_guard<std::mutex> lock(mQueueMutex);
			if (!mStop.load())
			{
				if (highPriority)
				{
					mJobQueue.push_front(job);
				}
				else
				{
					mJobQueue.push_back(job);
				}
			}
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
		std::unique_lock<std::mutex> lock(mQueueMutex);
		mCompletionVar.wait(lock, [this]() { return (mJobQueue.empty() && mActiveJob == nullptr) || mStop.load(); });
		AAMPLOG_DEBUG("Job wait completed for media type %s", GetMediaTypeName(mMediaType));
	}

	/**
	 * @brief Waits for the current job to complete or until the specified timeout.
	 *
	 * Blocks the calling thread until the current job has been processed by the worker thread
	 * or the timeout duration has been reached.
	 *
	 * @param[in] timeoutMs Timeout value in milliseconds
	 *
	 * @return true if the job completed within the timeout duration, false otherwise.
	 */
	bool AampTrackWorker::WaitForCompletionWithTimeout(int timeout)
	{
		std::unique_lock<std::mutex> lock(mQueueMutex);
		AAMPLOG_DEBUG("Entering Job wait with timeout for media type %s", GetMediaTypeName(mMediaType));
		bool completed = mCompletionVar.wait_for(lock, std::chrono::milliseconds(timeout), [this]() { return (mJobQueue.empty() && mActiveJob == nullptr) || mStop.load(); });
		AAMPLOG_DEBUG("Job wait with timeout %s for media type %s", completed ? "completed" : "timed out", GetMediaTypeName(mMediaType));
		return completed;
	}

	/**
	 * @brief Pauses the worker thread.
	 *
	 * Signals the worker thread to pause and waits for it to acknowledge the pause signal.
	 *
	 * @return void
	 */
	void AampTrackWorker::Pause()
	{
		{
			std::lock_guard<std::mutex> lock(mQueueMutex);
			mPaused.store(true);
		}
		AAMPLOG_DEBUG("Pausing worker thread for media type %s", GetMediaTypeName(mMediaType));
		mCondVar.notify_one(); // Wake up thread to pause
	}

	/**
	 * @brief Resumes the worker thread.
	 *
	 * Signals the worker thread to resume and waits for it to acknowledge the resume signal.
	 *
	 * @return void
	 */
	void AampTrackWorker::Resume()
	{
		{
			std::lock_guard<std::mutex> lock(mQueueMutex);
			mPaused.store(false);
		}
		AAMPLOG_DEBUG("Resuming worker thread for media type %s", GetMediaTypeName(mMediaType));
		mCondVar.notify_one();
	}

	/**
	 * @brief Clears all jobs from the worker thread.
	 *
	 * Removes all jobs from the worker thread's queue.
	 *
	 * @return void
	 */
	void AampTrackWorker::ClearJobs()
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		mJobQueue.clear();
		mCompletionVar.notify_all();
	}

	/**
	 * @brief The main function executed by the worker thread.
	 *
	 * @param[in] weakSelf Weak pointer to the AampTrackWorker instance.
	 * Waits for jobs to be submitted, processes them, and signals their completion.
	 * The function runs in a loop until the worker is signaled to stop.
	 *
	 * @return void
	 */
	void AampTrackWorker::ProcessJob(AampTrackWorkerWeakPtr weakSelf)
	{
		if (auto self = weakSelf.lock())
		{
			AAMPLOG_INFO("Processing job for media type %s", GetMediaTypeName(self->mMediaType));
			UsingPlayerId playerId(self->aamp->mPlayerId);

			while (true)
			{
				std::unique_lock<std::mutex> lock(self->mQueueMutex);

				// Wait for a job, stop signal, or unpause
				self->mCondVar.wait(lock, [&] { return !self->mJobQueue.empty() || self->mStop.load() || !self->mPaused.load(); });

				// Check if we need to stop
				if (self->mStop.load())
				{
					AAMPLOG_DEBUG("Worker thread stopped for media type %s", GetMediaTypeName(self->mMediaType));
					break;
				}

				// Handle pause condition
				while (self->mPaused.load())
				{
					AAMPLOG_DEBUG("Worker thread paused for media type %s", GetMediaTypeName(self->mMediaType));
					self->mCondVar.wait(lock, [&] { return !self->mPaused.load() || self->mStop.load(); });

					if (self->mStop.load()) // Check stop again after unpausing
					{
						AAMPLOG_DEBUG("Worker thread stopping after being unpaused for media type %s", GetMediaTypeName(self->mMediaType));
						break;
					}
				}

				// Extract the job safely
				AampTrackWorkerJobPtr currentJob;
				if (!self->mJobQueue.empty())
				{
					self->mActiveJob = std::move(self->mJobQueue.front());
					self->mJobQueue.pop_front();
					currentJob = self->mActiveJob;
				}

				lock.unlock(); // Release lock before executing the job

				if (currentJob)
				{
					try
					{
						AAMPLOG_DEBUG("Executing job for media type %s", GetMediaTypeName(self->mMediaType));
						currentJob->Execute();
						AAMPLOG_DEBUG("Executed job for media type %s", GetMediaTypeName(self->mMediaType));
					}
					catch (const std::exception &e)
					{
						AAMPLOG_ERR("Exception caught while executing job: %s", e.what());
					}
					catch (...)
					{
						AAMPLOG_ERR("Unknown exception caught in ProcessJob.");
					}
					// Reset the active job to avoid dangling references
					self->mActiveJob = nullptr;
				}

				lock.lock(); // Re-acquire lock after job execution

				if (self->mJobQueue.empty() && nullptr == self->mActiveJob)
				{
					self->mCompletionVar.notify_all();
				}
				if (self->mStop.load()) // Ensure we don't continue if stopped
				{
					break;
				}
			}
			AAMPLOG_INFO("Exiting ProcessJob for media type %s", GetMediaTypeName(self->mMediaType));
		}
		else
		{
			AAMPLOG_WARN("AampTrackWorker instance is destroyed, exiting ProcessJob");
		}
	}

	void AampTrackWorker::RescheduleActiveJob()
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		if (mActiveJob)
		{
			// Reschedule the active job to the queue
			mJobQueue.push_front(mActiveJob);
			mActiveJob = nullptr; // Clear active job after rescheduling
			mCondVar.notify_one();
		}
	}
} // namespace aamp
