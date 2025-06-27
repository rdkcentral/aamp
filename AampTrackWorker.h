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
 * managing a worker thread that processes tasks submitted to it. Tasks can return values and
 * propagate exceptions through futures.
 */

#include <thread>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include "AampUtils.h"
#include "AampLogManager.h"
#include "AampConfig.h"
#include "priv_aamp.h"

namespace aamp
{
	/**
	 * @class AampTrackWorker
	 * @brief A class that manages a worker thread for processing asynchronous tasks.
	 *
	 * The AampTrackWorker class creates a worker thread that processes tasks submitted to it.
	 * Tasks are submitted with promises and can return values or throw exceptions which are
	 * propagated back to the caller via futures. Tasks can be cancelled when the worker is stopped.
	 */
	class AampTrackWorker
	{
	public:
		AampTrackWorker(PrivateInstanceAAMP *_aamp, AampMediaType _mediaType);
		~AampTrackWorker();

		/**
		 * @brief Submit a task to be executed asynchronously
		 * 
		 * @tparam F Type of the callable task
		 * @param task The task to execute
		 * @return std::future<void> A future that can be used to wait for task completion
		 * @throws std::runtime_error if the worker is stopped
		 */
		template<typename F>
		std::future<void> Submit(F task)
		{
			auto promise = std::make_shared<std::promise<void>>();
			auto future = promise->get_future();

			{
				std::lock_guard<std::mutex> lock(mMutex);
				if (mStop) {
					promise->set_exception(std::make_exception_ptr(
						std::runtime_error("Worker is stopped")));
				} else {
					mJobQueue.push([promise, task]() {
						try {
							task();
							promise->set_value();
						} catch (...) {
							promise->set_exception(std::current_exception());
						}
					});
					mCondVar.notify_one();
				}
			}
			return future;
		}

	private:
		void ProcessJob();

		AampMediaType mMediaType;
		std::thread mWorkerThread;
		std::mutex mMutex;
		std::condition_variable mCondVar;
		std::queue<std::function<void()>> mJobQueue;
		PrivateInstanceAAMP *aamp;
		bool mStop;
	};

} // namespace aamp

#endif // AAMP_TRACK_WORKER_H
