/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file fragmentcollector_mpd.h
 * @brief Fragment collector MPEG DASH declarations
 */

#ifndef FRAGMENTCOLLECTOR_MPD_H_
#define FRAGMENTCOLLECTOR_MPD_H_

#include "StreamAbstractionAAMP.h"
#include "AampJsonObject.h" /**< For JSON parsing */
#include <string>
#include <stdint.h>
#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>
#include <thread>
#include "admanager_mpd.h"
#include "AampMPDDownloader.h"
#include "AampDRMLicPreFetcher.h"
#include "AampMPDParseHelper.h"
#include "AampTrackWorker.h"

using namespace dash;
using namespace std;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash::helpers;
#define MAX_MANIFEST_DOWNLOAD_RETRY_MPD 2

/*Common MPD util functions (admanager_mpd.cpp and fragmentcollector_mpd.cpp */
double aamp_GetPeriodNewContentDuration(dash::mpd::IMPD *mpd, IPeriod * period, uint64_t &curEndNumber);


/**
 * @struct ProfileInfo
 * @brief Manifest file adaptation and representation info
 */
struct ProfileInfo
{
	int adaptationSetIndex;
	int representationIndex;
};

/**
 * @struct FragmentDescriptor
 * @brief Stores information of dash fragment
 */
struct FragmentDescriptor
{
private :
	std::string matchingBaseURL;
public :
	std::string manifestUrl;
	uint32_t Bandwidth;
	std::string RepresentationID;
	uint64_t Number;
	double Time;				//In units of timescale
	bool bUseMatchingBaseUrl;
	int64_t nextfragmentNum;
	double nextfragmentTime;
	uint32_t TimeScale;
	FragmentDescriptor() : manifestUrl(""), Bandwidth(0), Number(0), Time(0), RepresentationID(""),matchingBaseURL(""),bUseMatchingBaseUrl(false),nextfragmentNum(-1),nextfragmentTime(0), TimeScale(1)
	{
	}

	FragmentDescriptor(const FragmentDescriptor& p) : manifestUrl(p.manifestUrl), Bandwidth(p.Bandwidth), RepresentationID(p.RepresentationID), Number(p.Number), Time(p.Time),matchingBaseURL(p.matchingBaseURL),bUseMatchingBaseUrl(p.bUseMatchingBaseUrl),nextfragmentNum(p.nextfragmentNum),nextfragmentTime(p.nextfragmentTime), TimeScale(p.TimeScale)
	{
	}

	FragmentDescriptor& operator=(const FragmentDescriptor &p)
	{
		manifestUrl = p.manifestUrl;
		RepresentationID.assign(p.RepresentationID);
		Bandwidth = p.Bandwidth;
		Number = p.Number;
		Time = p.Time;
		matchingBaseURL = p.matchingBaseURL;
		nextfragmentNum = p.nextfragmentNum;
		nextfragmentTime = p.nextfragmentTime;
		TimeScale = p.TimeScale;
		return *this;
	}
	std::string GetMatchingBaseUrl() const
	{
		return matchingBaseURL;
	}

	void ClearMatchingBaseUrl()
	{
		matchingBaseURL.clear();
	}
	void AppendMatchingBaseUrl( const std::vector<IBaseUrl *>*baseUrls )
	{
		if( baseUrls && baseUrls->size()>0 )
		{
			const std::string &url = baseUrls->at(0)->GetUrl();
			if( url.empty() )
			{
			}
			else if( aamp_IsAbsoluteURL(url) )
			{
				if(bUseMatchingBaseUrl)
				{
					std::string prefHost = aamp_getHostFromURL(manifestUrl);
					for (auto & item : *baseUrls) {
						std::string itemUrl = item->GetUrl();
						std::string host  = aamp_getHostFromURL(itemUrl);
						if(0 == prefHost.compare(host))
						{
							matchingBaseURL = item->GetUrl();
							return;
						}
					}
				}
				matchingBaseURL = url;
			}
			else if( url.rfind("/",0)==0 )
			{
				matchingBaseURL = aamp_getHostFromURL(matchingBaseURL);
				matchingBaseURL += url;
				AAMPLOG_WARN( "baseURL with leading /" );
			}
			else
			{
				if( !matchingBaseURL.empty() && matchingBaseURL.back() != '/' )
				{ // add '/' delimiter only if parent baseUrl doesn't already end with one
					matchingBaseURL += "/";
				}
				matchingBaseURL += url;
			}
		}
	}
};

/**
 * @class StreamAbstractionAAMP_MPD
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_MPD : public StreamAbstractionAAMP
{
public:
	/**
	 * @fn StreamAbstractionAAMP_MPD
	 * @param aamp pointer to PrivateInstanceAAMP object associated with player
	 * @param seek_pos Seek position
	 * @param rate playback rate
	 */
	StreamAbstractionAAMP_MPD(class PrivateInstanceAAMP *aamp, double seekpos, float rate,
		id3_callback_t id3Handler = nullptr);
	/**
	 * @fn ~StreamAbstractionAAMP_MPD
	 */
	~StreamAbstractionAAMP_MPD();
	/**
	 * @fn StreamAbstractionAAMP_MPD Copy constructor disabled
	 *
	 */
	StreamAbstractionAAMP_MPD(const StreamAbstractionAAMP_MPD&) = delete;
	/**
	 * @fn StreamAbstractionAAMP_MPD assignment operator disabled
	 *
	 */
	StreamAbstractionAAMP_MPD& operator=(const StreamAbstractionAAMP_MPD&) = delete;

	/**
	 * @fn Start
	 * @brief Start streaming
	 */
	void Start() override;

	/**
	 * @fn Stop
	 * @param  clearChannelData - ignored.
	 */
	void Stop(bool clearChannelData) override;

	/**
	 * @fn Init
	 * @param  tuneType to set type of object.
	 */
	AAMPStatusType Init(TuneType tuneType) override;

	/**
	 * @fn InitTsbReader
	 * @param  tuneType to set type of object.
	 */
	AAMPStatusType InitTsbReader(TuneType tuneType) override;

	/**
	 * @fn GetStreamFormat
	 * @param[out]  primaryOutputFormat - format of primary track
	 * @param[out]  audioOutputFormat - format of audio track
	 * @param[out]  auxOutputFormat - format of aux audio track
	 * @param[out]  subtitleOutputFormat - format of subtitle track
	 */
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat) override;
	/**
	 * @fn GetStreamPosition
	 */
	double GetStreamPosition() override;
	/**
	 * @fn GetMediaTrack
	 * @param[in]  type - track type
	 */
	MediaTrack* GetMediaTrack(TrackType type) override;
	/**
	 * @fn GetFirstPTS
	 */
	double GetFirstPTS() override;
	/**
	 * @fn GetMidSeekPosOffset
	 */
	double GetMidSeekPosOffset() override;
	/**
	 * @fn GetStartTimeOfFirstPTS
	 */
	double GetStartTimeOfFirstPTS() override;
	/**
	 * @fn GetBWIndex
	 * @param[in] bitrate Bitrate to lookup profile
	 */
	int GetBWIndex(BitsPerSecond bitrate) override;
	/**
	 * @fn GetVideoBitrates
	 */
	std::vector<BitsPerSecond> GetVideoBitrates(void) override;
	/**
	 * @fn GetAudioBitrates
	 */
	std::vector<BitsPerSecond> GetAudioBitrates(void) override;
	/**
	 * @fn GetMaxBitrate
	 */
	BitsPerSecond GetMaxBitrate(void) override;
	/**
	 * @fn StopInjection
	 * @return void
	 */
	void StopInjection(void) override;
	/**
	 * @fn StartInjection
	 * @return void
	 */
	void StartInjection(void) override;
	double GetBufferedDuration() override;
	/**
	 * @fn SeekPosUpdate
	 * @brief Function to update seek position
	 * @param[in] secondsRelativeToTuneTime - can be the offset (seconds from tune time) or absolute position (seconds from 1970)
	 */
	void SeekPosUpdate(double secondsRelativeToTuneTime) override;
	virtual void SetCDAIObject(CDAIObject *cdaiObj) override;
	/**
	 * @fn GetAvailableAudioTracks
	 * @param[in] tracks - available audio tracks in period
	 * @param[in] trackIndex - index of current audio track
	 */
	virtual std::vector<AudioTrackInfo> & GetAvailableAudioTracks(bool allTrack=false) override;
	/**
		 * @brief Gets all/current available text tracks
		 * @retval vector of tracks
		 */
	virtual std::vector<TextTrackInfo>& GetAvailableTextTracks(bool allTrack=false) override;

	/**
	 * @fn Is4KStream
	 * @brief check if current stream have 4K content
	 * @param height - resolution of 4K stream if found
	 * @param bandwidth - bandwidth of 4K stream if found
	 * @return true on success
	 */
	virtual bool Is4KStream(int &height, BitsPerSecond &bandwidth) override;

	/**
	 * @fn GetProfileCount
	 *
	 */
	int GetProfileCount() override;
	/**
	 * @fn GetProfileIndexForBandwidth
	 * @param mTsbBandwidth - bandwidth to identify profile index from list
	 */
	int GetProfileIndexForBandwidth( BitsPerSecond mTsbBandwidth) override;
	/**
	 * @fn GetAvailableThumbnailTracks
	 */
	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
	/**
	 * @fn GetAvailableVideoTracks
	 */
	std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
	/**
	 * @fn SetThumbnailTrack
	 * @param thumbnail index value indicating the track to select
	 */
	bool SetThumbnailTrack(int) override;
	/**
	 * @fn GetThumbnailRangeData
	 * @param tStart start duration of thumbnail data.
	 * @param tEnd end duration of thumbnail data.
	 * @param *baseurl base url of thumbnail images.
	 * @param *raw_w absolute width of the thumbnail spritesheet.
	 * @param *raw_h absolute height of the thumbnail spritesheet.
	 * @param *width width of each thumbnail tile.
	 * @param *height height of each thumbnail tile.
	 */
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;

	// ideally below would be private, but called from MediaStreamContext
	/**
	 * @fn GetAdaptationSetAtIndex
	 * @param[in] idx - Adaptation Set Index
	 */
	const IAdaptationSet* GetAdaptationSetAtIndex(int idx);
	/**
	 * @fn GetAdaptationSetAndRepresentationIndicesForProfile
	 * @param[in] idx - Profile Index
	 */
	struct ProfileInfo GetAdaptationSetAndRepresentationIndicesForProfile(int profileIndex);
	int64_t GetMinUpdateDuration() { return mMinUpdateDurationMs;}
	/**
	 * @fn FetchFragment
	 * @param pMediaStreamContext Track object pointer
	 * @param media media descriptor string
	 * @param fragmentDuration duration of fragment in seconds
	 * @param isInitializationSegment true if fragment is init fragment
	 * @param curlInstance curl instance to be used to fetch
	 * @param discontinuity true if fragment is discontinuous
	 * @param pto presentation time offset in seconds
	 * @param timeScale  denominator for fixed point math
	 */
	bool FetchFragment( class MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity = false, double pto = 0 , uint32_t timeScale = 0);
	/**
	 * @fn PushNextFragment
	 * @param pMediaStreamContext Track object
	 * @param curlInstance instance of curl to be used to fetch
	 * @param skipFetch true if fragment fetch is to be skipped for seamlessaudio
	 */
	bool PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance, bool skipFetch=false);
	/**
	 * @fn SkipFragments
	 * @param pMediaStreamContext Media track object
	 * @param skipTime time to skip in seconds
	 * @param updateFirstPTS true to update first pts state variable
	 * @param skipToEnd true to skip to the end of content
	 */
	double SkipFragments( class MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS = false, bool skipToEnd = false);
	/**
	 * @fn GetFirstPeriodStartTime
	 */
	double GetFirstPeriodStartTime(void) override;
	void MonitorLatency();
	void StartSubtitleParser() override;
	void PauseSubtitleParser(bool pause) override;
	/**
	 * @fn GetCurrPeriodTimeScale
	 */
	uint32_t GetCurrPeriodTimeScale() override;
	dash::mpd::IMPD *GetMPD( void );
	IPeriod *GetPeriod( void );
	/**
	 * @fn GetPreferredTextRepresentation
	 * @param[in] adaptationSet Adaptation set object
	 * @param[out] selectedRepIdx - Selected representation index
	 * @param[out] selectedRepBandwidth - selected audio track bandwidth
	  */
	void GetPreferredTextRepresentation(IAdaptationSet *adaptationSet, int &selectedRepIdx,	uint32_t &selectedRepBandwidth, unsigned long long &score, std::string &name, std::string &codec);
	static Accessibility getAccessibilityNode(void *adaptationSet);
	static Accessibility getAccessibilityNode(AampJsonObject &accessNode);
	/**
	 * @fn GetBestTextTrackByLanguage
	 * @param[out] selectedTextTrack selected representation Index
	 */
	bool GetBestTextTrackByLanguage( TextTrackInfo &selectedTextTrack);
	/**
	 * @fn ParseTrackInformation
	 * @param adaptationSet Adaptation Node
	 * @param AampMediaType type of media
	 * @param[out] aTracks audio tracks
	 * @param[out] tTracks text tracks
	 */
	void ParseTrackInformation(IAdaptationSet *adaptationSet, uint32_t iAdaptationIndex, AampMediaType media, std::vector<AudioTrackInfo> &aTracks, std::vector<TextTrackInfo> &tTracks);
	uint64_t mLastPlaylistDownloadTimeMs; // Last playlist refresh time

	//Apis for sidecar caption support

	/**
	 * @fn InitSubtitleParser
	 * @param[in] data - subtitle data from application
	 * @return void
	 */
	void InitSubtitleParser(char *data) override;

	/**
	 * @fn ResetSubtitle
	 * @return void
	 */
	void ResetSubtitle() override;

	/**
	 * @fn MuteSubtitleOnPause
	 * @return void
	 */
	void MuteSubtitleOnPause() override;

	/**
	 * @fn ResumeSubtitleOnPlay
	 * @param[in] mute - mute status
	 * @param[in] data - subtitle data from application
	 * @return void
	 */
	void ResumeSubtitleOnPlay(bool mute, char *data) override;

	/**
	 * @fn MuteSidecarSubtitles
	 * @param[in] mute - mute/unmute
	 * @return void
	 */
	void MuteSidecarSubtitles(bool mute) override;

	/**
	 * @fn ResumeSubtitleAfterSeek
	 * @param[in] mute - mute status
	 * @param[in] data - subtitle data from application
	 * @return void
	 */
	void ResumeSubtitleAfterSeek(bool mute, char *data) override;

	/**
	 *   @fn SetTextStyle
	 *   @brief Set the text style of the subtitle to the options passed
	 *   @param[in] options - reference to the Json string that contains the information
	 *   @return - true indicating successful operation in passing options to the parser
	 */
	bool SetTextStyle(const std::string &options) override;

	/**
	 * @fn UpdateFailedDRMStatus
	 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
	 * @param[in] object  - Prefetch object instance which failed
	 */
	// TODO: Add implementation to mark the failed DRM's adaptation set as failed/un-usable
	void UpdateFailedDRMStatus(LicensePreFetchObject *object) override;

	/**
	 * @brief Get the ABR mode.
	 *
	 * @return the ABR mode.
	 */
	ABRMode GetABRMode() override { return mABRMode; };

	static void MPDUpdateCallback(void *);

	// CMCD Get nor and nrr fields
	void setNextobjectrequestUrl(std::string media,const FragmentDescriptor *fragmentDescriptor,AampMediaType mediaType);
	void setNextRangeRequest(std::string fragmentUrl,std::string nextrange,long bandwidth,AampMediaType mediaType);

	 /*
	 * @fn NotifyFirstVideoPTS
	 * @param[in] pts base pts
	 * @param[in] timeScale time scale
	 * @return void
	 */
	void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale) override;

	/**
	 * @fn GetAvailabilityStartTime
	 * @brief  Returns AvailabilityStartTime from the manifest
	 * @retval double . AvailabilityStartTime
	 */
	double GetAvailabilityStartTime() override;

	/**
	 * @brief Selects the audio track based on the available audio tracks and updates the desired representation index.
	 *
	 * This function selects the audio track from the given vector of AC4 audio tracks based on audio track selection logic
	 * It also updates the audioAdaptationSetIndex and audioRepresentationIndex variables accordingly.
	 *
	 * @param[in] aTracks The vector of available audio tracks. These are the parsed ac4 tracks
	 * @param[out] aTrackIdx The selected audio track index.
	 * @param[out] audioAdaptationSetIndex The index of the selected audio adaptation set.
	 * @param[out] audioRepresentationIndex The index of the selected audio representation.
	 */
	void SelectAudioTrack(std::vector<AudioTrackInfo> &aTracks, std::string &aTrackIdx, int &audioAdaptationSetIndex, int &audioRepresentationIndex);

	/***************************************************************************
	 * @fn SwitchAudioTrack
	 *
	 * @return void
	 ***************************************************************************/
	void SwitchAudioTrack();

	/***************************************************************************
	 * @fn UpdateMediaTrackInfo
	 *
	 * @return AAMPStatusType
	 ***************************************************************************/
	AAMPStatusType UpdateMediaTrackInfo(AampMediaType type);

	/***************************************************************************
	 * @fn GetCurrentFragmentDuration
	 *
	 * @return uint32_t
	 ***************************************************************************/
	uint32_t GetCurrentFragmentDuration( MediaStreamContext *pMediaStreamContext );

	/***************************************************************************
	 * @fn UpdateSeekPeriodOffset
	 *
	 * @return void
	 ***************************************************************************/
	void UpdateSeekPeriodOffset( double &offsetFromStart );

	/**
	 * @fn GetNextAdInBreak
	 * @brief Get the next valid ad in the ad break
	 * @param[in] direction will be 1 or -1 depending on the playback rate
	 */
	void GetNextAdInBreak(int direction);

	/**
	 * @fn UseIframeTrack
	 * @brief Check if AAMP is using an iframe track
	 *
	 * @return true if AAMP is using an iframe track, false otherwise
	 */
	bool UseIframeTrack(void) override;

	/*
	 * @fn DoEarlyStreamSinkFlush
	 * @brief Checks if the stream need to be flushed or not
	 *
	 * @param newTune true if this is a new tune, false otherwise
	 * @param rate playback rate
	 * @return true if stream should be flushed, false otherwise
	 */
	virtual bool DoEarlyStreamSinkFlush(bool newTune, float rate) override;

	/**
	 * @brief Should flush the stream sink on discontinuity or not.
	 * When segment timeline is enabled, media processor will be in pass-through mode
	 * and will not do delayed flush.
	 * @return true if stream should be flushed, false otherwise
	 */
	virtual bool DoStreamSinkFlushOnDiscontinuity() override;

	/**
	 * @fn clearFirstPTS
	 * @brief Clears the mFirstPTS value to trigger update of first PTS
	 */
	void clearFirstPTS(void) override;

protected:
	/**
	 * @fn StartFromAampLocalTsb
	 *
	 * @brief Start streaming from AAMP Local TSB
	 */
	void StartFromAampLocalTsb();

	/**
	 * @fn StartFromOtherThanAampLocalTsb
	 *
	 * @brief Start streaming content from a source other than AAMP Local TSB (live, CloudTSB, FOG...)
	 */
	void StartFromOtherThanAampLocalTsb();

	/**
	 * @fn GetStartAndDurationForPtsRestamping
	 *
	 * @brief Get the start and duration from the current timeline for the
	 *        current period, required for PTS restamping.
	 *
	 * @param[out] start - Start of the current timeline in seconds
	 * @param[out] duration - duration of the current period returned
	 */
	void GetStartAndDurationForPtsRestamping(AampTime &start, AampTime &duration);

	/**
	 * @fn UpdatePtsOffset
	 *
	 * @brief Calculate PTS offset value at the start of each period.
	 *
	 * @param[in] isNewPeriod - true for calculation on starting new period
	 */
	void UpdatePtsOffset(bool isNewPeriod);

	/**
	 * @fn RestorePtsOffsetCalculation
	 *
	 * @brief Restore variables used for PTS offset calculation,
	 *        after downloading the init fragment of an ad failed.
	 */
	void RestorePtsOffsetCalculation(void);

	/**
	 * @fn printSelectedTrack
	 * @param[in] trackIndex - selected track index
	 * @param[in] media - Media type
	 */

	void printSelectedTrack(const std::string &trackIndex, AampMediaType media);
	/**
	 * @fn AdvanceTrack
	 * @param[in] trackIdx - track index
	 * @param[in] trickPlay - flag indicates if its trickplay
	 * @param[in/out] waitForFreeFrag - flag is updated if we are waiting for free fragment
	 * @param[in/out] bCacheFullState - flag is updated if the cache is full for this track
	 * @param[in] throttleAudio - flag indicates if we should throttle audio download
	 * @param[in] isDiscontinuity - flag indicates if its a discontinuity
	 * @return void
	 */
	void AdvanceTrack(int trackIdx, bool trickPlay, double *delta, bool &waitForFreeFrag, bool &bCacheFullState,bool throttleAudio,bool isDiscontinuity = false);
	/**
	 * @fn AdvanceTsbFetch
	 * @param[in] trackIdx - trackIndex
	 * @param[in] trickPlay - trickplay flag
	 * @param[in] delta - delta for skipping fragments
	 * @param[out] waitForFreeFrag - waitForFreeFragmentAvailable flag
	 * @param[out] bCacheFullState - cache status for track
	 *
	 * @return void
	 */
	void AdvanceTsbFetch(int trackIdx, bool trickPlay, double delta, bool &waitForFreeFrag, bool &bCacheFullState);

	/**
	 * @fn FetcherLoop
	 * @return void
	 */
	void FetcherLoop();

	/**
	 * @fn SelectSourceOrAdPeriod
	 *
	 * @param[in,out] periodChanged flag
	 * @param[in,out] mpdChanged flag
	 * @param[in,out] AdStateChanged flag
	 * @param[in,out] waitForAdBreakCatchup flag
	 * @param[in,out] requireStreamSelection flag
	 * @param[in,out] currentPeriodId string
	 * @return bool - true if new period selected, false otherwise
	 */
	bool SelectSourceOrAdPeriod(bool &periodChanged, bool &mpdChanged, bool &adStateChanged, bool &waitForAdBreakCatchup, bool &requireStreamSelection, std::string &currentPeriodId);

	/**
	 * @fn IndexSelectedPeriod
	 *
	 * @param[in] periodChanged flag
	 * @param[in] AdStateChanged flag
	 * @param[in] requireStreamSelection flag
	 * @param[in] currentPeriodId string
	 * @return bool - true if new period indexed, false otherwise
	 */
	bool IndexSelectedPeriod(bool periodChanged, bool adStateChanged, bool requireStreamSelection, std::string currentPeriodId);

	/**
	 * @fn CheckEndOfStream
	 *
	 * @param[in] waitForAdBreakCatchup flag
	 * @return bool - true if end of stream reached, false otherwise
	 */
	bool CheckEndOfStream(bool waitForAdBreakCatchup);

	/**
	 * @fn DetectDiscontinuityAndFetchInit
	 *
	 * @param[in] periodChanged flag
	 * @param[in] nextSegmentTime
	 *
	 * @return void
	 */
	void DetectDiscontinuityAndFetchInit(bool periodChanged, uint64_t nextSegmentTime);

	/**
	 * @fn TsbReader
	 * @return void
	 */
	virtual void TsbReader();

	/**
	 * @fn GetStreamInfo
	 * @param[in]  idx - profile index.
	 */
	StreamInfo* GetStreamInfo(int idx) override;
	/**
	 * @fn CheckLLProfileAvailable
	 * @param mpd Pointer to manifest
	 */
	bool CheckLLProfileAvailable(IMPD *mpd);
	/**
	 * @fn EnableAndSetLiveOffsetForLLDashPlayback
	 * @param[In] const mpd Pointer to FragmentCollector
	 * @retval AAMPStatusType
	 */
	AAMPStatusType  EnableAndSetLiveOffsetForLLDashPlayback(const MPD* mpd);

	/**
	 * @fn GetLowLatencyParams
	 * @param[In] const mpd Pointer to FragmentCollector
	 * @param[Out] LLDashData Reference to LowLatency element parsed data
	 * @retval bool true if successfully Parsed Low Latency elements. Else false
	 */
	bool GetLowLatencyParams(const MPD* mpd,AampLLDashServiceData &LLDashData);

	/**
	 * @fn ParseMPDLLData
	 * @param[In] mpd Pointer to FragmentCollector
	 * @param[Out] stAampLLDashServiceData Reference to LowLatency element parsed data
	 * @retval bool true if successfully Parsed Low Latency elements. Else false
	 */
	bool ParseMPDLLData(MPD* mpd, AampLLDashServiceData &stAampLLDashServiceData);
	/**
	 * @fn UpdateMPD
	 * @param init retrievePlaylistFromCache true to try to get from cache
	 */
	AAMPStatusType UpdateMPD(bool init = false);
	/**
	 * @fn FindServerUTCTime
	 * @param mpd:  MPD top level element
	 * @param root: XML root node
	 */
	bool FindServerUTCTime(Node* root);
	/**
	 * @fn FetchDashManifest
	 */
	AAMPStatusType FetchDashManifest();
	/**
	 * @fn FindTimedMetadata
	 * @param mpd MPD top level element
	 * @param root XML root node
	 * @param init true if this is the first playlist download for a tune/seek/trickplay
	 * @param reportBulkMeta true if bulkTimedMetadata feature is enabled
	 */
	void FindTimedMetadata(MPD* mpd, Node* root, bool init = false, bool reportBulkMet = false);
	/**
	 * @fn ProcessPeriodSupplementalProperty
	 * @param node SupplementalProperty node
	 * @param[out] AdID AD Id
	 * @param startMS start time in MS
	 * @param durationMS duration in MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta=false);
	/**
	 * @fn ProcessPeriodAssetIdentifier
	 * @param node AssetIdentifier node
	 * @param startMS start time MS
	 * @param durationMS duration MS
	 * @param AssetID Asset Id
	 * @param ProviderID Provider Id
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& assetID, std::string& providerID,bool isInit, bool reportBulkMeta=false);
	/**
	 * @fn ProcessEventStream
	 * @param startMS the start time of the event derived from the (previous) period info (ms)
	 * @param startOffsetMS the start time of the stream derived from the first segment
	 * @param[in] period instance.
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	bool ProcessEventStream(uint64_t startMS, int64_t startOffsetMS, IPeriod * period, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestrictionList
	 * @param node StreamRestrictionListType node
	 * @param AdID Ad Id
	 * @param startMS start time MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestriction
	 * @param node StreamRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS Start time in MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestrictionExt
	 * @param node Ext child of StreamRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS start time in ms
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessTrickModeRestriction
	 * @param node TrickModeRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS start time in ms
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessTrickModeRestriction(Node *node, const std::string &AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn Fragment downloader thread
	 * @param trackIdx track index
	 * @param initialization Initialization string
	 */
	void TrackDownloader(int trackIdx, std::string initialization);
	/**
	 * @fn FetchAndInjectInitFragments
	 * @param discontinuity number of tracks and discontinuity true if discontinuous fragment
	 */
	void FetchAndInjectInitFragments(bool discontinuity = false);
	/**
	 * @fn FetchAndInjectInitialization
	 * @param trackIdx,discontinuity  number of tracks and discontinuity true if discontinuous fragment
	 */
	void FetchAndInjectInitialization(int trackIdx, bool discontinuity = false);
	/**
	 * @fn RefreshTrack
	 * @param type media type
	 * @return void
	 */
	void RefreshTrack(AampMediaType type) override;
	/**
	 * @fn SwitchSubtitleTrack
	 * @param newTune true if this is a new tune
	 * @return void
	 */
	void SwitchSubtitleTrack(bool newTune);
	/**
	 * @fn SelectSubtitleTrack
	 * @param newTune true if this is a new tune
	 * @return void
	 */
	void SelectSubtitleTrack(bool newTune, std::vector<TextTrackInfo> &tTracks , std::string &tTrackIdx);
	/**
	 * @fn StreamSelection
	 * @param newTune true if this is a new tune
	 */
	void StreamSelection(bool newTune = false, bool forceSpeedsChangedEvent = false);
	/**
	 * @fn CheckForInitalClearPeriod
	 */
	bool CheckForInitalClearPeriod();
	/**
	 * @fn PushEncryptedHeaders
	 */
	void PushEncryptedHeaders(std::map<int, std::string>& mappedHeaders);

	/**
	 * @fn CacheEncryptedHeader
	 */
	void CacheEncryptedHeader(int trackIndex, std::string headerUrl);
	/**
	 * @fn GetEncryptedHeaders
	 * @return bool
	 */
	bool GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders);
	/**
	 * @fn ExtractAndAddSubtitleMediaHeader
	 * @return bool
	 */
	bool ExtractAndAddSubtitleMediaHeader();
	/**
	 * @fn GetProfileIdxForBandwidthNotification
	 * @param bandwidth - bandwidth to identify profile index from list
	 */
	int GetProfileIdxForBandwidthNotification(uint32_t bandwidth);
	/**
	 * @fn GetCurrentMimeType
	 * @param AampMediaType type of media
	 * @retval mimeType
	 */
	std::string GetCurrentMimeType(AampMediaType mediaType);
	/**
	 * @fn UpdateTrackInfo
	 */
	AAMPStatusType UpdateTrackInfo(bool modifyDefaultBW, bool resetTimeLineIndex = false);
	/**
	 * @fn SkipToEnd
	 * @param pMediaStreamContext Track object pointer
	 */
	void SkipToEnd( class MediaStreamContext *pMediaStreamContext); //Added to support rewind in multiperiod assets

	/**
	 * @fn SeekInPeriod
	 * @param seekPositionSeconds seek position in seconds
	 */
	void SeekInPeriod( double seekPositionSeconds, bool skipToEnd = false);
	/**
	 * @fn ApplyLiveOffsetWorkaroundForSAP
	 * @param seekPositionSeconds seek position in seconds.
	 */
	void ApplyLiveOffsetWorkaroundForSAP(double seekPositionSeconds);
	/**
	 * @fn GetFirstValidCurrMPDPeriod
	 * @param currMPDPeriodDetails vector containing the current MPD period info
	 */
	PeriodInfo GetFirstValidCurrMPDPeriod(std::vector<PeriodInfo> currMPDPeriodDetails);
	/**
	 * @fn GetCulledSeconds
	 */
	double GetCulledSeconds(std::vector<PeriodInfo> &currMPDPeriodDetails);
	/**
	 * @fn UpdateCulledAndDurationFromPeriodInfo
	 */
	void UpdateCulledAndDurationFromPeriodInfo(std::vector<PeriodInfo> &currMPDPeriodDetails);
	/**
	 * @fn UpdateLanguageList
	 * @return void
	 */
	void UpdateLanguageList();
	/**
	 * @fn GetBestAudioTrackByLanguage
	 * @param desiredRepIdx [out] selected representation Index
	 * @param CodecType [out] selected codec type
	 * @param ac4Tracks parsed track from preselection node
	 * @param audioTrackIndex selected audio track index
	 */
	int GetBestAudioTrackByLanguage(int &desiredRepIdx,AudioType &selectedCodecType, std::vector<AudioTrackInfo> &ac4Tracks, std::string &audioTrackIndex);
	int GetPreferredAudioTrackByLanguage();
	/**
	 * @fn CheckProducerReferenceTimeUTCTimeMatch
	 * @param pRT Pointer to ProducerReferenceTime
	 */
	bool CheckProducerReferenceTimeUTCTimeMatch(IProducerReferenceTime *pRT);
	/**
	 * @fn PrintProducerReferenceTimeAttributes
	 * @param pRT Pointer to ProducerReferenceTime
	 */
	void PrintProducerReferenceTimeAttributes(IProducerReferenceTime *pRT);
	/**
	 * @fn GetProducerReferenceTimeForAdaptationSet
	 * @param adaptationSet Pointer to AdaptationSet
	 */
	IProducerReferenceTime *GetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet);
	/**
	 * @fn GetLanguageForAdaptationSet
	 * @param adaptationSet Pointer to adaptation set
	 * @retval language of adaptation set
	 */
	std::string GetLanguageForAdaptationSet( IAdaptationSet *adaptationSet );
	/**
	 * @fn GetMpdFromManifest
	 * @param manifest buffer pointer
	 * @param mpd MPD object of manifest
	 * @param manifestUrl manifest url
	 * @param init true if this is the first playlist download for a tune/seek/trickplay
	 */
	AAMPStatusType GetMPDFromManifest( ManifestDownloadResponsePtr mpdDnldResp, bool init);
	void ProcessMetadataFromManifest( ManifestDownloadResponsePtr mpdDnldResp, bool init);
	void ProcessManifestHeaderResponse(ManifestDownloadResponsePtr mpdDnldResp,bool init);
	void MPDUpdateCallbackExec();
	/**
	 * @fn GetDrmPrefs
	 * @param The UUID for the DRM type
	 */
	int GetDrmPrefs(const std::string& uuid);
	/**
	 * @fn GetPreferredDrmUUID
	 */
	std::string GetPreferredDrmUUID();
	/**
	 * @fn IsMatchingLanguageAndMimeType
	 * @param[in] type - media type
	 * @param[in] lang - language to be matched
	 * @param[in] adaptationSet - adaptation to be checked for
	 * @param[out] representationIndex - representation within adaptation with matching params
	 */
	bool IsMatchingLanguageAndMimeType(AampMediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex);
	/**
	 * @fn ConstructFragmentURL
	 * @param[out] fragmentUrl fragment url
	 * @param fragmentDescriptor descriptor
	 * @param media media information string
	 */
	void ConstructFragmentURL( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media);
	double GetEncoderDisplayLatency();
	/**
	 * @fn StartLatencyMonitorThread
	 * @return void
	 */
	void StartLatencyMonitorThread();
	LatencyStatus GetLatencyStatus() { return latencyStatus; }
	/**
	 * @fn GetPreferredCodecIndex
	 * @param adaptationSet Adaptation set object
	 * @param[out] selectedRepIdx - Selected representation index
	 * @param[out] selectedCodecType type of desired representation
	 * @param[out] selectedRepBandwidth - selected audio track bandwidth
	 * @param disableEC3 whether EC3 disabled by config
	 * @param disableATMOS whether ATMOS audio disabled by config
	  */
	bool GetPreferredCodecIndex(IAdaptationSet *adaptationSet, int &selectedRepIdx, AudioType &selectedCodecType,
	uint32_t &selectedRepBandwidth, long &bestScore, bool disableEC3, bool disableATMOS, bool disableAC4, bool disableAC3, bool &disabled);

	/**
		 * @brief Get the audio track information from all period
		 * updated member variable mAudioTracksAll
		 * @return void
		 */
	void PopulateAudioTracks(void);

	/**
		 * @brief Get the audio track information from all preselection node of the period
		 * @param period Node ans IMPDElement
		 * @return void
		 */
	void ParseAvailablePreselections(IMPDElement *period, std::vector<AudioTrackInfo> & audioAC4Tracks);

	/**
	 * @fn PopulateTrackInfo
	 * @param media - Media type
	 * @param - Do need to reset vector?
	 * @return none
	 */
	void PopulateTrackInfo(AampMediaType media, bool reset=false);

	/**
	 * @fn QueueContentProtection
	 * @param[in] period - period
	 * @param[in] adaptationSetIdx - adaptation set index
	 * @param[in] mediaType - media type
	 * @param[in] qGstProtectEvent - Flag denotes if GST protection event should be queued in sink
	 * @param[in] isVssPeriod flag denotes if this is for a VSS period
	 * @brief queue content protection for the given adaptation set
	 * @retval true on success
	 */
	void QueueContentProtection(IPeriod* period, uint32_t adaptationSetIdx, AampMediaType mediaType, bool qGstProtectEvent = true, bool isVssPeriod = false);

	/**
	 * @fn ProcessAllContentProtectionForMediaType
	 * @param[in] type - media type
	 * @param[in] priorityAdaptationIdx - selected adaption index, to be processed with priority
	 * @param[in] chosenAdaptationIdxs - selected adaption indexes, might be empty for certain playback cases
	 * @brief process content protection of all the adaptation for the given media type
	 * @retval none
	 */
	void ProcessAllContentProtectionForMediaType(AampMediaType type, uint32_t priorityAdaptationIdx, std::set<uint32_t> &chosenAdaptationIdxs);

	/**
	 * @brief Retrieves the index of a valid period based on the given period index.
	 *
	 * @param periodIdx The index of the period to check.
	 * @return The index of a valid period.
	 */
	int GetValidPeriodIdx(int periodIdx);

	void UpdateMPDPeriodDetails(std::vector<PeriodInfo>& currMPDPeriodDetails,uint64_t &durMs);

	/*
	* @brief CheckAdResolvedStatus
	*
	* @param[in] ads - Ads vector (optional)
	* @param[in] adIdx - AdIndex (optional)
	* @param[in] periodId - periodId (optional)
	*/
	void CheckAdResolvedStatus(AdNodeVectorPtr &ads, int adIdx, const std::string &periodId = "");

	/**
	* @fn SetSubtitleTrackOffset
	* @brief Function to calculate the start time offset between subtitle and video tracks
	*/
	void SetSubtitleTrackOffset();

	/**
	 * @fn InitializeWorkers
	 * @brief Initialize worker threads
	 *
	 * @return void
	 */
	void InitializeWorkers();

	uint64_t FindPositionInTimeline(class MediaStreamContext *pMediaStreamContext, const std::vector<ITimeline *>&timelines);

	/**
	 * @brief Send ad placement event to listeners
	 *
	 * @param[in] type Event type
	 * @param[in] adId Identifier of the ad
	 * @param[in] position Position relative to the start of the reservation
	 * @param[in] absolutePosition Absolute position
	 * @param[in] offset Offset from the start of the ad
	 * @param[in] duration Duration of the ad in milliseconds
	 * @param[in] immediate Flag to indicate if event(s) should be sent immediately
	 */
	void SendAdPlacementEvent(AAMPEventType type, const std::string& adId,
							 uint32_t position, AampTime absolutePosition, uint32_t offset,
							 uint32_t duration, bool immediate);

	/**
	 * @brief Send ad reservation event to listeners
	 *
	 * @param[in] type Event type
	 * @param[in] adBreakId Identifier of the ad break
	 * @param[in] position Period position of the ad break
	 * @param[in] absolutePosition Absolute position
	 * @param[in] immediate Flag to indicate if event(s) should be sent immediately
	 */
	void SendAdReservationEvent(AAMPEventType type, const std::string& adBreakId,
							   uint64_t position, AampTime absolutePosition, bool immediate);

	/**
	 * @brief Send any cached init fragments to be injected on disabled streams to generate the pipeline
	 */
	void SendMediaHeaders(void);

	std::mutex mStreamLock;
	bool abortTsbReader;
	std::set<std::string> mLangList;
	double seekPosition;    // Seek offset from or position at time of tuning, in seconds.
							// The same variable is used for offset (e.g. for HLS) and position (e.g. most of the time for DASH).
	float rate;
	std::thread fragmentCollectorThreadID;
	std::thread tsbReaderThreadID;
	ManifestDownloadResponsePtr mManifestDnldRespPtr ;
	bool    mManifestUpdateHandleFlag;
	AampMPDParseHelperPtr	mMPDParseHelper;
	bool mLowLatencyMode;
	dash::mpd::IMPD *mpd;
	class MediaStreamContext *mMediaStreamContext[AAMP_TRACK_COUNT];
	int mNumberOfTracks;
	int mCurrentPeriodIdx;
	int mNumberOfPeriods;	// Number of periods in the updated manifest
	int mIterPeriodIndex;	// FetcherLoop period iterator index
	double mEndPosition;
	bool mIsLiveStream;    	    	   /**< Stream is live or not; won't change during runtime. */
	bool mIsLiveManifest;   	   /**< Current manifest is dynamic or static; may change during runtime. eg: Hot DVR. */
	bool mUpdateManifestState;
	StreamInfo* mStreamInfo;
	bool mUpdateStreamInfo;		   /**< Indicates mStreamInfo needs to be updated */
	double mPrevStartTimeSeconds;
	std::string mPrevLastSegurlMedia;
	long mPrevLastSegurlOffset; 	   /**< duration offset from beginning of TSB */
	double mPeriodEndTime;
	double mPeriodStartTime;
	double mPeriodDuration;
	uint64_t mMinUpdateDurationMs;
	double mTSBDepth;
	double mPresentationOffsetDelay;
	double mFirstPTS;
	double mStartTimeOfFirstPTS;
	double mVideoPosRemainder;
	double mFirstVideoFragPTS;
	AudioType mAudioType;
	int mPrevAdaptationSetCount;
	std::vector<BitsPerSecond> mBitrateIndexVector;
	bool playlistDownloaderThreadStarted; // Playlist downloader thread start status
	bool isVidDiscInitFragFail;
	double mLivePeriodCulledSeconds;
	bool mIsSegmentTimelineEnabled;   /**< Flag to indicate if segment timeline is enabled, to determine if PTS is available from manifest */

	// In case of streams with multiple video Adaptation Sets, A profile
	// is a combination of an Adaptation Set and Representation within
	// that Adaptation Set. Hence we need a mapping from a profile to
	// corresponding Adaptation Set and Representation Index
	std::map<int, struct ProfileInfo> mProfileMaps;

	bool mIsFogTSB;
	IPeriod *mCurrentPeriod;
	std::string mBasePeriodId;
	double mBasePeriodOffset;
	class PrivateCDAIObjectMPD *mCdaiObject;
	std::vector<std::string> mEarlyAvailablePeriodIds;
	int mCommonKeyDuration;

	// DASH does not use abr manager to store the supported bandwidth values,
	// hence storing max TSB bandwidth in this variable which will be used for VideoEnd Metric data via
	// StreamAbstractionAAMP::GetMaxBitrate function,
	long mMaxTSBBandwidth;

	double mLiveEndPosition;    // Live end absolute position
	double mCulledSeconds;      // Culled absolute position
	double mPrevFirstPeriodStart;
	bool mAdPlayingFromCDN;   /*Note: TRUE: Ad playing currently & from CDN. FALSE: Ad "maybe playing", but not from CDN.*/
	double mAvailabilityStartTime;
	std::map<std::string, int> mDrmPrefs;
	int mMaxTracks; /* Max number of tracks for this session */
	double mDeltaTime;
	bool mHasServerUtcTime;
	uint32_t prevTimeScale;
	bool mIsFcsRepresentation;
	int mFcsRepresentationId;
	std::vector<IFCS *>mFcsSegments;
	AampTime mAudioSurplus;
	AampTime mVideoSurplus;
	bool mSeekedInPeriod; /*< Flag to indicate if seeked in period */
	/**
	 * @fn isAdbreakStart
	 * @param[in] period instance.
	 * @param[in] startMS start time in milliseconds.
	 * @param[in] eventBreakVec vector of EventBreakInfo structure.
	 */
	bool isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec);
	/**
	 * @fn onAdEvent
	 */
	bool onAdEvent(AdEvent evt);
	bool onAdEvent(AdEvent evt, double &adOffset);
	 /**
	 * @fn SetAudioTrackInfo
	 * @param[in] tracks - available audio tracks in period
	 * @param[in] trackIndex - index of current audio track
	 */

	void SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex);
	void SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex);
	/**
	 * @fn FindPeriodGapsAndReport
	 */
	void FindPeriodGapsAndReport();
	/**
	 * @fn IndexNewMPDDocument
	 */
	AAMPStatusType IndexNewMPDDocument(bool updateTrackInfo = true);

	/**
	 * @fn CreateDrmHelper
	 * @param adaptationSet Adaptation set object
	 * @param mediaType type of track
	 */
	DrmHelperPtr CreateDrmHelper(const IAdaptationSet * adaptationSet,AampMediaType mediaType);

	/**
	* @fn CheckForVssTags
	*/
	bool CheckForVssTags();
	/**
	* @fn ProcessVssLicenseRequest
	*/
	void ProcessVssLicenseRequest();
	/**
	* @fn GetAvailableVSSPeriods
	* @param PeriodIds VSS Periods
	*/
	void GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds);
	/**
	* @fn GetVssVirtualStreamID
	*/
	std::string GetVssVirtualStreamID();

	/**
	 * @brief Get a timeline segment repeat count
	 * @param[in] pMediaStreamContext Media stream context
	 * @param[in] timeLineIndex Timeline index
	 * @return The timeline segment repeat count or zero
	 */
	uint32_t GetSegmentRepeatCount(MediaStreamContext *pMediaStreamContext, int timeLineIndex);

	std::vector<StreamInfo*> thumbnailtrack;
	std::vector<TileInfo> indexedTileInfo;
	double mFirstPeriodStartTime; /*< First period start time for progress report*/

	LatencyStatus latencyStatus; 		 /**< Latency status of the playback*/
	LatencyStatus prevLatencyStatus;	 /**< Previous latency status of the playback*/
	bool latencyMonitorThreadStarted;	 /**< Monitor latency thread  status*/
	std::thread latencyMonitorThreadID;	 /**< Fragment injector thread id*/
	int mProfileCount;			 /**< Total video profile count*/
	std::unique_ptr<SubtitleParser> mSubtitleParser;	/**< Parser for subtitle data*/
	bool mMultiVideoAdaptationPresent;
	double mLocalUtcTime;
	ABRMode mABRMode;					 /**< ABR mode*/
	size_t mLastManifestFileSize;
	double mFragmentTimeOffset;     /**< denotes the offset added to fragment time when absolute timeline is disabled, holds currentPeriodOffset*/
	bool mShortAdOffsetCalc;
	AampTime mNextPts;					/*For PTS restamping*/
	std::vector<std::unique_ptr<aamp::AampTrackWorker>> mTrackWorkers;	/**< Track workers for fetching fragments*/
	bool mIsFinalFirstPTS; /**< Flag to indicate if the first PTS is final or not */
};

#endif //FRAGMENTCOLLECTOR_MPD_H_
/**
 * @}
 */
