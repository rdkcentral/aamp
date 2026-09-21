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
#include <cstdint>
#include <cstdarg>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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
static std::string LogPreamble(const char *function, int line)
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	const auto milliseconds = std::chrono::duration_cast<
		std::chrono::milliseconds>(now).count();
	const char *format = "%lld.%03lld: [RialtoSim][%s][%d]";

	auto size = std::snprintf(nullptr,0, format,
		static_cast<long long>(milliseconds / 1000),
		static_cast<long long>(milliseconds % 1000),
		function,
		line);
	if (size <= 0)
 	{
 		return "Unknown error";
 	}
	std::string preamble(size+1, '\0');
	std::sprintf(&preamble[0], format,
		static_cast<long long>(milliseconds / 1000),
		static_cast<long long>(milliseconds % 1000),
		function,
		line);
	preamble.pop_back(); //Remove the c string termination char. Not needed for std::string
	return preamble;
}

static void RialtoSimLog(const std::string &preamble, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

static void RialtoSimLog(const std::string &preamble, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	auto len = std::vsnprintf(nullptr, 0, fmt, args);
	va_end(args);
	std::string s2("snprintf Error");
	if (len >= 0)
	{
		s2.resize(len + 1);
		va_start(args, fmt);
		std::vsnprintf(&s2[0], len + 1, fmt, args);
		va_end(args);
		// We initially allocated one extra character for the null terminator, but std::string doesn't need it.
		s2.resize(len);
	}

	fputs((preamble + s2).c_str(), stderr);
}
// If we use fprintf and formatting as we write to stderr then the line gets interleaved
// with AAMP logging before completely written out. To fix this write the entire log line
// to a buffer first, then output it in one go to reduce chance of interleaving.
// Not completely thread-safe since arguments can change between the two snprintf calls.
#define RIALTO_SIM_LOG(fmt, ...) \
    RialtoSimLog(LogPreamble(__func__, __LINE__), fmt "\n", ##__VA_ARGS__) \

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
constexpr int64_t kBufferHighWaterNs = 40000000000LL; // 40 seconds

// Number of frames requested per needMediaData.  A real pipeline only asks
// for as much data as its buffers can currently accept; modelling that with
// a small per-request count (combined with kBufferHighWaterNs pacing) keeps
// AAMP from draining many fragments from its local TSB in a single batch.
// A large value here would let one needData admit an unbounded burst,
// defeating the backpressure model.
constexpr unsigned int kNeedDataFrameCount = 24;

// Slack applied when clamping the shared clock to the slowest attached
// track's horizon (see refreshMasterClockLocked()). Covers two distinct,
// legitimate cases without a flat "disable the clamp" escape hatch:
//   - ordinary A/V injection-cadence skew during healthy playback (tracks
//     are rarely injected in perfect lockstep, so a zero-slack clamp
//     falsely treats normal skew as a stall);
//   - the live-edge manifest-refresh gap (AAMP-CONFIG-2033_live): segments
//     become available in bursts on the backend's own ~1.9s grid, not one
//     per AAMP poll, and measured worst-case gaps between successive
//     segments reached ~2.72s (see the manifest-poll analysis for that
//     test) before content resumed.
// 3000ms gives headroom over that measured 2.72s worst case. This is
// deliberately much shorter than a genuine stall (AAMP-BUFFER-6002_UnderflowMonitor
// delays fragments by 9s) so a real stall still holds the reported clock
// back for several seconds after the tolerance is exhausted - long enough
// for AampUnderflowMonitor's deadline to expire.
constexpr int64_t kClockClampToleranceNs = 3000000000LL; // 3000ms

// One queued unit of media: the fields the master-clock/backpressure model
// needs from a MediaSegment. Ingestion order for video is decode order, not
// presentation order (see ComparePts below); audio/subtitle ingestion order
// is already presentation order (no B-frame-style reordering for audio).
struct QueuedSample
{
	int64_t ptsNs;
	int64_t durationNs;
};

// Orders queued video samples by presentation timestamp rather than arrival
// (decode) order, modelling a decoder's reorder buffer/frame store.
struct ComparePts
{
	bool operator()(const QueuedSample &a, const QueuedSample &b) const
	{
		return a.ptsNs < b.ptsNs;
	}
};

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
		, m_generation(0)
		, m_stopRequested(false)
		, m_eosSourceCount(0)
		, m_eosNotified(false)
		, m_eosDrainGeneration(0)
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
			// Audio is always the master (A/V sync) track when present; video
			// is only master if no audio ever attaches.
			if (type == MediaSourceType::AUDIO)
			{
				m_masterSourceId = id;
			}
			else if (type == MediaSourceType::VIDEO && !m_masterSourceId)
			{
				m_masterSourceId = id;
			}
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
		pausePlayback();
		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::PAUSED);
		}
		return true;
	}

	bool stop() override
	{
		RIALTO_SIM_LOG("stop");
		pausePlayback();
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
		// Snapshot the current position before changing rate so that
		// subsequent elapsed-time calculations use the new rate from
		// this point onward.
		if (m_playing.load(std::memory_order_relaxed))
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			refreshMasterClockLocked();
		}
		m_rate.store(rate, std::memory_order_relaxed);
		if (!m_playbackRateEnabled)
		{
			RIALTO_SIM_LOG("setPlaybackRate: rate simulation disabled (set RIALTO_SIM_ENABLE_PLAYBACK_RATE=1 to enable)");
			return false;
		}
		return true;
	}

	bool setPosition(int64_t position) override
	{
		RIALTO_SIM_LOG("setPosition: position=%ld", static_cast<long>(position));

		// Real Rialto reports SEEKING as soon as the seek is accepted, i.e.
		// before the flush/reposition work below has happened, then does
		// that work, then reports SEEK_DONE once it has completed.  Send
		// SEEKING first so observers see the same ordering.
		auto client = m_client.lock();
		if (client)
		{
			RIALTO_SIM_LOG("setPosition: notifying SEEKING");
			client->notifyPlaybackState(PlaybackState::SEEKING);
		}

		// setPosition() models a pipeline-wide flushing seek: a real Rialto
		// server treats it as an implicit Flush() of every source followed
		// by setSourcePosition() to the new position.  Reset the same
		// play-readiness/EOS/backpressure state that flush() resets (see
		// flush() above for why every source, not just one, must
		// re-buffer), so a stale ready/EOS source from before the seek
		// can't make maybeStartPlayback() fire PLAYING before the
		// post-seek data has actually arrived.
		m_eosSourceCount.store(0, std::memory_order_relaxed);
		m_eosNotified.store(false, std::memory_order_relaxed);
		m_playing = false;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			m_fifoQueue.clear();
			m_videoQueue.clear();
			m_totalEnqueuedDurationNs.clear();
			m_trackHorizonNs.clear();
			m_underflowNotifiedSources.clear();
			m_pendingUnderflowNotifications.clear();
			m_readySources.clear();
			m_eosSources.clear();
			m_pendingSegments.clear();

			// As with flush(), any needData request issued before this seek
			// is now stale.  Bump the generation and drop the outstanding
			// request bookkeeping so a late haveData() for one of them is
			// ignored instead of being applied to the post-seek state.
			m_generation.fetch_add(1, std::memory_order_relaxed);
			m_requestIdToSource.clear();

			// Apply the new position, as setSourcePosition() would after a flush.
			m_masterClockAnchorNs = position;
			m_horizonFloorNs = position;
			m_masterClockAnchorWallTime = std::chrono::steady_clock::now();
		}

		// Restart the needData pump immediately, as flush() does, instead of
		// waiting on requests that have just been invalidated above.
		if (m_allSourcesAttached)
		{
			startNeedDataPump();
		}

		m_basePositionNs.store(position, std::memory_order_relaxed);
		m_basePositionSet.store(true, std::memory_order_relaxed);

		// AampRialtoPlayer's Flush() blocks on SEEK_DONE (via its state
		// machine) to restore state and commit the pending rate/position,
		// so this notification must be sent for every setPosition() call —
		// not just flush-initiated ones — or that wait never completes.
		if (client)
		{
			RIALTO_SIM_LOG("setPosition: notifying SEEK_DONE");
			client->notifyPlaybackState(PlaybackState::SEEK_DONE);
		}
		return true;
	}

	bool getPosition(int64_t &position) override
	{
		position = refreshAndGetPositionNs();
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

		// Resolve which source this response belongs to, and pick up any
		// segment data staged by addSegment() for this request.
		int32_t sourceId = -1;
		bool requestKnown = false;
		uint64_t requestGeneration = 0;
		PendingSegmentData pending;
		bool havePending = false;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			auto it = m_requestIdToSource.find(needDataRequestId);
			if (it != m_requestIdToSource.end())
			{
				sourceId = it->second.sourceId;
				requestGeneration = it->second.generation;
				requestKnown = true;
				m_requestIdToSource.erase(it);
			}
			auto pendingIt = m_pendingSegments.find(needDataRequestId);
			if (pendingIt != m_pendingSegments.end())
			{
				pending = pendingIt->second;
				havePending = true;
				m_pendingSegments.erase(pendingIt);
			}
		}

		// A flush()/setPosition() that happened after this request was issued
		// bumps the generation and abandons every outstanding requestId: such
		// a fresh pump has already been sent, so a (possibly late-arriving)
		// response for the old requestId must not be applied to playback
		// state or trigger another round of needData.  Any segments staged
		// against it were already discarded above.
		if (!requestKnown || requestGeneration != m_generation.load(std::memory_order_relaxed))
		{
			RIALTO_SIM_LOG("haveData: ignoring stale/unknown requestId=%u",
				needDataRequestId);
			return true;
		}

		// Apply the effect of the segments addSegment() staged for this
		// request now that the client has confirmed the request is
		// complete.  Real Rialto only considers a request's data delivered
		// once haveData() is called for it, so the base-position update and
		// the duration accounting that gates PLAYING must happen here, not
		// eagerly in addSegment().
		if (havePending)
		{
			if (sourceId < 0)
			{
				sourceId = pending.sourceId;
			}
			if (!m_basePositionSet.load(std::memory_order_relaxed))
			{
				// Guard against a zero/negative PTS (treated as "not set"),
				// matching the pre-rework firstTimeStampNs behaviour: scan
				// for the first genuinely positive PTS in this batch rather
				// than blindly taking the first sample.
				for (const auto &sample : pending.samples)
				{
					if (sample.ptsNs > 0)
					{
						m_basePositionNs.store(sample.ptsNs, std::memory_order_relaxed);
						m_basePositionSet.store(true, std::memory_order_relaxed);
						break;
					}
				}
			}
			if (pending.sourceId >= 0 && !pending.samples.empty())
			{
				commitPendingSamples(pending.sourceId, pending.samples);
			}
			maybeStartPlayback();
		}

		// Schedule the next needData for this specific source even while
		// paused: a real GStreamer/Rialto pipeline accepts data in PAUSED
		// state (appSrc queues continue to buffer).  Without this, an inject
		// thread waiting in injectOneSample() for hasPending can block forever
		// after a seek arrives shortly after EOS, because StopInjectLoop
		// (called during TeardownStream) hangs waiting for inject threads
		// that will never exit, and Flush/unblockInjection is never
		// reached.  Guard only against stop, not against pause.
		if (status == MediaSourceStatus::OK &&
			!m_stopRequested.load(std::memory_order_relaxed))
		{
			scheduleNextNeedData(sourceId);
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
				// Snapshot the maximum *unplayed backlog* (injected minus
				// already-played, per bufferedAheadNsLocked()) across
				// non-subtitle sources.  A real renderer must play out
				// buffered-but-not-yet-rendered frames before signalling
				// END_OF_STREAM, so the drain wait must cover at least this
				// many nanoseconds of elapsed wall time.
				int64_t maxBufferedAheadNs = 0;
				{
					std::lock_guard<std::mutex> lock(m_trackMutex);
					refreshMasterClockLocked();
					for (const auto &[srcId, injectedNs] : m_totalEnqueuedDurationNs)
					{
						auto typeIt = m_sourceTypes.find(srcId);
						if (typeIt != m_sourceTypes.end() &&
							typeIt->second != MediaSourceType::SUBTITLE)
						{
							int64_t bufferedNs = bufferedAheadNsLocked(srcId);
							if (!m_playing.load(std::memory_order_relaxed))
							{
								// bufferedAheadNsLocked() reports 0 while not PLAYING; for
								// EOS drain timing, fall back to injected duration so we
								// don’t under-wait when EOS is reached while paused/preroll.
								bufferedNs = injectedNs;
							}
							maxBufferedAheadNs = std::max(maxBufferedAheadNs, bufferedNs);
						}
					}
				}
				// All non-subtitle sources EOS'd — delay END_OF_STREAM
				// to model the real pipeline's drain time (renderer must
				// play out buffered frames before signalling EOS).
				startEosDrain(maxBufferedAheadNs);
			}
		}
		return true;
	}

	AddSegmentStatus addSegment(uint32_t needDataRequestId,
		const std::unique_ptr<MediaSegment> &mediaSegment) override
	{
		// Real Rialto does not consider a needData request's data delivered
		// until the client calls haveData() for that requestId — addSegment()
		// only stages the segment into the pipeline's shared buffer.  Mirror
		// that here: record the derived fields we need (source, accumulated
		// duration, first PTS) against the requestId, and apply their effect
		// on playback state in haveData() instead of immediately.  The
		// sample payload itself is never used by the simulator, so it is not
		// stored.
		if (mediaSegment)
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			// Only stage segments for requests that are still outstanding
			// and belong to the current generation.  After flush()/
			// setPosition() bumps m_generation and clears
			// m_requestIdToSource, any addSegment() for an old requestId
			// is stale: haveData() will never be called for it, so the
			// pending entry would accumulate without being consumed.
			auto reqIt = m_requestIdToSource.find(needDataRequestId);
			if (reqIt == m_requestIdToSource.end() ||
				reqIt->second.generation !=
					m_generation.load(std::memory_order_relaxed))
			{
				RIALTO_SIM_LOG(
					"addSegment: discarding stale segment for requestId=%u",
					needDataRequestId);
				return AddSegmentStatus::OK;
			}
			PendingSegmentData &pending = m_pendingSegments[needDataRequestId];
			pending.sourceId = mediaSegment->getId();
			pending.samples.push_back(QueuedSample{
				mediaSegment->getTimeStamp(), mediaSegment->getDuration()});
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

		// A flush always represents a pipeline-wide restart (seek or
		// trickplay): every non-subtitle source must re-buffer before the
		// pipeline returns to PLAYING.  Clearing only the flushed source
		// would leave stale readiness on other sources, which causes
		// play() — called by DirectRialto after each individual source
		// flush — to fire PLAYING immediately via the stale source.  That
		// triggers MonitorProgress before video has any injected frames,
		// producing a position-unchanged suppression that prevents the
		// first trickplay progress event from appearing.
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			m_fifoQueue.clear();
			m_videoQueue.clear();
			m_totalEnqueuedDurationNs.clear();
			m_trackHorizonNs.clear();
			m_underflowNotifiedSources.clear();
			m_pendingUnderflowNotifications.clear();
			m_readySources.clear();
			m_eosSources.clear();
			m_pendingSegments.clear();

			// Any needData request issued before this flush is now stale: a
			// real Rialto server abandons in-flight requests on a flushing
			// seek and issues fresh ones.  Bump the generation and drop the
			// bookkeeping for outstanding requests so a late haveData() for
			// one of them is ignored rather than applied to (or triggering
			// another data pump for) the post-flush state.
			m_generation.fetch_add(1, std::memory_order_relaxed);
			m_requestIdToSource.clear();
		}
		m_playing = false;

		// Restart the needData pump immediately so the client gets fresh
		// requests rather than waiting on ones that have just been
		// invalidated above.
		if (m_allSourcesAttached)
		{
			startNeedDataPump();
		}

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
			std::lock_guard<std::mutex> lock(m_trackMutex);
			m_basePositionNs.store(position, std::memory_order_relaxed);
			m_basePositionSet.store(true, std::memory_order_relaxed);
			m_masterClockAnchorNs = position;
			m_horizonFloorNs = position;
			m_masterClockAnchorWallTime = std::chrono::steady_clock::now();
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
	// Segment data staged by addSegment() for a given needDataRequestId,
	// consumed by haveData() once the client confirms that request is
	// complete.  The sample payload itself is never used by the simulator,
	// only the per-sample PTS/duration needed by the master-clock model.
	struct PendingSegmentData
	{
		int32_t sourceId = -1;
		std::vector<QueuedSample> samples;
	};

	// Bookkeeping for an outstanding notifyNeedMediaData() request, tagged
	// with the pump generation it was issued under (see m_generation).
	struct RequestInfo
	{
		int32_t sourceId = -1;
		uint64_t generation = 0;
	};

	// Commits every sample staged by addSegment() for a completed request:
	// enqueues it into the appropriate per-track structure (FIFO for audio/
	// subtitle, PTS-ordered reorder buffer for video), and updates the
	// running duration total (for the kMinPlayDurationNs gate) and horizon
	// (furthest presentation time we have real data for; see
	// refreshMasterClockLocked()).
	void commitPendingSamples(int32_t sourceId, const std::vector<QueuedSample> &samples)
	{
		std::lock_guard<std::mutex> lock(m_trackMutex);
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt == m_sourceTypes.end())
		{
			return;
		}
		const bool isSubtitle = (typeIt->second == MediaSourceType::SUBTITLE);
		for (const auto &sample : samples)
		{
			if (!isSubtitle)
			{
				if (typeIt->second == MediaSourceType::VIDEO)
				{
					m_videoQueue[sourceId].insert(sample);
				}
				else
				{
					m_fifoQueue[sourceId].push_back(sample);
				}
				m_totalEnqueuedDurationNs[sourceId] += sample.durationNs;
				m_trackHorizonNs[sourceId] = std::max(
					m_trackHorizonNs[sourceId], sample.ptsNs + sample.durationNs);
			}
		}
		if (!isSubtitle && !samples.empty())
		{
			m_eosSources.erase(sourceId);
			m_eosSourceCount.store(
				static_cast<int>(m_eosSources.size()), std::memory_order_relaxed);
			m_eosNotified.store(false, std::memory_order_relaxed);
			RIALTO_SIM_LOG("DBG commit sourceId=%d samples=%zu totalNs=%lld horizonNs=%lld",
				sourceId, samples.size(),
				static_cast<long long>(m_totalEnqueuedDurationNs[sourceId]),
				static_cast<long long>(m_trackHorizonNs[sourceId]));
			if (m_totalEnqueuedDurationNs[sourceId] >= kMinPlayDurationNs)
			{
				m_readySources.insert(sourceId);
			}
		}
	}

	// Pops the head of a track's queue while a real successor exists whose
	// PTS has cleared clockNs (3.2.2 of the design doc): a sample's own
	// duration only defines the horizon (used when it's still the tail with
	// no successor yet); once a successor is known, that successor's PTS is
	// the authoritative completion boundary for the previous sample.  The
	// current tail is never popped here.
	// Caller must hold m_trackMutex.
	void popMaturedSamplesLocked(int32_t sourceId, int64_t clockNs)
	{
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt == m_sourceTypes.end())
		{
			return;
		}
		if (typeIt->second == MediaSourceType::VIDEO)
		{
			auto &q = m_videoQueue[sourceId];
			while (q.size() >= 2)
			{
				auto second = std::next(q.begin());
				if (second->ptsNs > clockNs)
				{
					break;
				}
				q.erase(q.begin());
			}
		}
		else
		{
			auto &q = m_fifoQueue[sourceId];
			while (q.size() >= 2 && q[1].ptsNs <= clockNs)
			{
				q.pop_front();
			}
		}
	}

	// Computes the current master clock estimate as a wall-clock projection
	// from the last anchor, clamped to the slowest attached (non-subtitle,
	// non-EOS) track's horizon plus kClockClampToleranceNs - mirroring real
	// GStreamer's single shared pipeline clock, which cannot let one sink
	// outrun another's real progress (its buffering_timeout/queued_frames
	// mechanism pauses the whole pipeline when one decoder queue starves
	// while another keeps flowing - see InterfacePlayerRDK.cpp).  This
	// clamp is applied unconditionally (not gated on the master track's own
	// state) so the returned value is always monotonic non-decreasing:
	// AAMP's own PrivateInstanceAAMP::GetPositionMilliseconds() silently
	// discards and re-substitutes the previous position whenever it sees a
	// backward jump ("restore prev-pos as current-pos!!" in priv_aamp.cpp),
	// so a clamp that first lets the clock overshoot and then corrects it
	// backward gets permanently stuck at the overshot value client-side -
	// this happened when the clamp was gated on the master track's horizon
	// (see git history) and broke AAMP-BUFFER-6002_UnderflowMonitor even
	// though the simulator's own reported value was correct at each step.
	// kClockClampToleranceNs absorbs ordinary A/V injection-cadence skew and
	// the live-edge manifest-refresh gap (AAMP-CONFIG-2033_live measured up
	// to ~2.72s) without a flat "disable the clamp" branch, while staying
	// far shorter than a genuine stall (AAMP-BUFFER-6002_UnderflowMonitor's
	// deliberate 9s fragment delay), so a real stall still holds the clock
	// back long enough for AampUnderflowMonitor's deadline to expire.
	// Pops any now-matured samples from every track, and re-anchors.
	// Returns the (possibly clamped) master clock value in nanoseconds.
	//
	// Still detects per-track underflow: a track is starved once the clock
	// has passed the furthest point it has real data for.  This is a
	// narrower, purely informational signal - dispatched via
	// notifyBufferUnderflow(), mirroring real Rialto (see
	// AampRialtoMediaPipelineClient) - computed from the raw (unclamped)
	// projection, independently of whatever gets reported as position.
	// Newly-starved sources are queued in m_pendingUnderflowNotifications
	// for refreshAndGetPositionNs() to dispatch after releasing the lock;
	// debounced via m_underflowNotifiedSources so it fires once per stall,
	// not on every poll.  A source that has legitimately reached EOS is
	// finished, not starved (e.g. audio during video-only trickplay), and a
	// source with no horizon entry yet has never received any data - that's
	// preroll, not a stall - so both are excluded from the check.
	// Caller must hold m_trackMutex.
	int64_t refreshMasterClockLocked()
	{
		if (!m_playing.load(std::memory_order_relaxed) || !m_masterSourceId)
		{
			return m_basePositionNs.load(std::memory_order_relaxed);
		}
		auto elapsed = std::chrono::steady_clock::now() - m_masterClockAnchorWallTime;
		auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
		int64_t projectedClockNs = m_masterClockAnchorNs +
			static_cast<int64_t>(elapsedNs * m_rate.load(std::memory_order_relaxed));

		for (int32_t sourceId : m_attachedSources)
		{
			auto typeIt = m_sourceTypes.find(sourceId);
			if (typeIt == m_sourceTypes.end() || typeIt->second == MediaSourceType::SUBTITLE)
			{
				continue;
			}
			// A source that has legitimately reached EOS is finished, not
			// starved - don't report underflow for it (e.g. audio during
			// video-only trickplay must not spuriously fire on every trick
			// session once the video-driven clock passes its frozen horizon).
			if (m_eosSources.find(sourceId) != m_eosSources.end())
			{
				continue;
			}
			auto trackHorizonIt = m_trackHorizonNs.find(sourceId);
			if (trackHorizonIt == m_trackHorizonNs.end())
			{
				// Never received any data yet - that's preroll, not underflow.
				continue;
			}
			bool starved = projectedClockNs > std::max(trackHorizonIt->second, m_horizonFloorNs);
			bool alreadyNotified = m_underflowNotifiedSources.count(sourceId) > 0;
			if (starved && !alreadyNotified)
			{
				m_underflowNotifiedSources.insert(sourceId);
				m_pendingUnderflowNotifications.push_back(sourceId);
			}
			else if (!starved && alreadyNotified)
			{
				m_underflowNotifiedSources.erase(sourceId);
			}
		}

		// Unconditional clamp: the reported clock can never run further
		// ahead of the slowest active track's horizon than
		// kClockClampToleranceNs (see comment above this function).
		int64_t clockNs = projectedClockNs;
		for (int32_t sourceId : m_attachedSources)
		{
			auto typeIt = m_sourceTypes.find(sourceId);
			if (typeIt == m_sourceTypes.end() || typeIt->second == MediaSourceType::SUBTITLE)
			{
				continue;
			}
			if (m_eosSources.find(sourceId) != m_eosSources.end())
			{
				continue;
			}
			auto horizonIt = m_trackHorizonNs.find(sourceId);
			if (horizonIt == m_trackHorizonNs.end())
			{
				continue;
			}
			int64_t limitNs = std::max(horizonIt->second, m_horizonFloorNs) + kClockClampToleranceNs;
			clockNs = std::min(clockNs, limitNs);
		}

		for (int32_t sourceId : m_attachedSources)
		{
			popMaturedSamplesLocked(sourceId, clockNs);
		}

		m_masterClockAnchorNs = clockNs;
		m_masterClockAnchorWallTime = std::chrono::steady_clock::now();
		return clockNs;
	}

	// Convenience wrapper for call sites that don't already hold m_trackMutex.
	// Also dispatches any underflow notifications queued by
	// refreshMasterClockLocked(), outside the lock (the client callback must
	// not be invoked while holding m_trackMutex).
	int64_t refreshAndGetPositionNs()
	{
		std::vector<int32_t> toNotify;
		int64_t pos;
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			pos = refreshMasterClockLocked();
			toNotify.swap(m_pendingUnderflowNotifications);
		}
		if (!toNotify.empty())
		{
			if (auto client = m_client.lock())
			{
				for (int32_t sourceId : toNotify)
				{
					RIALTO_SIM_LOG("notifyBufferUnderflow: sourceId=%d", sourceId);
					client->notifyBufferUnderflow(sourceId);
				}
			}
		}
		return pos;
	}

	// Pause playback: snapshot the current position (to freeze it for
	// subsequent queries) and clear the play/playRequested state.
	void pausePlayback()
	{
		if (m_playing.load(std::memory_order_relaxed) && m_basePositionSet.load(std::memory_order_relaxed))
		{
			std::lock_guard<std::mutex> lock(m_trackMutex);
			int64_t currentPos = refreshMasterClockLocked();
			m_basePositionNs.store(currentPos, std::memory_order_relaxed);
		}
		m_playRequested.store(false, std::memory_order_relaxed);
		m_playing = false;
	}

	// Amount of injected-but-not-yet-played media held for a single
	// non-subtitle source, i.e. how far the track's horizon (furthest PTS +
	// duration it has real data for; see refreshMasterClockLocked()) is
	// ahead of the master clock.  popMaturedSamplesLocked() only ever pops
	// from the low-PTS end, so the horizon always equals the still-queued
	// maximum-PTS entry's own pts+duration - using it here (rather than
	// re-reading the queue tail) also means this reflects the tail sample's
	// full duration, not just its PTS.  Backpressure is only meaningful
	// while the pipeline is PLAYING: during preroll/seek/flush the pipeline
	// buffers freely to (re)reach the play threshold, so report no
	// backpressure when not playing to avoid starving the pipeline (and
	// deadlocking, since buffered would never drain while paused).  Subtitle
	// sources are never gated (see kMinPlayDurationNs).
	// Caller must hold m_trackMutex, and should have called
	// refreshMasterClockLocked() recently so m_masterClockAnchorNs is current.
	int64_t bufferedAheadNsLocked(int32_t sourceId) const
	{
		if (!m_playing.load(std::memory_order_relaxed) || !m_basePositionSet.load(std::memory_order_relaxed))
		{
			return 0;
		}
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt == m_sourceTypes.end() ||
			typeIt->second == MediaSourceType::SUBTITLE)
		{
			return 0;
		}
		auto horizonIt = m_trackHorizonNs.find(sourceId);
		if (horizonIt == m_trackHorizonNs.end())
		{
			return 0;
		}
		int64_t buffered = horizonIt->second - m_masterClockAnchorNs;
		return buffered > 0 ? buffered : 0;
	}

	void startEosDrain(int64_t maxBufferedAheadNs)
	{
		RIALTO_SIM_LOG("maxBufferedAheadNs %lld", static_cast<long long>(maxBufferedAheadNs));
		// A flush()/setPosition() clears m_eosNotified, so EOS can be reached
		// again and this can be called more than once.  Move-assigning onto a
		// still-joinable std::thread calls std::terminate, so retire the
		// previous drain first: bumping the generation makes it exit its wait
		// loop promptly, then join it.
		const uint64_t drainGeneration =
			m_eosDrainGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
		if (m_eosThread.joinable())
		{
			RIALTO_SIM_LOG("joining");
			m_eosThread.join();
		}
		m_eosThread = std::thread([this, maxBufferedAheadNs, drainGeneration]()
								  {
			using namespace std::chrono;
			constexpr int64_t kMinDrainNs = 6000000000LL; // 6 s
			// Gate on the user's play/pause intent (m_playRequested), not
			// m_playing: flush() clears m_playing on every internal
			// seek/trickplay cycle even though playback was never
			// actually paused, which would otherwise stall this
			// drain (and END_OF_STREAM) indefinitely during ff/rew.
			const int64_t waitUntilNs = std::max(kMinDrainNs, maxBufferedAheadNs);
			int64_t drainedWhilePlayingNs = 0;
			auto lastTick = steady_clock::now();
			for (;;)
			{
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					RIALTO_SIM_LOG("startEosDrain m_stopRequested");
					return;
				}
				if (m_eosDrainGeneration.load(std::memory_order_relaxed) !=
					drainGeneration)
				{
					RIALTO_SIM_LOG("startEosDrain superseded");
					return;
				}

				auto now = steady_clock::now();
				auto deltaNs = duration_cast<nanoseconds>(now - lastTick).count();
				lastTick = now;

				if (!m_playRequested.load(std::memory_order_relaxed))
				{
					std::this_thread::sleep_for(milliseconds(50));
					continue;
				}

				drainedWhilePlayingNs += deltaNs;
				if (drainedWhilePlayingNs >= waitUntilNs)
				{
					break;
				}

				const int64_t remainingNs = waitUntilNs - drainedWhilePlayingNs;
				const int64_t sleepNs = std::min<int64_t>(remainingNs, 200000000LL);
				std::this_thread::sleep_for(nanoseconds(sleepNs));
			}

			// Re-validate EOS after draining: new media may have arrived
			// during the drain window, which clears the EOS tracking.
			{
				std::lock_guard<std::mutex> lock(m_trackMutex);
				if (m_eosDrainGeneration.load(std::memory_order_relaxed) !=
						drainGeneration ||
					!m_eosNotified.load(std::memory_order_relaxed) ||
					!allNonSubtitleSourcesEosLocked())
				{
					RIALTO_SIM_LOG("startEosDrain END_OF_STREAM cancelled: new media arrived during drain");
					return;
				}
			}
			if (auto client = m_client.lock())
			{
				RIALTO_SIM_LOG("startEosDrain END_OF_STREAM (after drain)");
				client->notifyPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
			}
			RIALTO_SIM_LOG("startEosDrain end of thread");
		});
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
			uint64_t generation = m_generation.load(std::memory_order_relaxed);
			for (int32_t sourceId : m_attachedSources)
			{
				uint32_t reqId = m_needDataRequestId++;
				m_requestIdToSource[reqId] = RequestInfo{sourceId, generation};
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

	void scheduleNextNeedData(int32_t sourceId)
	{
		const uint64_t expectedGeneration = m_generation.load(std::memory_order_relaxed);
		RIALTO_SIM_LOG("sourceId %d ", sourceId);
		if (m_needDataThread.find(sourceId) != m_needDataThread.end() && m_needDataThread[sourceId].joinable())
		{
			m_needDataThread[sourceId].join();
		}
		m_needDataThread[sourceId] = std::thread([this, sourceId, expectedGeneration]() {
			// Wait until this source's buffered (injected-but-not-played) media
			// drops below the high-water mark before asking it for more.
			// This keeps injection at ~real time instead of draining AAMP's
			// local TSB as fast as fragments can be produced, and — since
			// each source is paced independently — a fast source can't have
			// its next request blocked on a slower sibling source.
			for (;;)
			{
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					RIALTO_SIM_LOG("scheduleNextNeedData: aborting sourceId=%d - stop requested", sourceId);
					return;
				}
				if (m_generation.load(std::memory_order_relaxed) != expectedGeneration)
				{
					// flush()/setPosition() has already restarted the pump
					// with a fresh generation since this response was
					// received; that pump already re-requested data for
					// every source, so sending another round here would
					// just create a duplicate outstanding request.
					RIALTO_SIM_LOG("scheduleNextNeedData: aborting sourceId=%d - generation changed",
						sourceId);
					return;
				}
				int64_t buffered = 0;
				{
					std::lock_guard<std::mutex> lock(m_trackMutex);
					refreshMasterClockLocked();
					buffered = bufferedAheadNsLocked(sourceId);
				}
				RIALTO_SIM_LOG("DBG scheduleNextNeedData sourceId=%d buffered=%lld high=%lld playing=%d baseSet=%d",
					sourceId,
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

			uint32_t reqId = 0;
			{
				std::lock_guard<std::mutex> lock(m_trackMutex);
				if (m_generation.load(std::memory_order_relaxed) != expectedGeneration)
				{
					RIALTO_SIM_LOG("scheduleNextNeedData: aborting sourceId=%d"
						" - generation changed before send", sourceId);
					return;
				}
				reqId = m_needDataRequestId++;
				m_requestIdToSource[reqId] = RequestInfo{sourceId, expectedGeneration};
			}
			if (!m_stopRequested.load(std::memory_order_relaxed))
			{
				RIALTO_SIM_LOG("notifyNeedMediaData: sourceId=%d requestId=%u (re-request)",
					sourceId, reqId);
				client->notifyNeedMediaData(sourceId, kNeedDataFrameCount, reqId, nullptr);
			}
		RIALTO_SIM_LOG("scheduleNextNeedData end of thread for sourceId=%d", sourceId);
		});
	}

	void markSourceReadyForPlay(int32_t sourceId)
	{
		std::lock_guard<std::mutex> lock(m_trackMutex);
		auto typeIt = m_sourceTypes.find(sourceId);
		if (typeIt == m_sourceTypes.end() ||
			typeIt->second == MediaSourceType::SUBTITLE)
		{
			return;
		}
		// A source that EOS-es with zero injected duration (e.g. audio
		// during video-only trickplay) must not be counted as ready for
		// playback.  Adding it would fire PLAYING before the video source
		// has primed its firstPtsMs, causing MonitorProgress to see an
		// unchanged position and suppress the first trickplay progress
		// event, which blocks callback-driven test steps.
		auto durIt = m_totalEnqueuedDurationNs.find(sourceId);
		if (durIt == m_totalEnqueuedDurationNs.end() || durIt->second == 0)
		{
			return;
		}
		m_readySources.insert(sourceId);
	}

	// Caller must hold m_trackMutex.
	bool allNonSubtitleSourcesReadyLocked() const
	{
		if (!m_allSourcesAttached)
		{
			return false;
		}

		// Check if there are any non-subtitle sources ready.
		// When multiple non-subtitle sources are present (video + audio),
		// transition to PLAYING once at least one of these sources has
		// accumulated sufficient data. Audio may not be present in some
		// streams (e.g., HLS TS) or may be disabled. Once playback has begun
		// and one source is ready, Rialto starts playback immediately
		// rather than waiting for all possible sources.
		for (const auto &source : m_readySources)
		{
			auto typeIt = m_sourceTypes.find(source);
			if (typeIt != m_sourceTypes.end() &&
				typeIt->second != MediaSourceType::SUBTITLE)
			{
				return true;
			}
		}
		return false;
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
			bool playRequested = m_playRequested.load(std::memory_order_relaxed);
			bool alreadyPlaying = m_playing.load(std::memory_order_relaxed);
			bool sourcesReady = allNonSubtitleSourcesReadyLocked();
			// Use RIALTO_SIM_LOG below to avoid noisy/unconditional stderr output.
			RIALTO_SIM_LOG("maybeStartPlayback: playRequested=%d playing=%d ready=%d #readySources=%zu",
				playRequested, alreadyPlaying, sourcesReady, m_readySources.size());
			if (playRequested && !alreadyPlaying && sourcesReady)
			{
				// Mark playing under the lock to prevent another
				// callback thread from also transitioning.
				m_playing.store(true, std::memory_order_relaxed);
				startPlaying = true;
			}
		}
		if (startPlaying)
		{
			{
				std::lock_guard<std::mutex> lock(m_trackMutex);
				m_masterClockAnchorNs = m_basePositionNs.load(std::memory_order_relaxed);
				m_masterClockAnchorWallTime = std::chrono::steady_clock::now();
			}
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

				int64_t pos = refreshAndGetPositionNs();
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
		if (m_eosThread.joinable())
		{
			m_eosThread.join();
		}
		for (auto &pair : m_needDataThread)
		{
			if (pair.second.joinable())
			{
				pair.second.join();
			}
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
	std::atomic<uint32_t> m_needDataRequestId;
	// Bumped whenever flush()/setPosition() abandons in-flight needData
	// requests, so haveData() can recognise and ignore responses for
	// requests issued under an earlier (now-stale) generation.
	std::atomic<uint64_t> m_generation;
	std::atomic<bool> m_stopRequested;
	std::atomic<int> m_eosSourceCount;
	std::atomic<bool> m_eosNotified;
	// Incremented each time a new EOS drain supersedes a previous one.
	std::atomic<uint64_t> m_eosDrainGeneration;
	std::thread m_positionThread;
	std::thread m_eosThread;
	std::map<int32_t, std::thread> m_needDataThread;
	mutable std::mutex m_trackMutex;
	std::map<int32_t, MediaSourceType> m_sourceTypes;
	// Audio / subtitle: FIFO (arrival order == presentation order).
	std::map<int32_t, std::deque<QueuedSample>> m_fifoQueue;
	// Video: PTS-ordered reorder buffer (arrival is decode order).
	std::map<int32_t, std::multiset<QueuedSample, ComparePts>> m_videoQueue;
	// Running per-source enqueued-duration totals, for the kMinPlayDurationNs
	// gate without re-summing the queue on every check.
	std::map<int32_t, int64_t> m_totalEnqueuedDurationNs;
	// Furthest presentation time we actually have real data for, per track.
	// Monotonically non-decreasing; updated only when a new sample arrives.
	std::map<int32_t, int64_t> m_trackHorizonNs;
	// Sources currently in a notified-underflow state (debounce: cleared once
	// that source's horizon catches back up, so a later stall renotifies).
	std::set<int32_t> m_underflowNotifiedSources;
	// Newly-starved sources queued by refreshMasterClockLocked() for
	// refreshAndGetPositionNs() to dispatch outside the lock.
	std::vector<int32_t> m_pendingUnderflowNotifications;
	// Master (A/V-sync-leading) track: audio if attached, else video.
	std::optional<int32_t> m_masterSourceId;
	int64_t m_masterClockAnchorNs = 0;
	// Floor for horizon-based clamping: the position most recently applied
	// by setPosition()/setSourcePosition(). A track's horizon can only cap
	// the clock below this if that horizon was actually established after
	// the reset - see refreshMasterClockLocked().
	int64_t m_horizonFloorNs = 0;
	std::chrono::steady_clock::time_point m_masterClockAnchorWallTime;
	std::set<int32_t> m_readySources;
	std::set<int32_t> m_eosSources;
	std::map<uint32_t, RequestInfo> m_requestIdToSource;
	std::map<uint32_t, PendingSegmentData> m_pendingSegments;
	bool m_playbackRateEnabled;
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
