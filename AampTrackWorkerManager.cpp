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

#include "AampTrackWorkerManager.hpp"

namespace aamp
{
	/**
	 * @brief Default constructor.
	 */
	AampTrackWorkerManager::AampTrackWorkerManager() : mWorkers(), mMutex(), mStopInProgress(false)
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

		std::shared_ptr<AampTrackWorker> worker;
		try
		{
			worker = std::make_shared<AampTrackWorker>(aamp, mediaType);
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
	 * @brief Submits a job to the specified worker.
	 *
	 * @param[in] mediaType The media type of the worker.
	 * @param[in] job The job to submit.
	 * @param[in] highPriority Whether the job should be treated as high priority.
	 *
	 * @note If the worker is not found, a default-constructed future is returned.
	 * @return A future representing the submitted job, or a default-constructed future if worker not found.
	 */
	std::shared_future<void> AampTrackWorkerManager::SubmitJob(AampMediaType mediaType, aamp::AampTrackWorkerJobSharedPtr job, bool highPriority)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		// check again under lock to avoid race where stop started after first check
		if (mStopInProgress.load())
		{
			AAMPLOG_WARN("SubmitJob rejected (post-lock): stop in progress for media type %s", GetMediaTypeName(mediaType));
			return std::shared_future<void>();
		}

		auto it = mWorkers.find(mediaType);
		if (it != mWorkers.end())
		{
			std::shared_ptr<AampTrackWorker> worker = it->second;
			if (worker)
			{
				return worker->SubmitJob(std::move(job), highPriority);
			}
		}
		AAMPLOG_ERR("Worker for media type %s not found", GetMediaTypeName(mediaType));
		return std::shared_future<void>();
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
		mStopInProgress.store(false);
	}

	/**
	 * @brief Starts all AampTrackWorker instances.
	 *
	 * Starts all saved workers
	 */
	void AampTrackWorkerManager::StartWorkers()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		for (const auto &worker : mWorkers)
		{
			if(worker.second)
			{
				try
				{
					worker.second->StartWorker();
				}
				catch (const std::exception &e)
				{
					AAMPLOG_ERR("Exception caught for while starting %s", e.what());
				}
			}
		}
	}

	/**
	 * @brief Stops all AampTrackWorker instances.
	 *
	 * Stops all saved workers
	 */
	void AampTrackWorkerManager::StopWorkers()
	{
		mStopInProgress.store(true);
		// Copy workers under lock then release before calling into worker shutdown
		std::vector<std::shared_ptr<AampTrackWorker>> workers;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			workers.reserve(mWorkers.size());
			for (const auto &p : mWorkers)
			{
				if (p.second)
				{
					workers.push_back(p.second);
				}
			}
		}

		// Now safely call Pause()/StopWorker() outside mMutex to avoid deadlock with workers that call back
		for (const auto &worker : workers)
		{
			if (worker)
			{
				try
				{
					worker->Pause();
					worker->StopWorker(); // Join the worker thread internally
				}
				catch (const std::exception &e)
				{
					AAMPLOG_ERR("Exception while stopping worker: %s", e.what());
				}
			}
		}
	}

	/**
	 * @brief Waits for all workers to complete their jobs.
	 *
	 * @param[in] timeInterval The time interval to wait for each onTimeout in milliseconds.
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
			// Submit a dummy job to ensure the worker is active and can process jobs
			auto job = std::make_shared<AampTrackWorkerJob>();
			auto future = worker->SubmitJob(std::move(job));
			AampMediaType mediaType = worker->GetMediaType();
			if(future.valid())
			{
				// Wait for the reset job to complete
				AAMPLOG_DEBUG("Waiting for worker job completion for media type %s", GetMediaTypeName(mediaType));
			}
			else
			{
				AAMPLOG_ERR("Failed to submit job to worker for media type %s", GetMediaTypeName(mediaType));
				continue; // Skip this worker if job submission failed
			}

			if(mediaType > eMEDIATYPE_AUDIO)
			{
				// This is added for backward compatibility to avoid waiting on non critical workers
				// TODO : Make sure text track is controlled by proper buffer control logic and remove this check
				AAMPLOG_DEBUG("Skipping wait for media type %s", GetMediaTypeName(mediaType));
				continue; // Skip default media type worker
			}
			try
			{
				while (true)
				{
					auto status = future.wait_for(std::chrono::milliseconds(timeInterval));
					if (status == std::future_status::ready)
					{
						// Job completed: check for cancellation or error
						future.get(); // Will throw if exception/cancelled
						break;
					}
					else
					{
						onTimeout();
					}
				}
			}
			catch (const std::exception &e)
			{
				AAMPLOG_WARN("Exception in %s worker: %s", GetMediaTypeName(mediaType), e.what());
			}
		}
	}

	/**
	 * @brief Reset the worker by clearing all jobs
	 *
	 * @param[in] mediaType The media type of the worker to reset.
	 */
	void AampTrackWorkerManager::ResetWorker(AampMediaType mediaType)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mWorkers.find(mediaType);
		if (it != mWorkers.end())
		{
			std::shared_ptr<AampTrackWorker> worker = it->second;
			if(worker)
			{
				worker->ClearJobs();
				// Submit a dummy job to ensure the worker is active and can process jobs
				auto job = std::make_shared<AampTrackWorkerJob>();
				auto future = worker->SubmitJob(std::move(job));
				// If the future is valid, wait for the reset job to complete
				if(future.valid())
				{
					// Wait for the reset job to complete
					future.get();
				}
			}
		}
		else
		{
			AAMPLOG_ERR("Worker for media type %s not found", GetMediaTypeName(mediaType));
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
