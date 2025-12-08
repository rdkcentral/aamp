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

#include "AampTrackWorkerManager.hpp"

namespace aamp
{
	/**
	 * @brief Default constructor.
	 */
	AampTrackWorkerManager::AampTrackWorkerManager()
	{
	}

	/**
	 * @brief Default destructor.
	 */
	AampTrackWorkerManager::~AampTrackWorkerManager()
	{
	}

	/**
	 * @brief Creates an AampTrackWorker instance.
	 *
	 * If an instance with the same media type already exists, it returns the existing instance.
	 * @param[in] aamp Pointer to the PrivateInstanceAAMP.
	 * @param[in] mediaType The media type for the worker.
	 *
	 * @return Shared pointer to the created or existing AampTrackWorker instance.
	 */
	std::shared_ptr<AampTrackWorker> AampTrackWorkerManager::CreateWorker(PrivateInstanceAAMP *aamp, AampMediaType mediaType)
	{
		return nullptr;
	}

	/**
	 * @brief Gets an existing AampTrackWorker instance.
	 *
	 * @param[in] mediaType The media type of the worker.
	 *
	 * @return Shared pointer to the AampTrackWorker instance, or nullptr if not found.
	 */
	std::shared_ptr<AampTrackWorker> AampTrackWorkerManager::GetWorker(AampMediaType mediaType)
	{
		return nullptr;
	}

	/**
	 * @brief Removes all AampTrackWorker instances.
	 *
	 * Removes the worker instances
	 */
	void AampTrackWorkerManager::RemoveWorkers()
	{
	}

	/**
	 * @brief Starts all workers.
	 */
	void AampTrackWorkerManager::StartWorkers()
	{
	}

	/**
	 * @brief Stops all workers.
	 */
	void AampTrackWorkerManager::StopWorkers()
	{
	}

	/**
	 * @brief Waits for all workers to complete their jobs.
	 *
	 * @param[in] timeInterval The time interval to wait for each onTimeout in milliseconds.
	 * @param[in] onTimeout callback function
	 */
	void AampTrackWorkerManager::WaitForCompletionWithTimeout(int timeout, std::function<void()> onTimeout)
	{
	}

	/**
	 * @brief Checks if there are any workers.
	 *
	 * @return True if there are no workers, false otherwise.
	 */
	bool AampTrackWorkerManager::IsEmpty()
	{
		return false;
	}

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
	std::shared_future<void> AampTrackWorkerManager::SubmitJob(AampMediaType mediaType, std::shared_ptr<AampTrackWorkerJob> job, bool highPriority)
	{
		if (job)
		{
			job->Run(); // Execute the job immediately for testing purposes
			return job->GetFuture();
		}
		else
		{
			AAMPLOG_ERR("AampTrackWorkerManager::SubmitJob: Job is null");
			return std::shared_future<void>();
		}
	}

	/**
	 * @brief Reset the worker by clearing all jobs
	 *
	 * @param[in] mediaType The media type of the worker to reset.
	 */
	void AampTrackWorkerManager::ResetWorker(AampMediaType mediaType)
	{
	}

	/**
	 * @brief Gets the count of workers.
	 *
	 * @return The number of workers.
	 */
	size_t AampTrackWorkerManager::GetWorkerCount()
	{
		return 0;
	}
} // namespace aamp
