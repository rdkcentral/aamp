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
 * @file AampRialtoPlayer.h
 * @brief Rialto client based player for AAMP.
 */

#ifndef AAMP_RIALTO_PLAYER_H
#define AAMP_RIALTO_PLAYER_H

#include "StreamSink.h"
#include "ID3Metadata.hpp"
#include "IMediaPipeline.h"
#include "IMediaPipelineCapabilities.h"
#include "IClientLogControl.h"
#include "IClientLogHandler.h"
#include "StreamOutputFormat.h"
#include "AampDemuxDataTypes.h"
#include "IDrmBridge.h"
#include "AampPlayerStateMachine.h"
#include "AampDemuxDataTypes.h"
#include "IRialtoControlBackend.h"
#include "IStreamSinkNotifiable.h"
#include "AampRialtoMediaSource.h"
#include "IDirectRialtoCC.h"
#include "AampRialtoMonitorAV.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

class PrivateInstanceAAMP;
class PrivateInstanceAAMPNotifiable;
class AampRialtoMediaPipelineClient;
class Mp4Demux;

/// Callable that creates a per-track AampRialtoMediaSource.
using SourceCreator =
	std::function<std::unique_ptr<AampRialtoMediaSource>(AampMediaType)>;

/**
 * @class AampRialtoPlayer
 * @brief StreamSink implementation that interfaces with Rialto client.
 */
class AampRialtoPlayer : public StreamSink, public IDirectRialtoCC
{
public:
	~AampRialtoPlayer() override;

	/**
	 * @brief Construct an AampRialtoPlayer for production use.
	 *
	 * Internally wraps @p aamp in a PrivateInstanceAAMPNotifiable adapter so
	 * that all notification calls go through the IStreamSinkNotifiable
	 * interface.
	 *
	 * @param[in] aamp               Pointer to the owning PrivateInstanceAAMP.
	 * @param[in] id3HandlerCallback Callback invoked for each ID3 metadata
	 *                               packet encountered in the stream.
	 * @param[in] exportFrames       Optional callback used to export raw YUV
	 *                               frames; defaults to nullptr.
	 */
	explicit AampRialtoPlayer(
		PrivateInstanceAAMP *aamp,
		id3_callback_t id3HandlerCallback,
		std::function<void(const unsigned char *, int, int, int)> exportFrames = nullptr);

	/**
	 * @brief Construct an AampRialtoPlayer with an injected notifiable.
	 *
	 * Intended for unit tests: callers supply their own IStreamSinkNotifiable
	 * implementation (e.g. a GoogleMock object) so notification calls can be
	 * asserted without instantiating the full AAMP player.
	 *
	 * @param[in] aamp               Pointer to the owning PrivateInstanceAAMP
	 *                               (used for non-notification calls such as
	 *                               ResumeTrackDownloads).  May point to a
	 *                               fake/stub in tests.
	 * @param[in] notifiable         Non-null pointer to the notification
	 *                               listener.  Must outlive this player.
	 * @param[in] controlBackend     Control backend used to wait for
	 *                               ApplicationState::RUNNING before pipeline
	 *                               creation.  Ownership is transferred.
	 * @param[in] id3HandlerCallback Callback invoked for each ID3 metadata
	 *                               packet encountered in the stream.
	 * @param[in] exportFrames       Optional YUV-frame export callback.
	 */
	AampRialtoPlayer(
		PrivateInstanceAAMP *aamp,
		IStreamSinkNotifiable *notifiable,
		std::unique_ptr<IRialtoControlBackend> controlBackend,
		id3_callback_t id3HandlerCallback,
		std::function<void(const unsigned char *, int, int, int)> exportFrames = nullptr,
		SourceCreator sourceCreator = nullptr);

	/// @copydoc StreamSink::Configure
	void Configure(
		StreamOutputFormat format,
		StreamOutputFormat audioFormat,
		StreamOutputFormat subFormat,
		bool bESChangeStatus,
		bool setReadyAfterPipelineCreation = false) override;

	/**
	 * @brief Return the current player state identifier.
	 *
	 * Returns the live state from the GoF state machine.  Useful for
	 * diagnostics, logging, and unit test assertions.
	 */
	PlayerStateId GetCurrentPlayerState() const
	{
		return m_stateMachine.currentState();
	}

	/// @copydoc StreamSink::SendCopy
	bool SendCopy(
		AampMediaType mediaType,
		std::vector<uint8_t> &&buffer,
		double fpts,
		double fdts,
		double fDuration) override;

	/// @copydoc StreamSink::SendTransfer
	bool SendTransfer(
		AampMediaType mediaType,
		std::vector<uint8_t> &&buffer,
		double fpts,
		double fdts,
		double fDuration,
		double fragmentPTSoffset,
		bool initFragment = false,
		bool discontinuity = false) override;

	/// @copydoc StreamSink::SendSample
	bool SendSample(AampMediaType mediaType, AampMediaSample &&sample, bool morePending = false) override;

	/// @copydoc StreamSink::PipelineConfiguredForMedia
	bool PipelineConfiguredForMedia(AampMediaType type) override;

	/// @copydoc StreamSink::EndOfStreamReached
	void EndOfStreamReached(AampMediaType type) override;

	/// @copydoc StreamSink::Stream
	void Stream() override;

	/// @copydoc StreamSink::Stop
	void Stop(bool keepLastFrame) override;

	/// @copydoc StreamSink::Flush
	void Flush(
		double position = 0,
		int rate = AAMP_NORMAL_PLAY_RATE,
		bool shouldTearDown = true) override;

	/// @copydoc StreamSink::FlushTrack
	void FlushTrack(AampMediaType mediaType, double position = 0) override;

	/// @copydoc StreamSink::SetPlayBackRate
	bool SetPlayBackRate(double rate) override;

	/// @copydoc StreamSink::Pause
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering) override;

	/// @copydoc StreamSink::GetDurationMilliseconds
	long GetDurationMilliseconds() override;

	/// @copydoc StreamSink::GetPositionMilliseconds
	long long GetPositionMilliseconds() override;

	/// @copydoc StreamSink::GetVideoPTS
	long long GetVideoPTS() override;

	/// @copydoc StreamSink::SetVideoRectangle
	void SetVideoRectangle(int x, int y, int w, int h) override;

	/// @copydoc StreamSink::SetVideoZoom
	void SetVideoZoom(VideoZoomMode zoom) override;

	/// @copydoc StreamSink::SetVideoMute
	void SetVideoMute(bool muted) override;

	/// @copydoc StreamSink::SetSubtitleMute
	void SetSubtitleMute(bool muted) override;

	/// @copydoc StreamSink::SetSubtitlePtsOffset
	void SetSubtitlePtsOffset(std::uint64_t pts_offset) override;

	/// @copydoc StreamSink::SetAudioVolume
	void SetAudioVolume(int volume) override;

	/// @copydoc StreamSink::Discontinuity
	bool Discontinuity(AampMediaType mediaType) override;

	/// @copydoc StreamSink::CheckForPTSChangeWithTimeout
	bool CheckForPTSChangeWithTimeout(long timeout) override;

	/// @copydoc StreamSink::IsCacheEmpty
	bool IsCacheEmpty(AampMediaType mediaType) override;

	/// @copydoc StreamSink::ResetEOSSignalledFlag
	void ResetEOSSignalledFlag() override;

	/// @copydoc StreamSink::NotifyFragmentCachingComplete
	void NotifyFragmentCachingComplete() override;

	/// @copydoc StreamSink::NotifyFragmentCachingOngoing
	void NotifyFragmentCachingOngoing() override;

	/// @copydoc StreamSink::GetVideoSize
	void GetVideoSize(int &w, int &h) override;

	/// @copydoc StreamSink::QueueProtectionEvent
	void QueueProtectionEvent(
		const char *protSystemId,
		const void *ptr,
		size_t len,
		AampMediaType type) override;

	/// @copydoc StreamSink::ClearProtectionEvent
	void ClearProtectionEvent() override;

	/// @copydoc StreamSink::SignalTrickModeDiscontinuity
	void SignalTrickModeDiscontinuity() override;

	/// @copydoc StreamSink::SeekStreamSink
	void SeekStreamSink(double position, double rate) override;

	/// @copydoc StreamSink::GetVideoRectangle
	std::string GetVideoRectangle() override;

	/// @copydoc StreamSink::StopBuffering
	void StopBuffering(bool forceStop) override;

	/// @copydoc StreamSink::SetTextStyle
	bool SetTextStyle(const std::string &options) override;

	/// @copydoc StreamSink::GetVideoPlaybackQuality
	PlaybackQualityStruct *GetVideoPlaybackQuality() override;

	/// @copydoc StreamSink::SignalSubtitleClock
	bool SignalSubtitleClock() override;

	/// @copydoc StreamSink::SetPauseOnStartPlayback
	void SetPauseOnStartPlayback(bool enable) override;

	/// @copydoc StreamSink::NotifyInjectorToResume
	void NotifyInjectorToResume() override;

	/// @copydoc StreamSink::NotifyInjectorToPause
	void NotifyInjectorToPause() override;

	/// @copydoc StreamSink::UnblockTrackInjection
	void UnblockTrackInjection(AampMediaType type) override;

	/// @copydoc StreamSink::SetStreamCaps
	void SetStreamCaps(AampMediaType type, MediaCodecInfo &&codecInfo) override;

	/// @copydoc StreamSink::IsAssociatedAamp
	bool IsAssociatedAamp(PrivateInstanceAAMP *aampInstance) override;

	/// @copydoc StreamSink::ChangeAamp
	void ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback) override;

	/// @copydoc StreamSink::SetEncryptedAamp
	void SetEncryptedAamp(PrivateInstanceAAMP *aamp) override;

	/// @copydoc StreamSink::ResetFirstFrame
	void ResetFirstFrame() override;

	// -------------------------------------------------------------------
	// IDirectRialtoCC interface
	// -------------------------------------------------------------------

	/// @copydoc IDirectRialtoCC::setTextTrackIdentifier
	bool setTextTrackIdentifier(const std::string &id) override;

	/// @copydoc IDirectRialtoCC::setCCMute
	bool setCCMute(bool muted) override;

	/// @brief Start periodic MonitorProgress() reporting.
	///
	/// Fires immediately on first call, then at configured interval.
	/// If called while already running, kicks the timer for immediate dispatch.
	void StartProgressTimer();

	/// @brief Stop periodic MonitorProgress() reporting.
	void StopProgressTimer();

	/// @brief Timer tick handler that forwards progress to AAMP.
	///
	/// Public for testing purposes; called internally by the progress timer.
	void OnProgressTimerTick();

private:
	/**
	 * @brief Bridges Rialto client log messages into AAMP's logging system.
	 */
	class RialtoLogHandler : public firebolt::rialto::IClientLogHandler
	{
	public:
		void log(
			Level level,
			const std::string &file,
			int line,
			const std::string &function,
			const std::string &message) override;
	};

	/// Current playback rate, updated by Flush().  Used by
	/// GetPositionMilliseconds() to mirror GStreamer's rate multiplication:
	///   elapsed * rate
	/// giving a negative delta for reverse trickplay (rate < 0).
	std::atomic<int> m_rate{1};

	/// Pending playback rate staged by Flush() for the current flush cycle.
	/// Becomes active in m_rate once all sources report SourceFlushedEvent.
	std::atomic<int> m_pendingFlushRate{1};

	/// Set to true once the first PLAYING playback state is forwarded to
	/// the notifiable.  Reset to false on each Configure() call so that
	/// re-tunes correctly forward the first-frame notification again.
	std::atomic<bool> m_firstFrameNotified{false};

	/// Last known stream duration in milliseconds, updated via
	/// notifyDuration callbacks from the Rialto server.
	std::atomic<int64_t> m_durationMs{0};

	/// Guards m_lastKnownPts / m_ptsUpdatedTimeMs, which must be read and
	/// updated together by CheckForPTSChangeWithTimeout().
	std::mutex m_ptsCheckMutex;

	/// Video PTS observed on the previous CheckForPTSChangeWithTimeout()
	/// call, and the steady-clock time (ms) at which it last changed.
	/// Reset by Stop(), mirroring InterfacePlayerRDK.
	long long m_lastKnownPts{0};
	long long m_ptsUpdatedTimeMs{0};

	/// Current video rectangle stored as "x,y,w,h".  Updated by
	/// SetVideoRectangle() and returned by GetVideoRectangle().
	std::string m_videoRectangle;

	PrivateInstanceAAMP *m_aamp;                           ///< Owning AAMP instance

	id3_callback_t m_ID3MetadataHandler;                   ///< Function to call to generate the JS event for in ID3 packet

	IStreamSinkNotifiable *m_notifiable{nullptr};          ///< Playback-state notifier (not owned)
	/// Owned adapter wrapping PrivateInstanceAAMP as an IStreamSinkNotifiable.
	/// Non-null only when no test notifiable was injected.
	std::unique_ptr<IStreamSinkNotifiable> m_notifiableAdapter;

	std::shared_ptr<RialtoLogHandler> m_rialtoLogHandler; ///< Rialto log bridge
	/// Rialto pipeline factory; null until Configure() calls createFactory().
	std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> m_pipelineFactory;
	/// Rialto pipeline capabilities; created once in Configure() so that
	/// computeAppliedRate() and any future callers share a single instance.
	std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities>
		m_pipelineCapabilities;
	/// Control backend used to wait for ApplicationState::RUNNING before
	/// creating the media pipeline.
	std::unique_ptr<IRialtoControlBackend> m_controlBackend;
	/// Callable for creating per-track media source objects.
	SourceCreator m_sourceCreator;
	std::shared_ptr<AampRialtoMediaPipelineClient> m_client;
	std::shared_ptr<firebolt::rialto::IMediaPipeline> m_pipeline;

	/// DRM bridge used to create key sessions and obtain mks_ids.
	std::shared_ptr<IDrmBridge> m_drmBridge;

	// -----------------------------------------------------------------------
	// Per-source state (polymorphic source hierarchy)
	// -----------------------------------------------------------------------

	/// Maximum number of source types (VIDEO=0, AUDIO=1, SUBTITLE=2).
	static constexpr size_t kMaxSourceTypes = 3;

	/// Per-track source objects.  Each entry owns pacing state, demuxer,
	/// DRM session, codec data, and type-specific metadata.
	std::array<std::unique_ptr<AampRialtoMediaSource>, kMaxSourceTypes>
		m_sources;

	/// Player-level protection buffer.  QueueProtectionEvent stores here
	/// unconditionally so that protection data survives even if no source
	/// exists yet (i.e. before the first Configure call).  Configure
	/// applies any pending protection to newly created sources.
	/// Guarded by m_pendingProtectionMutex: written from the DRM/manifest
	/// thread, read from the injector thread in AttachSource().
	std::array<std::optional<AampRialtoMediaSource::ProtectionParams>, kMaxSourceTypes>
		m_pendingProtection;
	std::mutex m_pendingProtectionMutex;

	/// Deferred attachment buffer.  Rialto's GStreamer pipeline requires
	/// video to be attached before audio.  When a non-video init fragment
	/// arrives before video is attached, its codec info is buffered here
	/// (protected by m_attachMutex).  Once video attaches, any deferred
	/// sources are processed.
	std::array<std::optional<MediaCodecInfo>, kMaxSourceTypes>
		m_pendingAttach;

	/// Look up a source by media type; returns nullptr if not created.
	AampRialtoMediaSource *getSource(AampMediaType type);

	/// Find a source by its Rialto-assigned source ID; returns nullptr if
	/// no source matches.
	AampRialtoMediaSource *findSourceByRialtoId(int32_t rialtoSourceId);

	/// Serialises AttachSource / CheckAllSourcesAttached so that concurrent
	/// init-fragment delivery from multiple download threads cannot race.
	std::mutex m_attachMutex;

	/// Mutex guarding the flush-complete condition variable.
	std::mutex m_flushMutex;

	/// Signalled by OnSourceFlushed() when all sources have finished
	/// flushing, allowing Configure() to proceed safely.
	std::condition_variable m_flushCv;

	/**
	 * @brief Block until any in-progress flush cycle completes.
	 *
	 * Acquires m_flushMutex and waits on m_flushCv until the state machine
	 * is no longer FLUSHING.
	 *
	 * Used by Configure(), Stop(), and (via a separate claim step) Flush().
	 * The claim-FLUSHING step in Flush() is done in a separate locked section
	 * after the teardown check so that Stop() — which also calls this — never
	 * deadlocks on a FLUSHING state claimed by the same Flush() call.
	 */
	void WaitForFlushToComplete();

	/// Position (ns) stored by Flush(); used to set the initial GStreamer
	/// segment via setSourcePosition() once each source is attached.
	/// -1 means no flush position has been set yet.
	std::atomic<int64_t> m_pendingPositionNs{-1};

	/// Position (ns) the current segment actually started at: either the
	/// value consumed by AttachSource() when the video source newly attaches
	/// (covers the first tune, and content types where Flush() pre-stages a
	/// position before any source is attached - see DoStreamSinkFlushOnDiscontinuity
	/// in priv_aamp.cpp), or the value committed by the SEEK_DONE handler for
	/// a later mid-playback Flush()/seek.  Used by GetPositionMilliseconds()
	/// as the segment-start baseline instead of the first injected sample's
	/// own PTS, since a flush/seek position can land mid-fragment.
	std::atomic<int64_t> m_segmentStartPositionNs{0};

	/// Set by Stream(); cleared once play() is issued.  Lets us defer the
	/// play() call until after allSourcesAttached() so the Rialto server
	/// transitions PAUSED→PLAYING only after all sources are registered.
	std::atomic<bool> m_playRequested{false};

	/// Tracks whether allSourcesAttached() was successfully sent for the
	/// current pipeline session.  Reset to false on pipeline rebuild.
	///
	/// IMPORTANT — seq_cst rendezvous with m_playRequested:
	///   Stream() stores m_playRequested=true (seq_cst) THEN loads this flag
	///   (seq_cst).  CheckAllSourcesAttached() stores this flag=true (seq_cst)
	///   THEN loads m_playRequested (seq_cst).  The seq_cst total order
	///   guarantees one side always observes the other's write and calls
	///   play().  A mutex-based state read does NOT participate in that total
	///   order and cannot substitute for this atomic.
	std::atomic<bool> m_allSourcesAttachedFlag{false};

	/// True when a discontinuity/retune has left the player without an
	/// established position for the new period.  Set by Discontinuity();
	/// cleared once Flush() resolves a position (onFlushComplete()/
	/// SEEK_DONE).  Generalizes the old DISCONTINUITY state to any
	/// "position not yet known" window - notably HLS fMP4, where
	/// ProcessPendingDiscontinuity() issues no Flush() of its own and
	/// MaybeFlushForPendingPosition() must supply one once the new
	/// period's first sample is demuxed.  Not armed on an initial/fresh
	/// Configure(): there is no prior position to invalidate, so the
	/// first sample of a new tune is injected without an implicit flush.
	std::atomic<bool> m_positionPending{false};

	/// Claimed by the first sample that drives the deferred implicit Flush(),
	/// so that concurrent video/audio injector threads cannot both trigger
	/// it.  Reset whenever m_positionPending is (re)armed.
	std::atomic<bool> m_pendingPositionFlushClaimed{false};

	/// Issue the deferred Flush() using @p position, if this sample is the
	/// first from the track elected to supply the new period's PTS.  No-op
	/// unless m_positionPending is true.
	void MaybeFlushForPendingPosition(AampMediaType mediaType, double position);

	/// Cached subtitle mute state.  Set by SetSubtitleMute() and re-applied
	/// via m_pipeline->setMute() whenever the subtitle source first attaches.
	bool m_subtitleMuted{true};

	/// Cached audio volume (0-100), matching PrivateInstanceAAMP::audio_volume.
	/// Set by SetAudioVolume() and re-applied via applyAudioVolume() whenever
	/// the pipeline is (re)created or the audio source newly attaches.
	int m_audioVolume{100};

	/// Cached video mute state.  Set by SetVideoMute() and re-applied via
	/// m_pipeline->setMute() whenever the video source first attaches.
	bool m_videoMuted{false};

	/// Backing storage for the pointer returned by GetVideoPlaybackQuality().
	/// Overwritten on each call; not re-applied anywhere (read-only query).
	PlaybackQualityStruct m_playbackQuality{};

	/// @brief Embedded progress timer with immediate-start and kick capability.
	///
	/// Fires immediately on start, then continues at specified interval.
	/// Can be kicked to force immediate dispatch while maintaining interval.
	class ProgressTimer
	{
	public:
		using Callback = std::function<void()>;

		ProgressTimer() = default;
		~ProgressTimer();

		/// Start the timer: fires immediately, then at interval.
		/// Does nothing if already running.
		void start(guint interval_ms, Callback cb);

		/// Force immediate callback dispatch and restart the interval.
		void kick();

		/// Stop the timer and clean up resources.
		void stop();

		/// Return true if the timer is currently running.
		bool isRunning() const { return started; }

	private:
		guint interval = 0;
		Callback callback;

		guint source_id = 0;
		bool started = false;

	private:
		// Periodic timeout handler (will be called with AampRialtoPlayer as data)
		static gboolean timeout_handler(gpointer data);

		// Run callback once
		void runOnce();

		friend class AampRialtoPlayer;
	};

	/// Progress timer instance.
	std::unique_ptr<ProgressTimer> m_progressTimer;

	/// AV health monitor.  Non-null only when eAAMPConfig_MonitorAV is true
	/// and a pipeline has been successfully created.
	std::unique_ptr<AampRialtoMonitorAV> m_monitorAV;

	/// GoF State-pattern state machine tracking the player lifecycle.
	PlayerStateMachine m_stateMachine;

	/// @brief Dispatches need-data events from the pipeline client.
	void OnNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId);

	/// @brief Dispatches cancel-need-data events from the pipeline client.
	void OnCancelNeedMediaData(int32_t sourceId);

	/// @brief Called (via callback) when the Rialto server changes state.
	void OnPlaybackState(firebolt::rialto::PlaybackState state);

	/// @brief Build the CC decoder handle used by NotifyFirstFrameReceived.
	unsigned long GetCCHandle() const;

	/// @brief Called when the Rialto server reports a new playback position.
	void OnPosition(int64_t positionNs);

	/// @brief Called when the Rialto server reports the stream duration.
	void OnDuration(int64_t durationNs);

	/// @brief Called when Rialto reports that a media source has run dry.
	void OnBufferUnderflow(int32_t sourceId);

	/// @brief Called when Rialto reports a non-fatal playback error.
	void OnPlaybackError(int32_t sourceId, firebolt::rialto::PlaybackError error);

	/// @brief Called when Rialto confirms a source flush is complete.
	///
	/// Clears the flushing flag on the source and calls
	/// setSourcePosition() now that the server has confirmed the flush.
	/// This ensures the SEGMENT event is not discarded while the server
	/// is still processing the flush.
	void OnSourceFlushed(int32_t sourceId);

	/**
	 * @brief Attach a source via its polymorphic attachOrUpdate method.
	 *
	 * Caller must hold m_attachMutex and verify m_pipeline is non-null.
	 */
	void AttachSource(AampRialtoMediaSource &source, MediaCodecInfo &codecInfo);

	/**
	 * @brief Apply m_audioVolume to the current pipeline.
	 *
	 * Mirrors the GStreamer path (InterfacePlayerRDK::SetVolumeOrMuteUnMute):
	 * volume 0 mutes the audio source without touching setVolume(); any other
	 * value unmutes the audio source (if attached) and forwards the 0-100
	 * value as 0.0-1.0 to IMediaPipeline::setVolume().  Does nothing if no
	 * pipeline exists yet.
	 */
	void applyAudioVolume();

	/**
	 * @brief Compute the appliedRate to pass to setSourcePosition().
	 *
	 * Only the video source's segment carries a trickplay applied_rate,
	 * so @p type must be eMEDIATYPE_VIDEO for the isVideoMaster IPC query
	 * to run at all; any other type (or normal play rate) short-circuits
	 * to 1.0 without contacting Rialto.
	 */
	double computeAppliedRate(int candidateRate, AampMediaType type) const;

	/**
	 * @brief Call allSourcesAttached() once every expected source has
	 *        been attached.
	 */
	void CheckAllSourcesAttached();

	/**
	 * @brief Clear (ungate) injection on every source, logging the reason.
	 *
	 * Must be called only from the specific points where pipeline play()
	 * is actually issued or confirmed — Stream(), CheckAllSourcesAttached(),
	 * Pause(false), StopBuffering(), the SEEK_DONE play branch, and the
	 * PLAYING playback-state handler.  See the gateMode field
	 * comment in AampRialtoMediaSource.h for the rationale.
	 *
	 * @param reason  Short human-readable description of the caller,
	 *                included in the per-source log line.
	 */
	void UngateAllSources(const char *reason);

	/**
	 * @brief Return true when Configure() must recreate the pipeline.
	 *
	 * Rialto does not support dynamic source management; any change to
	 * the source set requires a full pipeline teardown and recreation.
	 * The exception is audio transitioning to FORMAT_INVALID (trickplay),
	 * which is signalled as EOS on the audio source rather than a
	 * pipeline rebuild.
	 *
	 * @param videoFormat                  Requested video format.
	 * @param audioFormat                  Requested audio format.
	 * @param subFormat                    Requested subtitle format.
	 * @param bESChangeStatus              True when an ES change forces
	 *                                     recreation.
	 * @param setReadyAfterPipelineCreation True when track-ID mismatch
	 *                                     forces recreation.
	 * @return true if the pipeline must be recreated; false otherwise.
	 */
	bool ShouldRecreatePipeline(
		StreamOutputFormat videoFormat,
		StreamOutputFormat audioFormat,
		StreamOutputFormat subFormat,
		bool bESChangeStatus,
		bool setReadyAfterPipelineCreation) const;
};

#endif // AAMP_RIALTO_PLAYER_H
