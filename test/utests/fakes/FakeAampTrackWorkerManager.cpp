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

	std::shared_ptr<AampTrackWorker> AampTrackWorkerManager::CreateWorker(PrivateInstanceAAMP *aamp, AampMediaType mediaType)
	{
		return nullptr;
	}

	std::shared_ptr<AampTrackWorker> AampTrackWorkerManager::GetWorker(AampMediaType mediaType)
	{
		return nullptr;
	}

	void AampTrackWorkerManager::RemoveWorkers()
	{
	}

	void AampTrackWorkerManager::StartWorkers()
	{
	}

	void AampTrackWorkerManager::StopWorkers()
	{
	}

	void AampTrackWorkerManager::WaitForCompletionWithTimeout(int timeout, std::function<void()> onTimeout)
	{
	}

	bool AampTrackWorkerManager::IsEmpty()
	{
		return false;
	}

	std::shared_future<void> AampTrackWorkerManager::SubmitJob(AampMediaType mediaType, std::shared_ptr<AampTrackWorkerJob> job, bool highPriority)
	{
		if(job)
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

	void AampTrackWorkerManager::ResetWorker(AampMediaType mediaType)
	{
	}

	size_t AampTrackWorkerManager::GetWorkerCount()
	{
		return 0;
	}
} // namespace aamp
