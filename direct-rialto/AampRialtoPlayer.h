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
 * @brief Rialto-based player for AAMP — delegates all StreamSink calls to
 *        an internally owned AAMPGstPlayer instance.
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

class AAMPGstPlayer;
class PrivateInstanceAAMP;
class PrivateInstanceAAMPNotifiable;
class AampRialtoMediaPipelineClient;
class Mp4Demux;

/**
 * @class AampRialtoPlayer
 * @brief StreamSink implementation that delegates media-pipeline operations
 *        to an internally owned AAMPGstPlayer.
 *
 * AampRialtoPlayer provides the same public interface as StreamSink while
 * encapsulating an AAMPGstPlayer instance, allowing callers to swap in a
 * Rialto-aware sink without changing the rest of the AAMP pipeline.
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
		std::function<void(const unsigned char *, int, int, int)> exportFrames = nullptr);

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
	bool SendSample(AampMediaType mediaType, AampMediaSample &sample) override;

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


	std::atomic<int64_t> m_positionMs{0};

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
	std::shared_ptr<AampRialtoMediaPipelineClient> m_client;
	std::shared_ptr<firebolt::rialto::IMediaPipeline> m_pipeline;
	std::unique_ptr<Mp4Demux> m_videoDemuxer;
	std::unique_ptr<Mp4Demux> m_audioDemuxer;
	std::unique_ptr<Mp4Demux> m_subtitleDemuxer;
	int32_t m_videoSourceId{-1};  ///< Rialto source ID for the video track
	int32_t m_audioSourceId{-1};  ///< Rialto source ID for the audio track
	int32_t m_videoWidth{0};      ///< Video frame width (pixels)
	int32_t m_videoHeight{0};     ///< Video frame height (pixels)
	int32_t m_audioSampleRate{0}; ///< Audio sample rate (Hz)
	int32_t m_audioChannels{0};   ///< Audio channel count

	/// Codec data staged by AttachVideoSource/AttachAudioSource and consumed
	/// by SendTransfer's enqueue path, which stamps it onto the first sample
	/// of the next non-init fragment.  Only accessed on the calling thread
	/// (same thread as SendTransfer), so no mutex is required.
	std::shared_ptr<firebolt::rialto::CodecData> m_pendingVideoCodecData;
	std::shared_ptr<firebolt::rialto::CodecData> m_pendingAudioCodecData;

	/// DRM bridge used to create key sessions and obtain mks_ids.
	std::shared_ptr<IDrmBridge> m_drmBridge;
	/// Rialto MediaKeySession IDs for each track (-1 = no active DRM session).
	int32_t m_videoMksId{-1};
	int32_t m_audioMksId{-1};

	/**
	 * @brief Protection parameters saved by QueueProtectionEvent.
	 *
	 * createSession() is deferred until the init fragment arrives so that the
	 * license pre-fetcher has had time to acquire the license first, making
	 * the DrmSessionManager::createDrmSession() call non-blocking in the
	 * common case.
	 */
	struct ProtectionParams
	{
		std::string          systemId;
		std::vector<uint8_t> initData;
		AampMediaType        type;
	};
	std::optional<ProtectionParams> m_videoProt;
	std::optional<ProtectionParams> m_audioProt;

	// -----------------------------------------------------------------------
	// Segment injection pacing
	// -----------------------------------------------------------------------

	/// Serialises AttachVideoSource / AttachAudioSource / CheckAllSourcesAttached
	/// so that concurrent init-fragment delivery from the video and audio
	/// download threads cannot race on m_videoSourceId / m_audioSourceId.
	std::mutex m_attachMutex;

	/**
	 * @brief Per-track pacing state used to coordinate Rialto's
	 *        needData/haveData IPC handshake with AAMP's push-style
	 *        SendTransfer calls without an internal sample queue.
	 *
	 * SendTransfer (running on AAMP's per-track injector thread) blocks on
	 * @c cv until a needData request arrives from the Rialto server.  Each
	 * sample is then injected directly via @c IMediaPipeline::addSegment;
	 * once the requested @c frameCount has been reached, @c haveData(OK) is
	 * called and the request is cleared.  AAMP's existing fragment cache
	 * therefore acts as the only buffer between the network and the
	 * pipeline, eliminating the in-class sample queue.
	 */
	struct SourceState
	{
		std::mutex              mu;
		std::condition_variable cv;

		bool     hasPending{false};      ///< True while a needData request is open
		uint32_t pendingRequestId{0};    ///< Token returned via haveData()
		size_t   pendingFrameCount{0};   ///< Max segments to deliver for this request
		size_t   addedInPending{0};      ///< Segments accepted so far for this request

		bool eos{false};                 ///< Set by EndOfStreamReached()

		/// Bumped by Flush()/Stop() to invalidate any in-flight SendTransfer
		/// batch.  SendTransfer captures the current value at entry and aborts
		/// when it observes a different value.
		uint64_t generation{0};
	};

	SourceState m_videoSrc;
	SourceState m_audioSrc;

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

	/**
	 * @brief Block until a needData request arrives for @p sourceId, then
	 *        deliver one sample via @c addSegment.
	 *
	 * Called by SendTransfer() once per demuxed sample.  The call returns
	 * after @c addSegment has been issued (and @c haveData when the current
	 * request is satisfied), or earlier if the batch was invalidated by
	 * @c Flush()/@c Stop().
	 *
	 * @param[in]      sourceId   Rialto source identifier.
	 * @param[in,out]  st         Per-source pacing state.
	 * @param[in]      capturedGen Generation value captured by SendTransfer
	 *                             at the start of the current batch.
	 * @param[in]      sample     Demuxed sample (moved in).
	 * @param[in]      isVideo    True for video, false for audio.
	 * @param[in]      width      Frame width (video only).
	 * @param[in]      height     Frame height (video only).
	 * @param[in]      sampleRate Sample rate (audio only).
	 * @param[in]      channels   Channel count (audio only).
	 * @param[in]      codecData  Optional codec data to stamp on the segment.
	 * @returns                   True if @p sample was injected; false if the
	 *                            batch was aborted (caller should stop the loop).
	 */
	bool InjectOneSample(
		int32_t sourceId,
		SourceState &st,
		uint64_t capturedGen,
		AampMediaSample &&sample,
		bool isVideo,
		int32_t width,
		int32_t height,
		int32_t sampleRate,
		int32_t channels,
		std::shared_ptr<firebolt::rialto::CodecData> codecData);

	/// @brief Dispatches need-data events from the pipeline client to workers.
	void OnNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t requestId);

	/// @brief Dispatches cancel-need-data events from the pipeline client.
	void OnCancelNeedMediaData(int32_t sourceId);

	/// @brief Called (via callback) when the Rialto server changes state.
	void OnPlaybackState(firebolt::rialto::PlaybackState state);

	/// @brief Called when the Rialto server reports a new playback position.
	void OnPosition(int64_t positionNs);

	/// @brief Called when the Rialto server reports the stream duration.
	void OnDuration(int64_t durationNs);

	/// @brief Attach video source after parsing the init segment.
	void AttachVideoSource(Mp4Demux &demuxer);

	/// @brief Attach audio source after parsing the init segment.
	void AttachAudioSource(Mp4Demux &demuxer);

	/**
	 * @brief Call allSourcesAttached() once every expected source has
	 *        been attached.
	 */
	void CheckAllSourcesAttached();
};

#endif // AAMP_RIALTO_PLAYER_H
