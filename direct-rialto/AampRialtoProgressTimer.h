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
 * @file AampRialtoProgressTimer.h
 * @brief Progress-report timer with immediate start and kick, for the
 *        direct-Rialto player.
 */

#ifndef AAMP_RIALTO_PROGRESS_TIMER_H
#define AAMP_RIALTO_PROGRESS_TIMER_H

#include "AampLogManager.h"
#include "AampRialtoGlibTimer.h"

#include <glib.h>

#include <functional>
#include <mutex>

/**
 * @class AampRialtoProgressTimer
 * @brief Timer that fires immediately on start, then at a fixed interval.
 *
 * Can be kicked to force an immediate dispatch while keeping the interval.
 * All methods are thread-safe; see AampRialtoGlibTimer.
 */
class AampRialtoProgressTimer
{
public:
	using Callback = std::function<void()>;

	/// Start the timer: fires immediately, then at interval.
	/// Does nothing if already running.
	void start(guint interval_ms, Callback cb)
	{
		if (m_timer.IsRunning())
		{
			AAMPLOG_INFO("Progress timer already running");
			return;
		}

		if (interval_ms == 0)
		{
			AAMPLOG_WARN("Invalid interval=%u ms; timer not started",
				interval_ms);
			return;
		}

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_interval = interval_ms;
			m_callback = cb;
		}

		const bool scheduled = m_timer.Start(interval_ms,
			[cb]() { cb(); return true; });
		if (!scheduled && m_timer.IsRunning())
		{
			// Another thread started it first.
			AAMPLOG_INFO("Progress timer already running");
			return;
		}

		if (!scheduled)
		{
			AAMPLOG_WARN("Failed to schedule progress timer");
		}
		else
		{
			AAMPLOG_INFO("Started progress timer (interval=%u ms)",
				interval_ms);
		}

		// Report immediately rather than waiting a full interval.
		cb();
	}

	/// Force immediate callback dispatch and restart the interval.
	void kick()
	{
		if (!m_timer.IsRunning())
		{
			return;
		}

		guint interval = 0;
		Callback cb;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			interval = m_interval;
			cb = m_callback;
		}
		if (!cb)
		{
			return;
		}

		// Reschedule so the interval restarts from now, then dispatch
		// immediately.
		if (!m_timer.Restart(interval, [cb]() { cb(); return true; }))
		{
			AAMPLOG_WARN("Failed to reschedule progress timer after kick");
		}
		else
		{
			AAMPLOG_INFO("Progress timer kicked (rescheduled)");
		}
		cb();
	}

	/// Stop the timer; no callback runs once this returns.
	void stop()
	{
		if (m_timer.IsRunning())
		{
			m_timer.Stop();
			AAMPLOG_INFO("Stopped progress timer");
		}
	}

	/// Return true if the timer is currently running.
	bool isRunning() const { return m_timer.IsRunning(); }

private:
	AampRialtoGlibTimer m_timer;

	/// Guards m_interval and m_callback, which kick() reads.
	std::mutex m_mutex;
	guint m_interval{0};
	Callback m_callback;
};

#endif // AAMP_RIALTO_PROGRESS_TIMER_H
