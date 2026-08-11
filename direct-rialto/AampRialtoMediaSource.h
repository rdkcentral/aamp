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
#include "StreamOutputFormat.h"

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
	 * @brief Injection gating mode for the needData/haveData handshake.
	 */
	enum class GateMode
	{
		NONE,     ///< Normal operation — samples flow through as needData arrives.
		BLOCKED,  ///< New samples block (waiting) until gateInjection(false) or a newer generation.
		DROPPED   ///< New samples are discarded immediately — never blocks the caller thread.
	};

	/**
	 * @brief Per-track pacing state for the Rialto needData/haveData
	 *        handshake.
	 */
	struct SourceState
	{
		/// Protects all fields in this struct.
		std::mutex              mu;
		/// Notified whenever hasPending, generation, gateMode,
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
		/// True once the first segment of the current batch has set
		/// batchFirstPtsSec.  Distinguishes "first frame had PTS 0" from
		/// "no frame added yet", mirroring kFirstPtsNotSet's role for
		/// m_firstPtsMs but scoped to a single needData batch.
		bool     batchHasFirstPts{false};
		/// PTS (seconds) of the first segment added in the current batch.
		/// Only meaningful when batchHasFirstPts is true.
		double   batchFirstPtsSec{0.0};
		/// Running sum of durations (seconds) of segments added in the
		/// current batch.
		double   batchDurationSecSum{0.0};
		/// Running sum of raw media payload bytes (AampMediaSample::mDataSize)
		/// of segments added in the current batch.
		size_t   batchMediaBytesSum{0};
		/// Running sum of DRM metadata bytes (key id + IV + subsample table)
		/// attached to segments added in the current batch.
		size_t   batchMetadataBytesSum{0};
		/// Set once signalEos() has been called.  Causes the next batch
		/// completion (or an idle needData) to fire haveData(EOS) instead
		/// of haveData(OK).
		bool     eos{false};
		/// Monotonically-increasing abort token.  Bumped by reset() and
		/// unblockInjection(); injectors capture it at entry and
		/// abort if it changes while they are blocked.
		uint64_t generation{0};
		/// Injection gating mode — see GateMode.  Set to DROPPED by
		/// unblockInjection() (flush/stop/pause-notify: abort now, never
		/// block a caller thread again) and to BLOCKED or NONE by
		/// gateInjection(gate), called via AampRialtoPlayer::UngateAllSources()
		/// (gate=false) at each site that actually issues (or confirms)
		/// pipeline play() — Stream(), CheckAllSourcesAttached(),
		/// Pause(false), StopBuffering(), NotifyInjectorToResume(), the
		/// SEEK_DONE play branch, and the PLAYING playback-state handler —
		/// or (gate=true) by Configure() and Flush()'s flushable path, which
		/// expect genuinely-wanted data to arrive shortly and must block it
		/// rather than drop it.
		/// Deliberately NOT touched by reset(): clearing it there would
		/// reopen the gate before a multi-step Configure()/Flush() sequence
		/// (e.g. trickplay's Flush(pos=0) -> Configure() -> Flush(correctPos)
		/// -> Stream()) has finished, letting stale-position data slip
		/// through on an early needData.  NOT related to the pipeline PAUSED
		/// state — see the comment in waitForAttach() for the important
		/// distinction.
		/// While BLOCKED, injectOneSample() blocks newly-submitted samples
		/// until the gate clears or a newer generation supersedes them.
		/// While DROPPED, injectOneSample() discards newly-submitted samples
		/// immediately without ever blocking the caller thread — see
		/// injectOneSample() for details.
		GateMode gateMode{GateMode::NONE};
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

	/**
	 * @brief Snapshot of the segments staged for a needData batch, taken
	 *        at the moment the batch is claimed/abandoned so the eventual
	 *        haveData() response can be logged alongside what data (if
	 *        any) it is answering for.
	 */
	struct BatchSummary
	{
		size_t frameCount{0};       ///< Segments added to this batch.
		bool   hasFirstPts{false};  ///< True once at least one segment was added.
		double firstPtsSec{0.0};    ///< PTS (seconds) of the first segment added.
		double durationSecSum{0.0}; ///< Sum of durations (seconds) added.
		size_t mediaBytes{0};       ///< Sum of media payload bytes added.
		size_t metadataBytes{0};    ///< Sum of DRM metadata bytes added.
	};

	// -----------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------

	virtual ~AampRialtoMediaSource();

	/// Media type identifier for this source.
	virtual AampMediaType mediaType() const = 0;

	/// Reset all per-session state (source ID, mks ID, pacing, codec data).
	void reset();

	/// Bump the pacing generation to abort any in-flight injection batch,
	/// set gateMode to DROPPED (new samples are discarded, never blocked),
	/// and wake the condition variable.
	///
	/// If a needData request is currently pending and no injector is
	/// active to answer it, this closes it out immediately with
	/// haveData(NO_AVAILABLE_SAMPLES) so AAMP never abandons a request
	/// Rialto is still waiting on.  If an injector *is* active, the
	/// request is left for that injector to close out itself when it
	/// observes the generation change (see injectOneSample()).
	///
	/// Unlike gateInjection(true), this never leaves the calling
	/// (fragment-injector) thread blocked waiting for a later ungate —
	/// callers that need genuinely-wanted data to be held (rather than
	/// dropped) until playback resumes must follow this with
	/// gateInjection(pipeline, true, reason).
	///
	/// @param pipeline     The active Rialto media pipeline, or nullptr if
	///                     no pipeline exists for this session (e.g. Stop()
	///                     called before Configure()).
	/// @param reason       Short human-readable description of the caller
	///                     (e.g. "Flush", "Stop", "NotifyInjectorToPause"),
	///                     included in the gateMode-set log line to aid
	///                     debugging of gating behaviour.
	/// @param newEosState  Optional new value for eos, applied atomically
	///                     with the generation bump/gate-set above.  Callers
	///                     that need to change eos alongside invalidating the
	///                     generation (e.g. Stop(), Flush()) must use this
	///                     parameter rather than locking m_state.mu
	///                     separately afterwards, which would otherwise open
	///                     a window where a concurrent needData/signalEos
	///                     could observe the gate set but the old eos value.
	void unblockInjection(firebolt::rialto::IMediaPipeline *pipeline,
		const char *reason = "unblockInjection",
		std::optional<bool> newEosState = std::nullopt);

	/// Set (gate=true) or clear (gate=false) BLOCKED gating for this
	/// source, logging the transition when it actually changes gateMode.
	/// gate=true is a plain mode flip only — no generation bump, no
	/// pending-request abandonment — used by callers (Configure(),
	/// Flush()'s flushable path) that expect genuinely-wanted data to
	/// arrive shortly and must hold it rather than drop it.
	/// gate=false must be called only from the specific points where
	/// playback is genuinely about to resume — see the gateMode field
	/// comment above for the authoritative list.
	///
	/// If a needData was staged while BLOCKED and EOS was signalled in the
	/// meantime (both intentionally deferred while gateMode==BLOCKED — see
	/// signalEos()/handleNeedData()), gate=false replays that resolution
	/// now that the gate has cleared, so the request is answered here
	/// rather than being left pending until a fresh needData or injector
	/// run.
	///
	/// @param pipeline  The active Rialto media pipeline, or nullptr if none
	///                  exists for this session.  Only used if a deferred
	///                  EOS resolution must be replayed (gate=false).
	/// @param gate      true to enter BLOCKED; false to clear to NONE.
	/// @param reason    Short human-readable description of the caller
	///                  (e.g. "Stream", "OnPlaybackState(PLAYING)"),
	///                  included in the log line.
	void gateInjection(firebolt::rialto::IMediaPipeline *pipeline,
		bool gate, const char *reason);

	/// @brief Directly set the eos flag, logging the transition.
	///
	/// For callers that need to change eos outside the needData/signalEos
	/// handshake (e.g. AampRialtoPlayer::Configure()'s trickplay-exit path,
	/// which must clear a previously-forced audio EOS).  Does not attempt to
	/// resolve any pending request — unlike signalEos(), this is not
	/// expected to be called while a needData is outstanding.
	///
	/// @param eos     The new eos value.
	/// @param reason  Short human-readable description of the caller,
	///                included in the log line when the value changes.
	void setEos(bool eos, const char *reason);

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

	// -----------------------------------------------------------------
	// Codec data
	// -----------------------------------------------------------------

	std::shared_ptr<firebolt::rialto::CodecData> takePendingCodecData();

	// -----------------------------------------------------------------
	// Stream format
	// -----------------------------------------------------------------

	/// @brief Return the StreamOutputFormat recorded by setFormat().
	StreamOutputFormat format() const { return m_streamFormat; }

	/// @brief Record the StreamOutputFormat for which this source was created.
	///
	/// Called by AampRialtoPlayer::Configure() immediately after source
	/// construction.  Used to detect format changes on subsequent Configure()
	/// calls so the pipeline is only recreated when necessary.  Never
	/// cleared by reset().
	void setFormat(StreamOutputFormat f) { m_streamFormat = f; }

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
	 * @param protection  Optional protection params for DRM session creation.
	 * @return AttachResult indicating what happened.
	 */
	AttachResult attachOrUpdate(
		firebolt::rialto::IMediaPipeline &pipeline,
		MediaCodecInfo &codecInfo,
		IDrmBridge *drmBridge,
		int64_t flushPosNs,
		const std::optional<ProtectionParams> &protection = std::nullopt,
		double appliedRate = 1.0);

	/**
	 * @brief Inject one sample into the Rialto pipeline.
	 *
	 * If gateMode is BLOCKED for the current generation (i.e. capturedGen),
	 * first blocks until either the gate clears (same generation) or a newer
	 * flush supersedes this sample (generation changes) — see
	 * AampRialtoMediaSource::SourceState::gateMode.  If gateMode is DROPPED,
	 * the sample is discarded immediately without ever blocking the caller
	 * thread.  Once past the gate, blocks until a needData request arrives
	 * for this source, then delivers the sample via addSegment.  Returns
	 * false if the sample was abandoned because it was superseded by a
	 * newer flush/stop or because injection is in DROPPED mode.
	 *
	 * @param pipeline       The Rialto media pipeline.
	 * @param capturedGen    Generation token captured before blocking.
	 * @param sample         The sample to inject (moved in).
	 * @param codecData      Optional codec data to attach.
	 * @param morePending    True if more samples are available to inject after this one (default: false).
	 * @return true on successful injection; false if aborted.
	 */
	bool injectOneSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		uint64_t capturedGen,
		AampMediaSample &&sample,
		std::shared_ptr<firebolt::rialto::CodecData> codecData,
		bool morePending = false);

	/**
	 * @brief Parse an init segment and return the decoded codec info.
	 *
	 * A demuxer is created internally on the first call if one has not
	 * been created already.  The caller does not need to call hasDemuxer()
	 * before invoking this method.
	 *
	 * The default implementation delegates to the owned demuxer.
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
	 * @param pipeline     The active Rialto media pipeline.
	 * @param sample       The decoded sample to inject (moved in).
	 * @param morePending  True if more samples are available to inject after this one (default: false).
	 * @return true on successful injection; false otherwise.
	 */
	virtual bool injectSingleSample(
		firebolt::rialto::IMediaPipeline &pipeline,
		AampMediaSample &&sample,
		bool morePending = false);

	/// Sentinel value returned by firstPtsMs() when no sample has been
	/// injected yet in the current session.  Mirrors the -1 sentinel
	/// used by GStreamer's segmentStart in InterfacePlayerRDK.
	static constexpr int64_t kFirstPtsNotSet = -1LL;

	/**
	 * @brief PTS of the first sample injected since the last reset or
	 *        unblockInjection().
	 *
	 * Set when the first addSegment() call succeeds in each session.
	 * This avoids establishing a segment-start baseline for samples that
	 * never become accepted pipeline content.
	 * Returns kFirstPtsNotSet if no sample has been injected yet.
	 *
	 * Used by AampRialtoPlayer::GetPositionMilliseconds() as the segment-
	 * start baseline, mirroring GStreamer's segmentStart subtraction in
	 * InterfacePlayerRDK::GetPositionMilliseconds().
	 */
	virtual int64_t firstPtsMs() const;

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

	/**
	 * @brief Returns true when inband closed-caption mode is active.
	 *
	 * Default returns false; AampRialtoSubtitleSource overrides to return
	 * true if the current selected subtitle is inband-CC.
	 */
	virtual bool isInbandCC() const { return false; }

protected:
	// -----------------------------------------------------------------
	// Subclass hooks (pure virtual)
	// -----------------------------------------------------------------

	/**
	 * @brief Map a codec format to a MIME type and Rialto stream format.
	 * @param naluLengthPrefixed True when the codec's NAL units are
	 *        length-prefixed (AVCC/HVCC), false for Annex-B byte-stream.
	 *        Only meaningful for NAL-unit video codecs (H.264/HEVC).
	 * @return true if the codec is recognised, false otherwise.
	 */
	virtual bool mapCodecToMime(
		GstStreamOutputFormat codecFormat,
		bool naluLengthPrefixed,
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
	std::shared_ptr<firebolt::rialto::CodecData> m_pendingCodecData;
	/// Stream format passed to Configure() when this source was created.
	/// Never cleared by reset() so Configure() can compare formats across
	/// sessions without unnecessarily recreating the pipeline.
	StreamOutputFormat m_streamFormat{FORMAT_INVALID};

	/// PTS of the first sample injected since the last reset or
	/// unblockInjection(), in milliseconds.  Set lazily via
	/// compare-exchange after addSegment(OK) in injectOneSample().
	/// kFirstPtsNotSet = not set.
	std::atomic<int64_t> m_firstPtsMs{kFirstPtsNotSet};

private:
	// -----------------------------------------------------------------
	// injectOneSample() helpers
	// -----------------------------------------------------------------

	/**
	 * @brief Populate DRM/encryption metadata on a segment.
	 *
	 * No-op (other than a trace log) when the sample is not encrypted.
	 * Logs a warning when the sample is encrypted but no DRM session
	 * exists (m_mksId < 0).
	 */
	void annotateEncryption(
		const AampMediaSample &sample,
		firebolt::rialto::IMediaPipeline::MediaSegment &segment) const;

	/**
	 * @brief Handle an AddSegmentStatus::NO_SPACE result from addSegment().
	 *
	 * Closes out the current batch with haveData(OK) or
	 * haveData(NO_AVAILABLE_SAMPLES) depending on whether any segments
	 * were already delivered, then leaves the injector waiting for the
	 * next needData event (does not set injectorActive/done).
	 */
	void handleAddSegmentNoSpace(
		firebolt::rialto::IMediaPipeline &pipeline,
		uint64_t capturedGen,
		uint32_t reqId);

	/**
	 * @brief Handle a non-NO_SPACE AddSegmentStatus result from
	 *        addSegment() (i.e. the sample was consumed one way or
	 *        another and this injector is done with it).
	 *
	 * Updates firstPtsMs() on success, advances the batch counter, and
	 * sends haveData(OK) or haveData(EOS) when the batch/stream is
	 * complete.  Always clears injectorActive.
	 */
	void handleAddSegmentCompletion(
		firebolt::rialto::IMediaPipeline &pipeline,
		firebolt::rialto::AddSegmentStatus addStatus,
		uint64_t capturedGen,
		uint32_t reqId,
		bool morePending,
		double samplePts,
		double sampleDurationSec,
		size_t sampleMediaBytes,
		size_t sampleMetadataBytes);

	// -----------------------------------------------------------------
	// Pending-request handshake helpers
	// -----------------------------------------------------------------

	/**
	 * @brief Claim the current pending request so the caller can close
	 *        it out.
	 *
	 * Caller must hold m_state.mu.  Clears hasPending and the batch
	 * tracking fields (via snapshotAndClearBatchLocked()), returning their
	 * pre-claim values in outBatch.  Does not itself send any response —
	 * the caller decides what status to reply with (or whether it is even
	 * safe to reply, e.g. only when no injector is active).
	 *
	 * @param outBatch  Set to a summary of the segments staged for this
	 *                  batch when this returns true; left unmodified
	 *                  otherwise.
	 * @return true if there was a pending request to claim.
	 */
	bool claimPendingRequestLocked(uint32_t &outReqId, BatchSummary &outBatch);

	/**
	 * @brief Attempt to resolve a pending request as EOS.
	 *
	 * Caller must hold m_state.mu.  Claims the pending request (via
	 * claimPendingRequestLocked()) only if this source is genuinely ready
	 * to answer haveData(EOS) right now: eos is set, a request is
	 * pending, no injector is active to answer it on its own, and
	 * gateMode is not BLOCKED.  The gate check is what distinguishes this
	 * from the older behaviour: while gateMode==BLOCKED, EOS resolution is
	 * deliberately deferred (the request is left pending) so it is not
	 * resolved out of order with samples that are still blocked behind the
	 * gate.  gateInjection(false) calls this again once the gate clears to
	 * replay the resolution.  gateMode==DROPPED does not defer EOS —
	 * dropped injection has no samples left to be resolved out of order
	 * with, so EOS resolves immediately.
	 *
	 * @param outReqId  Set to the claimed request ID when this returns
	 *                  true; left unmodified otherwise.
	 * @param outBatch  Set to a summary of the segments staged for this
	 *                  batch when this returns true; left unmodified
	 *                  otherwise.
	 * @return true if a pending request was claimed for EOS.
	 */
	bool tryClaimEosLocked(uint32_t &outReqId, BatchSummary &outBatch);

	/**
	 * @brief Send haveData(NO_AVAILABLE_SAMPLES) for a request this
	 *        source is abandoning (Flush()/Stop()/generation change).
	 *
	 * Logs and drops the request if @p pipeline is null (Stop() has
	 * already destroyed it) - there is nothing left to answer.
	 *
	 * Must be called without holding m_state.mu.
	 */
	void respondAbandonedRequest(
		firebolt::rialto::IMediaPipeline *pipeline,
		uint32_t reqId,
		const BatchSummary &batch);

	/**
	 * @brief Respond to a needData request via
	 *        IMediaPipeline::haveData(), logging which source is
	 *        responding and with what data before forwarding the call.
	 *
	 * @param pipeline   The Rialto media pipeline to respond to.
	 * @param status     The status being reported to Rialto.
	 * @param requestId  The needData request ID being answered.
	 * @param batch      Summary of the segments staged for this batch.
	 * @return The result of the underlying haveData() call.
	 */
	bool sendHaveData(
		firebolt::rialto::IMediaPipeline &pipeline,
		firebolt::rialto::MediaSourceStatus status,
		uint32_t requestId,
		const BatchSummary &batch);

	/**
	 * @brief Snapshot the current batch's frameCount/firstPts/duration
	 *        totals and reset them to their empty state.
	 *
	 * Caller must hold m_state.mu.  Does not touch hasPending/
	 * pendingRequestId — the caller decides whether/when the batch is
	 * fully claimed.
	 */
	BatchSummary snapshotAndClearBatchLocked();
};

#endif /* AAMP_RIALTO_MEDIA_SOURCE_H */
