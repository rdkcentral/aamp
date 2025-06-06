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

#ifndef AAMP_TRACK_WORKER_H
#define AAMP_TRACK_WORKER_H

/**
 * @file AampTrackWorker.h
 * @brief Implementation of the AampTrackWorker class.
 *
 * This file contains the implementation of the AampTrackWorker class, which is responsible for
 * managing a worker thread that processes jobs submitted to it. The worker thread waits for jobs
 * to be submitted, processes them, and signals their completion.
 */

#include <thread>
#include <condition_variable>
#include <functional>
#include <vector>
#include <mutex>
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampConfig.h"
#include "priv_aamp.h"

namespace aamp
{
	class AampTrackWorkerJob
	{
	public:
		virtual ~AampTrackWorkerJob() = default;
		virtual void Execute() {};
	};

	/**
	 * @typedef AampTrackWorkerJobPtr
	 * @brief Represents a job to download a media fragment.
	 *
	 * The DownloadJob typedef encapsulates the job to download a media fragment.
	 **/
	typedef std::shared_ptr<AampTrackWorkerJob> AampTrackWorkerJobPtr;

	/**
	 * Forward declaration of AampTrackWorker to resolve unknown type error.
	 */
	class AampTrackWorker;

	/**
	 * @typedef AampTrackWorkerWeakPtr
	 * @brief Represents a weak pointer to an AampTrackWorker instance.
	 *
	 * This typedef is used to avoid circular references between the worker and the job.
	 */
	typedef std::weak_ptr<AampTrackWorker> AampTrackWorkerWeakPtr;

	/**
	 * @class AampTrackWorker
	 * @brief A class that manages a worker thread for processing jobs.
	 *
	 * The AampTrackWorker class creates a worker thread that waits for jobs to be submitted,
	 * processes them, and signals their completion. The class provides methods to submit jobs,
	 * wait for job completion, and clean up the worker thread.
	 */

	class AampTrackWorker : public std::enable_shared_from_this<AampTrackWorker>
	{
	public:
		AampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType);
		~AampTrackWorker();

		void SubmitJob(AampTrackWorkerJobPtr job, bool highPriority = false);
		void WaitForCompletion();
		bool WaitForCompletionWithTimeout(int timeout);
		void Pause();
		void Resume();
		void ClearJobs();
		void RescheduleActiveJob();
		void StartWorker();
		void StopWorker();
		bool IsStopped() const { return mStop.load(); }

	protected:
		AampMediaType mMediaType;
		std::thread mWorkerThread;
		std::mutex mQueueMutex; // Mutex to protect job queue
		std::condition_variable mCondVar;
		std::condition_variable mCompletionVar;
		std::deque<AampTrackWorkerJobPtr> mJobQueue; // Job queue
		PrivateInstanceAAMP *aamp;
		std::atomic<bool> mStop;
		std::atomic<bool> mPaused; // Flag to pause the worker threads

	private:
		void ProcessJob(AampTrackWorkerWeakPtr weakSelf);
		AampTrackWorkerJobPtr mActiveJob; // Active job being processed
	};
} // namespace aamp

#endif // AAMP_TRACK_WORKER_H
