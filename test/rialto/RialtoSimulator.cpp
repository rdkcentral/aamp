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
 * @file RialtoSimulator.cpp
 * @brief In-process Rialto server simulator for L2 integration tests.
 *
 * Implements IMediaPipeline, IMediaPipelineFactory, IControl, IControlFactory,
 * and IClientLogControl factories with enough behaviour for AAMP DirectRialto
 * code to progress through its state machine and report playback.
 *
 * When built without the "rialto" option, this library is linked into AAMP
 * instead of the real libRialtoClient.so.  It does not decode or render A/V
 * but it drives the IMediaPipelineClient callbacks (notifyPlaybackState,
 * notifyNeedMediaData, notifyPosition) so that AAMP behaves as if connected
 * to a real Rialto server.
 *
 * Log lines are printed with "[RialtoSim]" prefix so L2 tests can match them.
 */

#include "IMediaPipeline.h"
#include "IMediaPipelineCapabilities.h"
#include "IMediaKeys.h"
#include "IClientLogControl.h"
#include "IControl.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <cstdlib>
#include <utility>
#include <vector>

namespace firebolt::rialto
{

// ===========================================================================
// Logging helper
// ===========================================================================

#define RIALTO_SIM_LOG(fmt, ...) \
	fprintf(stderr, "[RialtoSim] " fmt "\n", ##__VA_ARGS__)

// Minimum amount of media data (per non-subtitle track) that must be
// injected — or an EOS received — before the pipeline transitions to
// PLAYING.  Subtitle tracks are excluded: the pipeline reaches PLAYING
// even if no subtitle data is injected and no subtitle EOS is sent.
constexpr int64_t kMinPlayDurationNs = 1000000000LL; // 1 second

// High-water mark for buffered (injected-but-not-yet-played) media per
// non-subtitle track.  A real Rialto/GStreamer pipeline only requests more
// data (needMediaData) while its internal queues have room; once it holds
// roughly this much un-rendered data it stops asking until playback drains
// it, pacing injection to ~real time.  Without this, the simulator would
// request data greedily and AAMP would over-inject from its local TSB,
// racing a trickplay reader to the start of the buffer and signalling a
// premature EOS.  Expressed in the pipeline (restamped) timebase, so it
// applies equally to normal play and trickplay.
constexpr int64_t kBufferHighWaterNs = 10000000000LL; // 10 seconds

// Number of frames requested per needMediaData.  A real pipeline only asks
// for as much data as its buffers can currently accept; modelling that with
// a small per-request count (combined with kBufferHighWaterNs pacing) keeps
// AAMP from draining many fragments from its local TSB in a single batch.
// A large value here would let one needData admit an unbounded burst,
// defeating the backpressure model.
constexpr unsigned int kNeedDataFrameCount = 24;

// ===========================================================================
// SimMediaPipeline - simulates the Rialto media pipeline
// ===========================================================================

class SimMediaPipeline : public IMediaPipeline
{
public:
	explicit SimMediaPipeline(
		std::weak_ptr<IMediaPipelineClient> client,
		const VideoRequirements &reqs)
		: m_client(client)
		, m_nextSourceId(1)
		, m_loaded(false)
		, m_allSourcesAttached(false)
		, m_playing(false)
		, m_playRequested(false)
		, m_rate(1.0)
		, m_basePositionNs(0)
		, m_basePositionSet(false)
		, m_needDataRequestId(1)
		, m_stopRequested(false)
		, m_eosSourceCount(0)
		, m_eosNotified(false)
		, m_playbackRateEnabled(false)
	{
		const char *envRate = std::getenv("RIALTO_SIM_ENABLE_PLAYBACK_RATE");
		if (envRate && std::string(envRate) == "1")
		{
			m_playbackRateEnabled = true;
		}
		RIALTO_SIM_LOG("SimMediaPipeline: created (width=%u height=%u)",
			reqs.maxWidth, reqs.maxHeight);
	}

	~SimMediaPipeline() override
	{
		stopThreads();
		RIALTO_SIM_LOG("SimMediaPipeline: destroyed");
	}

	// -- IMediaPipeline interface --

	std::weak_ptr<IMediaPipelineClient> getClient() override
	{
		return m_client;
	}

	bool load(MediaType type, const std::string &mimeType,
		const std::string &url, bool isLive) override
	{
		RIALTO_SIM_LOG("load: type=%d mime=%s url=%s isLive=%d",
			static_cast<int>(type), mimeType.c_str(), url.c_str(), isLive);
		m_loaded = true;
		return true;
	}

	bool attachSource(
		const std::unique_ptr<MediaSource> &source) override
	{
		int32_t id = m_nextSourceId++;
		const_cast<MediaSource &>(*source).setId(id);
		MediaSourceType type = source->getType();
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			m_attachedSources.push_back(id);
			m_sourceTypes[id] = type;
		}
		RIALTO_SIM_LOG("attachSource: assigned sourceId=%d type=%d",
			id, static_cast<int>(type));
		return true;
	}

	bool removeSource(int32_t id) override
	{
		RIALTO_SIM_LOG("removeSource: sourceId=%d", id);
		return true;
	}

	bool allSourcesAttached() override
	{
		RIALTO_SIM_LOG("allSourcesAttached: %zu sources",
			m_attachedSources.size());
		m_allSourcesAttached = true;
		startNeedDataPump();
		return true;
	}

	bool play(bool &async) override
	{
		RIALTO_SIM_LOG("play");
		async = false;
		m_playRequested.store(true, std::memory_order_relaxed);

		// After resume (flush+play), re-send needMediaData so the
		// injection pipeline restarts — matching real Rialto server
		// behavior where play() after a flush triggers new requests.
		if (m_allSourcesAttached)
		{
			startNeedDataPump();
		}

		// Transition to PLAYING is deferred until each non-subtitle
		// track has buffered at least kMinPlayDurationNs of data or has
		// reached EOS.  See maybeStartPlayback().
		maybeStartPlayback();
		return true;
	}

	bool pause() override
	{
		RIALTO_SIM_LOG("pause");
		m_playRequested.store(false, std::memory_order_relaxed);
		m_playing = false;
		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::PAUSED);
		}
		return true;
	}

	bool stop() override
	{
		RIALTO_SIM_LOG("stop");
		m_playRequested.store(false, std::memory_order_relaxed);
		m_playing = false;
		stopThreads();
		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::STOPPED);
		}
		return true;
	}
	bool setPlaybackRate(double rate) override
	{
		RIALTO_SIM_LOG("setPlaybackRate: rate=%f", rate);
		if (!m_playbackRateEnabled)
		{
			RIALTO_SIM_LOG("setPlaybackRate: rate simulation disabled (set RIALTO_SIM_ENABLE_PLAYBACK_RATE=1 to enable)");
			return false;
		}
		// Snapshot the current position before changing rate so that
		// subsequent elapsed-time calculations use the new rate from
		// this point onward.
		if (m_playing && m_basePositionSet.load(std::memory_order_relaxed))
		{
			m_basePositionNs.store(getCurrentPositionNs(), std::memory_order_relaxed);
			m_playStartTime = std::chrono::steady_clock::now();
		}
		m_rate.store(rate, std::memory_order_relaxed);
		return true;
	}

	bool setPosition(int64_t position) override
	{
		RIALTO_SIM_LOG("setPosition: position=%ld", static_cast<long>(position));
		m_basePositionNs.store(position, std::memory_order_relaxed);
		m_basePositionSet.store(true, std::memory_order_relaxed);
		m_playStartTime = std::chrono::steady_clock::now();
		return true;
	}

	bool getPosition(int64_t &position) override
	{
		position = getCurrentPositionNs();
		return true;
	}

	bool getStats(int32_t, uint64_t &renderedFrames,
		uint64_t &droppedFrames) override
	{
		renderedFrames = 100;
		droppedFrames = 0;
		return true;
	}

	bool setImmediateOutput(int32_t, bool) override { return true; }
	bool getImmediateOutput(int32_t, bool &io) override { io = false; return true; }

	bool setVideoWindow(uint32_t x, uint32_t y,
		uint32_t width, uint32_t height) override
	{
		RIALTO_SIM_LOG("setVideoWindow: x=%u y=%u w=%u h=%u",
			x, y, width, height);
		return true;
	}

	bool haveData(MediaSourceStatus status,
		uint32_t needDataRequestId) override
	{
		RIALTO_SIM_LOG("haveData: status=%d requestId=%u",
			static_cast<int>(status), needDataRequestId);

		// Resolve which source this response belongs to.
		int32_t sourceId = -1;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			auto it = m_requestIdToSource.find(needDataRequestId);
			if (it != m_requestIdToSource.end())
			{
				sourceId = it->second;
				m_requestIdToSource.erase(it);
			}
		}

		// Schedule the next needData even while paused: a real
		// GStreamer/Rialto pipeline accepts data in PAUSED state (appSrc
		// queues continue to buffer).  Without this, an inject thread
		// waiting in injectOneSample() for hasPending can block forever
		// after a seek arrives shortly after EOS, because StopInjectLoop
		// (called during TeardownStream) hangs waiting for inject threads
		// that will never exit, and Flush/invalidateGeneration is never
		// reached.  Guard only against stop, not against pause.
		if (status == MediaSourceStatus::OK &&
			!m_stopRequested.load(std::memory_order_relaxed))
		{
			scheduleNextNeedData();
		}
		else if (status == MediaSourceStatus::EOS)
		{
			// An EOS satisfies the play-readiness requirement for this
			// (non-subtitle) source.
			markSourceReadyForPlay(sourceId);
			maybeStartPlayback();

			bool allNonSubtitleSourcesEos = false;
			{
				std::lock_guard<std::mutex> lock(m_trackMutex);
				auto typeIt = m_sourceTypes.find(sourceId);
				if (typeIt != m_sourceTypes.end() &&
					typeIt->second != MediaSourceType::SUBTITLE)
				{
					m_eosSources.insert(sourceId);
				}
				allNonSubtitleSourcesEos = allNonSubtitleSourcesEosLocked();
				m_eosSourceCount.store(
					static_cast<int>(m_eosSources.size()),
					std::memory_order_relaxed);
			}

			if (allNonSubtitleSourcesEos &&
				!m_eosNotified.exchange(true, std::memory_order_relaxed))
			{
				// All non-subtitle sources EOS'd — delay END_OF_STREAM
				// to model the real pipeline's drain time (renderer must
				// play out buffered frames before signalling EOS).
				std::thread([this]() {
					using namespace std::chrono;
					constexpr auto kMinDrainTime = seconds(6);
					auto elapsed = steady_clock::now() - m_playStartTime;
					if (elapsed < kMinDrainTime)
					{
						auto remaining = kMinDrainTime - elapsed;
						RIALTO_SIM_LOG("draining: waiting %ld ms before END_OF_STREAM",
							duration_cast<milliseconds>(remaining).count());
						std::this_thread::sleep_for(remaining);
					}
					if (m_stopRequested.load(std::memory_order_relaxed))
					{
						return;
					}
					// Re-validate EOS after draining: new media may have
					// arrived during the drain window (e.g. trickplay
					// injection resuming), which clears the EOS tracking.
					// Firing END_OF_STREAM in that case would signal a
					// spurious/premature EOS to the player.
					{
						std::lock_guard<std::mutex> lock(m_trackMutex);
						if (!m_eosNotified.load(std::memory_order_relaxed) ||
							!allNonSubtitleSourcesEosLocked())
						{
							RIALTO_SIM_LOG(
								"END_OF_STREAM cancelled: new media arrived during drain");
							return;
						}
					}
					if (auto client = m_client.lock())
					{
						RIALTO_SIM_LOG("END_OF_STREAM (after drain)");
						client->notifyPlaybackState(
							firebolt::rialto::PlaybackState::END_OF_STREAM);
					}
				}).detach();
			}
		}
		return true;
	}

	AddSegmentStatus addSegment(uint32_t needDataRequestId,
		const std::unique_ptr<MediaSegment> &mediaSegment) override
	{
		// Capture the first segment PTS as the base position for
		// simulated playback.  Subsequent segments do NOT override —
		// position advances by elapsed wall-clock time from this base.
		if (mediaSegment && !m_basePositionSet.load(std::memory_order_relaxed))
		{
			int64_t pts = mediaSegment->getTimeStamp();
			if (pts > 0)
			{
				m_basePositionNs.store(pts, std::memory_order_relaxed);
				m_basePositionSet.store(true, std::memory_order_relaxed);
				m_playStartTime = std::chrono::steady_clock::now();
			}
		}

		// Accumulate injected duration per source to gate the
		// transition to PLAYING (subtitle tracks are ignored).
		if (mediaSegment)
		{
			accumulateInjectedDuration(mediaSegment->getId(),
				mediaSegment->getDuration());
			maybeStartPlayback();
		}
		return AddSegmentStatus::OK;
	}

	bool renderFrame() override
	{
		RIALTO_SIM_LOG("renderFrame");
		return true;
	}

	bool setVolume(double targetVolume, uint32_t volumeDuration,
		EaseType easeType) override
	{
		RIALTO_SIM_LOG("setVolume: vol=%f dur=%u", targetVolume, volumeDuration);
		return true;
	}

	bool getVolume(double &currentVolume) override
	{
		currentVolume = 1.0;
		return true;
	}

	bool setMute(int32_t sourceId, bool mute) override
	{
		RIALTO_SIM_LOG("setMute: sourceId=%d mute=%d", sourceId, mute);
		return true;
	}

	bool getMute(int32_t, bool &mute) override { mute = false; return true; }
	bool setTextTrackIdentifier(const std::string &) override { return true; }
	bool getTextTrackIdentifier(std::string &) override { return true; }
	bool setLowLatency(bool) override { return true; }
	bool setSync(bool) override { return true; }
	bool getSync(bool &sync) override { sync = true; return true; }
	bool setSyncOff(bool) override { return true; }
	bool setStreamSyncMode(int32_t, int32_t) override { return true; }
	bool getStreamSyncMode(int32_t &mode) override { mode = 0; return true; }

	bool flush(int32_t sourceId, bool resetTime, bool &async) override
	{
		RIALTO_SIM_LOG("flush: sourceId=%d resetTime=%d", sourceId, resetTime);
		async = false;
		// Reset position tracking for the next segment
		m_basePositionSet.store(false, std::memory_order_relaxed);
		m_eosSourceCount.store(0, std::memory_order_relaxed);
		m_eosNotified.store(false, std::memory_order_relaxed);

		// A flushed source must re-buffer before the pipeline returns to
		// PLAYING, so clear its readiness/injected-duration tracking and
		// force the next play() to re-evaluate the transition.
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			m_injectedDurationNs[sourceId] = 0;
			m_readySources.erase(sourceId);
			m_eosSources.clear();
		}
		m_playing = false;

		// Notify the client that the flush completed so AampRialtoPlayer
		// calls setSourcePosition (matching real Rialto server behavior).
		if (auto client = m_client.lock())
		{
			client->notifySourceFlushed(sourceId);
		}
		return true;
	}

	bool setSourcePosition(int32_t sourceId, int64_t position,
		bool resetTime, double appliedRate,
		uint64_t stopPosition) override
	{
		RIALTO_SIM_LOG("setSourcePosition: sourceId=%d position=%ld resetTime=%d",
			sourceId, static_cast<long>(position), resetTime);
		if (position >= 0)
		{
			m_basePositionNs.store(position, std::memory_order_relaxed);
			m_basePositionSet.store(true, std::memory_order_relaxed);
			m_playStartTime = std::chrono::steady_clock::now();
		}
		return true;
	}

	bool setSubtitleOffset(int32_t, int64_t) override { return true; }
	bool processAudioGap(int64_t, uint32_t, int64_t, bool) override { return true; }
	bool setBufferingLimit(uint32_t) override { return true; }
	bool getBufferingLimit(uint32_t &lim) override { lim = kInvalidLimitBuffering; return true; }
	bool setUseBuffering(bool) override { return true; }
	bool getUseBuffering(bool &ub) override { ub = false; return true; }

	bool switchSource(
		const std::unique_ptr<MediaSource> &source) override
	{
		RIALTO_SIM_LOG("switchSource: type=%d",
			static_cast<int>(source->getType()));
		return true;
	}

	bool getDuration(int64_t &duration) override
	{
		duration = 0;
		return true;
	}

private:
	int64_t getCurrentPositionNs() const
	{
		int64_t base = m_basePositionNs.load(std::memory_order_relaxed);
		if (!m_playing)
		{
			return base;
		}
		auto elapsed = std::chrono::steady_clock::now() - m_playStartTime;
		auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
		// Position increases at 1x regardless of rate; the Rialto player
		// applies the rate multiplier in GetPositionMilliseconds().
		return base + static_cast<int64_t>(elapsedNs);
	}

	// Largest amount of injected-but-not-yet-played media held across the
	// non-subtitle sources, in the pipeline (restamped) timebase.  Used to
	// model buffer-fill backpressure: injected duration accumulates as
	// segments arrive, while playback drains it at 1x wall-clock time.
	// Backpressure is only meaningful while the pipeline is PLAYING: during
	// preroll/seek/flush the pipeline buffers freely to (re)reach the play
	// threshold, so report no backpressure when not playing to avoid
	// starving the pipeline (and deadlocking, since buffered would never
	// drain while paused).
	// Caller must hold m_trackMutex.
	int64_t maxBufferedAheadNsLocked() const
	{
		if (!m_playing || !m_basePositionSet.load(std::memory_order_relaxed))
		{
			return 0;
		}
		int64_t playedNs = 0;
		{
			auto elapsed = std::chrono::steady_clock::now() - m_playStartTime;
			playedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
				elapsed).count();
		}
		int64_t maxBuffered = 0;
		for (const auto &entry : m_sourceTypes)
		{
			if (entry.second == MediaSourceType::SUBTITLE)
			{
				continue;
			}
			auto it = m_injectedDurationNs.find(entry.first);
			if (it == m_injectedDurationNs.end())
			{
				continue;
			}
			int64_t buffered = it->second - playedNs;
			if (buffered > maxBuffered)
			{
				maxBuffered = buffered;
			}
		}
		return maxBuffered;
	}

	void startNeedDataPump()
	{
		auto client = m_client.lock();
		if (!client)
		{
			return;
		}
		std::vector<std::pair<int32_t, uint32_t>> sends;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			for (int32_t sourceId : m_attachedSources)
			{
				uint32_t reqId = m_needDataRequestId++;
				m_requestIdToSource[reqId] = sourceId;
				sends.emplace_back(sourceId, reqId);
			}
		}
		for (const auto &send : sends)
		{
			RIALTO_SIM_LOG("notifyNeedMediaData: sourceId=%d requestId=%u",
				send.first, send.second);
			client->notifyNeedMediaData(send.first, kNeedDataFrameCount, send.second, nullptr);
		}
	}

	void scheduleNextNeedData()
	{
		std::thread([this]() {
			// Pace data requests to model pipeline buffer backpressure:
			// wait until the buffered (injected-but-not-played) media drops
			// below the high-water mark before asking for more.  This keeps
			// injection at ~real time instead of draining AAMP's local TSB
			// as fast as fragments can be produced.
			for (;;)
			{
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					return;
				}
				int64_t buffered = 0;
				{
					std::lock_guard<std::mutex> lock(m_trackMutex);
					buffered = maxBufferedAheadNsLocked();
				}
				RIALTO_SIM_LOG("DBG scheduleNextNeedData buffered=%lld high=%lld playing=%d baseSet=%d",
					static_cast<long long>(buffered),
					static_cast<long long>(kBufferHighWaterNs),
					m_playing.load(std::memory_order_relaxed) ? 1 : 0,
					m_basePositionSet.load(std::memory_order_relaxed) ? 1 : 0);
				if (buffered < kBufferHighWaterNs)
				{
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			auto client = m_client.lock();
			if (!client)
			{
				return;
			}
			std::vector<std::pair<int32_t, uint32_t>> sends;
			{
				std::lock_guard<std::mutex> lock(m_trackMutex);
				for (int32_t sourceId : m_attachedSources)
				{
					uint32_t reqId = m_needDataRequestId++;
					m_requestIdToSource[reqId] = sourceId;
					sends.emplace_back(sourceId, reqId);
				}
			}
			for (const auto &send : sends)
			{
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					break;
				}
				client->notifyNeedMediaData(send.first, kNeedDataFrameCount, send.second, nullptr);
			}
		}).detach();
	}

	void accumulateInjectedDuration(int32_t sourceId, int64_t durationNs)
	{
		std::lock_guard<std::mutex> lock(m_trackMutex);
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt == m_sourceTypes.end() ||
			typeIt->second == MediaSourceType::SUBTITLE)
		{
			return;
		}
		if (durationNs > 0)
		{
			m_injectedDurationNs[sourceId] += durationNs;
			m_eosSources.erase(sourceId);
			m_eosSourceCount.store(
				static_cast<int>(m_eosSources.size()),
				std::memory_order_relaxed);
			m_eosNotified.store(false, std::memory_order_relaxed);
		}
		RIALTO_SIM_LOG("DBG accumulate sourceId=%d durationNs=%lld totalNs=%lld",
			sourceId, static_cast<long long>(durationNs),
			static_cast<long long>(m_injectedDurationNs[sourceId]));
		if (m_injectedDurationNs[sourceId] >= kMinPlayDurationNs)
		{
			m_readySources.insert(sourceId);
		}
	}

	void markSourceReadyForPlay(int32_t sourceId)
	{
		std::lock_guard<std::mutex> lock(m_trackMutex);
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt != m_sourceTypes.end() &&
			typeIt->second != MediaSourceType::SUBTITLE)
		{
			m_readySources.insert(sourceId);
		}
	}

	// Caller must hold m_trackMutex.
	bool allNonSubtitleSourcesReadyLocked() const
	{
		if (!m_allSourcesAttached)
		{
			return false;
		}
		bool haveNonSubtitle = false;
		for (const auto &entry : m_sourceTypes)
		{
			if (entry.second == MediaSourceType::SUBTITLE)
			{
				continue;
			}
			haveNonSubtitle = true;
			if (m_readySources.find(entry.first) == m_readySources.end())
			{
				return false;
			}
		}
		return haveNonSubtitle;
	}

	// Caller must hold m_trackMutex.
	bool allNonSubtitleSourcesEosLocked() const
	{
		bool haveNonSubtitle = false;
		for (const auto &entry : m_sourceTypes)
		{
			if (entry.second == MediaSourceType::SUBTITLE)
			{
				continue;
			}
			haveNonSubtitle = true;
			if (m_eosSources.find(entry.first) == m_eosSources.end())
			{
				return false;
			}
		}
		return haveNonSubtitle;
	}

	void maybeStartPlayback()
	{
		bool startPlaying = false;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			if (m_playRequested.load(std::memory_order_relaxed) &&
				!m_playing.load(std::memory_order_relaxed) &&
				allNonSubtitleSourcesReadyLocked())
			{
				// Mark playing under the lock to prevent another
				// callback thread from also transitioning.
				m_playing.store(true, std::memory_order_relaxed);
				startPlaying = true;
			}
		}
		if (startPlaying)
		{
			m_playStartTime = std::chrono::steady_clock::now();
			if (auto client = m_client.lock())
			{
				RIALTO_SIM_LOG("transition to PLAYING "
					"(per-track inject threshold reached)");
				client->notifyPlaybackState(PlaybackState::PLAYING);
			}
			startPositionThread();
		}
	}

	void startPositionThread()
	{
		if (m_positionThread.joinable())
		{
			return;
		}

		m_stopRequested.store(false, std::memory_order_relaxed);
		m_positionThread = std::thread([this]() {
			while (!m_stopRequested.load(std::memory_order_relaxed))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					break;
				}
				if (!m_playing)
				{
					continue;
				}

				int64_t pos = getCurrentPositionNs();
				if (auto client = m_client.lock())
				{
					client->notifyPosition(pos);
				}
			}
		});
	}

	void stopThreads()
	{
		m_stopRequested.store(true, std::memory_order_relaxed);
		if (m_positionThread.joinable())
		{
			m_positionThread.join();
		}
	}

	std::weak_ptr<IMediaPipelineClient> m_client;
	std::atomic<int32_t> m_nextSourceId;
	std::vector<int32_t> m_attachedSources;
	bool m_loaded;
	bool m_allSourcesAttached;
	std::atomic<bool> m_playing;
	std::atomic<bool> m_playRequested;
	std::atomic<double> m_rate;
	std::atomic<int64_t> m_basePositionNs;
	std::atomic<bool> m_basePositionSet;
	std::chrono::steady_clock::time_point m_playStartTime;
	std::atomic<uint32_t> m_needDataRequestId;
	std::atomic<bool> m_stopRequested;
	std::atomic<int> m_eosSourceCount;
	std::atomic<bool> m_eosNotified;
	std::thread m_positionThread;
	bool m_playbackRateEnabled;
	mutable std::mutex m_trackMutex;
	std::map<int32_t, MediaSourceType> m_sourceTypes;
	std::map<int32_t, int64_t> m_injectedDurationNs;
	std::set<int32_t> m_readySources;
	std::set<int32_t> m_eosSources;
	std::map<uint32_t, int32_t> m_requestIdToSource;
};

// ===========================================================================
// SimMediaPipelineFactory
// ===========================================================================

class SimMediaPipelineFactory : public IMediaPipelineFactory
{
public:
	std::unique_ptr<IMediaPipeline> createMediaPipeline(
		std::weak_ptr<IMediaPipelineClient> client,
		const VideoRequirements &videoRequirements) const override
	{
		RIALTO_SIM_LOG("createMediaPipeline");
		return std::make_unique<SimMediaPipeline>(client, videoRequirements);
	}
};

// ===========================================================================
// SimControl - immediately reports RUNNING to the registered client
// ===========================================================================

class SimControl : public IControl
{
public:
	bool registerClient(std::weak_ptr<IControlClient> client,
		ApplicationState &appState) override
	{
		appState = ApplicationState::RUNNING;
		RIALTO_SIM_LOG("IControl::registerClient: state=RUNNING");
		return true;
	}
};

class SimControlFactory : public IControlFactory
{
public:
	std::shared_ptr<IControl> createControl() const override
	{
		RIALTO_SIM_LOG("createControl");
		return std::make_shared<SimControl>();
	}
};

// ===========================================================================
// SimClientLogControl - no-op log control
// ===========================================================================

class SimClientLogControl : public IClientLogControl
{
public:
	bool registerLogHandler(const std::shared_ptr<IClientLogHandler> &,
		bool) override
	{
		return true;
	}
};

class SimClientLogControlFactory : public IClientLogControlFactory
{
public:
	IClientLogControl &createClientLogControl() override
	{
		static SimClientLogControl instance;
		return instance;
	}
};

// ===========================================================================
// Factory singletons
// ===========================================================================

std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
	RIALTO_SIM_LOG("IMediaPipelineFactory::createFactory");
	static auto factory = std::make_shared<SimMediaPipelineFactory>();
	return factory;
}

std::shared_ptr<IClientLogControlFactory> IClientLogControlFactory::createFactory()
{
	static auto factory = std::make_shared<SimClientLogControlFactory>();
	return factory;
}

std::shared_ptr<IControlFactory> IControlFactory::createFactory()
{
	RIALTO_SIM_LOG("IControlFactory::createFactory");
	static auto factory = std::make_shared<SimControlFactory>();
	return factory;
}

// ===========================================================================
// SimMediaPipelineCapabilitiesFactory - minimal factory returning null
// ===========================================================================

class SimMediaPipelineCapabilitiesFactory : public IMediaPipelineCapabilitiesFactory
{
public:
	std::unique_ptr<IMediaPipelineCapabilities>
	createMediaPipelineCapabilities() const override
	{
		// Return nullptr - capabilities queries are optional for simulator
		return nullptr;
	}
};

// ===========================================================================
// SimMediaKeysFactory - minimal factory returning null
// ===========================================================================

class SimMediaKeysFactory : public IMediaKeysFactory
{
public:
	std::unique_ptr<IMediaKeys> createMediaKeys(
		const std::string &keySystem) const override
	{
		// Return nullptr - full DRM support not needed in simulator
		// Production code already handles null IMediaKeys gracefully
		RIALTO_SIM_LOG("createMediaKeys: keySystem=%s (returning null)", 
			keySystem.c_str());
		return nullptr;
	}
};

// ===========================================================================
// Additional factory functions
// ===========================================================================

std::shared_ptr<IMediaPipelineCapabilitiesFactory>
IMediaPipelineCapabilitiesFactory::createFactory()
{
	RIALTO_SIM_LOG("IMediaPipelineCapabilitiesFactory::createFactory");
	static auto factory = std::make_shared<SimMediaPipelineCapabilitiesFactory>();
	return factory;
}

std::shared_ptr<IMediaKeysFactory> IMediaKeysFactory::createFactory()
{
	RIALTO_SIM_LOG("IMediaKeysFactory::createFactory");
	static auto factory = std::make_shared<SimMediaKeysFactory>();
	return factory;
}

// ===========================================================================
// MediaSegment::copy - non-inline members declared in IMediaPipeline.h
// ===========================================================================

void IMediaPipeline::MediaSegment::copy(const MediaSegment &other)
{
	m_sourceId = other.m_sourceId;
	m_type = other.m_type;
	m_data = other.m_data;
	m_dataLength = other.m_dataLength;
	m_timeStamp = other.m_timeStamp;
	m_duration = other.m_duration;
	m_codecData = other.m_codecData;
	m_extraData = other.m_extraData;
	m_encrypted = other.m_encrypted;
	m_mediaKeySessionId = other.m_mediaKeySessionId;
	m_keyId = other.m_keyId;
	m_initVector = other.m_initVector;
	m_subSamples = other.m_subSamples;
	m_initWithLast15 = other.m_initWithLast15;
	m_alignment = other.m_alignment;
	m_cipherMode = other.m_cipherMode;
	m_crypt = other.m_crypt;
	m_skip = other.m_skip;
	m_encryptionPatternSet = other.m_encryptionPatternSet;
	m_displayOffset = other.m_displayOffset;
}

void IMediaPipeline::MediaSegmentAudio::copy(const MediaSegmentAudio &other)
{
	MediaSegment::copy(other);
	m_sampleRate = other.m_sampleRate;
	m_numberOfChannels = other.m_numberOfChannels;
	m_clippingStart = other.m_clippingStart;
	m_clippingEnd = other.m_clippingEnd;
}

void IMediaPipeline::MediaSegmentVideo::copy(const MediaSegmentVideo &other)
{
	MediaSegment::copy(other);
	m_width = other.m_width;
	m_height = other.m_height;
	m_frameRate = other.m_frameRate;
}

} // namespace firebolt::rialto
