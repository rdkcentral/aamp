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
		std::mutex              mu;
		std::condition_variable cv;

		bool     hasPending{false};
		uint32_t pendingRequestId{0};
		size_t   pendingFrameCount{0};
		size_t   addedInPending{0};
		bool     eos{false};
		uint64_t generation{0};
		bool     paused{false};
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
	int32_t mksId() const { return m_mksId; }

	// -----------------------------------------------------------------
	// Pacing state
	// -----------------------------------------------------------------

	SourceState &state() { return m_state; }
	uint64_t captureGeneration();

	// -----------------------------------------------------------------
	// Demuxer
	// -----------------------------------------------------------------

	void setDemuxer(std::unique_ptr<Mp4Demux> demuxer);
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
	 */
	bool injectOneSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		uint64_t capturedGen,
		AampMediaSample &&sample,
		std::shared_ptr<firebolt::rialto::CodecData> codecData);

	/**
	 * @brief Signal end-of-stream for this source.
	 */
	void signalEos(firebolt::rialto::IMediaPipeline *pipeline);

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
	 */
	virtual std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
		createSegment(int64_t ptsNs, int64_t durationNs) const = 0;

	// -----------------------------------------------------------------
	// Members
	// -----------------------------------------------------------------

	SourceState m_state;
	int32_t m_sourceId{-1};
	int32_t m_mksId{-1};
	std::unique_ptr<Mp4Demux> m_demuxer;
	std::optional<ProtectionParams> m_protection;
	std::shared_ptr<firebolt::rialto::CodecData> m_pendingCodecData;
};

#endif /* AAMP_RIALTO_MEDIA_SOURCE_H */
