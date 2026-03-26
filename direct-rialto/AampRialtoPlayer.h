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

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <cstdint>

class AAMPGstPlayer;
class PrivateInstanceAAMP;
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
	/**
	 * @brief Construct an AampRialtoPlayer and its underlying AAMPGstPlayer.
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

	AampRialtoPlayer(const AampRialtoPlayer &) = delete;
	AampRialtoPlayer &operator=(const AampRialtoPlayer &) = delete;

	/**
	 * @brief Destroy the AampRialtoPlayer and its underlying AAMPGstPlayer.
	 */
	~AampRialtoPlayer() override;

	// -----------------------------------------------------------------------
	// StreamSink overrides — each call is forwarded to mGstPlayer
	// -----------------------------------------------------------------------

	/// @copydoc StreamSink::Configure
	void Configure(
		StreamOutputFormat format,
		StreamOutputFormat audioFormat,
		StreamOutputFormat subFormat,
		bool bESChangeStatus,
		bool setReadyAfterPipelineCreation = false) override;

	/**
	 * @brief Inject a custom pipeline factory before calling Configure().
	 *
	 * For unit testing only.  Passing a non-null factory makes Configure()
	 * use it instead of the production singleton, allowing tests to inject
	 * a MockIMediaPipelineFactory without a live Rialto server.
	 *
	 * @param[in] factory  Factory to use; nullptr restores production behaviour.
	 */
	void SetPipelineFactoryForTesting(
		std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> factory)
	{
		m_pipelineFactory = std::move(factory);
	}

	/**
	 * @brief Install a playback-state observer for testing.
	 *
	 * For unit testing only.  The observer is called whenever
	 * OnPlaybackState() is invoked (i.e. when the pipeline client's
	 * notifyPlaybackState fires).
	 *
	 * @param observer  Callable to invoke with the new PlaybackState.
	 */
	void SetPlaybackObserverForTesting(
		std::function<void(firebolt::rialto::PlaybackState)> observer)
	{
		m_testPlaybackObserver = std::move(observer);
	}

	/**
	 * @brief Simulate a Rialto need-data event (for unit testing only).
	 *
	 * Bypasses the IPC callback path so tests can drive injection without
	 * running a live Rialto server.
	 */
	void OnNeedMediaData(
		int32_t sourceId, size_t frameCount, uint32_t requestId);

	/**
	 * @brief Simulate a Rialto cancel-need-data event (for unit testing only).
	 */
	void OnCancelNeedMediaData(int32_t sourceId);

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

	PrivateInstanceAAMP *m_aamp{nullptr}; ///< Owning AAMP instance
	std::shared_ptr<RialtoLogHandler> m_rialtoLogHandler; ///< Rialto log bridge
	/// Factory override set via SetPipelineFactoryForTesting(); null in production.
	std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> m_pipelineFactory;
#ifdef USE_AAMP_GST_PLAYER
	std::unique_ptr<AAMPGstPlayer> mGstPlayer; ///< Underlying GStreamer player
#else
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

	/// Codec data cached at attachSource time; applied to every MediaSegment.
	std::shared_ptr<firebolt::rialto::CodecData> m_videoCodecData;
	std::shared_ptr<firebolt::rialto::CodecData> m_audioCodecData;

	// -----------------------------------------------------------------------
	// Segment injection thread
	// -----------------------------------------------------------------------

	/**
	 * @brief Pending need-data request from Rialto, queued per source.
	 */
	struct PendingNeedData
	{
		uint32_t requestId;
		size_t   frameCount;
	};

	std::mutex              m_injectorMutex;
	std::condition_variable m_injectorCv;
	std::atomic<bool>       m_stopInjection{false};
	std::thread             m_injectionThread;

	std::deque<AampMediaSample> m_videoSampleQueue;  ///< Buffered video samples
	std::deque<AampMediaSample> m_audioSampleQueue;  ///< Buffered audio samples
	std::deque<PendingNeedData> m_videoPendingReqs;  ///< Pending video need-data
	std::deque<PendingNeedData> m_audioPendingReqs;  ///< Pending audio need-data
	bool m_videoEos{false}; ///< All video data has been queued
	bool m_audioEos{false}; ///< All audio data has been queued

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

	/// Optional observer installed by tests to receive playback state changes.
	std::function<void(firebolt::rialto::PlaybackState)> m_testPlaybackObserver;

	/// @brief Start the segment injection thread (idempotent).
	void StartInjectionThread();

	/// @brief Signal and join the injection thread (idempotent).
	void StopInjectionThread();

	/// @brief Main loop executed by the injection thread.
	void RunInjectionThread();

	/**
	 * @brief Inject buffered samples for one source and call haveData.
	 *
	 * Samples that could not be sent (due to NO_SPACE) are pushed back to
	 * the front of @p requeueDest so they are retried on the next needData
	 * request.  The data pointers inside each AampMediaSample remain valid
	 * until haveData() returns, satisfying the Rialto data-lifetime contract.
	 *
	 * @param sourceId     Rialto source identifier.
	 * @param requestId    The needDataRequestId to close out.
	 * @param samples      Samples to inject (taken by value / moved in).
	 * @param eos          True if these are the last samples for this source.
	 * @param requeueDest  Deque to push rejected samples back to (front).
	 */
	void InjectSamples(
		int32_t sourceId,
		uint32_t requestId,
		std::vector<AampMediaSample> &&samples,
		bool eos,
		std::deque<AampMediaSample> &requeueDest);

	/// @brief Called (via callback) when the Rialto server changes state.
	void OnPlaybackState(firebolt::rialto::PlaybackState state);

	/// @brief Attach video source after parsing the init segment.
	void AttachVideoSource(Mp4Demux &demuxer);

	/// @brief Attach audio source after parsing the init segment.
	void AttachAudioSource(Mp4Demux &demuxer);

	/**
	 * @brief Call allSourcesAttached() once every expected source has
	 *        been attached.
	 */
	void CheckAllSourcesAttached();
#endif
};

#endif // AAMP_RIALTO_PLAYER_H
