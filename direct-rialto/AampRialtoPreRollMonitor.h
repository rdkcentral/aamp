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
 * @file AampRialtoPreRollMonitor.h
 * @brief Decides when enough data has been buffered ahead of play().
 */

#ifndef AAMP_RIALTO_PRE_ROLL_MONITOR_H
#define AAMP_RIALTO_PRE_ROLL_MONITOR_H

#include <glib.h>

#include <cstdint>
#include <functional>
#include <mutex>

/**
 * @class AampRialtoPreRollMonitor
 * @brief Polls the decoder queue until a pre-roll target is met.
 *
 * Direct-Rialto counterpart of InterfacePlayerRDK's buffering_timeout: while
 * running it polls the primary track's queued frame count on a GLib timer
 * and signals completion exactly once, when the first of these is true:
 *  - queued frames have reached the configured floor;
 *  - the queued frame count cannot be read (mirrors the reference treating
 *    an unreadable count as satisfied);
 *  - the primary track has reached end of stream;
 *  - the timeout has elapsed.
 *
 * The injected duration is reported in the logs only; it does not gate.
 *
 * Thread-safe.  Start()/Stop() may be called from any thread; polling and
 * completion run on the GLib main loop.  The completion action is invoked
 * without the internal mutex held.
 */
class AampRialtoPreRollMonitor
{
public:
	/// Pre-roll target and timing.
	struct Config
	{
		uint32_t minQueuedFrames; ///< Frame floor for the primary track.
		guint    pollIntervalMs;  ///< Interval between polls.
		int64_t  timeoutMs;       ///< Give up and complete after this long.
	};

	/// Read-only views of the primary track, captured for one pre-roll.
	struct Probe
	{
		/// Fill in the decoder's queued frame count; false if unavailable.
		std::function<bool(uint32_t &)> queuedFrames;
		/// True once the primary track has signalled end of stream.
		std::function<bool()> endOfStream;
		/// Span of media accepted by Rialto so far, for logging.
		std::function<int64_t()> injectedSpanMs;
	};

	/// Invoked once when a pre-roll completes for any reason.
	using Completion = std::function<void()>;

	/// Monotonic time source in milliseconds; injectable for tests.
	using Clock = std::function<int64_t()>;

	/**
	 * @param[in] config      Target and timing.
	 * @param[in] onComplete  Non-null action invoked on completion.
	 * @param[in] clock       Optional time source; defaults to steady_clock.
	 */
	AampRialtoPreRollMonitor(Config config, Completion onComplete,
		Clock clock = nullptr);

	~AampRialtoPreRollMonitor();

	AampRialtoPreRollMonitor(const AampRialtoPreRollMonitor &) = delete;
	AampRialtoPreRollMonitor &operator=(const AampRialtoPreRollMonitor &) = delete;

	/// Begin a pre-roll against @p probe, restarting any pre-roll already
	/// in progress.  Completes immediately if no timer can be scheduled.
	void Start(Probe probe);

	/// Abandon any pre-roll in progress without signalling completion.
	void Stop();

	/// True while a pre-roll is in progress.
	bool IsRunning() const;

	/**
	 * @brief Evaluate the pre-roll once.
	 *
	 * Called by the GLib timer; public so tests can drive it directly.
	 *
	 * @return true while polling should continue.
	 */
	bool Poll();

private:
	static gboolean TimerCallback(gpointer data);

	const Config m_config;
	const Completion m_onComplete;
	const Clock m_clock;

	mutable std::mutex m_mutex;
	/// Guarded by m_mutex.  Bumped by Start()/Stop() so a poll that was
	/// already in flight cannot complete an abandoned pre-roll.
	uint64_t m_epoch{0};
	bool m_running{false};
	guint m_timerId{0};
	int64_t m_startMs{0};
	uint32_t m_lastLoggedFrames{0};
	bool m_haveLoggedFrames{false};
	Probe m_probe;
};

#endif // AAMP_RIALTO_PRE_ROLL_MONITOR_H
