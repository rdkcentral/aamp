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

#ifndef AAMP_TRACK_WORKER_MANAGER_HPP
#define AAMP_TRACK_WORKER_MANAGER_HPP

#include "AampTrackWorker.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <future>

namespace aamp
{
	class AampTrackWorker;
	class AampTrackWorkerJob;
	using AampTrackWorkerJobSharedPtr = std::shared_ptr<AampTrackWorkerJob>;

	/**
	 * @class AampTrackWorkerManager
	 * @brief Factory class for managing AampTrackWorker instances.
	 *
	 * Provides methods to create, retrieve, and remove AampTrackWorker instances
	 * in a thread-safe manner.
	 */
	class AampTrackWorkerManager
	{
	public:
		/**
		 * @brief Default constructor.
		 */
		AampTrackWorkerManager();

		/**
		 * @brief Default destructor.
		 */
		~AampTrackWorkerManager();

		/**
		 * @brief Creates an AampTrackWorker instance.
		 *
		 * If an instance with the same media type already exists, it returns the existing instance.
		 * @param aamp Pointer to the PrivateInstanceAAMP.
		 * @param mediaType The media type for the worker.
		 * @return Shared pointer to the created or existing AampTrackWorker instance.
		 */
		std::shared_ptr<AampTrackWorker> CreateWorker(PrivateInstanceAAMP *aamp, AampMediaType mediaType);

		/**
		 * @brief Gets an existing AampTrackWorker instance.
		 *
		 * @param mediaType The media type of the worker.
		 * @return Shared pointer to the AampTrackWorker instance, or nullptr if not found.
		 */
		std::shared_ptr<AampTrackWorker> GetWorker(AampMediaType mediaType);

		/**
		 * @brief Submits a job to the specified worker.
		 *
		 * @param[in] mediaType The media type of the worker.
		 * @param[in] job The job to submit.
		 * @param[in] highPriority Whether the job should be treated as high priority.
		 *
		 * @note If the worker is not found, a default-constructed future is returned.
		 * @return A future representing the submitted job, or a default-constructed future if worker not found.
		 */
		std::shared_future<void> SubmitJob(AampMediaType mediaType, AampTrackWorkerJobSharedPtr job, bool highPriority = false);

		/**
		 * @brief Reset the worker by clearing all jobs
		 *
		 * @param[in] mediaType The media type of the worker to reset.
		 */
		void ResetWorker(AampMediaType mediaType);

		/**
		 * @brief Removes an AampTrackWorker instance.
		 *
		 * Removes the worker instances
		 */
		void RemoveWorkers();

		/**
		 * @brief Starts all AampTrackWorker instances.
		 *
		 * Starts all saved workers
		 */
		void StartWorkers();

		/**
		 * @brief Stops all AampTrackWorker instances.
		 *
		 * Stops all saved workers
		 */
		void StopWorkers();

		/**
		 * @brief Wait for completion of all workers with a timeout.
		 *
		 * @param timeout The timeout value in milliseconds.
		 * @param onTimeout The lambda function to execute if a timeout occurs.
		 */
		void WaitForCompletionWithTimeout(int timeout, std::function<void()> onTimeout);

		/**
		 * @brief Checks if there are any workers.
		 *
		 * @return True if there are no workers, false otherwise.
		 */
		bool IsEmpty();

		/**
		 * @brief Gets the number of workers.
		 *
		 * @return The number of workers.
		 */
		size_t GetWorkerCount();

	private:
		std::unordered_map<AampMediaType, std::shared_ptr<AampTrackWorker>> mWorkers;
		std::mutex mMutex; // Protect access to the workers map
	};

} // namespace aamp

#endif // AAMP_TRACK_WORKER_MANAGER_HPP
