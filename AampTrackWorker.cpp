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

#include "AampTrackWorker.h"

namespace aamp
{
	/**
	 * @brief Constructs an AampTrackWorker object.
	 *
	 * Initializes the worker thread and sets the initial state of the worker.
	 *
	 * @param[in] _aamp The PrivateInstanceAAMP instance.
	 * @param[in] _mediaType The media type of the track.
	 */
	AampTrackWorker::AampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType)
		: aamp(_aamp), mMediaType(_mediaType), mStop(false), mWorkerThread(), mMutex(), mCondVar()
	{
		if (_aamp == nullptr)
		{
			AAMPLOG_ERR("AampTrackWorker constructor received null aamp");
			mStop = true;
			return;
		}

		try
		{
			mWorkerThread = std::thread(&AampTrackWorker::ProcessJob, this);
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Exception caught in AampTrackWorker constructor: %s", e.what());
			mStop = true;
		}
		catch (...)
		{
			AAMPLOG_ERR("Unknown exception caught in AampTrackWorker constructor");
			mStop = true;
		}
	}

	/**
	 * @brief Destructs the AampTrackWorker object.
	 *
	 * Signals the worker thread to stop and cleans up resources.
	 * Any pending tasks will be cancelled.
	 */
	AampTrackWorker::~AampTrackWorker()
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mStop = true;
			// Clear queue and reject all pending tasks
			std::queue<std::function<void()>> empty;
			mJobQueue.swap(empty);
		}
		mCondVar.notify_one();
		if (mWorkerThread.joinable())
		{
			mWorkerThread.join();
		}
	}

	/**
	 * @brief The main function executed by the worker thread.
	 *
	 * Waits for jobs to be submitted and processes them until stopped.
	 * Tasks are executed in order of submission.
	 */
	void AampTrackWorker::ProcessJob()
	{
		UsingPlayerId playerId(aamp->mPlayerId);
		AAMPLOG_INFO("Process Job for media type %s", GetMediaTypeName(mMediaType));

		while (!mStop)
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(mMutex);
				mCondVar.wait(lock, [this]() { return !mJobQueue.empty() || mStop; });

				if (mStop) {
					break;
				}

				if (!mJobQueue.empty()) {
					job = std::move(mJobQueue.front());
					mJobQueue.pop();
				}
			}

			if (job) {
				try {
					job();
				}
				catch (const std::exception& e) {
					AAMPLOG_ERR("Exception in worker thread: %s", e.what());
				}
				catch (...) {
					AAMPLOG_ERR("Unknown exception in worker thread");
				}
			}
		}

		AAMPLOG_INFO("Worker thread exiting for media type %s", GetMediaTypeName(mMediaType));
	}

} // namespace aamp
