/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
#include "MockAampTrackWorker.h"

MockAampTrackWorker *g_mockAampTrackWorker = nullptr;

namespace aamp
{

	/**
	 * @brief Default constructor for AampTrackWorkerJob.
	 *
	 * Initializes the promise and sets the shared future.
	 */
	AampTrackWorkerJob::AampTrackWorkerJob()
		: mCancelled(false),
		  mPromise()
	{
		mSharedFuture = mPromise.get_future().share();
		AAMPLOG_DEBUG("AampTrackWorkerJob constructor");
	}

	/**
	 * @brief Destructor for AampTrackWorkerJob.
	 *
	 * Cleans up resources used by the job.
	 */
	AampTrackWorkerJob::~AampTrackWorkerJob() = default;

	/**
	 * @brief Runs the job in the worker thread.
	 *
	 * This method is called by the worker thread to execute the job.
	 * It catches any exceptions thrown during execution and sets them on the promise.
	 */
	void AampTrackWorkerJob::Run()
	{
		try
		{
			if (!mCancelled.load())
			{
				Execute(); // calls derived class's Execute method
			}
			mPromise.set_value(); // Set the promise to indicate job completion
		}
		catch (...)
		{
			try
			{
				mPromise.set_exception(std::current_exception()); // Set the exception on the promise
			}
			catch (...)
			{
				AAMPLOG_ERR("Exception in AampTrackWorkerJob::Run: Failed to set exception on promise");
			}
		}
	}

	/**
	 * @brief Default implementation of Execute method.
	 *
	 * This method does nothing by default and should be overridden in derived classes.
	 */
	void AampTrackWorkerJob::Execute()
	{
		// Default implementation does nothing
	}

	/**
	 * @brief Clones the job for worker pool.
	 *
	 * This method creates a new instance of AampTrackWorkerJob.
	 *
	 * @return std::unique_ptr<AampTrackWorkerJob> A unique pointer to the cloned job.
	 */
	std::unique_ptr<AampTrackWorkerJob> AampTrackWorkerJob::Clone() const
	{
		return aamp_utils::make_unique<AampTrackWorkerJob>();
	}

	/**
	 * @brief Cancels the job by setting the cancelled flag.
	 *
	 * If the job is already cancelled, it does nothing.
	 * If not, it sets the exception on the promise to indicate cancellation.
	 */
	void AampTrackWorkerJob::SetCancelled()
	{
		if (!mCancelled.exchange(true)) // Atomically set cancelled to true
		{
			try
			{
				mPromise.set_exception(std::make_exception_ptr(std::runtime_error("Job cancelled"))); // Set exception on promise
			}
			catch (...)
			{
				AAMPLOG_ERR("Exception in AampTrackWorkerJob::SetCancelled: Failed to set exception on promise");
			}
		}
	}

	/**
	 * @brief Checks if the job has been cancelled.
	 *
	 * @return true if the job is cancelled, false otherwise.
	 */
	bool AampTrackWorkerJob::IsCancelled() const
	{
		return mCancelled.load(); // Return the current value of cancelled flag
	}

	/**
	 * @brief Gets a future to wait for job completion.
	 *
	 * This method returns a shared_future that can be used to wait for the job to complete.
	 *
	 * @return std::shared_future<void> A future that will be set when the job is completed.
	 */
	std::shared_future<void> AampTrackWorkerJob::GetFuture() const
	{
		return mSharedFuture; // Return the shared future for job completion
	}

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
		: aamp(_aamp), mMediaType(_mediaType), mStop(false), mWorkerThread(), mJobQueue(), mQueueMutex(), mCondVar(), mPaused(false), mActiveJob(nullptr), mInitialized(false)
	{
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
	}

	/**
	 * @brief Submits a job to the worker thread.
	 *
	 * The job is a function that will be executed by the worker thread.
	 *
	 * @param[in] job The job to be executed by the worker thread.
	 * @param[in] highPriority Indicates whether the job is high priority.
	 *
	 * @return void
	 */
	std::shared_future<void> AampTrackWorker::SubmitJob(AampTrackWorkerJobSharedPtr job, bool highPriority)
	{
		if(job)
		{
			job->Run(); // Execute the job immediately for testing purposes
			return job->GetFuture(); // Return the future representing the job completion
		}
		return std::shared_future<void>(); // Return an empty future if job is null
	}

	/**
	 * @brief The main function executed by the worker thread.
	 *
	 * @param[in] weakSelf A weak pointer to the AampTrackWorker instance.
	 *
	 * Waits for jobs to be submitted, processes them, and signals their completion.
	 * The function runs in a loop until the worker is signaled to stop.
	 *
	 * @return void
	 */
	void AampTrackWorker::ProcessJob(AampTrackWorkerWeakPtr weakSelf)
	{
	}

	/**
	 * @brief Pauses the worker thread.
	 *
	 * Blocks the worker thread until Resume() is called.
	 *
	 * @return void
	 */
	void AampTrackWorker::Pause()
	{
	}

	/**
	 * @brief Resumes the worker thread.
	 *
	 * Unblocks the worker thread if it is paused.
	 *
	 * @return void
	 */
	void AampTrackWorker::Resume()
	{
	}

	/**
	 * @brief Clears the job queue.
	 *
	 * Removes all jobs from the queue.
	 *
	 * @return void
	 */
	void AampTrackWorker::ClearJobs()
	{
	}

	/**
	 * @brief Reschedules the active job.
	 *
	 * Moves the active job to the front of the queue.
	 *
	 * @return void
	 */
	void AampTrackWorker::RescheduleActiveJob()
	{
		if(g_mockAampTrackWorker)
		{
			g_mockAampTrackWorker->RescheduleActiveJob();
		}
	}

	/**
	 * @brief Starts the worker thread.
	 *
	 * Creates the worker thread and starts processing jobs.
	 *
	 * @return void
	 */
	void AampTrackWorker::StartWorker()
	{
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
	}

} // namespace aamp
