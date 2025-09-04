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
    MediaStreamContext(TrackType type, StreamAbstractionAAMP_MPD* ctx, PrivateInstanceAAMP* aamp, const char* name) :
            MediaTrack(type, aamp, name),
            mediaType((AampMediaType)type), adaptationSet(NULL), representation(NULL),
            fragmentIndex(0), timeLineIndex(0), fragmentRepeatCount(0), fragmentOffset(0),
            eos(false), fragmentTime(0), periodStartOffset(0), timeStampOffset(0), IDX("fragment-IDX"),
	        lastSegmentTime(0), lastSegmentNumber(0), lastSegmentDuration(0), adaptationSetIdx(0), representationIndex(0), profileChanged(true),
            adaptationSetId(0), fragmentDescriptor(), context(ctx), initialization(""),
            mDownloadedFragment("downloaded-fragment"), discontinuity(false), mSkipSegmentOnError(true),
            lastDownloadedPosition(0)//,mCMCDNetworkMetrics{-1,-1,-1}
		   , scaledPTO(0)
		   , failAdjacentSegment(false),httpErrorCode(0)
	       , mPlaylistUrl(""), mEffectiveUrl(""),freshManifest(false),nextfragmentIndex(-1)
	       , mReachedFirstFragOnRewind(false),fetchChunkBufferMutex()
    {
        AAMPLOG_INFO("[%s] Create new MediaStreamContext",
            GetMediaTypeName(mediaType));
        mPlaylistUrl = aamp->GetManifestUrl();
        fragmentDescriptor.bUseMatchingBaseUrl = ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
        mTempFragment = std::make_shared<AampGrowableBuffer>("temp");
    }

    /**
     * @brief MediaStreamContext Destructor
     */
    ~MediaStreamContext()
    {
        mDownloadedFragment.Free();
        mTempFragment.reset();
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
    StreamAbstractionAAMP* GetContext() override
    {
        return context;
    }

    /**
     * @fn InjectFragmentInternal
     *
     * @param[in] cachedFragment - contains fragment to be processed and injected
     * @param[out] fragmentDiscarded - true if fragment is discarded.
     */
    void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity=false) override;

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
	 * @param overWriteTrackId flag to overwrite the trackID of the init fragment  with the current one if those are different
     * @retval true on success
     */
    bool CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range = NULL, bool initSegment = false, bool discontinuity = false, bool playingAd = false, double pto = 0, uint32_t scale = 0, bool overWriteTrackId = false);

    /**
     * @fn CacheTsbFragment
     * @param[in] fragment TSB fragment pointer
     * @retval true on success
     */
    bool CacheTsbFragment(std::shared_ptr<CachedFragment> fragment);

    /**
     * @fn CacheFragmentChunk
     * @param actualType AampMediaType type of cached media
     * @param ptr CURL provided chunk data
     * @param size CURL provided chunk data size
     * @param remoteUrl url of fragment
     * @param dnldStartTime of the download
     */
    bool CacheFragmentChunk(AampMediaType actualType, const char *ptr, size_t size, std::string remoteUrl,long long dnldStartTime);

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
    std::string& GetEffectivePlaylistUrl() override;

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
	void ProcessPlaylist(AampGrowableBuffer& newPlaylist, int http_error) override {};
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

    AampMediaType mediaType;
    struct FragmentDescriptor fragmentDescriptor;
    const IAdaptationSet *adaptationSet;
    const IRepresentation *representation;
    int fragmentIndex;
    int timeLineIndex;
    int fragmentRepeatCount;
    uint64_t fragmentOffset;
    bool eos;
    bool profileChanged;
    bool discontinuity;
    AampGrowableBuffer mDownloadedFragment;
    std::shared_ptr<AampGrowableBuffer> mTempFragment;

    double fragmentTime; // Absolute Fragment time from Availability start
    std::atomic<double> lastDownloadedPosition;
    double periodStartOffset;
    uint64_t timeStampOffset;
	AampGrowableBuffer IDX;
    uint64_t lastSegmentTime;       // zeroed at start of period and also 0 when first segment of an ad has been sent otherwise fragmentDescriptor.Time
    uint64_t lastSegmentNumber;
    uint64_t lastSegmentDuration;   //lastSegmentTime+ duration of that segment
    int adaptationSetIdx;
    int representationIndex;
    StreamAbstractionAAMP_MPD* context;
    std::string initialization;
    uint32_t adaptationSetId;
    bool mSkipSegmentOnError;
    double scaledPTO;
    bool failAdjacentSegment;
    int httpErrorCode;
    std::string mPlaylistUrl;
    std::string mEffectiveUrl; 		/**< uri associated with downloaded playlist (takes into account 302 redirect) */
    bool freshManifest;
    int nextfragmentIndex; //CMCD get next index to fetch url from Segment List
    bool mReachedFirstFragOnRewind; /**< flag denotes if we reached the first fragment in a period on rewind */
    std::mutex fetchChunkBufferMutex;
}; // MediaStreamContext

#endif /* MEDIASTREAMCONTEXT_H */
