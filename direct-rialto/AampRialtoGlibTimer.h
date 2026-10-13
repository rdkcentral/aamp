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
 * @file AampRialtoGlibTimer.h
 * @brief Thread-safe periodic GLib timer shared by the direct-Rialto classes.
 */

#ifndef AAMP_RIALTO_GLIB_TIMER_H
#define AAMP_RIALTO_GLIB_TIMER_H

#include <glib.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

/**
 * @class AampRialtoGlibTimer
 * @brief Periodic timer on the default GLib main context.
 *
 * Start(), Restart(), Stop() and IsRunning() may be called from any thread.
 * Ticks run on the GLib main loop thread under an internal lock, so:
 *  - Stop() blocks until a tick already running on another thread has
 *    returned, and no tick begins afterwards;
 *  - a dispatch GLib had already picked up when Stop() ran, or after the
 *    owner was destroyed, finds the timer inactive and does nothing.
 *
 * The lock is recursive: a tick may call Stop(), Restart() or IsRunning() on
 * its own timer.  A tick must not wait on another thread that is itself
 * calling into the timer.
 */
class AampRialtoGlibTimer
{
public:
	/// Return true to keep the timer running, false to stop it.
	using Tick = std::function<bool()>;

	AampRialtoGlibTimer() : m_state(std::make_shared<State>()) {}

	~AampRialtoGlibTimer() { Stop(); }

	AampRialtoGlibTimer(const AampRialtoGlibTimer &) = delete;
	AampRialtoGlibTimer &operator=(const AampRialtoGlibTimer &) = delete;

	/// Schedule @p tick every @p intervalMs.  Does nothing and returns false
	/// if already running, or if GLib could not schedule the source.
	bool Start(guint intervalMs, Tick tick)
	{
		std::lock_guard<std::recursive_mutex> lock(m_state->mutex);
		return StartLocked(intervalMs, std::move(tick));
	}

	/// Stop any running timer, then schedule @p tick as Start() does.
	bool Restart(guint intervalMs, Tick tick)
	{
		std::lock_guard<std::recursive_mutex> lock(m_state->mutex);
		StopLocked();
		return StartLocked(intervalMs, std::move(tick));
	}

	/// Cancel the timer; see the class comment for the blocking guarantee.
	void Stop()
	{
		std::lock_guard<std::recursive_mutex> lock(m_state->mutex);
		StopLocked();
	}

	bool IsRunning() const
	{
		std::lock_guard<std::recursive_mutex> lock(m_state->mutex);
		return m_state->running;
	}

private:
	struct State
	{
		mutable std::recursive_mutex mutex;
		bool running{false};
		guint sourceId{0};
		/// Bumped by every Start so a dispatch from an earlier run is ignored.
		uint64_t generation{0};
		Tick tick;
	};

	/// Owned by the GLib source; keeps State alive for as long as a dispatch
	/// can still arrive, whatever happens to the AampRialtoGlibTimer.
	struct Handle
	{
		std::shared_ptr<State> state;
		uint64_t generation;
	};

	bool StartLocked(guint intervalMs, Tick tick)
	{
		bool started = false;
		if (!m_state->running)
		{
			m_state->tick = std::move(tick);
			const uint64_t generation = ++m_state->generation;
			auto *handle = new Handle{m_state, generation};
			const guint id = g_timeout_add_full(G_PRIORITY_DEFAULT, intervalMs,
				&AampRialtoGlibTimer::Dispatch, handle,
				&AampRialtoGlibTimer::Release);
			if (id == 0)
			{
				delete handle;
			}
			else
			{
				m_state->running = true;
				m_state->sourceId = id;
				started = true;
			}
		}
		return started;
	}

	void StopLocked()
	{
		if (m_state->running)
		{
			m_state->running = false;
			const guint id = m_state->sourceId;
			m_state->sourceId = 0;
			if (id != 0)
			{
				g_source_remove(id);
			}
		}
	}

	static gboolean Dispatch(gpointer data)
	{
		auto *handle = static_cast<Handle *>(data);
		State &state = *handle->state;
		bool keep = false;
		{
			std::lock_guard<std::recursive_mutex> lock(state.mutex);
			if (state.running && (state.generation == handle->generation))
			{
				// Copied so the tick may Restart() its own timer.
				Tick tick = state.tick;
				keep = tick();
				if (state.running && (state.generation == handle->generation))
				{
					if (!keep)
					{
						// Returning false removes the source, so forget its id.
						state.running = false;
						state.sourceId = 0;
					}
				}
				else
				{
					keep = false;
				}
			}
		}
		return keep ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
	}

	static void Release(gpointer data)
	{
		delete static_cast<Handle *>(data);
	}

	std::shared_ptr<State> m_state;
};

#endif // AAMP_RIALTO_GLIB_TIMER_H
