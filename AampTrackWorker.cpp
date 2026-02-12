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

#include "AampTrackWorker.hpp"
 #include "priv_aamp.h"
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
		: mMediaType(_mediaType), mWorkerThread(), mQueueMutex(), mCondVar(), mJobQueue(),
		  aamp(_aamp), mStopped(true), mActiveJob(nullptr), mPaused(false)
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
		if (mWorkerThread.joinable() || !mStopped)
		{
			AAMPLOG_WARN("Worker thread for media type %s is already running", GetMediaTypeName(mMediaType));
			throw std::runtime_error("Worker thread is already running");
		}

		// No lock needed here as this is called before the thread starts
		mStopped = false;

		try
		{
			mWorkerThread = std::thread(&AampTrackWorker::ProcessJob, std::weak_ptr<AampTrackWorker>(shared_from_this()));
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception caught in AampTrackWorker %s", e.what());
			mStopped = true;
		}
		catch (...)
		{
			AAMPLOG_ERR("Unknown exception caught in AampTrackWorker for media type %s", GetMediaTypeName(mMediaType));
			mStopped = true;
		}
	}

	/**
	 * @brief Stops the worker thread.
	 *
	 * Signals the worker thread to stop and waits for it to finish.
	 * This method is noexcept because it's called from the destructor.
	 * Any errors are logged but do not propagate.
	 *
	 * @return void
	 */
	void AampTrackWorker::StopWorker() noexcept
	{
		AAMPLOG_DEBUG("Stopping worker thread for media type %s", GetMediaTypeName(mMediaType));
		
		try
		{
			{
				std::lock_guard<std::mutex> lock(mQueueMutex);
				mStopped = true;
				mCondVar.notify_all();
			}
			
			if (mWorkerThread.joinable())
			{
				mWorkerThread.join();
			}
			
			ClearJobs();
			
			std::lock_guard<std::mutex> queueLock(mQueueMutex);
			mActiveJob = nullptr;
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception in StopWorker for media type %s: %s", GetMediaTypeName(mMediaType), e.what());
		}
		catch (...)
		{
			AAMPLOG_ERR("Unknown exception in StopWorker for media type %s", GetMediaTypeName(mMediaType));
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
			if (!mStopped)
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
	void AampTrackWorker::SubmitJob(std::function<void()> job)
	{
		std::lock_guard<std::mutex> lock(mQueueMutex);
		mPaused = true;
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
		std::lock_guard<std::mutex> lock(mQueueMutex);
		mPaused = false;
		AAMPLOG_DEBUG("Resuming worker thread for media type %s", GetMediaTypeName(mMediaType));
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
				AampTrackWorkerJobSharedPtr currentJob;

				{
					std::unique_lock<std::mutex> lock(self->mQueueMutex);

					// Wait while (queue is empty or paused) and not stopped
					self->mCondVar.wait(lock, [self]() -> bool
										{ return self->mStopped || (!self->mPaused && !self->mJobQueue.empty()); });

					if (self->mStopped)
					{
						AAMPLOG_DEBUG("Worker thread stopped for media type %s", GetMediaTypeName(self->mMediaType));
						break;
					}

					if (self->mPaused)
					{
						AAMPLOG_DEBUG("Worker thread paused for media type %s", GetMediaTypeName(self->mMediaType));
						continue;
					}

					// Extract the job safely
					if (!self->mJobQueue.empty())
					{
						self->mActiveJob = std::move(self->mJobQueue.front());
						self->mJobQueue.pop_front();
						currentJob = self->mActiveJob;
					}
				}

				// Execute job without holding lock
				if (currentJob)
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

				{
					std::lock_guard<std::mutex> lock(self->mQueueMutex);

					self->mActiveJob = nullptr;
					if (self->mStopped)
					{
						break;
					}
				}
			}
		}

		AAMPLOG_INFO("Exiting Process Job for media type %s", GetMediaTypeName(mMediaType));
	}
} // namespace aamp
