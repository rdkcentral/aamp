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

#ifndef AAMP_TRACK_WORKER_MANAGER_CPP
#define AAMP_TRACK_WORKER_MANAGER_CPP

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
		StopWorkers();
		RemoveWorkers();
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
		std::lock_guard<std::mutex> lock(mMutex);

		auto it = mWorkers.find(mediaType);
		if (it != mWorkers.end())
		{
			return it->second;
		}

		auto worker = std::make_shared<AampTrackWorker>(aamp, mediaType);
		try
		{
			worker->StartWorker();
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception caught in AampTrackWorkerManager::CreateWorker: %s", e.what());
			return nullptr;
		}

		mWorkers[mediaType] = worker;
		return worker;
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
		std::lock_guard<std::mutex> lock(mMutex);

		auto it = mWorkers.find(mediaType);
		if (it != mWorkers.end())
		{
			return it->second;
		}
		return nullptr;
	}

	/**
	 * @brief Removes all AampTrackWorker instances.
	 *
	 * Removes the worker instances
	 */
	void AampTrackWorkerManager::RemoveWorkers()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mWorkers.clear();
	}

	/**
	 * @brief Stops all AampTrackWorker instances.
	 *
	 * Stops all saved workers
	 */
	void AampTrackWorkerManager::StopWorkers()
	{
		std::vector<std::shared_ptr<AampTrackWorker>> workers;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			for (const auto &worker : mWorkers)
			{
				worker.second->Pause(); // Ensure the worker is paused before removal
				workers.push_back(worker.second);
			}
		}
		for (auto &worker : workers)
		{
			worker->StopWorker();
		}
	}

	/**
	 * @brief Waits for all workers to complete their jobs.
	 *
	 * @param[in] timeInterval The time interval to wait for each onTimeout.
	 * @param[in] onTimeout callback function
	 */
	void AampTrackWorkerManager::WaitForCompletionWithTimeout(int timeInterval, std::function<void()> onTimeout)
	{
		std::vector<std::shared_ptr<AampTrackWorker>> workers;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			for (const auto &worker : mWorkers)
			{
				workers.push_back(worker.second);
			}
		}

		for (auto &worker : workers)
		{
			while(!worker->WaitForCompletionWithTimeout(timeInterval))
			{
				onTimeout();
			}
		}
	}

	/**
	 * @brief Checks if there are any workers.
	 *
	 * @return True if there are no workers, false otherwise.
	 */
	bool AampTrackWorkerManager::IsEmpty()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mWorkers.empty();
	}

	/**
	 * @brief Gets the number of workers.
	 *
	 * @return The number of workers.
	 */
	size_t AampTrackWorkerManager::GetWorkerCount()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mWorkers.size();
	}
}
#endif // AAMP_TRACK_WORKER_MANAGER_CPP
