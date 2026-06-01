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
class AampRialtoPlayer : public StreamSink
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

	/// Set to true once the first PLAYING playback state is forwarded to
	/// the notifiable.  Reset to false on each Configure() call so that
	/// re-tunes correctly forward the first-frame notification again.
	std::atomic<bool> m_firstFrameNotified{false};

	/// Last known stream duration in milliseconds, updated via
	/// notifyDuration callbacks from the Rialto server.
	std::atomic<int64_t> m_durationMs{0};

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
	std::array<std::optional<AampRialtoMediaSource::ProtectionParams>, kMaxSourceTypes>
		m_pendingProtection;

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

	/// Position (ns) stored by Flush(); used to set the initial GStreamer
	/// segment via setSourcePosition() once each source is attached.
	/// -1 means no flush position has been set yet.
	std::atomic<int64_t> m_pendingFlushPositionNs{-1};

	/// Set by Stream(); cleared once play() is issued.  Lets us defer the
	/// play() call until after allSourcesAttached() so the Rialto server
	/// transitions PAUSED→PLAYING only after all sources are registered.
	std::atomic<bool> m_playRequested{false};

	/// Set by CheckAllSourcesAttached() after allSourcesAttached() succeeds.
	/// Stream() reads this to decide whether it can call play() immediately.
	std::atomic<bool> m_allSourcesAttachedFlag{false};

	/// GoF State-pattern state machine tracking the player lifecycle.
	PlayerStateMachine m_stateMachine;

	/// @brief Dispatches need-data events from the pipeline client.
	void OnNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId);

	/// @brief Dispatches cancel-need-data events from the pipeline client.
	void OnCancelNeedMediaData(int32_t sourceId);

	/// @brief Called (via callback) when the Rialto server changes state.
	void OnPlaybackState(firebolt::rialto::PlaybackState state);

	/// @brief Called when the Rialto server reports a new playback position.
	void OnPosition(int64_t positionNs);

	/// @brief Called when the Rialto server reports the stream duration.
	void OnDuration(int64_t durationNs);

	/// @brief Called when Rialto reports that a media source has run dry.
	void OnBufferUnderflow(int32_t sourceId);

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
	 * @brief Call allSourcesAttached() once every expected source has
	 *        been attached.
	 */
	void CheckAllSourcesAttached();

	/// Set by Stop() to guarantee the next Configure() always recreates
	/// the pipeline even when stream formats are unchanged.
	std::atomic<bool> m_pipelineStopped{false};

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
