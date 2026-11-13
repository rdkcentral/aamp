/*
- * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

#ifndef MEDIASTREAMCONTEXT_H
#define MEDIASTREAMCONTEXT_H

/**
 * @file MediaStreamContext.h
 * @brief Handles operations for Media stream context
 */

#include "StreamAbstractionAAMP.h"
#include "fragmentcollector_mpd.h"
#include "SegmentIndex.h"

/**
 * @class MediaStreamContext
 * @brief MPD media track
 */
class MediaStreamContext : public MediaTrack
{
public:
	/**
	 * @brief MediaStreamContext Constructor
	 * @param type Type of track
	 * @param ctx context  MPD collector context
	 * @param aamp Pointer to associated aamp instance
	 * @param name Name of the track
	 */
	MediaStreamContext(TrackType type, StreamAbstractionAAMP_MPD* ctx,
			PrivateInstanceAAMP* aamp, const char* name) :
			MediaTrack(type, aamp, name),
			mediaType((AampMediaType)type),
			context(ctx)
	{
		AAMPLOG_INFO("[%s] Create new MediaStreamContext",
				GetMediaTypeName(mediaType));
		mPlaylistUrl = aamp->GetManifestUrl();
		fragmentDescriptor.bUseMatchingBaseUrl =
				ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
		mTimeBasedBufferManager =
				std::make_shared<aamp::AampTimeBasedBufferManager>(
						GETCONFIGVALUE(eAAMPConfig_MaxDownloadBuffer),
						std::abs(aamp->rate), mediaType);
	}

	/**
	 * @brief MediaStreamContext Destructor
	 */
	~MediaStreamContext() override
	{
		// Join the buffer monitor while GetContext() is still callable
		StopBufferMonitor();
	}

	/**
	 * @brief MediaStreamContext Copy Constructor
	 */
	MediaStreamContext(const MediaStreamContext&) = delete;

	/**
	 * @brief MediaStreamContext Assignment operator overloading
	 */
	MediaStreamContext& operator=(const MediaStreamContext&) = delete;

	/**
	 * @brief Get the context of media track. To be implemented by subclasses
	 * @retval Context of track.
	 */
	StreamAbstractionAAMP *GetContext() override
	{
		return context;
	}

	/**
	 * @fn InjectFragmentInternal
	 *
	 * @param[in] cachedFragment - contains fragment to be processed and injected
	 * @param[out] fragmentDiscarded - true if fragment is discarded.
	 */
	void InjectFragmentInternal(CachedFragment *cachedFragment, bool &fragmentDiscarded, bool isDiscontinuity = false) override;

	/**
	 * @fn CacheFragment
	 * @param fragmentUrl url of fragment
	 * @param curlInstance curl instance to be used to fetch
	 * @param position position of fragment in seconds
	 * @param duration duration of fragment in seconds
	 * @param range byte range
	 * @param initSegment true if fragment is init fragment
	 * @param discontinuity true if fragment is discontinuous
	 * @param playingAd flag if playing Ad
	 * @param pto unscaled pto value from mpd
	 * @param scale timeScale value from mpd
	 * @retval true on success
	 */
	bool CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range = NULL, bool initSegment = false, bool discontinuity = false, bool playingAd = false, uint32_t scale = 0);

	/**
	 * @fn CacheTsbFragment
	 * @param[in] fragment TSB fragment pointer (must be passed with std::move)
	 * @retval true on success
	 */
	bool CacheTsbFragment(std::shared_ptr<CachedFragment> &&fragment);

	/**
	 * @fn CacheFragmentChunk
	 * @param actualType AampMediaType type of cached media
	 * @param ptr CURL provided chunk data
	 * @param size CURL provided chunk data size
	 * @param remoteUrl url of fragment
	 * @param dnldStartTime of the download
	 * @param durationInTicks duration of the chunk in ticks
	 */
	bool CacheFragmentChunk(AampMediaType actualType, const uint8_t *ptr, size_t size, std::string remoteUrl, uint64_t dnldStartTime, uint64_t durationInTicks);

	/**
	 * @fn TransferFragmentBuffer
	 * @brief Transfer buffer data into a CachedFragment using the appropriate
	 *        semantics for the caching mode.
	 */
	static void TransferFragmentBuffer(CachedFragment *cached,
									   const uint8_t *chunkPayload,
									   std::vector<uint8_t> *downloadBuffer,
									   size_t payloadSize,
									   bool isChunkMode);

	/**
	 * @fn PopulateCommonMetadata
	 * @brief Populate common metadata shared by fragment and chunk caching paths.
	 */
	static void PopulateCommonMetadata(CachedFragment* cached,
			std::string url,
			AampMediaType mediaType,
			int profileIndex,
			bool isInitSegment,
			bool isDiscontinuity);

	/**
	 * @fn ProcessInitSegmentIfNeeded
	 * @brief Parse an init segment and extract the timescale.
	 */
	static uint32_t ProcessInitSegmentIfNeeded(const CachedFragment* cached,
			bool isInitSegment);

	/**
	 * @fn ABRProfileChanged
	 */
	void ABRProfileChanged(void) override;

	/**
	 * @fn updateSkipPoint
	 * @param skipPoint - indicates at what position fragments to be skipped
	 * @param skipDuration - duration of fragments to be skipped
	 * @return void
	 */
	void updateSkipPoint(double position, double duration ) override;

	/**
	 * @fn setDiscontinuityState
	 * @param isDiscontinuity - true if discontinuity false otherwise
	 * @return void
	 */
	void setDiscontinuityState(bool isDiscontinuity) override;

	/**
	 * @fn abortWaitForVideoPTS
	 * @return void
	 */
	void abortWaitForVideoPTS() override;

	/**
	 * @fn GetBufferedDuration
	 */
	double GetBufferedDuration() override;

	/**
	 * @brief Get the absolute end position of the last downloaded fragment
	 *        (position + duration). This tracks the live downloader's leading
	 *        edge and is independent of GStreamer injection state.
	 * @return last downloaded fragment end position in seconds
	 */
	double GetLastDownloadedPosition() override { return lastDownloadedPosition.load(); }

	/**
	 * @brief Manifest-declared timescale for this representation (DASH
	 *        SegmentTemplate\@timescale), populated during manifest parsing.
	 * @return manifest timescale; defaults to 1 when no timescale is declared
	 */
	uint32_t GetManifestTimeScale() const override { return fragmentDescriptor.TimeScale; }

	/**
	 * @fn SignalTrickModeDiscontinuity
	 * @return void
	 */
	void SignalTrickModeDiscontinuity() override;

	/**
	 * @fn IsAtEndOfTrack
	 * @return true - If yes
	 */
	bool IsAtEndOfTrack() override;

	/**
	 * @fn GetPlaylistUrl
	 * @return string - playlist URL
	 */
	std::string& GetPlaylistUrl() override;

	/**
	 * @fn GetEffectivePlaylistUrl
	 * @return string - original playlist URL(redirected)
	 */
	std::string GetEffectivePlaylistUrl() override;

	/**
	 * @fn SetEffectivePlaylistUrl
	 * @param string - original playlist URL
	 */
	void SetEffectivePlaylistUrl(std::string url) override;

	/**
	 * @fn GetLastPlaylistDownloadTime
	 * @return lastPlaylistDownloadTime
	 */
	long long GetLastPlaylistDownloadTime() override;

	/**
	 * @fn SetLastPlaylistDownloadTime
	 * @param[in] time last playlist download time
	 * @return void
	 */
	void SetLastPlaylistDownloadTime(long long time) override;

	/**
	 * @fn GetMinUpdateDuration
	 * @return minimumUpdateDuration
	 */
	long GetMinUpdateDuration() override;

	/**
	 * @fn GetDefaultDurationBetweenPlaylistUpdates
	 * @return maxIntervalBtwPlaylistUpdateMs
	 */
	int GetDefaultDurationBetweenPlaylistUpdates() override;

	/**
	 * @fn ProcessPlaylist
	 * @param[in] newPlaylist buffer
	 * @param[in] http error code
	 * @return void
	 */
	void ProcessPlaylist(std::vector<uint8_t>& newPlaylist, int http_error) override {};
	/**
	 * @fn resetAbort
	 * @param[in] bool value
	 */

	void resetAbort(bool value)
	{
		abort = value;
	}
	/**
	 * @fn GetLastInjectedPosition
	 * @return LastInjectedposition of type double
	 */
	double GetLastInjectedPosition()
	{
		return GetLastInjectedFragmentPosition( );
	}

	/**
	 * @fn OnFragmentDownloadSuccess
	 * @brief Function called on fragment download success
	 * @param[in] downloadInfo - download information
	 */
	void OnFragmentDownloadSuccess(DownloadInfoPtr downloadInfo);

	/**
	 * @fn OnFragmentDownloadFailed
	 * @brief Callback on fragment download failure
	 * @param[in] downloadInfo - download information
	 */
	void OnFragmentDownloadFailed(DownloadInfoPtr downloadInfo);

	/**
	 * @fn DownloadFragment
	 * @brief Download submitted fragment
	 * @param[in] downloadInfo - download information
	 *
	 * @return true on success
	 */
	bool DownloadFragment(DownloadInfoPtr downloadInfo);

	AampMediaType mediaType;
	struct FragmentDescriptor fragmentDescriptor;
	const IAdaptationSet *adaptationSet{nullptr};
	const IRepresentation *representation{nullptr};
	int fragmentIndex{0};
	int timeLineIndex{0};
	int fragmentRepeatCount{0};
	uint64_t fragmentOffset{0};
	bool eos{false};
	bool profileChanged{true};
	bool discontinuity{false};
	std::vector<uint8_t> mDownloadedFragment{};	/**< Fragment stored across ABR profile changes */
	std::vector<uint8_t> mTempFragment{};		/**< Scratch buffer for init/download fragments */

	double fragmentTime{0}; // Absolute Fragment time from Availability start
	std::atomic<double> lastDownloadedPosition{0};
	double periodStartOffset{0};
	uint64_t timeStampOffset{0};
	SegmentIndex IDX{};				/**< Index (sidx) data for DASH byte-range segments; thread-safe */
	uint64_t lastSegmentTime{0};    // zeroed at start of period and also 0 when first segment of an ad has been sent otherwise fragmentDescriptor.Time
	uint64_t lastSegmentNumber{0};
	uint64_t lastSegmentDuration{0};   //lastSegmentTime+ duration of that segment
	int adaptationSetIdx{0};
	int representationIndex{0};
	StreamAbstractionAAMP_MPD* context;
	std::string initialization{};
	uint32_t adaptationSetId{0};
	bool adaptationSetIdValid{false};      /**< true once adaptationSetId reflects a resolved selection; guards reorder detection on first use */
	bool mSkipSegmentOnError{true};
	double scaledPTO{0};
	bool failAdjacentSegment{false};
	int httpErrorCode{0};
	std::string mPlaylistUrl{};
	std::string mEffectiveUrl{}; 		/**< uri associated with downloaded playlist (takes into account 302 redirect) */
	bool freshManifest{false};
	int nextfragmentIndex{-1}; //CMCD get next index to fetch url from Segment List
	bool mReachedFirstFragOnRewind{false}; /**< flag denotes if we reached the first fragment in a period on rewind */
	std::mutex fetchChunkBufferMutex;
	DownloadInfoPtr mActiveDownloadInfo{nullptr};
	std::recursive_mutex mMediaStreamContextMutex; /**< Recursive mutex to protect MediaStreamContext state during ABR profile changes and playlist updates */

protected:
	CachedFragment mStagingFragment{};		/**< Staging buffer for fragment download in progress */

private:
	/**
	 * @fn CacheStagingFragmentForInjection
	 * @brief Copy the staging fragment into a chunk-cache slot for the inject
	 *        thread (non-LLD DASH path only).
	 */
	void CacheStagingFragmentForInjection();
}; // MediaStreamContext

#endif /* MEDIASTREAMCONTEXT_H */
