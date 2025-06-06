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

#include "AampTrackWorker.h"
#include "MockAampTrackWorker.h"

MockAampTrackWorker *g_mockAampTrackWorker = nullptr;

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
		: aamp(_aamp), mMediaType(_mediaType), mStop(false), mWorkerThread(), mJobQueue(), mQueueMutex(), mCondVar(), mCompletionVar()
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
	void AampTrackWorker::SubmitJob(AampTrackWorkerJobPtr job, bool highPriority)
	{
		if(job)
		{
			job->Execute();
		}
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
		return true;
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
