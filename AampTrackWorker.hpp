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

#ifndef AAMP_TRACK_WORKER_HPP
#define AAMP_TRACK_WORKER_HPP

/**
 * @file AampTrackWorker.hpp
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
#include <future>
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampConfig.h"
#include "AampMediaType.h"

// Forward declaration to avoid recursive include
class PrivateInstanceAAMP;

namespace aamp
{
	class AampTrackWorkerJob
	{
	public:
		/**
		 * @brief Default constructor for AampTrackWorkerJob.
		 */
		AampTrackWorkerJob();

		/**
		 * @brief Destructor for AampTrackWorkerJob.
		 *
		 * Cleans up resources used by the job.
		 */
		virtual ~AampTrackWorkerJob();

		// Delete copy constructor and copy assignment operator
		AampTrackWorkerJob(const AampTrackWorkerJob&) = delete;
		AampTrackWorkerJob& operator=(const AampTrackWorkerJob&) = delete;
		// Default move constructor and move assignment operator
		AampTrackWorkerJob(AampTrackWorkerJob&&) = delete;
		AampTrackWorkerJob& operator=(AampTrackWorkerJob&&) = delete;

		/**
		 * @brief Called by the worker thread to run the job.
		 */
		void Run();

		/**
		 * @brief Virtual Execute method to override in subclasses.
		 */
		virtual void Execute();

		/**
		 * @brief Clones the job for worker pool.
		 */
		virtual std::unique_ptr<AampTrackWorkerJob> Clone() const;

		/**
		 * @brief Cancels the job by setting the cancelled flag.
		 */
		void SetCancelled();

		/**
		 * @brief Check if job has been cancelled.
		 *
		 * @return true if cancelled
		 */
		bool IsCancelled() const;

		/**
		 * @brief Get a future to wait for job completion.
		 */
		std::shared_future<void> GetFuture() const;

	private:
		std::atomic<bool> mCancelled{false};
		std::shared_future<void> mSharedFuture;
		std::promise<void> mPromise;
	};

	/**
	 * @typedef AampTrackWorkerJobSharedPtr
	 * @typedef AampTrackWorkerJobUniquePtr
	 * @brief Represents a job to download a media fragment.
	 *
	 * The DownloadJob typedef encapsulates the job to download a media fragment.
	 **/
	typedef std::shared_ptr<AampTrackWorkerJob> AampTrackWorkerJobSharedPtr;
	typedef std::unique_ptr<AampTrackWorkerJob> AampTrackWorkerJobUniquePtr;

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

		std::shared_future<void> SubmitJob(AampTrackWorkerJobSharedPtr job, bool highPriority = false);
		void Pause();
		void Resume();
		void ClearJobs();
		void RescheduleActiveJob();
		void StartWorker();
		void StopWorker();
		bool IsStopped() const { return mStop.load(); }
		AampMediaType GetMediaType() const { return mMediaType; }

	protected:
		AampMediaType mMediaType;
		std::thread mWorkerThread;
		std::mutex mQueueMutex; // Mutex to protect job queue
		std::condition_variable mCondVar; // Condition variable to notify worker thread
		std::deque<AampTrackWorkerJobSharedPtr> mJobQueue; // Job queue
		PrivateInstanceAAMP *aamp;
		std::atomic<bool> mInitialized; // Flag to indicate if the worker is initialized
		std::atomic<bool> mStop;
		std::atomic<bool> mPaused; // Flag to pause the worker threads

	private:
		void ProcessJob(AampTrackWorkerWeakPtr weakSelf);
		AampTrackWorkerJobSharedPtr mActiveJob; // Active job being processed
	};
} // namespace aamp

#endif // AAMP_TRACK_WORKER_HPP
