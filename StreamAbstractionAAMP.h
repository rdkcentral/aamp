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
 * @file StreamAbstractionAAMP.h
 * @brief Base classes of HLS/MPD collectors. Implements common caching/injection logic.
 */

#ifndef STREAMABSTRACTIONAAMP_H
#define STREAMABSTRACTIONAAMP_H

#include "AampMemoryUtils.h"
#include "priv_aamp.h"
#include "AampJsonObject.h"
#include "mediaprocessor.h"
#include "AdManagerBase.h"
#include <map>
#include <iterator>
#include <vector>
#include <condition_variable>

#include <glib.h>
#include "subtitleParser.h"
#include <CMCDHeaders.h>
#include <AudioCMCDHeaders.h>
#include <VideoCMCDHeaders.h>
#include <ManifestCMCDHeaders.h>
#include <SubtitleCMCDHeaders.h>

#include "AampDRMLicPreFetcherInterface.h"
#include "AampTime.h"

/**
 * @brief Media Track Types
 */
typedef enum
{ // note: MUST match first four enums of AampMediaType
	eTRACK_VIDEO,     /**< Video track */
	eTRACK_AUDIO,     /**< Audio track */
	eTRACK_SUBTITLE,  /**< Subtitle track */
	eTRACK_AUX_AUDIO  /**< Auxiliary audio track */
} TrackType;

AampMediaType TrackTypeToMediaType( TrackType trackType );

/**
 * @brief Structure holding the resolution of stream
 */
struct StreamResolution
{
	int width;        /**< Width in pixels*/
	int height;       /**< Height in pixels*/
	double framerate; /**< Frame Rate */

	StreamResolution(): width(0), height(0), framerate(0.0)
	{
	}
};

/**
 * @brief Structure holding the information of a stream.
 */
struct StreamInfo
{
	bool enabled;			/**< indicates if the streamInfo profile is enabled */
	bool isIframeTrack;             /**< indicates if the stream is iframe stream*/
	bool validity;		        /**< indicates profile validity against user configured profile range */
	std::string codecs;	        /**< Codec String */
	BitsPerSecond bandwidthBitsPerSecond;    /**< Bandwidth of the stream bps*/
	StreamResolution resolution;    /**< Resolution of the stream*/
	BitrateChangeReason reason;     /**< Reason for bitrate change*/
	std::string baseUrl;
	StreamInfo():enabled(false),isIframeTrack(false),validity(false),codecs(),bandwidthBitsPerSecond(0),resolution(),reason(){};
};


struct TileLayout
{
	int numRows; 		/**< Number of Rows from Tile Inf */
	int numCols; 		/**< Number of Cols from Tile Inf */
	double posterDuration; 	/**< Duration of each Tile in Spritesheet */
	double tileSetDuration; /**< Duration of whole tile set */
};

/**
*	\struct	TileInfo
* 	\brief	TileInfo structure for Thumbnail data
*/
class TileInfo
{
public:
	TileInfo(): layout(), startTime(), url()
	{
	}

	~TileInfo()
	{
	}

	TileLayout layout;
	double startTime;
	std::string url;
};

/**
 * @brief Structure of cached fragment data
 *        Holds information about a cached fragment
 */
class CachedFragment
{
public:
	AampGrowableBuffer fragment;	/**< Buffer to keep fragment content */
	double position;				/**< Position in the playlist, in seconds */
	double duration;				/**< Fragment duration, in seconds */
	bool initFragment;				/**< Is init fragment */
	bool discontinuity;				/**< PTS discontinuity status */
	bool isDummy;			/**< Is dummy fragment */
	int profileIndex;				/**< Profile index; Updated internally */
	uint32_t timeScale;				/* timescale of this fragment as read from manifest */
	std::string uri;				/* for debug */
	StreamInfo cacheFragStreamInfo; /**< Bitrate info of the fragment */
	AampMediaType type;				/**< AampMediaType info of the fragment */
	long long downloadStartTime;	/**< The start time of file download */
	long long discontinuityIndex;
	double PTSOffsetSec; 			/* PTS offset to apply for this segment */
	double absPosition;		/** Absolute position */
	CachedFragment() : fragment(AampGrowableBuffer("cached-fragment")), position(0.0), duration(0.0),
					   initFragment(false), discontinuity(false), profileIndex(0), cacheFragStreamInfo(StreamInfo()),
					   type(eMEDIATYPE_DEFAULT), downloadStartTime(0), timeScale(0), PTSOffsetSec(0), absPosition(0.0),
					   isDummy(false)
	{
	}

	void Copy(CachedFragment* other, size_t len)
	{
		this->position = other->position;
		this->duration = other->duration;
		this->initFragment = other->initFragment;
		this->discontinuity = other->discontinuity;
		this->profileIndex = other->profileIndex;
		this->cacheFragStreamInfo = other->cacheFragStreamInfo;
		this->type = other->type;
		this->fragment.AppendBytes(other->fragment.GetPtr(), len);
		this->downloadStartTime = other->downloadStartTime;
		this->uri = other->uri;
		this->timeScale = other->timeScale;
		this->PTSOffsetSec = other->PTSOffsetSec;
		this->absPosition =  other->absPosition;
		this->isDummy = other->isDummy;
	}
	void Clear()
	{
		fragment.Free();
		type = eMEDIATYPE_DEFAULT;
		downloadStartTime = 0;
		initFragment = false;
		position = 0.0;
		duration = 0.0;
		discontinuity = false;
		profileIndex = 0;
		cacheFragStreamInfo = StreamInfo();
		timeScale = 0;
		PTSOffsetSec = 0;
		absPosition = 0.0;
		isDummy = false;
	}
};

/**
 * @brief Playlist Types
 */
typedef enum
{
	ePLAYLISTTYPE_UNDEFINED,    /**< Playlist type undefined */
	ePLAYLISTTYPE_EVENT,        /**< Playlist may grow via appended lines, but otherwise won't change */
	ePLAYLISTTYPE_VOD,          /**< Playlist will never change */
} PlaylistType;

/**
 * @brief Buffer health status
 */
enum BufferHealthStatus
{
	BUFFER_STATUS_GREEN,  /**< Healthy state, where buffering is close to being maxed out */
	BUFFER_STATUS_YELLOW, /**< Danger  state, where buffering is close to being exhausted */
	BUFFER_STATUS_RED     /**< Failed state, where buffers have run dry, and player experiences underrun/stalled video */
};

/**
 * @brief Media Discontinuity state
 */
typedef enum
{
	eDISCONTINUITY_FREE = 0,       /**< No Discontinuity */
	eDISCONTINUITY_IN_VIDEO = 1,   /**< Discontinuity in Video */
	eDISCONTINUITY_IN_AUDIO = 2,   /**< Discontinuity in audio */
	eDISCONTINUITY_IN_BOTH = 3     /**< Discontinuity in Both Audio and Video */
} MediaTrackDiscontinuityState;

class IsoBmffHelper;

/**
 * @brief Base Class for Media Track
 */
class MediaTrack
{
public:
	/**
	 * @fn MediaTrack
	 *
	 * @param[in] type - Media track type
	 * @param[in] aamp - Pointer to PrivateInstanceAAMP
	 * @param[in] name - Media track name
	 */
	MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name);

	/**
	 * @fn ~MediaTrack
	 */
	virtual ~MediaTrack();

	/**
	* @brief MediaTrack Copy Constructor
	*/
	MediaTrack(const MediaTrack&) = delete;

	/**
	* @brief MediaTrack assignment operator overloading
	*/
	MediaTrack& operator=(const MediaTrack&) = delete;

	/**
	 * @fn StartInjectLoop
	 *
	 * @return void
	 */
	void StartInjectLoop();

	/**
	 * @fn StopInjectLoop
	 *
	 * @return void
	 */
	void StopInjectLoop();

	/**
	 * @fn StopPlaylistDownloaderThread
	 *
	 * @return void
	 */
	void StopPlaylistDownloaderThread();

	/**
	 * @fn StartPlaylistDownloaderThread
	 *
	 * @return void
	 */
	void StartPlaylistDownloaderThread();

	/**
	 * @fn AbortWaitForPlaylistDownload
	 *
	 * @return void
	 */
	void AbortWaitForPlaylistDownload();

	/**
	 * @fn AbortFragmentDownloaderWait
	 *
	 * @return void
	 */
	void AbortFragmentDownloaderWait();

	/**
	 * @fn AbortWaitForCachedFragmentChunk
	 *
	 * @return void
	 */
	void AbortWaitForCachedFragmentChunk();

	/**
	 * @fn WaitForManifestUpdate
	 *
	 * @return void
	 */
	void WaitForManifestUpdate();

	/**
	 * @fn PlaylistDownloader
	 *
	 * @return void
	 */
	void PlaylistDownloader();

	/**
	 * @fn WaitTimeBasedOnBufferAvailable
	 *
	 * @return minDelayBetweenPlaylistUpdates - wait time for playlist refresh
	 */
	int WaitTimeBasedOnBufferAvailable();

	/**
	 * @fn ProcessPlaylist
	 *
	 * @param[in] newPlaylist - newly downloaded playlist buffer
	 * @param[in] http_error - Download response code
	 *
	 * @return void
	 */
	virtual void ProcessPlaylist(AampGrowableBuffer& newPlaylist, int http_error) = 0;
	/**
	 * @fn GetPlaylistUrl
	 *
	 * @return URL string
	 */
	virtual std::string& GetPlaylistUrl() = 0;

	/**
	 * @fn GetEffectivePlaylistUrl
	 *
	 * @return string - original playlist URL(redirected)
	 */
	virtual std::string& GetEffectivePlaylistUrl() = 0;

	/**
	 * @fn SetEffectivePlaylistUrl
	 *
	 * @param string - original playlist URL
	 */
	virtual void SetEffectivePlaylistUrl(std::string url) = 0;

	/**
	 * @fn GetLastPlaylistDownloadTime
	 *
	 * @return lastPlaylistDownloadTime
	 */
	virtual long long GetLastPlaylistDownloadTime() = 0;

	/**
	 * @fn GetMinUpdateDuration
	 *
	 * @return minimumUpdateDuration
	 */
	virtual long GetMinUpdateDuration() = 0;

	/**
	 * @fn GetDefaultDurationBetweenPlaylistUpdates
	 *
	 * @return maxIntervalBtwPlaylistUpdateMs
	 */
	virtual int GetDefaultDurationBetweenPlaylistUpdates() = 0;

	/**
	 * @fn SetLastPlaylistDownloadTime
	 *
	 * @param[in] time last playlist download time
	 * @return void
	 */
	virtual void SetLastPlaylistDownloadTime(long long time) = 0;

	/**
	 * @fn GetPlaylistMediaTypeFromTrack
	 *
	 * @param[in] type - track type
	 * @param[in] isIframe - Flag to indicate whether the track is iframe or not
	 *
	 * @return Mediatype
	 */
	AampMediaType GetPlaylistMediaTypeFromTrack(TrackType type, bool isIframe);

	/**
	 * @fn NotifyFragmentCollectorWait
	 *
	 * @return void
	 */
	void NotifyFragmentCollectorWait() {fragmentCollectorWaitingForPlaylistUpdate = true;}

	/**
	 * @fn EnterTimedWaitForPlaylistRefresh
	 *
	 * @param[in] timeInMs timeout in milliseconds
	 * @return void
	 */
	void EnterTimedWaitForPlaylistRefresh(int timeInMs);

	/**
	 * @fn Enabled
	 * @retval true if enabled, false if disabled
	 */
	bool Enabled();

	/**
	 * @fn InjectFragment
	 *
	 * @return Success/Failure
	 */
	bool InjectFragment();

	/**
	 * @fn ProcessFragmentChunk
	 */
	bool ProcessFragmentChunk();

	/**
	 * @fn CheckForDiscontinuity
	 *
	 * @return true/false
	 */
	bool CheckForDiscontinuity(CachedFragment* cachedFragment, bool& fragmentDiscarded, bool& isDiscontinuity, bool &ret);

	/**
	 * @fn ProcessAndInjectFragment
	 *
	 * @return true/false
	 */
	void ProcessAndInjectFragment(CachedFragment *cachedFragment, bool fragmentDiscarded, bool isDiscontinuity, bool &ret);

	/**
	 * @brief Get total fragment injected duration
	 *
	 * @return Total duration in seconds
	 */
	double GetTotalInjectedDuration();

	/**
 	* @brief update total fragment injected duration
	*
 	* @return void
	*/
	void UpdateInjectedDuration(double surplusDuration);

	/**
	* @brief Get total fragment chunk injected duration
	*
	* @return Total duration in seconds
	*/
	double GetTotalInjectedChunkDuration() { return totalInjectedChunksDuration; };

	/**
	 * @fn RunInjectLoop
	 *
	 * @return void
	 */
	void RunInjectLoop();

	/**
	 * @fn UpdateTSAfterFetch
	 * @param[in] IsInitSegment - Set to true for initialization segments; otherwise, set to false
	 * @return void
	 */
	void UpdateTSAfterFetch(bool IsInitSegment);

	/**
	 * @fn UpdateTSAfterChunkFetch
	 *
	 * @return void
	 */
	void UpdateTSAfterChunkFetch();

	/**
	 * @fn WaitForFreeFragmentAvailable
	 * @param timeoutMs - timeout in milliseconds. -1 for infinite wait
	 * @retval true if fragment available, false on abort.
	 */
	bool WaitForFreeFragmentAvailable( int timeoutMs = -1);

	/**
	 * @fn WaitForCachedFragmentChunkInjected
	 * @retval true if fragment chunk injected , false on abort.
	 */
	bool WaitForCachedFragmentChunkInjected(int timeoutMs = -1);

	/**
	 * @fn AbortWaitForCachedAndFreeFragment
	 *
	 * @param[in] immediate - Forced or lazy abort as in a seek/ stop
	 * @return void
	 */
	void AbortWaitForCachedAndFreeFragment(bool immediate);
	/**
	 * @brief Notifies profile changes to subclasses
	 *
	 * @return void
	 */
	virtual void ABRProfileChanged(void) = 0;

	/**
	 * @brief Function to update skip duration on PTS restamp
	 *
	 * @param position point to which fragment need to be skipped
	 * @param duration fragment duration to be skipped
	 * @return void
	 */
	virtual void updateSkipPoint(double position, double duration ) = 0 ;

	 /**
	 * @fn setDiscontinuityState
	 *
	 * @param isDiscontinuity - true if discontinuity false otherwise
	 * @return void
	 */
	virtual void setDiscontinuityState(bool isDiscontinuity) = 0;

	/**
	 * @fn abortWaitForVideoPTS
	 * @return void
	 */
	virtual void abortWaitForVideoPTS() = 0;

	/**
	 * @fn resetPTSOnAudioSwitch
	 * @return void
	 */
	virtual void resetPTSOnAudioSwitch(CachedFragment* cachedFragment = nullptr) {};

	/**
	 *   @brief Function to get the buffer duration
	 *
	 *   @return buffer value
	 */
	virtual double GetBufferedDuration (void) = 0;

	/**
	 * @brief Get number of fragments downloaded
	 *
	 * @return Number of downloaded fragments
	 */
	int GetTotalFragmentsFetched(){ return totalFragmentsDownloaded; }

	/**
	 * @fn GetFetchBuffer
	 *
	 * @param[in] initialize - Buffer to to initialized or not
	 * @return Fragment cache buffer
	 */
	CachedFragment* GetFetchBuffer(bool initialize);

	/**
	 * @fn GetFetchChunkBuffer
	 * @param[in] initialize true to initialize the fragment chunk
	 * @retval Pointer to fragment chunk buffer.
	 */
	CachedFragment* GetFetchChunkBuffer(bool initialize);

	/**
	 * @fn SetCurrentBandWidth
	 *
	 * @param[in] bandwidthBps - Bandwidth in bps
	 * @return void
	 */
	void SetCurrentBandWidth(int bandwidthBps);

	/**
	 * @fn GetProfileIndexForBW
	 * @param mTsbBandwidth - bandwidth to identify profile index from list
	 * @retval profile index of the current bandwidth
	 */
	int GetProfileIndexForBW(BitsPerSecond mTsbBandwidth);

	/**
	 * @fn GetCurrentBandWidth
	 *
	 * @return Bandwidth in bps
	 */
	int GetCurrentBandWidth();

	/**
	 * @brief Get total duration of fetched fragments
	 *
	 * @return Total duration in seconds
	 */
	double GetTotalFetchedDuration() { return totalFetchedDuration; };

	/**
	 * @brief Get total duration of fetched fragments
	 *
	 * @return Total duration in seconds
	 */
	double GetTotalInjectedChunksDuration() { return totalInjectedChunksDuration; };

	/**
	 * @brief Check if discontinuity is being processed
	 *
	 * @return true if discontinuity is being processed
	 */
	bool IsDiscontinuityProcessed() { return discontinuityProcessed; }

	bool isFragmentInjectorThreadStarted( ) {  return fragmentInjectorThreadStarted;}
	void MonitorBufferHealth();
	/**
	 * @brief Signal the clock to subtitle module
	 */
	void UpdateSubtitleClockTask();

	void ScheduleBufferHealthMonitor();

	/**
	 * @fn GetBufferStatus
	 *
	 * @return BufferHealthStatus
	 */
	BufferHealthStatus GetBufferStatus();

	/**
	 * @brief Get buffer health status
	 *
	 * @return current buffer health status
	 */
	BufferHealthStatus GetBufferHealthStatus() { return bufferStatus; };

	/**
	 * @fn AbortWaitForCachedFragment
	 *
	 * @return void
	 */
	void AbortWaitForCachedFragment();

	/**
	 * @brief Check whether track data injection is aborted
	 *
	 * @return true if injection is aborted, false otherwise
	 */
	bool IsInjectionAborted() { return (abort || abortInject); }

	/**
	 * @brief Set local TSB injection flag
	 */
	void SetLocalTSBInjection(bool value);

	/**
	 * @brief Is injection from local AAMP TSB
	 *
	 * @return true if injection is from local AAMP TSB, false otherwise
	 */
	bool IsLocalTSBInjection() {return mIsLocalTSBInjection.load();}

	/**
	 * @brief Returns if the end of track reached.
	 */
	virtual bool IsAtEndOfTrack() { return eosReached;}

	/**
	 * @fn CheckForFutureDiscontinuity
	 *
	 * @param[out] cacheDuration - cached fragment duration in seconds
	 * @return bool - true if discontinuity present, false otherwise
	 */
	bool CheckForFutureDiscontinuity(double &cacheDuration);

	/**
	 * @fn OnSinkBufferFull
	 *
	 * @return void
	 */
	void OnSinkBufferFull();

	/**
	 * @fn FlushFetchedFragments
	 * @return void
 	 */
	void FlushFetchedFragments();

	/**
	 * @fn FlushFragments
	 * @return void
	 */
	void FlushFragments();

	/**
	 * @fn FlushFragmentChunks
	 *
	 * @return void
	 */
	void FlushFragmentChunks();
	/**
	 * @brief API to wait thread until the fragment cached after audio reconfiguration
	 */
	void WaitForCachedAudioFragmentAvailable(void);
	/**
	 * @brief API to wait thread until the fragment cached after subtitle reconfiguration
	 */
	void WaitForCachedSubtitleFragmentAvailable(void);

	/**
	 * @brief To Load New Audio on seamless audio switch
	 */
	void LoadNewAudio(bool val);
	/**
	 * @brief To Load New subtitle on seamless subtitle switch
	 */
	void LoadNewSubtitle(bool val);

	/**
	 * @brief To set Track's Fetch and Inject duration after playlist update
	 */
	void OffsetTrackParams(double deltaFetchedDuration, double deltaInjectedDuration, int deltaFragmentsDownloaded);

	/**
	 *  @brief SignalIfEOSReached - Signal end-of-stream to pipeline if injector at EOS
	 *
	 * @return bool
	 */
	bool SignalIfEOSReached();

	/**
	 * @brief GetCachedFragmentChunksSize - Getter for fragment chunks cache size
	 *
	 * @return size_t
	 */
	std::size_t GetCachedFragmentChunksSize() { return mCachedFragmentChunksSize; }

	/**
	 * @brief SetCachedFragmentChunksSize - Setter for fragment chunks cache size
	 *
	 * @param[in] size Size for fragment chunks cache
	 */
	void SetCachedFragmentChunksSize(size_t size);

	void SourceFormat(StreamOutputFormat fmt) { mSourceFormat = fmt; }

	/**
	 * APi to set monitor buffer status; this is to avoid running it in L1 test;
	 */
	void SetMonitorBufferDisabled(bool isStartd)
	{
		bufferMonitorThreadDisabled = isStartd;
	}

	/**
	 * @brief API to notify the after Aamp Audio fragment cached
	 */
	void NotifyCachedAudioFragmentAvailable(void);
	/**
	 * @brief API to notify the after Aamp Audio fragment cached
	 */
	void  FlushAudioPositionDuringTrackSwitch(  CachedFragment* cachedFragment );
	/**
	 * @brief API to notify the after Aamp Subtitle fragment cached
	 */
	void NotifyCachedSubtitleFragmentAvailable(void);
	/**
	 * @brief  To flush the subtitle position even if the MediaProcessor is not not enabled.
	 */
	void  FlushSubtitlePositionDuringTrackSwitch(  CachedFragment* cachedFragment );

	/**
	 * @fn ResetTrickModePtsRestamping
	 * @brief Reset trick mode PTS restamping
	 */
	void ResetTrickModePtsRestamping(void);

protected:
	/**
	 * @fn UpdateTSAfterInject
	 *
	 * @return void
	 */
	void UpdateTSAfterInject();

	/**
	 * @brief Update segment cache and inject buffer to gstreamer
	 *
	 * @return void
	 */
	void UpdateTSAfterChunkInject();

	/**
	 * @fn WaitForCachedFragmentAvailable
	 *
	 * @return TRUE if fragment available, FALSE if aborted/fragment not available.
	 */
	bool WaitForCachedFragmentAvailable();

	/**
	 * @fn WaitForCachedFragmentChunkAvailable
	 *
	 * @return TRUE if fragment chunk available, FALSE if aborted/fragment chunk not available.
	 */
	bool WaitForCachedFragmentChunkAvailable();

	/**
	 * @brief Get the context of media track. To be implemented by subclasses
	 *
	 * @return Pointer to StreamAbstractionAAMP object
	 */
	virtual class StreamAbstractionAAMP* GetContext() = 0;

	/**
	 * @brief To be implemented by derived classes to receive cached fragment.
	 *
	 * @param[in] cachedFragment - contains fragment to be processed and injected
	 * @param[out] fragmentDiscarded - true if fragment is discarded.
	 * @return void
	 */
	virtual void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity=false) = 0;

	/**
	 * @fn InjectFragmentChunkInternal
	 *
	 * @param[in] mediaType - Media type of the fragment
	 * @param[in] buffer - contains fragment to be processed and injected
	 * @param[in] fpts - fragment PTS
	 * @param[in] fdts - fragment DTS
	 * @param[in] fDuration - fragment duration
	 * @param[in] fragmentPTSOffset - PTS offset to be applied
	 * @param[in] init - true if fragment is init fragment
	 * @param[in] discontinuity - true if there is a discontinuity, false otherwise
	 * @return void
	 */
	void InjectFragmentChunkInternal(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSOffset, bool init=false, bool discontinuity=false);


	static int GetDeferTimeMs(long maxTimeSeconds);


	/**
	 * @brief To be implemented by derived classes if discontinuity on trick-play is to be notified.
	 *
	 */
	virtual void SignalTrickModeDiscontinuity(){};

	double GetLastInjectedFragmentPosition() { return lastInjectedPosition; }

	/**
	 * @fn IsInjectionFromCachedFragmentChunks
	 *
	 * @brief Are fragments to inject coming from mCachedFragmentChunks
	 *
	 * @return True if fragments to inject are coming from mCachedFragmentChunks
	 */
	bool IsInjectionFromCachedFragmentChunks();

private:
	/**
	 * @fn GetBufferHealthStatusString
	 *
	 * @return string representation of buffer status
	 */
	static const char* GetBufferHealthStatusString(BufferHealthStatus status);

	/**
	 * @fn TrickModePtsRestamp
	 *
	 * @brief PTS restamp one i-frame cached segment for trick modes
	 *
	 * @param[in] cachedFragment - fragment to be restamped for trickmodes
	 */
	void TrickModePtsRestamp(CachedFragment* cachedFragment);
	
	std::string RestampSubtitle( const char* buffer, size_t bufferLen, double position, double duration, double pts_offset );

	/**
	 * @fn TrickModePtsRestamp
	 *
	 * @brief PTS restamp one fragment of an i-frame cached segment for trick
	 *        modes. Used for low-latency DASH content.
	 *
	 * @param[in,out] fragment - fragment to be restamped for trickmodes
	 * @param[in,out] position - PTS of the fragment; in original, out restamped
	 * @param[in,out] duration - fragment duration; in original, out restamped
	 * @param[in] initFragment - true for init fragments, false for media fragments
	 * @param[in] discontinuity - true if there is a discontinuity, false otherwise
	 */
	void TrickModePtsRestamp(AampGrowableBuffer &fragment, double &position, double &duration,
							bool initFragment, bool  discontinuity);

	/**
	 * Handles the fragment position jump for the media track.
	 *
	 * This function is responsible for handling the fragment position jump for the media track.
	 * It calculates the delta between the last injected fragment end position and the current fragment position,
	 * and updates the total injected duration accordingly.
	 *
	 * @param cachedFragment pointer to the cached fragment.
	 */
	void HandleFragmentPositionJump(CachedFragment* cachedFragment);

public:
	bool eosReached;                    /**< set to true when a vod asset has been played to completion */
	bool enabled;                       /**< set to true if track is enabled */
	int numberOfFragmentsCached;        /**< Number of fragments cached in this track*/
	int numberOfFragmentChunksCached;   /**< Number of fragments cached in this track*/
	const char* name;                   /**< Track name used for debugging*/
	double fragmentDurationSeconds;     /**< duration in seconds for current fragment-of-interest */
	int segDLFailCount;                 /**< Segment download fail count*/
	int segDrmDecryptFailCount;         /**< Segment decryption failure count*/
	int mSegInjectFailCount;            /**< Segment Inject/Decode fail count */
	TrackType type;                     /**< Media type of the track*/
	std::unique_ptr<SubtitleParser> mSubtitleParser;    /**< Parser for subtitle data*/
	bool refreshSubtitles;              /**< Switch subtitle track in the FetchLoop */
	bool refreshAudio;                  /** Switch audio track in the FetcherLoop */
	int maxCachedFragmentsPerTrack;
	int maxCachedFragmentChunksPerTrack;
	std::condition_variable fragmentChunkFetched;/**< Signaled after a fragment Chunk is fetched*/
	int noMDATCount;                    /**< MDAT Chunk Not Found count continuously while chunk buffer processing*/
	double m_totalDurationForPtsRestamping;
	std::shared_ptr<MediaProcessor> playContext;		/**< state for s/w demuxer / pts/pcr restamper module */
	bool seamlessAudioSwitchInProgress; /**< Flag to indicate seamless audio track switch in progress */
	bool seamlessSubtitleSwitchInProgress;
	bool mCheckForRampdown;		        /**< flag to indicate if the track is undergoing rampdown or not */

protected:
	PrivateInstanceAAMP* aamp;          /**< Pointer to the PrivateInstanceAAMP*/
	std::shared_ptr<IsoBmffHelper> mIsoBmffHelper; /**< Helper class for ISO BMFF parsing */
	CachedFragment *mCachedFragment;    /**< storage for currently-downloaded fragment */
	CachedFragment mCachedFragmentChunks[DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK];
	AampGrowableBuffer unparsedBufferChunk; /**< Buffer to keep fragment content */
	AampGrowableBuffer parsedBufferChunk;   /**< Buffer to keep fragment content */
	bool abort;                         /**< Abort all operations if flag is set*/
	std::mutex mutex;                   /**< protection of track variables accessed from multiple threads */
	bool ptsError;                      /**< flag to indicate if last injected fragment has ptsError */
	bool abortInject;                   /**< Abort inject operations if flag is set*/
	bool abortInjectChunk;              /**< Abort inject operations if flag is set*/
	std::mutex audioMutex;              /**< protection of audio track reconfiguration */
	bool loadNewAudio;                  /**< Flag to indicate new audio loading started on seamless audio switch */
	std::mutex subtitleMutex;
	bool loadNewSubtitle;

	StreamOutputFormat mSourceFormat {StreamOutputFormat::FORMAT_INVALID};

private:
	enum class TrickmodeState
	{
		UNDEF,
		FIRST_FRAGMENT,
		DISCONTINUITY,
		STEADY
	};
	std::condition_variable fragmentFetched;     	/**< Signaled after a fragment is fetched*/
	std::condition_variable fragmentInjected;    	/**< Signaled after a fragment is injected*/
	std::thread fragmentInjectorThreadID;  	/**< Fragment injector thread id*/
	std::condition_variable fragmentChunkInjected;	/**< Signaled after a fragment is injected*/
	std::thread bufferMonitorThreadID;    	/**< Buffer Monitor thread id */
	std::thread subtitleClockThreadID;    	/**< subtitle clock synchronisation thread id */
	int totalFragmentsDownloaded;       	/**< Total fragments downloaded since start by track*/
	int totalFragmentChunksDownloaded;      /**< Total fragments downloaded since start by track*/
	bool fragmentInjectorThreadStarted; 	/**< Fragment injector's thread started or not*/
	bool bufferMonitorThreadStarted;    	/**< Buffer Monitor thread started or not */
	bool UpdateSubtitleClockTaskStarted;    /**< Subtitle clock synchronization thread started, or not */
	bool bufferMonitorThreadDisabled;    	/**< Buffer Monitor thread Disabled or not */
	double totalInjectedDuration;       	/**< Total fragment injected duration*/
	double totalInjectedChunksDuration;  	/**< Total fragment injected chunk duration*/
	int currentInitialCacheDurationSeconds; /**< Current cached fragments duration before playing*/
	bool sinkBufferIsFull;                	/**< True if sink buffer is full and do not want new fragments*/
	bool cachingCompleted;              	/**< Fragment caching completed or not*/
	int fragmentIdxToInject;            	/**< Write position */
	int fragmentChunkIdxToInject;       	/**< Write position */
	int fragmentIdxToFetch;             	/**< Read position */
	int fragmentChunkIdxToFetch;        	/**< Read position */
	int bandwidthBitsPerSecond;        	/**< Bandwidth of last selected profile*/
	double totalFetchedDuration;        	/**< Total fragment fetched duration*/
	bool discontinuityProcessed;
	BufferHealthStatus bufferStatus;     /**< Buffer status of the track*/
	BufferHealthStatus prevBufferStatus; /**< Previous buffer status of the track*/
	long long prevDownloadStartTime;		/**< Previous file download Start time*/

	std::thread *playlistDownloaderThread;	/**< PlaylistDownloadThread of track*/
	bool playlistDownloaderThreadStarted;	/**< Playlist downloader thread started or not*/
	bool abortPlaylistDownloader;			/**< Flag used to abort playlist downloader*/
	std::condition_variable plDownloadWait;	/**< Conditional variable for signaling timed wait*/
	std::mutex dwnldMutex;					/**< Download mutex for conditional timed wait, used for playlist and fragment downloads*/
	bool fragmentCollectorWaitingForPlaylistUpdate;	/**< Flag to indicate that the fragment collector is waiting for ongoing playlist download, used for profile changes*/
	std::condition_variable frDownloadWait;	/**< Conditional variable for signaling timed wait*/
	std::condition_variable audioFragmentCached;  /**< Signal after a audio fragment cached after reconfigure */
	double lastInjectedPosition;             /**< Last injected position */
	double lastInjectedDuration;             /**< Last injected fragment end position */
	std::condition_variable subtitleFragmentCached;
	std::atomic_bool mIsLocalTSBInjection;
	size_t mCachedFragmentChunksSize;		/**< Size of fragment chunks cache */
	AampTime mLastFragmentPts;				/**< pts of the previous fragment, used in trick modes */
	AampTime mRestampedPts;					/**< Restamped Pts of the segment, used in trick modes */
	AampTime mRestampedDuration;			/**< Restamped segment duration, used in trick modes */
	TrickmodeState mTrickmodeState;			/**< Current trick mode state */
	std::mutex mTrackParamsMutex;			/**< Mutex for track parameters */
};

/**
 * @brief StreamAbstraction class of AAMP
 */
class StreamAbstractionAAMP : public AampLicenseFetcher
{
public:
	std::map<long long, double> mPtsOffsetMap; /** @brief map from period index to pts offset, used for hls/ts pts restamping */

	/** @brief ABR mode */
	enum class ABRMode
	{
		UNDEF,
		ABR_MANAGER,	/**< @brief ABR manager is used by AAMP. */
		FOG_TSB			/**< @brief Fog manages ABR. */
	};

	/**
	 * @fn StreamAbstractionAAMP
	 * @param[in] aamp pointer to PrivateInstanceAAMP object associated with stream
	 */
	StreamAbstractionAAMP(PrivateInstanceAAMP* aamp, id3_callback_t mID3Handler = nullptr);

	/**
	 * @fn ~StreamAbstractionAAMP
	 */
	virtual ~StreamAbstractionAAMP();

	/**
	 * @brief StreamAbstractionAAMP Copy Constructor
	 */
	StreamAbstractionAAMP(const StreamAbstractionAAMP&) = delete;

	/**
	 * @brief StreamAbstractionAAMP assignment operator overloading
	 */
	StreamAbstractionAAMP& operator=(const StreamAbstractionAAMP&) = delete;

	/**
	 *   @brief  Initialize a newly created object.
	 *           To be implemented by sub classes
	 *
	 *   @param[in]  tuneType - to set type of playback.
	 *   @return true on success, false failure
	 */
	virtual AAMPStatusType Init(TuneType tuneType) = 0;

	/**
	 *   @brief  Initialize TSB Reader
	 *
	 *   @param[in]  tuneType - to set type of playback.
	 *   @return true on success, false failure
	 */
	virtual AAMPStatusType InitTsbReader(TuneType tuneType)
	{
		return eAAMPSTATUS_GENERIC_ERROR;
	}

	/**
	 *   @brief  Start streaming.
	 *
 	 *   @return void
	 */
	virtual void Start() = 0;

	/**
	*   @brief  Stops streaming.
	*
	*   @param[in]  clearChannelData - clear channel /drm data on stop.
	*   @return void
	*/
	virtual void Stop(bool clearChannelData) = 0;

	/**
	 *   @brief Get output format of stream.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @param[out]  auxAudioOutputFormat - format of aux audio track
	 *   @return void
	 */
	virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) = 0;

	/**
	 *   @brief Get current stream position.
	 *
	 *   @return current position of stream.
	 */
	virtual double GetStreamPosition()
	{
		return 0.0;
	}

	/**
	 *   @brief  Get PTS of first sample.
	 *
	 *   @return PTS of first sample
	 */
	virtual double GetFirstPTS()
	{
		return 0.0;
	}

	/**
	 *   @brief  Get Start time PTS of first sample.
	 *
	 *   @retval start time of first sample
	 */
	virtual double GetStartTimeOfFirstPTS()
	{
		return 0.0;
	}

	/**
	 *   @brief  Returns AvailabilityStartTime from the manifest
	 *
	 *   @retval double . AvailabilityStartTime
	 */
	virtual double GetAvailabilityStartTime()
	{
		return 0.0;
	}

	/**
	 *   @brief Return MediaTrack of requested type
	 *
	 *   @param[in]  type - track type
	 *   @return MediaTrack pointer.
	 */
	virtual MediaTrack* GetMediaTrack(TrackType type)
	{
		(void)type;
		return nullptr;
	}

	/**
	*   @brief  Get PTS offset for MidFragment Seek
	*
	*   @return seek PTS offset for midfragment seek
	*/
	virtual double GetMidSeekPosOffset()
	{
		return 0.0;
	}

	/**
	 * @brief Sets the minimum buffer for ABR (Adaptive Bit Rate).
	 *
	 * @param minbuffer The minimum buffer value to be set for ABR.
	 * @return void
	 */
	void SetABRMinBuffer(int minbuffer) { if( minbuffer>=0 ) mABRMinBuffer = minbuffer;}

	/**
	 * @brief Sets the maximum buffer for ABR (Adaptive Bit Rate).
	 *
	 * @param maxbuffer The maximum buffer value to be set for ABR.
	 * @return void
	 */
	void SetABRMaxBuffer(int maxbuffer) { if( maxbuffer>=0 ) mABRMaxBuffer = maxbuffer;}

	/**
	 *   @brief Waits audio track injection until caught up with video track.
	 *          Used internally by injection logic
	 *
	 *   @param None
	 *   @return void
	 */
	void WaitForVideoTrackCatchup();

	/**
	 *   @fn ReassessAndResumeAudioTrack
	 *
	 *   @return void
	 */
	void ReassessAndResumeAudioTrack(bool abort);

	/**
	 *   @brief When TSB is involved, use this to set bandwidth to be reported.
	 *
	 *   @param[in]  tsbBandwidth - Bandwidth of the track.
	 *   @return void
	 */
	void SetTsbBandwidth(long tsbBandwidth){ if( tsbBandwidth >=0 ) mTsbBandwidth  = tsbBandwidth; }
	/**
	 *   @brief When TSB is involved, use this to get bandwidth to be reported.
	 *
	 *   @return Bandwidth of the track.
	 */
	long GetTsbBandwidth() { return mTsbBandwidth ;}

	/**
	 *   @brief Set elementary stream type change status for reconfigure the pipeline.
	 *
	 *   @return void
	 */
	void SetESChangeStatus(void){mAudiostateChangeCount++; mESChangeStatus = true;}

	/**
	 *   @brief Reset elementary stream type change status once the pipeline reconfigured.
	 *
	 *   @return void
	 */
	void ResetESChangeStatus(void){
		if( (mAudiostateChangeCount > 0) && !(--mAudiostateChangeCount) )
		{
			mESChangeStatus = false;
		}
	}

	/**
	 *   @brief Get elementary stream type change status for reconfigure the pipeline..
	 *
	 *   @retval mESChangeStatus flag value ( true or false )
	 */
	bool GetESChangeStatus(void){ return mESChangeStatus;}

	PrivateInstanceAAMP* aamp;  /**< Pointer to PrivateInstanceAAMP object associated with stream*/

	/**
	 *   @fn RampDownProfile
	 *
	 *   @param[in] http_error - Http error code
	 *   @return True, if ramp down successful. Else false
	 */
	bool RampDownProfile(int http_error);
	/**
	 *   @fn GetDesiredProfileOnBuffer
	 *
	 *   @param [in] currProfileIndex
	 *   @param [in] newProfileIndex
	 *   @return None.
	 */
	void GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex);
	/**
	 *   @fn GetDesiredProfileOnSteadyState
	 *
	 *   @param [in] currProfileIndex
	 *   @param [in] newProfileIndex
	 *   @param [in] nwBandwidth
	 *   @return None.
	 */
	void GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth);
	/**
	 *   @fn ConfigureTimeoutOnBuffer
	 *
	 *   @return None.
	 */
	void ConfigureTimeoutOnBuffer();
	/**
	 *   @brief Function to get the buffer duration of stream
	 *
	 *   @return buffer value
	 */
	virtual double GetBufferedDuration (void)
	{
		return -1.0;
	}

	/**
	 *   @fn IsLowestProfile
	 *
	 *   @param currentProfileIndex - current profile index to be checked.
	 *   @return true if the given profile index is lowest.
	 */
	bool IsLowestProfile(int currentProfileIndex);

	/**
	 *   @fn getOriginalCurlError
	 *
	 *   @param[in] http_error - Error code
	 *   @return error code
	 */
	int getOriginalCurlError(int http_error);

	/**
	 *   @fn CheckForRampDownProfile
	 *
	 *   @param http_error - Http error code
	 *   @return true if rampdown needed in the case of fragment not available in higher profile.
	 */
	bool CheckForRampDownProfile(int http_error);

	/**
	 *   @fn CheckForProfileChange
	 *
	 *   @return void
	 */
	void CheckForProfileChange(void);

	/**
	 *   @fn GetIframeTrack
	 *
	 *   @return iframe track index.
	 */
	int GetIframeTrack();

	/**
	 *   @fn UpdateIframeTracks
	 *
	 *   @return void
	 */
	void UpdateIframeTracks();

	/**
	 *   @fn LastVideoFragParsedTimeMS
	 *
	 *   @return Last video fragment parsed time.
	 */
	double LastVideoFragParsedTimeMS(void);

	/**
	 *   @fn GetDesiredProfile
	 *
	 *   @param getMidProfile - Get the middle profile(True/False)
	 *   @return profile index to be used for the track.
	 */
	int GetDesiredProfile(bool getMidProfile);

	/**
	 *   @fn UpdateRampUpOrDownProfileReason
	 *
	 *   @return void
	 */
	void UpdateRampUpOrDownProfileReason(void);

	/**
	 *   @fn NotifyBitRateUpdate
	 *   Used internally by injection logic
	 *
	 *   @param[in]  profileIndex - profile index of last injected fragment.
	 *   @param[in]  cacheFragStreamInfo - stream info for the last injected fragment.
	 *   @return void
	 */
	void NotifyBitRateUpdate(int profileIndex, const StreamInfo &cacheFragStreamInfo, double position);

	/**
	 *   @fn IsInitialCachingSupported
	 *
	 *   @return true if is supported
	 */
	virtual bool IsInitialCachingSupported();

	/**
	 *   @brief Whether we are playing at live point or not.
	 *
	 *   @return true if we are at live point.
	 */
	bool IsStreamerAtLivePoint(double seekPosition = 0 );

	/**
	 *   @brief Whether we seeked to live offset range or not.
	 *
	 *   @return true if we seeked to live.
	 */
	bool IsSeekedToLive(double seekPosition);

	/**
	 * @fn Is4KStream
	 * @brief check if current stream have 4K content
	 * @param height - resolution of 4K stream if found
	 * @param bandwidth - bandwidth of 4K stream if found
	 * @return true on success
	 */
	virtual bool Is4KStream(int &height, BitsPerSecond &bandwidth)
	{
		return false;
	}

	/**
	 * @fn GetPreferredLiveOffsetFromConfig
	 * @brief Set the offset value Live object
	 * @return none
	 */
	virtual bool GetPreferredLiveOffsetFromConfig();

	/**
	 *   @fn NotifyPlaybackPaused
	 *
	 *   @param[in] paused - true, if playback was paused
	 *   @return void
	 */
	virtual void NotifyPlaybackPaused(bool paused);

	/**
	 *   @fn CheckIfPlayerRunningDry
	 *
	 *   @return true if player caches are dry, false otherwise.
	 */
	bool CheckIfPlayerRunningDry(void);

	/**
	 *   @fn CheckForPlaybackStall
	 *
	 *   @param[in] fragmentParsed - true if next fragment was parsed, otherwise false
	 */
	void CheckForPlaybackStall(bool fragmentParsed);

	/**
	 *   @fn NotifyFirstFragmentInjected
	 *   @return void
	 */
	void NotifyFirstFragmentInjected(void);

	/**
	 *   @fn GetElapsedTime
	 *
	 *   @return elapsed time.
	 */
	double GetElapsedTime();

	virtual double GetFirstPeriodStartTime() { return 0; }
	virtual double GetFirstPeriodDynamicStartTime() { return 0; }
	virtual void RefreshTrack(AampMediaType type) {};
	virtual uint32_t GetCurrPeriodTimeScale()  { return 0; }
	/**
	 *   @fn CheckForRampDownLimitReached
	 *   @return true if limit reached, false otherwise
	 */
	bool CheckForRampDownLimitReached();

	/**
	 * @fn UseIframeTrack
	 * @brief Check if AAMP is using an iframe track
	 *
	 * @return true if AAMP is using an iframe track, false otherwise
	 */
	virtual bool UseIframeTrack(void) { return trickplayMode; }

	/**
	 * @fn SetTrickplayMode
	 * @brief Set trickplay mode depending on rate
	 *
	 * @param rate - play rate
	 */
	void SetTrickplayMode(float rate) { trickplayMode = (rate != AAMP_NORMAL_PLAY_RATE); }

	/**
	 * @fn ResetTrickModePtsRestamping
	 * @brief Reset trick mode PTS restamping
	 */
	void ResetTrickModePtsRestamping(void);

	/**
	 * @fn SetVideoPlaybackRate
	 * @brief Set the Video playback rate
	 *
	 * @param[in] rate - play rate
	 */
	void SetVideoPlaybackRate(float rate);

	/**
	 * @fn ResumeTrackDownloadsHandler
	 *
	 * @return void
	 */
	void ResumeTrackDownloadsHandler();

	/**
	 * @fn ResumeTrackDownloadsHandler
	 *
	 * @return void
	 */
	void StopTrackDownloadsHandler();

	/**
	* @fn GetPlayerPositionsHandler
	*
	* @return seek & current position in seconds
	*/
	void GetPlayerPositionsHandler(long long& getPositionMS, double& seekPositionSeconds);

	/**
	* @fn SendVTTCueDataHandler
	*
	* @return void
	*/
	void SendVTTCueDataHandler(VTTCue* cueData);

	/**
	 * @fn InitializePlayerCallbacks
	 *
	 * @return void
	 */
	void InitializePlayerCallbacks(PlayerCallbacks& callbacks);

	/**
	 * @fn RegisterSubtitleParser_CB
	 * @brief Registers and initializes the subtitle parser based on the provided MIME type.
	 *
	 * @param[in] isExpectedMimeType - Indicates whether the expected MIME type.
	 * @param[in] mimeType - mime type as enum
	 * @param[out] SubtitleParser - Provides the created subtitle parser instance.
	 *
	 * @return SubtitleParser* - Pointer to the created subtitle parser instance.
	 */
	std::unique_ptr<SubtitleParser> RegisterSubtitleParser_CB( SubtitleMimeType mimeType, bool isExpectedMimeType = true);

	/**
	 * @fn RegisterSubtitleParser_CB
	 * @brief Registers and initializes the subtitle parser based on the provided MIME type.
	 *
	 * @param[in] isExpectedMimeType - Indicates whether the expected MIME type.
	 * @param[in] mimeType - mime type as string
	 * @param[out] SubtitleParser - Provides the created subtitle parser instance.
	 *
	 * @return SubtitleParser* - Pointer to the created subtitle parser instance.
	 */
	std::unique_ptr<SubtitleParser> RegisterSubtitleParser_CB(std::string mimeType, bool isExpectedMimeType = true);

	bool trickplayMode;                     /**< trick play flag to be updated by subclasses*/
	int currentProfileIndex;                /**< current Video profile index of the track*/
	int currentAudioProfileIndex;           /**< current Audio profile index of the track*/
	int currentTextTrackProfileIndex;       /**< current SubTitle profile index of the track*/
	int profileIdxForBandwidthNotification; /**< internal - profile index for bandwidth change notification*/
	bool hasDrm;                            /**< denotes if the current asset is DRM protected*/

	bool mIsAtLivePoint;                    /**< flag that denotes if playback is at live point*/

	bool mIsPlaybackStalled;                /**< flag that denotes if playback was stalled or not*/
	bool mNetworkDownDetected;              /**< Network down status indicator */
	TuneType mTuneType;                     /**< Tune type of current playback, initialize by derived classes on Init()*/
	int mRampDownCount;		        /**< Total number of rampdowns */
	double mProgramStartTime;	        /**< Indicate program start time or availability start time */
	int mTsbMaxBitrateProfileIndex;		/**< Indicates the index of highest profile in the saved stream info */
	bool mUpdateReason;			/**< flag to update the bitrate change reason */
	AampTime mPTSOffset;				/*For PTS restamping*/

	/**
	 *   @brief Get profile index of highest bandwidth
	 *
	 *   @return Profile index
	 */
	int GetMaxBWProfile();

	/**
	 *   @brief Get profile index of given bandwidth.
	 *
	 *   @param[in]  bandwidth - Bandwidth
	 *   @return Profile index
	 */
	virtual int GetBWIndex( BitsPerSecond bandwidth) { (void)bandwidth; return 0;	}

	/**
	 *    @brief Get the ABRManager reference.
	 *
	 *    @return The ABRManager reference.
	 */
	ABRManager& GetABRManager() {
		return aamp->mhAbrManager;
	}

	/**
	 *   @brief Get number of profiles/ representations from subclass.
	 *
	 *   @return number of profiles.
	 */
	virtual int GetProfileCount() {
		return aamp->mhAbrManager.getProfileCount();
	}

	/**
	 * @brief Get profile index for TsbBandwidth
	 * @param mTsbBandwidth - bandwidth to identify profile index from list
	 * @retval profile index of the current bandwidth
	 */
	virtual int GetProfileIndexForBandwidth(BitsPerSecond mTsbBandwidth)
	{
		return aamp->mhAbrManager.getBestMatchedProfileIndexByBandWidth((int)mTsbBandwidth);
	}

	BitsPerSecond GetCurProfIdxBW(){
		return aamp->mhAbrManager.getBandwidthOfProfile(this->currentProfileIndex);
	}


	/**
	 *   @brief Gets Max bitrate supported
	 *
	 *   @return max bandwidth
	 */
	virtual BitsPerSecond GetMaxBitrate(){
		return aamp->mhAbrManager.getBandwidthOfProfile(aamp->mhAbrManager.getMaxBandwidthProfile());
	}

	/**
	 *   @fn GetVideoBitrate
	 *
	 *   @return bitrate of current video profile.
	 */
	BitsPerSecond GetVideoBitrate(void);

	/**
	 *   @fn GetAudioBitrate
	 *
	 *   @return bitrate of current audio profile.
	 */
	BitsPerSecond GetAudioBitrate(void);

	/**
	 *   @brief Set a preferred bitrate for video.
	 *
	 *   @param[in] bitrate preferred bitrate.
	 */
	void SetVideoBitrate(BitsPerSecond bitrate);

	/**
	 *   @brief Get available video bitrates.
	 *
	 *   @return available video bitrates.
	 */
	virtual std::vector<BitsPerSecond> GetVideoBitrates(void)
	{
		return std::vector<BitsPerSecond>();
	}

	/**
	 *   @brief Get available audio bitrates.
	 *
	 *   @return available audio bitrates.
	 */
	virtual std::vector<BitsPerSecond> GetAudioBitrates(void)
	{
		return std::vector<BitsPerSecond>();
	}

	/**
	 *   @brief Check if playback stalled in fragment collector side.
	 *
	 *   @return true if stalled, false otherwise.
	 */
	bool IsStreamerStalled(void) { return mIsPlaybackStalled; }

	/**
	 *   @brief Stop injection of fragments.
	 */
	virtual void StopInjection(void) { }

	/**
	 *   @brief Start injection of fragments.
	 */
	virtual void StartInjection(void) { }

	/**
	 *   @fn IsMuxedStream
	 *
	 *   @return true if current stream is muxed
	 */
	bool IsMuxedStream();

	/**
	 *   @brief Receives first video PTS for the current playback
	 *
	 *   @param[in] pts - pts value
	 *   @param[in] timeScale - time scale value
	 */
	virtual void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale) { };

	/**
	 * @brief Kicks off subtitle display - sent at start of video presentation
	 *
	 */
	virtual void StartSubtitleParser() { };

	/**
	 *   @brief Pause/unpause subtitles
	 *
	 *   @param pause - enable or disable pause
	 *   @return void
	 */
	virtual void PauseSubtitleParser(bool pause) { };

	/**
	 *   @fn WaitForAudioTrackCatchup
	 *
	 *   @return void
	 */
	void WaitForAudioTrackCatchup(void);

	/**
	 *   @fn AbortWaitForAudioTrackCatchup
	 *
	 *   @return void
	 */
	void AbortWaitForAudioTrackCatchup(bool force);

	/**
	 *   @brief Set Client Side DAI object instance
	 *
	 *   @param[in] cdaiObj - Pointer to Client Side DAI object.
	 */
	virtual void SetCDAIObject(CDAIObject *cdaiObj) {};

	/**
	 *   @fn IsEOSReached
	 *
	 *   @return true if end of stream reached, false otherwise
	 */
	virtual bool IsEOSReached();

	/**
	 *   @brief Get available audio tracks.
	 *
	 *   @return std::vector<AudioTrackInfo> list of audio tracks
	 */
	virtual std::vector<AudioTrackInfo> &GetAvailableAudioTracks(bool allTrack = false) { return mAudioTracks; };

	/**
	 *   @brief Get available text tracks.
	 *
	 *   @return std::vector<TextTrackInfo> list of text tracks
	 */
	virtual std::vector<TextTrackInfo> &GetAvailableTextTracks(bool allTrack = false) { return mTextTracks; };

	/**
	*   @brief Update seek position when player is initialized
	*
	*   @param[in] secondsRelativeToTuneTime can be the offset (seconds from tune time) or absolute position (seconds from 1970)
	*/
	virtual void SeekPosUpdate(double secondsRelativeToTuneTime) { (void) secondsRelativeToTuneTime ;}

	/**
	 *   @fn GetLastInjectedFragmentPosition
	 *
	 *   @return double last injected fragment position in seconds
	 */
	double GetLastInjectedFragmentPosition();

	/**
	 *   @fn resetDiscontinuityTrackState
	 *
	 *   @return void
	 */
	void resetDiscontinuityTrackState();

	/**
	 *   @fn ProcessDiscontinuity
	 *
	 *   @param[in] type - track type.
	 */
	bool ProcessDiscontinuity(TrackType type);

	/**
	 *   @fn AbortWaitForDiscontinuity
	 *   @return void
	 */
	void AbortWaitForDiscontinuity();

	/**
	 *   @fn CheckForMediaTrackInjectionStall
	 *
	 *   @param[in] type - track type.
	 */
	void CheckForMediaTrackInjectionStall(TrackType type);

	/**
	 *   @fn GetBufferedVideoDurationSec
	 *
	 *   @return duration of currently buffered video in seconds
	 */
	double GetBufferedVideoDurationSec();

	/**
	 *   @fn UpdateStreamInfoBitrateData
	 *
	 *   @param[in]  profileIndex - profile index of current fetched fragment
	 *   @param[out]  cacheFragStreamInfo - stream info of current fetched fragment
	 */
	void UpdateStreamInfoBitrateData(int profileIndex, StreamInfo &cacheFragStreamInfo);

	/**
	 *   @fn GetAudioTrack
	 *
	 *   @return int - index of current audio track
	 */
	virtual int GetAudioTrack();

	/**
	 *   @fn GetCurrentAudioTrack
	 *
	 *   @param[out] audioTrack - current audio track
	 *   @return found or not
	 */
	virtual bool GetCurrentAudioTrack(AudioTrackInfo &audioTrack);

	/**
	 *   @fn GetCurrentTextTrack
	 *
	 *   @param[out] TextTrack - current text track
	 *   @return found or not
	 */
	virtual bool GetCurrentTextTrack(TextTrackInfo &textTrack);

	/**
	 *   @fn API to verify in-band CC availability for a stream.
	 *
	 *   @return bool  - ture-if the stream has inband cc
	 */
	bool isInBandCcAvailable();
	/**
	 *   @fn GetTextTrack
	 *
	 *   @return int - index of current text track
	 */
	int GetTextTrack();

	/**
	 *   @fn RefreshSubtitles
	 *
	 *   @return void
	 */
	void RefreshSubtitles();
	/**
	 * @brief setVideoRectangle sets the position coordinates (x,y) & size (w,h) for OTA streams only
	 *
	 * @param[in] x,y - position coordinates of video rectangle
	 * @param[in] w,h - width & height of video rectangle
	 */
	virtual void SetVideoRectangle(int x, int y, int w, int h) {}

	virtual std::vector<StreamInfo*> GetAvailableVideoTracks(void)
	{ // STUB
		return std::vector<StreamInfo*>();
	}

	/**
	 *   @brief Get available thumbnail bitrates.
	 *
	 *   @return available thumbnail bitrates.
	 */
	virtual std::vector<StreamInfo *> GetAvailableThumbnailTracks(void)
	{ // STUB
		return std::vector<StreamInfo *>();
	}

	/**
	 *   @brief Set thumbnail bitrate.
	 *
	 *   @return none.
	 */
	virtual bool SetThumbnailTrack(int thumbnailIndex)
	{
		(void) thumbnailIndex;	/* unused */
		return false;
	}

	/**
	 *   @brief Get thumbnail data for duration value.
	 *
	 *   @return thumbnail data.
	 */
	virtual std::vector<ThumbnailData> GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
	{
		(void)start;
		(void)end;
		(void)baseurl;
		(void)raw_w;
		(void)raw_h;
		(void)width;
		(void)height;

		return std::vector<ThumbnailData>();
	}

	/**
	 * @brief SetAudioTrack set the audio track using index value. [currently for OTA]
	 *
	 * @param[in] index -  Index of audio track
	 * @return void
	 */
	virtual void SetAudioTrack(int index) {}

	/**
	 * @brief SetAudioTrackByLanguage set the audio language. [currently for OTA]
	 *
	 * @param[in] lang Language to be set
	 * @param[in]
	 */
	virtual void SetAudioTrackByLanguage(const char *lang) {}

	/**
	 * @brief SetPreferredAudioLanguages set the preferred audio languages and rendition. [currently for OTA]
	 *
	 * @param[in]
	 * @param[in]
	 */
	virtual void SetPreferredAudioLanguages() {}

	/**
	 * @fn MuteSubtitles
	 *
	 * @param[in] mute mute/unmute
	 */
	void MuteSubtitles(bool mute);

	/**
	 * @fn WaitForVideoTrackCatchupForAux
	 *
	 * @return void
	 */
	void WaitForVideoTrackCatchupForAux();

	/**
	 *   @brief Set Content Restrictions
	 *   @param[in] restrictions - restrictions to be applied
	 *
	 *   @return void
	 */
	virtual void ApplyContentRestrictions(std::vector<std::string> restrictions){};

	/**
	 *   @brief Disable Content Restrictions - unlock
	 *   @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *   @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *   @param[in] eventChange - disable restriction handling till next program event boundary
	 *   @return void
	 */
	virtual void DisableContentRestrictions(long grace, long time, bool eventChange){};

	/**
	 *   @brief Enable Content Restrictions - lock
	 *   @return void
	 */
	virtual void EnableContentRestrictions(){};

	/**
	 *   @brief Get audio forward to aux pipeline status
	 *
	 *   @return bool true if audio buffers are to be forwarded
	 */
	bool GetAudioFwdToAuxStatus() { return mFwdAudioToAux; }

	/**
	 *   @brief Set audio forward to aux pipeline status
	 *
	 *   @param[in] status - enabled/disabled
	 *   @return void
	 */
	void SetAudioFwdToAuxStatus(bool status) { mFwdAudioToAux = status; }

	/**
	 * @brief Notify playlist downloader threads of tracks
	 *
	 * @return void
	 */
	void DisablePlaylistDownloads();

	/**
	 *   @brief Set AudioTrack info from Muxed stream
	 *
	 *   @param[in] string index
	 *   @return void
	 */
	virtual void SetAudioTrackInfoFromMuxedStream(std::vector<AudioTrackInfo>& vector);

	/**
	 *   @brief Set current audio track index
	 *
	 *   @param[in] string index
	 *   @return void
	 */
	void SetCurrentAudioTrackIndex(std::string& index) { mAudioTrackIndex = index; }

	/**
	 *   @brief Change muxed audio track index
	 *
	 *   @param[in] string index
	 *   @return void
	 */
	virtual void ChangeMuxedAudioTrackIndex(std::string& index){};

	//Apis for sidecar caption support

	/**
	 *   @brief Initialize subtitle parser for sidecar support
	 *
	 *   @param data - subtitle data received from application
	 *   @return void
	 */
	virtual void InitSubtitleParser(char *data) {};

	/**
	 *   @brief reset subtitle parser created for sidecar support
	 *
	 *   @return void
	 */
	virtual void ResetSubtitle() {};

	/**
	 *   @brief mute subtitles on pause
	 *
	 *   @return void
	 */
	virtual void MuteSubtitleOnPause() {};

	/**
	 *   @brief resume subtitles on play
	 *
	 *   @param mute - mute status
	 *   @param data - subtitle data received from application
	 *   @return void
	 */
	virtual void ResumeSubtitleOnPlay(bool mute, char *data) {};

	/**
	 *   @brief mute/unmute sidecar subtitles
	 *   @param mute - mute/unmute
	 *
	 *   @return void
	 */
	virtual void MuteSidecarSubtitles(bool mute) { };

	/**
	 *   @brief resume subtitles after trickplay
	 *
	 *   @param mute - mute status
	 *   @param data - subtitle data received from application
	 *   @return void
	 */
	virtual void ResumeSubtitleAfterSeek(bool mute, char *data) { };

	/**
	 *   @fn SetTextStyle
	 *   @brief Set the text style of the subtitle to the options passed
	 *   @param[in] options - reference to the Json string that contains the information
	 *   @return - true indicating successful operation in passing options to the parser
	 */
	virtual bool SetTextStyle(const std::string &options);

	/**
	 * @brief Get the ABR mode.
	 *
	 * @return the ABR mode.
	 */
	virtual ABRMode GetABRMode() { return ABRMode::UNDEF; };

	virtual bool SelectPreferredTextTrack(TextTrackInfo &selectedTextTrack) { return false; };

protected:
	/**
	 *   @brief Get stream information of a profile from subclass.
	 *
	 *   @param[in]  idx - profile index.
	 *   @return stream information corresponding to index.
	 */
	virtual StreamInfo* GetStreamInfo(int idx)
	{
		(void)idx;
		return NULL;
	}

	/**
	 * @brief Initialize ISOBMFF Media Processor
	 *
	 * @return void
	 */
	void InitializeMediaProcessor();

//private:
protected:

	/**
	 *   @brief Get buffer value
	 *
	 *   @return buffer value based on Local TSB
	 */
	double GetBufferValue(MediaTrack *video);

	/**
	 *   @fn GetDesiredProfileBasedOnCache
	 *
	 *   @return Profile index based on cached duration
	 */
	int GetDesiredProfileBasedOnCache(void);

	/**
	 *   @fn UpdateProfileBasedOnFragmentDownloaded
	 *
	 *   @return void
	 */
	void UpdateProfileBasedOnFragmentDownloaded(void);

	/**
	 *   @fn UpdateProfileBasedOnFragmentCache
	 *
	 *   @return true if profile was changed, false otherwise
	 */
	bool UpdateProfileBasedOnFragmentCache(void);

	std::mutex mLock;              /**< lock for A/V track catchup logic*/
	std::condition_variable mCond;               /**< condition for A/V track catchup logic*/
	std::condition_variable mSubCond;            /**< condition for Audio/Subtitle track catchup logic*/
	std::condition_variable mAuxCond;            /**< condition for Aux and video track catchup logic*/

	// abr variables
	long mCurrentBandwidth;             /**< stores current bandwidth*/
	int mLastVideoFragCheckedForABR;    /**< Last video fragment for which ABR is checked*/
	long mTsbBandwidth;                 /**< stores bandwidth when TSB is involved*/
	bool mNwConsistencyBypass;          /**< Network consistency bypass**/
	int mABRHighBufferCounter;	    /**< ABR High buffer counter */
	int mABRLowBufferCounter;	    /**< ABR Low Buffer counter */
	int mMaxBufferCountCheck;
	int mABRMaxBuffer;	            /**< ABR ramp up buffer*/
	int mABRCacheLength;		    /**< ABR cache length*/
	int mABRBufferCounter;              /**< ABR Buffer Counter*/
	int mABRMinBuffer;		    /**< ABR ramp down buffer*/
	int mABRNwConsistency;		    /**< ABR Network consistency*/
	bool mESChangeStatus;               /**< flag value which is used to call pipeline configuration if the audio type changed in mid stream */
	unsigned int mAudiostateChangeCount;/**< variable to know how many times player need to reconfigure the pipeline for audio type change*/
	double mLastVideoFragParsedTimeMS;  /**< timestamp when last video fragment was parsed */

	bool mIsPaused;                     /**< paused state or not */
	long long mTotalPausedDurationMS;   /**< Total duration for which stream is paused */
	long long mStartTimeStamp;          /**< stores timestamp at which injection starts */
	long long mLastPausedTimeStamp;     /**< stores timestamp of last pause operation */
	std::mutex mStateLock;         /**< lock for A/V track discontinuity injection*/
	std::condition_variable mStateCond;          /**< condition for A/V track discontinuity injection*/
	int mRampDownLimit;                 /**< stores ramp down limit value */
	BitrateChangeReason mBitrateReason; /**< holds the reason for last bitrate change */
protected:
	std::vector<AudioTrackInfo> mAudioTracks;     /**< Available audio tracks */
	std::vector<AudioTrackInfo> mAudioTracksAll;  /**< Alternative variable to store audio track information from all period */
	std::vector<TextTrackInfo> mTextTracksAll;    /**< Alternative variable to store text track information from all period */
	std::vector<TextTrackInfo> mTextTracks;       /**< Available text tracks */
	MediaTrackDiscontinuityState mTrackState;     /**< stores the discontinuity status of tracks*/
	std::string mAudioTrackIndex;                 /**< Current audio track index in track list */
	std::string mTextTrackIndex;                  /**< Current text track index in track list */
	bool mFwdAudioToAux;                          /**< If audio buffers are to be forwarded to auxiliary pipeline, happens if both are playing same language */

	id3_callback_t mID3Handler;				/**< Function to be used to emit the ID3 event */

};

#endif // STREAMABSTRACTIONAAMP_H
