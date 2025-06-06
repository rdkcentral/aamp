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
		: aamp(_aamp), mMediaType(_mediaType), mStop(false), mPaused(false), mActiveJob(nullptr), mWorkerThread(), mJobQueue(), mQueueMutex(), mCondVar()
	{
		if (_aamp == nullptr)
		{
			throw std::invalid_argument("AampTrackWorker: _aamp cannot be null");
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
		mStop.store(true);
		AAMPLOG_DEBUG("Stopping worker thread for media type %s", GetMediaTypeName(mMediaType));
		mCondVar.notify_one(); // Wake up the thread to stop
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
	 * @return std::shared_future<void> A future that will be set when the job is completed.
	 */
	std::shared_future<void> AampTrackWorker::SubmitJob(AampTrackWorkerJobSharedPtr job, bool highPriority)
	{
		if(nullptr == job)
		{
			AAMPLOG_ERR("Attempted to submit a null job to worker for media type %s", GetMediaTypeName(mMediaType));
			return std::shared_future<void>(); // Return an empty future
		}
		auto future = job->GetFuture();
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
			else
			{
				AAMPLOG_WARN("Attempted to submit job to stopped worker for media type %s", GetMediaTypeName(mMediaType));
				return std::shared_future<void>(); // Return an empty future
			}
		}
		AAMPLOG_DEBUG("Async job submitted for media type %s", GetMediaTypeName(mMediaType));
		mCondVar.notify_one();
		return future;
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
		mPaused.store(true);
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
		mPaused.store(false);
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
		// Signal all pending jobs that they've been cancelled
		for (auto& job : mJobQueue)
		{
			try
			{
				job->SetCancelled(); // Signal cancellation to the job
			}
			catch (const std::exception& e)
			{
				AAMPLOG_WARN("Exception while cancelling job: %s", e.what());
			}
		}
		mJobQueue.clear();
		AAMPLOG_DEBUG("All jobs cleared for media type %s", GetMediaTypeName(mMediaType));
	}

	/**
	 * @brief Reschedules the active job to the job queue.
	 *
	 * If there is an active job being processed, it is rescheduled to the front of the job queue.
	 *
	 * @return void
	 */
	void AampTrackWorker::RescheduleActiveJob()
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		if (mActiveJob)
		{
			// Reschedule the active job to the queue
			AAMPLOG_DEBUG("Rescheduling active job for media type %s", GetMediaTypeName(mMediaType));
			auto newJob = mActiveJob->Clone(); // Ensure the job can be cloned if needed
			mJobQueue.push_front(std::move(newJob));
			mActiveJob = nullptr; // Clear active job after rescheduling
			mCondVar.notify_one();
		}
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
				AampTrackWorkerJobSharedPtr currentJob;
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
						AAMPLOG_DEBUG("Running job for media type %s", GetMediaTypeName(self->mMediaType));
						// Run the job and catch any exceptions
						// The promise in the job will be set when the job completes
						// This allows the job to signal completion without blocking the worker thread
						currentJob->Run();
						AAMPLOG_DEBUG("Finished job for media type %s", GetMediaTypeName(self->mMediaType));
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

				lock.lock();			// Re-acquire lock after job execution
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
} // namespace aamp
