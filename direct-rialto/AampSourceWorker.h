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
 * @file AampSourceWorker.h
 * @brief Per-source injection worker for AampRialtoPlayer.
 *
 * Each Rialto media source (video, audio) owns one SourceWorker.  The worker
 * runs a dedicated thread that pulls pending needData requests and queued
 * samples and forwards them to the Rialto pipeline via the InjectFn callback.
 *
 * Design goals
 * ------------
 * - OnNeedMediaData IPC callbacks post to this worker without acquiring any
 *   lock on the caller side (fixes rialto-gstreamer comparison issue #1).
 * - Video and audio workers are independent so they process injections in
 *   parallel (fixes issue #2).
 * - Rejected segments (NO_SPACE) are re-queued at the front so they are
 *   retried on the next needData (fixes issue #3).
 */

#ifndef SOURCE_WORKER_H
#define SOURCE_WORKER_H

#include "AampDemuxDataTypes.h" // AampMediaSample

#include <IMediaPipeline.h>     // firebolt::rialto::CodecData
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Shared data types
// ---------------------------------------------------------------------------

/**
 * @brief A sample queued for injection carrying per-sample codec parameters
 *        stamped at enqueue time.
 *
 * Storing codec parameters on the sample (rather than in shared members)
 * ensures that an ABR codec change only takes effect at the correct sample
 * boundary.  Old samples in the queue retain the old parameters and play
 * through cleanly before the update is applied.
 */
struct QueuedSample
{
	AampMediaSample sample;
	/// Non-null only on the first sample after an init-fragment codec change.
	std::shared_ptr<firebolt::rialto::CodecData> codecData;
	/// Video frame dimensions at enqueue time (pixels).
	int32_t width{0};
	int32_t height{0};
	/// Audio parameters at enqueue time.
	int32_t sampleRate{0};
	int32_t channels{0};

	QueuedSample() = default;
	QueuedSample(QueuedSample &&)            = default;
	QueuedSample &operator=(QueuedSample &&) = default;
	QueuedSample(const QueuedSample &)            = delete;
	QueuedSample &operator=(const QueuedSample &) = delete;
};

/**
 * @brief Pending need-data request received from the Rialto server.
 */
struct PendingNeedData
{
	int32_t  sourceId;   ///< Rialto source identifier
	uint32_t requestId;  ///< Request token passed back via haveData()
	size_t   frameCount; ///< Maximum number of segments to send
};

// ---------------------------------------------------------------------------
// SourceWorker
// ---------------------------------------------------------------------------

/**
 * @class SourceWorker
 * @brief Dedicated injection thread for a single Rialto media source.
 *
 * The worker owns:
 *   - A queue of pending needData requests (from the Rialto IPC thread)
 *   - A queue of buffered QueuedSamples (from AAMP's download thread)
 *   - A worker thread that drains both queues by calling @p injectFn
 *
 * Each time the Rialto server issues a needData event for this source, the
 * caller posts to the worker via postNeedData().  The worker's thread wakes
 * up, pops the request, collects up to frameCount samples, and calls the
 * injection callback.  Rejected samples (NO_SPACE) are returned by
 * injectFn and re-inserted at the front of the sample queue.
 *
 * @note The worker thread starts at construction and runs until stop() is
 *       called (or the destructor fires).
 */
class SourceWorker
{
public:
	/**
	 * @brief Injection callback type.
	 *
	 * Signature: (sourceId, requestId, samples, eos) → rejected samples.
	 * The implementation is AampRialtoPlayer::InjectSamples (via lambda).
	 */
	using InjectFn = std::function<
		std::vector<QueuedSample>(
			int32_t  sourceId,
			uint32_t requestId,
			std::vector<QueuedSample> samples,
			bool     eos)>;

	/**
	 * @brief Construct a SourceWorker and start its injection thread.
	 * @param[in] injectFn  Callback used to push segments to the pipeline.
	 */
	explicit SourceWorker(InjectFn injectFn);

	SourceWorker(const SourceWorker &) = delete;
	SourceWorker &operator=(const SourceWorker &) = delete;

	/// Stop the worker thread and join it.
	~SourceWorker();

	// -----------------------------------------------------------------------
	// Thread-safe operations called from external threads
	// -----------------------------------------------------------------------

	/**
	 * @brief Post a needData request from the Rialto IPC callback.
	 *
	 * Called on the Rialto IPC thread — must not block.  The request is
	 * pushed to the pending-request queue and the worker is signalled.
	 *
	 * @param[in] sourceId    Rialto source identifier (forwarded to injectFn).
	 * @param[in] requestId   Request token for haveData().
	 * @param[in] frameCount  Maximum number of segments to deliver.
	 */
	void postNeedData(int32_t sourceId, uint32_t requestId, size_t frameCount);

	/**
	 * @brief Cancel all pending needData requests for this source.
	 *
	 * Called from the Rialto IPC callback thread.  Does not interrupt an
	 * already-executing InjectSamples call.
	 */
	void cancelNeedData();

	/**
	 * @brief Enqueue a batch of samples ready for injection.
	 *
	 * Called from AAMP's download thread (via AampRialtoPlayer::SendTransfer).
	 * Wakes the worker if there is a matching pending request.
	 *
	 * @param[in] samples  Samples to append to the internal queue.
	 */
	void enqueueSamples(std::vector<QueuedSample> samples);

	/**
	 * @brief Signal end-of-stream for this source.
	 *
	 * After all samples for this source have been enqueued, the AAMP
	 * pipeline calls EndOfStreamReached().  The worker will complete the
	 * current needData request with EOS status once the sample queue drains.
	 */
	void setEos();

	/**
	 * @brief Flush all enqueued samples and pending requests.
	 *
	 * Called from AampRialtoPlayer::Flush().  Clears both internal queues
	 * and resets the EOS flag so the worker is ready for a new stream.
	 */
	void flush();

	/**
	 * @brief Stop the worker thread and join it (idempotent).
	 *
	 * After stop() the worker cannot be reused.  Called from
	 * AampRialtoPlayer::Stop() and the destructor.
	 */
	void stop();

private:
	/// Main loop executed on the worker thread.
	void run();

	InjectFn m_injectFn;

	std::mutex              m_mutex;
	std::condition_variable m_cv;

	std::deque<PendingNeedData> m_pendingReqs;
	std::deque<QueuedSample>    m_sampleQueue;
	bool                        m_eos{false};
	bool                        m_stop{false};

	std::thread m_thread;
};

#endif // SOURCE_WORKER_H
