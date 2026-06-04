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
 * @file AampRialtoMediaSource.h
 * @brief Abstract base class for per-track Rialto media sources.
 *
 * Encapsulates all per-source state (pacing, DRM, codec data, demuxer) and
 * polymorphic behaviour (codec mapping, segment creation, Rialto source
 * construction) so that AampRialtoPlayer can treat video, audio and subtitle
 * sources uniformly.
 */

#ifndef AAMP_RIALTO_MEDIA_SOURCE_H
#define AAMP_RIALTO_MEDIA_SOURCE_H

#include "IMediaPipeline.h"
#include "AampDemuxDataTypes.h"
#include "AampMediaType.h"
#include "IDrmBridge.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "mp4demux/MP4Demux.h"

/// Nanoseconds per second — shared by all source classes.
constexpr int64_t kNsPerSecond = 1'000'000'000LL;

/**
 * @class AampRialtoMediaSource
 * @brief Abstract base encapsulating per-track Rialto source state.
 *
 * Concrete subclasses (AampRialtoVideoSource, AampRialtoAudioSource,
 * AampRialtoSubtitleSource) implement the codec-specific virtual methods
 * while the base class owns the common pacing, DRM, and injection logic.
 */
class AampRialtoMediaSource
{
public:
	// -----------------------------------------------------------------
	// Public nested types
	// -----------------------------------------------------------------

	/**
	 * @brief Per-track pacing state for the Rialto needData/haveData
	 *        handshake.
	 */
	struct SourceState
	{
		/// Protects all fields in this struct.
		std::mutex              mu;
		/// Notified whenever hasPending, generation, injectionGated,
		/// attachPending, or eos changes.
		std::condition_variable cv;

		/// True when Rialto has issued a needData request that has not
		/// yet been answered with a haveData response.
		bool     hasPending{false};
		/// Request ID from the current outstanding needData event; used
		/// to correlate the haveData response.
		uint32_t pendingRequestId{0};
		/// Number of frames requested in the current needData event;
		/// determines when the batch is considered complete.
		size_t   pendingFrameCount{0};
		/// Count of segments successfully delivered via addSegment for
		/// the current request batch; triggers haveData(OK) when it
		/// reaches pendingFrameCount.
		size_t   segmentsAddedInBatch{0};
		/// Set once signalEos() has been called.  Causes the next batch
		/// completion (or an idle needData) to fire haveData(EOS) instead
		/// of haveData(OK).
		bool     eos{false};
		/// Monotonically-increasing abort token.  Bumped by reset() and
		/// invalidateGeneration(); injectors capture it at entry and
		/// abort if it changes while they are blocked.
		uint64_t generation{0};
		/// Set by invalidateGeneration() (flush/seek) to gate injection
		/// until the next needData event or PLAYING state callback.
		/// Cleared by handleNeedData() and by the PLAYING playback-state
		/// handler.  NOT related to the pipeline PAUSED state — see the
		/// comment in waitForAttach() for the important distinction.
		bool     injectionGated{false};
		/// True while an injector thread is executing inside
		/// injectOneSample().  Prevents signalEos() and handleNeedData()
		/// from firing haveData(EOS) immediately when the active injector
		/// will send it after delivering its sample.
		bool     injectorActive{false};
		/// True while this source's Rialto attachment has been deferred
		/// (e.g. waiting for the video source to attach first).  Inject
		/// threads block on the cv when this is set so that no frames are
		/// silently discarded before the source is ready.
		bool     attachPending{false};
	};

	/**
	 * @brief Protection parameters saved by QueueProtectionEvent.
	 */
	struct ProtectionParams
	{
		std::string          systemId;
		std::vector<uint8_t> initData;
		AampMediaType        type;
	};

	/**
	 * @brief Result of an attachOrUpdate() call.
	 */
	enum class AttachResult
	{
		UPDATED,         ///< Source already attached; metadata refreshed
		NEWLY_ATTACHED,  ///< New source successfully attached to pipeline
		FAILED           ///< Invalid codec or attachSource() failed
	};

	// -----------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------

	virtual ~AampRialtoMediaSource();

	/// Media type identifier for this source.
	virtual AampMediaType mediaType() const = 0;

	/// Reset all per-session state (source ID, mks ID, pacing, codec data).
	void reset();

	/// Bump the pacing generation to abort any in-flight injection batch
	/// and wake the condition variable.
	void invalidateGeneration();

	// -----------------------------------------------------------------
	// Source identity
	// -----------------------------------------------------------------

	int32_t sourceId() const { return m_sourceId; }
	bool isAttached() const { return m_sourceId >= 0; }

	/// Block if this source's Rialto attachment is still deferred (waiting
	/// for video to attach first).  Returns true when injection may safely
	/// proceed; returns false when the caller should discard the frame
	/// (source not attached and not pending, or the wait was aborted by a
	/// generation change from Flush/Stop).
	bool waitForAttach();

	int32_t mksId() const { return m_mksId; }

	// -----------------------------------------------------------------
	// Pacing state
	// -----------------------------------------------------------------

	SourceState &state() { return m_state; }
	uint64_t captureGeneration();

	// -----------------------------------------------------------------
	// Demuxer
	// -----------------------------------------------------------------

	Mp4Demux *demuxer() const { return m_demuxer.get(); }
	bool hasDemuxer() const { return m_demuxer != nullptr; }

	// -----------------------------------------------------------------
	// Protection
	// -----------------------------------------------------------------

	void setProtection(ProtectionParams params);
	void clearProtection();
	bool hasProtection() const { return m_protection.has_value(); }
	ProtectionParams takeProtection()
	{
		ProtectionParams p = std::move(*m_protection);
		m_protection.reset();
		return p;
	}

	// -----------------------------------------------------------------
	// Codec data
	// -----------------------------------------------------------------

	std::shared_ptr<firebolt::rialto::CodecData> takePendingCodecData();

	// -----------------------------------------------------------------
	// Core operations
	// -----------------------------------------------------------------

	/**
	 * @brief Attach a new source or update metadata for an existing one.
	 *
	 * @param pipeline    The Rialto media pipeline.
	 * @param codecInfo   Codec information from the init segment.
	 * @param drmBridge   DRM bridge for session creation (may be null).
	 * @param flushPosNs  Pending flush position in nanoseconds (-1 = none).
	 * @return AttachResult indicating what happened.
	 */
	AttachResult attachOrUpdate(
		firebolt::rialto::IMediaPipeline &pipeline,
		MediaCodecInfo &codecInfo,
		IDrmBridge *drmBridge,
		int64_t flushPosNs);

	/**
	 * @brief Inject one sample into the Rialto pipeline.
	 *
	 * Blocks until a needData request arrives for this source, then
	 * delivers the sample via addSegment.  Returns false if the batch
	 * was aborted by Flush/Stop.
	 *
	 * The display offset (for subtitle timing correction) is derived
	 * inside createSegment() by the concrete subclass.
	 */
	bool injectOneSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		uint64_t capturedGen,
		AampMediaSample &&sample,
		std::shared_ptr<firebolt::rialto::CodecData> codecData);

	/**
	 * @brief Parse an init segment and return the decoded codec info.
	 *
	 * A demuxer is created internally on the first call if one has not
	 * been created already.  The caller does not need to call hasDemuxer()
	 * before invoking this method.
	 *
	 * The default implementation delegates to the owned demuxer.
	 * Subclasses may override to use an alternative parser.
	 *
	 * @param buffer  Shared ownership of the raw init-segment bytes.
	 * @return The parsed MediaCodecInfo on success; std::nullopt on
	 *         parse failure.
	 */
	virtual std::optional<MediaCodecInfo> processInitFragment(
		std::shared_ptr<std::vector<uint8_t>> buffer);

	/**
	 * @brief Parse a media segment and inject all contained samples.
	 *
	 * The caller is responsible for ensuring processInitFragment() has
	 * already been called on this source (which creates the demuxer).
	 * If no demuxer exists this method returns false immediately.
	 *
	 * The default implementation delegates to the owned demuxer and
	 * then injects each sample via injectOneSample().
	 * Subclasses may override to use an alternative parse/inject path
	 * (e.g. AampRialtoSubtitleSource handles raw TTML/WebVTT directly).
	 *
	 * @param pipeline          The active Rialto media pipeline.
	 * @param buffer            Shared ownership of the raw segment bytes.
	 * @param fpts              Fragment presentation timestamp (seconds).
	 * @param fdts              Fragment decode timestamp (seconds).
	 * @param fDuration         Fragment duration (seconds).
	 * @param fragmentPTSoffset Period-start PTS offset (seconds) from AAMP.
	 * @return true on success (including empty sample list); false on
	 *         parse failure.
	 */
	virtual bool processDataFragment(
		firebolt::rialto::IMediaPipeline &pipeline,
		std::shared_ptr<std::vector<uint8_t>> buffer,
		double fpts,
		double fdts,
		double fDuration,
		double fragmentPTSoffset);

	/**
	 * @brief Inject a single decoded sample into the pipeline.
	 *
	 * Blocks on waitForAttach(), then delivers the sample via
	 * injectOneSample().  Returns false if the source is not attached
	 * or if injection was aborted by Flush/Stop.
	 *
	 * @param pipeline  The active Rialto media pipeline.
	 * @param sample    The decoded sample to inject (moved in).
	 * @return true on successful injection; false otherwise.
	 */
	virtual bool injectSingleSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		AampMediaSample &&sample);

	/**
	 * @brief Signal end-of-stream for this source.
	 */
	void signalEos(firebolt::rialto::IMediaPipeline *pipeline);

	/**
	 * @brief Maximum segments to inject per needData batch.
	 *
	 * Returns 0 to use the frame count requested by the pipeline
	 * (default for audio/video).  Subtitle overrides to return 1 so
	 * that haveData() is sent after every injected segment, avoiding
	 * the Rialto EnoughData guard that fires before isDataPushed is set.
	 */
	virtual size_t needDataBatchSize() const { return 0; }

	/**
	 * @brief Handle a needData event from the pipeline client.
	 */
	void handleNeedData(
		size_t frameCount,
		uint32_t requestId,
		firebolt::rialto::IMediaPipeline *pipeline);

	/**
	 * @brief Handle a cancelNeedData event from the pipeline client.
	 */
	void handleCancelNeedData();

	/**
	 * @brief Flush this source on the pipeline and set source position.
	 */
	void flushSource(
		firebolt::rialto::IMediaPipeline &pipeline,
		int64_t positionNs);

	/**
	 * @brief Enable inband closed-caption mode for this source.
	 *
	 * When set, mapCodecToMime() returns "application/x-subtitle-cc" and
	 * handleNeedData() immediately acknowledges with NO_AVAILABLE_SAMPLES
	 * rather than queuing a data request — because the Rialto server
	 * extracts CC from the video bitstream internally and AAMP has no
	 * CC data to push.
	 */
	void enableInbandCC() { m_inbandCC = true; }

	/**
	 * @brief Returns true when inband closed-caption mode is active.
	 */
	bool isInbandCC() const { return m_inbandCC; }

protected:
	// -----------------------------------------------------------------
	// Subclass hooks (pure virtual)
	// -----------------------------------------------------------------

	/**
	 * @brief Map a codec format to a MIME type and Rialto stream format.
	 * @return true if the codec is recognised, false otherwise.
	 */
	virtual bool mapCodecToMime(
		GstStreamOutputFormat codecFormat,
		std::string &mimeType,
		firebolt::rialto::StreamFormat &streamFormat) const = 0;

	/**
	 * @brief Create the Rialto MediaSource object for attachSource().
	 */
	virtual std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
		createRialtoSource(
			const std::string &mimeType,
			bool hasDrm,
			const MediaCodecInfo &codecInfo,
			firebolt::rialto::StreamFormat streamFormat,
			std::shared_ptr<firebolt::rialto::CodecData> codecData) const = 0;

	/**
	 * @brief Update type-specific cached metadata from codec info.
	 *
	 * Called during attachOrUpdate() after codec validation succeeds.
	 * Video stores width/height; audio stores sampleRate/channels.
	 */
	virtual void updateCachedMetadata(const MediaCodecInfo &codecInfo) = 0;

	/**
	 * @brief Create a Rialto MediaSegment for injection.
	 *
	 * Video returns MediaSegmentVideo; audio returns MediaSegmentAudio.
	 * The full @p sample is provided so subtitle (and future) overrides
	 * can access any field beyond pts/duration.
	 */
	virtual std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
		createSegment(const AampMediaSample &sample) const = 0;



	// -----------------------------------------------------------------
	// Members
	// -----------------------------------------------------------------

	SourceState m_state;
	int32_t m_sourceId{-1};
	int32_t m_mksId{-1};
	std::unique_ptr<Mp4Demux> m_demuxer;
	std::optional<ProtectionParams> m_protection;
	std::shared_ptr<firebolt::rialto::CodecData> m_pendingCodecData;
	/// True when inband CC mode is active (set by enableInbandCC()).
	bool m_inbandCC{false};
};

#endif /* AAMP_RIALTO_MEDIA_SOURCE_H */
