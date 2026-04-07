/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file AampSourceWorker.cpp
 * @brief Implementation of SourceWorker — per-source injection thread.
 */

#include "AampSourceWorker.h"
#include "AampLogManager.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SourceWorker::SourceWorker(InjectFn injectFn)
	: m_injectFn(std::move(injectFn))
	, m_thread(&SourceWorker::run, this)
{
}

SourceWorker::~SourceWorker()
{
	stop();
}

// ---------------------------------------------------------------------------
// Public interface — called from external threads
// ---------------------------------------------------------------------------

void SourceWorker::postNeedData(
	int32_t  sourceId,
	uint32_t requestId,
	size_t   frameCount)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingReqs.push_back({sourceId, requestId, frameCount});
	}
	m_cv.notify_one();
}

void SourceWorker::cancelNeedData()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_pendingReqs.clear();
}

void SourceWorker::enqueueSamples(std::vector<QueuedSample> samples)
{
	if (!samples.empty())
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto &s : samples)
			{
				m_sampleQueue.push_back(std::move(s));
			}
		}
		m_cv.notify_one();
	}
}

void SourceWorker::setEos()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_eos = true;
	}
	m_cv.notify_one();
}

void SourceWorker::flush()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sampleQueue.clear();
	m_pendingReqs.clear();
	m_eos = false;
}

void SourceWorker::stop()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stop = true;
	}
	m_cv.notify_all();
	if (m_thread.joinable())
	{
		m_thread.join();
	}
}

// ---------------------------------------------------------------------------
// Worker thread main loop
// ---------------------------------------------------------------------------

void SourceWorker::run()
{
	AAMPLOG_INFO("SourceWorker: thread started");

	while (true)
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		// Block until there is work or we are asked to stop.
		m_cv.wait(lock, [this] {
			return m_stop ||
			       (!m_pendingReqs.empty() &&
			        (!m_sampleQueue.empty() || m_eos));
		});

		if (m_stop)
		{
			break;
		}

		// Drain all requests that have data (or EOS) available.
		while (!m_stop &&
		       !m_pendingReqs.empty() &&
		       (!m_sampleQueue.empty() || m_eos))
		{
			PendingNeedData req = m_pendingReqs.front();
			m_pendingReqs.pop_front();

			const size_t toSend =
				std::min(req.frameCount, m_sampleQueue.size());

			std::vector<QueuedSample> toInject;
			toInject.reserve(toSend);
			for (size_t i = 0; i < toSend; ++i)
			{
				toInject.push_back(std::move(m_sampleQueue.front()));
				m_sampleQueue.pop_front();
			}

			bool eos = m_eos && m_sampleQueue.empty();

			// Release lock during the (potentially slow) IPC call.
			lock.unlock();

			auto rejected = m_injectFn(
				req.sourceId, req.requestId,
				std::move(toInject), eos);

			lock.lock();

			// Re-queue any samples rejected by the pipeline (NO_SPACE) so
			// they are retried on the next needData request.
			for (auto it = rejected.rbegin(); it != rejected.rend(); ++it)
			{
				m_sampleQueue.push_front(std::move(*it));
			}
		}
	}

	AAMPLOG_INFO("SourceWorker: thread exiting");
}
