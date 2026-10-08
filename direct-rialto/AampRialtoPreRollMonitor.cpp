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
 * @file AampRialtoPreRollMonitor.cpp
 * @brief Implementation of the pre-roll completion monitor.
 */

#include "AampRialtoPreRollMonitor.h"
#include "AampLogManager.h"

#include <chrono>

namespace {

int64_t SteadyNowMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

AampRialtoPreRollMonitor::AampRialtoPreRollMonitor(
	Config config, Completion onComplete, Clock clock)
	: m_config(config)
	, m_onComplete(std::move(onComplete))
	, m_clock(clock ? std::move(clock) : Clock(&SteadyNowMs))
{
}

AampRialtoPreRollMonitor::~AampRialtoPreRollMonitor()
{
	Stop();
}

void AampRialtoPreRollMonitor::Start(Probe probe)
{
	guint previousTimer = 0;
	uint64_t epoch = 0;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		previousTimer = m_timerId;
		m_timerId = 0;
		epoch = ++m_epoch;
		m_running = true;
		m_probe = std::move(probe);
		m_startMs = m_clock();
		m_haveLoggedFrames = false;
	}
	if (previousTimer != 0)
	{
		g_source_remove(previousTimer);
	}

	const guint timerId = g_timeout_add(m_config.pollIntervalMs,
		&AampRialtoPreRollMonitor::TimerCallback, this);

	bool completeNow = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_running && (m_epoch == epoch))
		{
			if (timerId == 0)
			{
				m_running = false;
				completeNow = true;
			}
			else
			{
				m_timerId = timerId;
			}
		}
	}

	if (completeNow)
	{
		AAMPLOG_WARN("pre-roll timer could not be scheduled - completing now");
		m_onComplete();
	}
	else
	{
		AAMPLOG_INFO("pre-roll monitor started floor=%u timeout=%lld ms",
			m_config.minAcceptedFrames,
			static_cast<long long>(m_config.timeoutMs));
	}
}

void AampRialtoPreRollMonitor::Stop()
{
	guint timerId = 0;
	bool wasRunning = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		timerId = m_timerId;
		m_timerId = 0;
		wasRunning = m_running;
		m_running = false;
		++m_epoch;
	}
	if (timerId != 0)
	{
		g_source_remove(timerId);
	}
	if (wasRunning)
	{
		AAMPLOG_INFO("pre-roll monitor stopped before completion");
	}
	ReleaseProbe();
}

void AampRialtoPreRollMonitor::ReleaseProbe()
{
	// Moved out so the captured state is destroyed without m_mutex held.
	Probe discarded;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_running)
		{
			discarded = std::move(m_probe);
			m_probe = Probe{};
		}
	}
}

bool AampRialtoPreRollMonitor::IsRunning() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_running;
}

bool AampRialtoPreRollMonitor::Poll()
{
	bool keepPolling = false;
	Probe probe;
	uint64_t epoch = 0;
	int64_t startMs = 0;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		keepPolling = m_running;
		probe = m_probe;
		epoch = m_epoch;
		startMs = m_startMs;
	}

	if (keepPolling)
	{
		// Probes run without the mutex held; they read other objects' state.
		const uint32_t frames = probe.acceptedFrames ? probe.acceptedFrames() : 0u;
		const bool eos = probe.endOfStream && probe.endOfStream();
		const bool full = probe.bufferFull && probe.bufferFull();
		const int64_t elapsedMs = m_clock() - startMs;

		const char *reason = nullptr;
		if (frames >= m_config.minAcceptedFrames)
		{
			reason = "target reached";
		}
		else if (full)
		{
			reason = "buffer full";
		}
		else if (eos)
		{
			reason = "end of stream";
		}
		else if (elapsedMs >= m_config.timeoutMs)
		{
			reason = "timeout";
		}

		bool complete = false;
		bool logProgress = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_running || (m_epoch != epoch))
			{
				keepPolling = false;
			}
			else if (reason != nullptr)
			{
				// Returning false removes the GLib source, so forget its id.
				m_running = false;
				m_timerId = 0;
				keepPolling = false;
				complete = true;
			}
			else if (!m_haveLoggedFrames || (frames != m_lastLoggedFrames))
			{
				m_haveLoggedFrames = true;
				m_lastLoggedFrames = frames;
				logProgress = true;
			}
		}

		const long long spanMs = probe.injectedSpanMs
			? static_cast<long long>(probe.injectedSpanMs()) : 0LL;
		if (logProgress)
		{
			AAMPLOG_INFO("pre-roll progress acceptedFrames=%u/%u injected=%lld ms "
				"elapsed=%lld ms", frames, m_config.minAcceptedFrames, spanMs,
				static_cast<long long>(elapsedMs));
		}
		if (complete)
		{
			AAMPLOG_MIL("pre-roll complete (%s) after %lld ms acceptedFrames=%u "
				"injected=%lld ms", reason, static_cast<long long>(elapsedMs),
				frames, spanMs);
			ReleaseProbe();
			m_onComplete();
		}
	}

	return keepPolling;
}

gboolean AampRialtoPreRollMonitor::TimerCallback(gpointer data)
{
	auto *self = static_cast<AampRialtoPreRollMonitor *>(data);
	return self->Poll() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}
