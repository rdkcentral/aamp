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
 *	@file  fragmentcollector_hls.h
 *	@brief This file handles HLS Streaming functionality for AAMP player
 *
 *	@section DESCRIPTION
 *
 *	This file handles HLS Streaming functionality for AAMP player. Class/structures
 *	required for hls fragment collector is defined here.
 *	Major functionalities include
 *	a) Manifest / fragment collector and trick play handling
 *	b) DRM Initialization / Key acquisition
 *	c) Decrypt and inject fragments for playback
 *	d) Synchronize audio/video tracks .
 *
 */
#ifndef FRAGMENTCOLLECTOR_HLS_H
#define FRAGMENTCOLLECTOR_HLS_H

#include "StreamAbstractionAAMP.h"
#include "mediaprocessor.h"
#include <sys/time.h>
#include "HlsDrmBase.h"
#include <atomic>
#include "AampDRMLicPreFetcher.h"
#include "ID3Metadata.hpp"
#include "MetadataProcessor.hpp"
#include "AampTime.h"
#include <memory>
#include <atomic>
#include <deque>
#include <tuple>
#include "lstring.hpp"
#include "DrmInterface.h"
#define FOG_FRAG_BW_IDENTIFIER "bandwidth-"
#define FOG_FRAG_BW_IDENTIFIER_LEN 10
#define FOG_FRAG_BW_DELIMITER "-"
#define BOOLSTR(boolValue) (boolValue?"true":"false")
#define PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS (0.1f)
#define MAX_MANIFEST_DOWNLOAD_RETRY 3
#define DRM_IV_LEN 16

#define MAX_SEQ_NUMBER_LAG_COUNT 50					/*!< Configured sequence number max count to avoid continuous looping for an edge case scenario, which leads crash due to hung */
#define MAX_SEQ_NUMBER_DIFF_FOR_SEQ_NUM_BASED_SYNC 2		/*!< Maximum difference in sequence number to sync tracks using sequence number.*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT 5	/*!< Maximum playlist refresh count for discontinuity check for TSB/cDvr*/
#define MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE 3		/*!< Maximum playlist refresh count for discontinuity check for live without TSB*/
#define MAX_PDT_DISCONTINUITY_DELTA_LIMIT 1.0f			/*!< maximum audio/video track PDT delta to determine discontinuity using PDT*/

/**
 * \struct	HlsStreamInfo
 * \brief	HlsStreamInfo structure for stream related information
 */
typedef struct HlsStreamInfo: public StreamInfo
{ // #EXT-X-STREAM-INFs
	long program_id;	/**< Program Id */
	std::string audio;	/**< Audio */
	std::string uri;	/**< URI Information */

	// rarely present
	long averageBandwidth;			/**< Average Bandwidth */
	std::string closedCaptions;		/**< CC if present */
	std::string subtitles;			/**< Subtitles */
	StreamOutputFormat audioFormat; /**< Audio codec format*/

	HlsStreamInfo():program_id(),audio(),uri(),averageBandwidth(),closedCaptions(),subtitles(),audioFormat(){};

	// Copy constructor
	HlsStreamInfo(const HlsStreamInfo& other)
		:
		StreamInfo(other), // Initialize base class members
		program_id(other.program_id),
		audio(other.audio),
		uri(other.uri),
		averageBandwidth(other.averageBandwidth),
		closedCaptions(other.closedCaptions),
		subtitles(other.subtitles),
		audioFormat(other.audioFormat){};

	HlsStreamInfo& operator=(const HlsStreamInfo& other)=delete;
} HlsStreamInfo;

/**
 * \struct	MediaInfo
 * \brief	MediaInfo structure for Media related information
 */
typedef struct MediaInfo
{ // #EXT-X-MEDIA
	AampMediaType type;			/**< Media Type */
	std::string group_id;		/**< Group ID */
	std::string name;		/**< Name of Media */
	std::string language;		/**< Language */
	bool autoselect;		/**< AutoSelect */
	bool isDefault;			/**< IsDefault */
	std::string uri;		/**< URI Information */
	StreamOutputFormat audioFormat; /**< Audio codec format*/
	// rarely present
	int channels;			/**< Channel */
	std::string instreamID;		/**< StreamID */
	bool forced;			/**< Forced Flag */
	std::string characteristics;	/**< Characteristics */
	bool isCC;			/**< True if the text track is closed-captions */
} MediaInfo;

/**
 *	\struct	IndexNode
 *	\brief	IndexNode structure for Node/DRM Index
 */
struct IndexNode
{
	IndexNode() : completionTimeSecondsFromStart(0), mediaSequenceNumber(-1), pFragmentInfo(), drmMetadataIdx(-1), initFragmentPtr()
	{
	}
	AampTime completionTimeSecondsFromStart;	/**< Time of index from start */
	long long mediaSequenceNumber;		/**< Media sequence number>*/
	lstring pFragmentInfo;		/**< Fragment Information pointer */
	int drmMetadataIdx;			/**< DRM Index for Fragment */
	lstring initFragmentPtr;		/**< Fragmented MP4 specific pointer to associated (preceding) initialization fragment */
};

/**
 *	\struct	KeyTagStruct
 *	\brief	KeyTagStruct structure to store all Keytags with Hash
 */
struct KeyTagStruct
{
	KeyTagStruct() : mShaID(""), mKeyStartDuration(0), mKeyTagStr("")
	{
	}
	std::string mShaID;	   /**< ShaID of Key tag */
	AampTime mKeyStartDuration;  /**< duration in playlist where Keytag starts */
	std::string mKeyTagStr;	   /**< String to store key tag,needed for trickplay */
};

/**
 *	\struct	DiscontinuityIndexNode
 *	\brief	Index Node structure for Discontinuity Index
 */
struct DiscontinuityIndexNode
{
	uint64_t discontinuitySequenceIndex; /**< period index, useful for discontinuity synchronization across tracks */
	int fragmentIdx;                     /**< index of fragment in index table*/
	AampTime position;                   /**< Time of index from start */
	AampTime fragmentDuration;           /**< Fragment duration of current discontinuity index */
	AampTime discontinuityPDT;           /**< Program Date time value */
};

/**
 *	\enum DrmKeyMethod
 *	\brief	Enum for various EXT-X-KEY:METHOD= values
 */
typedef enum
{
	eDRM_KEY_METHOD_NONE,		/**< DRM key is none */
	eDRM_KEY_METHOD_AES_128,	/**< DRM key is AES 128 Method */
	eDRM_KEY_METHOD_SAMPLE_AES,	/**< DRM key is Sample AES method */
	eDRM_KEY_METHOD_SAMPLE_AES_CTR,	/**< DRM key is Sample AES CTR method */
	eDRM_KEY_METHOD_UNKNOWN		/**< DRM key is unknown method */
} DrmKeyMethod;

struct DrmMetadataNode
{
	int deferredInterval;
	long long drmKeyReqTime;
	std::string sha1Hash;
};

/// Function to call to update the local PTS record
using ptsoffset_update_t = std::function<void (double, bool)>;

/**
 * \class TrackState
 * \brief State Machine for each Media Track
 *
 * This class is meant to handle each media track of stream
 */
class TrackState : public MediaTrack
{
	public:

		/***************************************************************************
		 * @fn TrackState
		 *
		 * @param[in] type Type of the track
		 * @param[in] parent StreamAbstractionAAMP_HLS instance
		 * @param[in] aamp PrivateInstanceAAMP pointer
		 * @param[in] name Name of the track
		 * @return void
		 ***************************************************************************/
		TrackState(TrackType type, class StreamAbstractionAAMP_HLS* parent,
			PrivateInstanceAAMP* aamp, const char* name,
			id3_callback_t id3Handler = nullptr,
			ptsoffset_update_t ptsUpdate = nullptr
			);
		/***************************************************************************
		 * @fn ~TrackState
		 * @brief copy constructor function
		 *
		 * @return void
		 ***************************************************************************/
		TrackState(const TrackState&) = delete;
		/***************************************************************************
		 * @fn ~TrackState
		 *
		 * @return void
		 ***************************************************************************/
		~TrackState();
		/*************************************************************************
		 * @brief  Assignment operator Overloading
		 *************************************************************************/
		TrackState& operator=(const TrackState&) = delete;
		/***************************************************************************
		 * @fn Start
		 *
		 * @return void
		 **************************************************************************/
		void Start();
		/***************************************************************************
		 * @fn Stop
		 *
		 * @return void
		 ***************************************************************************/
		void Stop(bool clearDRM = false);
		/***************************************************************************
		 * @fn RunFetchLoop
		 *
		 * @return void
		 ***************************************************************************/
		virtual void RunFetchLoop();

		/***************************************************************************
		 * @fn FragmentCollector
		 * @brief Fragment collector thread function

		 * @return void
		 ***************************************************************************/
		void FragmentCollector(void);

		/***************************************************************************
		 * @fn IndexPlaylist
		 * @brief Function to parse playlist
		 *
		 * @param AampTime total duration from playlist
		 ***************************************************************************/
		void IndexPlaylist(bool IsRefresh, AampTime &culledSec);
		/***************************************************************************
		 * @fn ABRProfileChanged
		 *
		 * @return void
		 ***************************************************************************/
		void ABRProfileChanged(void) override;
		void resetPTSOnAudioSwitch(CachedFragment* cachedFragment) override;
		/***************************************************************************
		 * @fn GetNextFragmentUriFromPlaylist
		 * @param reloadUri reload uri on playlist refreshed scenario
		 * @param ignoreDiscontinuity Ignore discontinuity
		 * @return string fragment URI pointer
		 ***************************************************************************/
		lstring GetNextFragmentUriFromPlaylist(bool &reloadUri, bool ignoreDiscontinuity=false);
		/***************************************************************************
		 * @fn updateSkipPoint
		 * @param position point to which fragment need to be skipped
		 * @param duration fragment duration to be skipped
		 * @return void
		 ***************************************************************************/
		void updateSkipPoint(double position, double duration ) override;
		/***************************************************************************
		 * @fn setDiscontinuityState
		 * @param isDiscontinuity - true if discontinuity false otherwise
		 * @return void
		 ***************************************************************************/
		void setDiscontinuityState(bool isDiscontinuity) override;
		/***************************************************************************
		 * @fn abortWaitForVideoPTS
		 * @return void
		 ***************************************************************************/
		void abortWaitForVideoPTS() override;
		/***************************************************************************
		 * @fn UpdateDrmIV
		 *
		 * @param[in] ptr IV string from DRM attribute
		 * @return void
		 ***************************************************************************/
		void UpdateDrmIV(const std::string &ptr);
		/***************************************************************************
		 * @fn UpdateDrmCMSha1Hash
		 *
		 * @param[in] ptr ShaID string from DRM attribute
		 * @return void
		 ***************************************************************************/
		void UpdateDrmCMSha1Hash( const std::string &newSha1Hash );
		/***************************************************************************
		 * @fn DrmDecrypt
		 *
		 * @param[in] cachedFragment CachedFragment struct pointer
		 * @param[in] bucketTypeFragmentDecrypt ProfilerBucketType enum
		 * @return bool true if successfully decrypted
		 ***************************************************************************/
		DrmReturn DrmDecrypt(CachedFragment* cachedFragment, ProfilerBucketType bucketType);
		/***************************************************************************
		 * @fn CreateInitVectorByMediaSeqNo
		 *
		 * @param[in] ui32Seqno Current fragment's sequence number
		 * @return bool true if successfully created, false otherwise.
		 ***************************************************************************/
		void CreateInitVectorByMediaSeqNo( long long ui32Seqno );
		/***************************************************************************
		 * @fn FetchPlaylist
		 *
		 * @return void
		 ***************************************************************************/
		void FetchPlaylist();
		/****************************************************************************
		 * @fn GetNextFragmentPeriodInfo
		 *
		 * @param[out] periodIdx Index of the period in which next fragment belongs
		 * @param[out] offsetFromPeriodStart Offset from start position of the period
		 * @param[out] fragmentIdx Fragment index
		 ****************************************************************************/
		void GetNextFragmentPeriodInfo(int &periodIdx, AampTime &offsetFromPeriodStart, int &fragmentIdx);

		/***************************************************************************
		 * @fn GetPeriodStartPosition
		 * @param[in] periodIdx Period Index
		 * @return void
		 ***************************************************************************/
		AampTime GetPeriodStartPosition(int periodIdx);

		/***************************************************************************
		 * @fn GetNumberOfPeriods
		 *
		 * @return int number of periods
		 ***************************************************************************/
		int GetNumberOfPeriods();

		/***************************************************************************
		 * @fn HasDiscontinuityAroundPosition
		 * @param[in] position Position to check for discontinuity
		 * @param[in] useStartTime starting time to search discontinuity
		 * @param[out] diffBetweenDiscontinuities discontinuity position minus input position
		 * @param[in] playPosition playback position
		 * @param[in] inputCulledSec culled seconds
		 * @param [in] inputProgramDateTime program date and time in epoc format
		 * @param [out] isDiffChkReq indicates is diffBetweenDiscontinuities check required
		 * @return true if discontinuity present around given position
		 ***************************************************************************/
		bool HasDiscontinuityAroundPosition(AampTime position, bool useStartTime, AampTime &diffBetweenDiscontinuities, AampTime playPosition,AampTime inputCulledSec,AampTime inputProgramDateTime,bool &isDiffChkReq);

		/***************************************************************************
		 * @fn StartInjection
		 *
		 * @return void
		 ***************************************************************************/
		void StartInjection();

		/**
		 * @fn StopInjection
		 * @return void
		 */
		void StopInjection( void );

		/***************************************************************************
		 * @fn StopWaitForPlaylistRefresh
		 *
		 * @return void
		 ***************************************************************************/
		void StopWaitForPlaylistRefresh();

		/***************************************************************************
		 * @fn CancelDrmOperation
		 *
		 * @param[in] clearDRM flag indicating if DRM resources to be freed or not
		 * @return void
		 ***************************************************************************/
		void CancelDrmOperation(bool clearDRM);

		/***************************************************************************
		 * @fn RestoreDrmState
		 *
		 * @return void
		 ***************************************************************************/
		void RestoreDrmState();
		/***************************************************************************
		 * @fn IsLive
		 * @brief Function to check the IsLive status of track. Kept Public as its called from StreamAbstraction
		 *
		 * @return True if both or any track in live mode
		 ***************************************************************************/
		bool IsLive()  { return (ePLAYLISTTYPE_VOD != mPlaylistType);}
		/***************************************************************************
		 * @fn FindTimedMetadata
		 *
		 * @return void
		 ***************************************************************************/
		void FindTimedMetadata(bool reportbulk=false, bool bInitCall = false);
		/***************************************************************************
		 * @fn SetXStartTimeOffset
		 * @brief Function to set XStart Time Offset Value
		 *
		 * @return void
		 ***************************************************************************/
		void SetXStartTimeOffset(AampTime offset) { mXStartTimeOFfset = offset; }
		/***************************************************************************
		 * @fn SetXStartTimeOffset
		 * @brief Function to retune XStart Time Offset
		 *
		 * @return Start time
		 ***************************************************************************/
		AampTime GetXStartTimeOffset() { return mXStartTimeOFfset;}
		/***************************************************************************
		 * @fn GetBufferedDuration
		 *
		 * @return Buffer Duration
		 ***************************************************************************/
		double GetBufferedDuration() override;

		/***************************************************************************
		 * @fn GetPlaylistUrl
		 *
		 * @return string - playlist URL
		 ***************************************************************************/
		std::string& GetPlaylistUrl() override { return mPlaylistUrl; }
		/***************************************************************************
		 * @fn GetEffectivePlaylistUrl
		 *
		 * @return string - original playlist URL(redirected)
		 ***************************************************************************/
		std::string& GetEffectivePlaylistUrl() override { return mEffectiveUrl; }
		/***************************************************************************
		 * @fn SetEffectivePlaylistUrl
		 *
		 * @return none
		 ***************************************************************************/
		void SetEffectivePlaylistUrl(std::string url) override { mEffectiveUrl = url; }
		/***************************************************************************
		 * @fn GetLastPlaylistDownloadTime
		 *
		 * @return lastPlaylistDownloadTime
		 ****************************************************************************/
		long long GetLastPlaylistDownloadTime() override { return lastPlaylistDownloadTimeMS; }
		/****************************************************************************
		 * @fn SetLastPlaylistDownloadTime
		 *
		 * @return void
		 ****************************************************************************/
		void SetLastPlaylistDownloadTime(long long time) override { lastPlaylistDownloadTimeMS = time; }
		/****************************************************************************
		 * @fn GetMinUpdateDuration
		 *
		 * @return minimumUpdateDuration
		 ****************************************************************************/
		long GetMinUpdateDuration() override;
		/****************************************************************************
		 * fn GetDefaultDurationBetweenPlaylistUpdates
		 *
		 * @return maxIntervalBtwPlaylistUpdateMs
		 ****************************************************************************/
		int GetDefaultDurationBetweenPlaylistUpdates() override;

		/****************************************************************************
		 * @fn ProcessPlaylist
		 *
		 * @return none
		 ****************************************************************************/
		void ProcessPlaylist(AampGrowableBuffer& newPlaylist, int http_error) override;

		/**
		 * @brief Get byteRangeLength and byteRangeOffset from fragmentInfo.
		 */
		bool IsExtXByteRange(lstring fragmentInfo, size_t *byteRangeLength, size_t *byteRangeOffset);

		/**
		 * @brief Acquire playlist lock.
		 */
		void AcquirePlaylistLock();

		/**
		 * @brief Release playlist lock.
		 */
		void ReleasePlaylistLock();

	private:
		/***************************************************************************
		 * @fn GetIframeFragmentUriFromIndex
		 *
		 * @return string fragment URI pointer
		 ***************************************************************************/
		lstring GetIframeFragmentUriFromIndex(bool &bSegmentRepeated);
		/***************************************************************************
		 * @fn FlushIndex
		 *
		 * @return void
		 ***************************************************************************/
		void FlushIndex();
		/***************************************************************************
		 * @fn FetchFragment
		 *
		 * @return void
		 ***************************************************************************/
		void FetchFragment();
		/***************************************************************************
		 * @fn FetchFragmentHelper
		 *
		 * @param[out] http_error http error string
		 * @param[out] decryption_error decryption error
		 * @return bool true on success else false
		 ***************************************************************************/
		bool FetchFragmentHelper(int &http_error, bool &decryption_error, bool & bKeyChanged, int * fogError, double &downloadTime);
		/***************************************************************************
		 * @fn RefreshPlaylist
		 *
		 * @return void
		 ***************************************************************************/
		void RefreshPlaylist(void);
		/***************************************************************************
		 * @fn GetContext
		 *
		 * @return StreamAbstractionAAMP instance
		 ***************************************************************************/
		StreamAbstractionAAMP* GetContext() override;
		/***************************************************************************
		 * @fn InjectFragmentInternal
		 *
		 * @param[in] cachedFragment CachedFragment structure
		 * @param[out] fragmentDiscarded bool to indicate fragment successfully injected
		 * @param[in] isDiscontinuity bool to indicate if discontinuity
		 * @return void
		 ***************************************************************************/
		void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity=false)override;
		/***************************************************************************
		 * @fn FindMediaForSequenceNumber
		 * @return string fragment tag line pointer
		 ***************************************************************************/
		lstring FindMediaForSequenceNumber();
		/***************************************************************************
		 * @fn FetchInitFragment
		 *
		 * @return void
		 ***************************************************************************/
		void FetchInitFragment();
		/***************************************************************************
		 * @fn FetchInitFragmentHelper
		 * @return true if success
		 ***************************************************************************/
		bool FetchInitFragmentHelper(int &http_code, bool forcePushEncryptedHeader = false);
		/***************************************************************************
		 * @fn ProcessDrmMetadata
		 ***************************************************************************/
		void ProcessDrmMetadata();
		/***************************************************************************
		 * @fn ComputeDeferredKeyRequestTime
		 ***************************************************************************/
		void ComputeDeferredKeyRequestTime();
		/***************************************************************************
		 * @fn InitiateDRMKeyAcquisition
		 ***************************************************************************/
		void InitiateDRMKeyAcquisition(int indexPosn=-1);
		/***************************************************************************
		 * @fn SetDrmContext
		 * @return None
		 ***************************************************************************/
		void SetDrmContext();
		/***************************************************************************
		 * @fn SwitchSubtitleTrack
		 *
		 * @return void
		 ***************************************************************************/
		void SwitchSubtitleTrack();

		void getNextFetchRequestUri(); //CMCD Get next object request url(nor)
		/***************************************************************************
		 * @fn SwitchAudioTrack
		 *
		 * @return void
		 ***************************************************************************/
		void SwitchAudioTrack();


	public:
		std::string mEffectiveUrl;		 /**< uri associated with downloaded playlist (takes into account 302 redirect) */
		std::string mPlaylistUrl;		 /**< uri associated with downloaded playlist */
		AampGrowableBuffer playlist;		 /**< downloaded playlist contents */

		AampTime mProgramDateTime;
		std::vector<IndexNode> index;
		int currentIdx;				 /**< index for currently-presenting fragment used during FF/REW (-1 if undefined) */
		lstring mFragmentURIFromIndex;		 /**< storage for uri generated by GetIframeFragmentUriFromIndex */
		long long indexFirstMediaSequenceNumber; /**< first media sequence number from indexed manifest */

		lstring fragmentURI;					 /**< offset (into playlist) to URI of current fragment-of-interest */
		long long lastPlaylistDownloadTimeMS;	 /**< UTC time at which playlist was downloaded */
		size_t byteRangeLength;					 /**< state for \#EXT-X-BYTERANGE fragments */
		size_t byteRangeOffset;					 /**< state for \#EXT-X-BYTERANGE fragments */
		long long lastPlaylistIndexedTimeMS;	 /**< UTC time at which last playlist indexed */

		long long nextMediaSequenceNumber;		 /**< media sequence number following current fragment-of-interest */
		AampTime playlistPosition;				 /**< playlist-relative time of most recent fragment-of-interest; -1 if undefined */
		AampTime playTarget;					 /**< initially relative seek time (seconds) based on playlist window, but updated as a play_target */
		AampTime playTargetBufferCalc;
		AampTime playlistCulledOffset;			 /**< When seeking, the position takes into account the culled seconds. This needs applying subsequently when adjusting playTargetBufferCalc */
		AampTime lastDownloadedIFrameTarget;	 /**< stores last downloaded iframe segment target value for comparison */
		AampTime targetDurationSeconds;			 /**< copy of \#EXT-X-TARGETDURATION to manage playlist refresh frequency */
		int mDeferredDrmKeyMaxTime;				 /**< copy of \#EXT-X-X1-LIN DRM refresh randomization Max time interval */
		StreamOutputFormat streamOutputFormat;	 /**< type of data encoded in each fragment */
		AampTime startTimeForPlaylistSync;		 /**< used for time-based track synchronization when switching between playlists */
		AampTime playTargetOffset;				 /**< For correcting timestamps of streams with audio and video tracks */
		bool discontinuity;						 /**< Set when discontinuity is found in track*/
		StreamAbstractionAAMP_HLS* context;		 /**< To get  settings common across tracks*/
		bool fragmentEncrypted;					 /**< In DAI, ad fragments can be clear. Set if current fragment is encrypted*/
		bool mKeyTagChanged;					 /**< Flag to indicate Key tag got changed for decryption context setting */
		bool mIVKeyChanged;                      /**< Flag to indicate Key info got changed (may be able to use existing flag with some restructuring) */
		int mLastKeyTagIdx ;					 /**< Variable to hold the last keyTag index,to check if key tag changed */
		struct DrmInfo mDrmInfo;			 /**< Structure variable to hold Drm Information */
		std::string mCMSha1Hash;					 /**< variable to store ShaID*/
		long long mDrmTimeStamp;			 /**< variable to store Drm Time Stamp */
		int mDrmMetaDataIndexPosition;			 /**< Variable to store Drm Meta data Index position*/
		std::vector<DrmMetadataNode> mDrmMetaDataIndex;		 /**< DrmMetadata records for associated playlist */
		int mDrmKeyTagCount;					 /**< number of EXT-X-KEY tags present in playlist */
		bool mIndexingInProgress;				 /**< indicates if indexing is in progress*/
		std::vector<DiscontinuityIndexNode> mDiscontinuityIndex;
		AampTime mDuration;						 /** Duration of the track*/
		typedef std::vector<KeyTagStruct> KeyHashTable;
		typedef std::vector<KeyTagStruct>::iterator KeyHashTableIter;
		KeyHashTable mKeyHashTable;
		bool mCheckForInitialFragEnc;			/**< Flag that denotes if we should check for encrypted init header and push it to GStreamer*/
		DrmKeyMethod mDrmMethod;				/**< denotes the X-KEY method for the fragment of interest */
		bool fragmentEncChange;				/**< Flag to denote there is change in encrypted<->clear, used to reconfigure audio pipeline */

	private:
		bool refreshPlaylist;					/**< bool flag to indicate if playlist refresh required or not */
		bool isFirstFragmentAfterABR;			/**< bool flag to indicate whether the fragment is first fragment after ABR */
		std::thread fragmentCollectorThreadID;	/**< Thread Id for Fragment  collector Thread */
		int manifestDLFailCount;		/**< Manifest Download fail count for retry*/
		bool firstIndexDone;					/**< Indicates if first indexing is done*/
		std::shared_ptr<HlsDrmBase> mDrm;		/**< DRM decrypt context*/
		std::shared_ptr<DrmInterface> mDrmInterface;		/**< Interface bw drm and application */
		bool mDrmLicenseRequestPending;			/**< Indicates if DRM License Request is Pending*/
		bool mInjectInitFragment;				/**< Indicates if init fragment injection is required*/
		lstring mInitFragmentInfo;			/**< Holds init fragment Information index*/
		bool mForceProcessDrmMetadata;			/**< Indicates if processing drm metadata to be forced on indexing*/
		std::mutex mPlaylistMutex;			/**< protect playlist update */
		std::condition_variable mPlaylistIndexed;		/**< Notifies after a playlist indexing operation */
		std::mutex mTrackDrmMutex;			/**< protect DRM Interactions for the track */
		AampTime mLastMatchedDiscontPosition;		/**< Holds discontinuity position last matched	by other track */
		AampTime mCulledSeconds;					/**< Total culled duration in this streamer instance*/
		AampTime mCulledSecondsOld;				/**< Total culled duration in this streamer instance*/
		bool mSyncAfterDiscontinuityInProgress; /**< Indicates if a synchronization after discontinuity tag is in progress*/
		PlaylistType mPlaylistType;		/**< Playlist Type */
		bool mReachedEndListTag;		/**< Flag indicating if End list tag reached in parser */
		bool mByteOffsetCalculation;			/**< Flag used to calculate byte offset from byte length for fragmented streams */
		bool mSkipAbr;							/**< Flag that denotes if previous cached fragment is init fragment or not */
		const char* mFirstEncInitFragmentInfo;	/**< Holds first encrypted init fragment Information index*/
		AampTime mXStartTimeOFfset;		/**< Holds value of time offset from X-Start tag */
		AampTime mCulledSecondsAtStart;		/**< Total culled duration with this asset prior to streamer instantiation*/
		bool mSkipSegmentOnError;		/**< Flag used to enable segment skip on fetch error */
		AampMediaType playlistMediaType;		/**< Media type of playlist of this track */
public:
		StreamOperation demuxOp; /** denotes whether a given (hls/ts) track is muxed */
};

class PrivateInstanceAAMP;
/**
 * \class StreamAbstractionAAMP_HLS
 *
 * \brief HLS Stream handler class
 *
 * This class is meant to handle download of HLS manifest and interface play controls
 */
class StreamAbstractionAAMP_HLS : public StreamAbstractionAAMP
{
	public:
		/**************************************************************************
		 * @fn IndexPlaylist
		 * @return void
		 *************************************************************************/
		void IndexPlaylist(TrackState *trackState);
		/***************************************************************************
		 * @fn StreamAbstractionAAMP_HLS
		 *
		 * @param[in] aamp PrivateInstanceAAMP pointer
		 * @param[in] seekpos Seek position
		 * @param[in] rate Rate of playback
		 * @return void
		 ***************************************************************************/
		StreamAbstractionAAMP_HLS(class PrivateInstanceAAMP *aamp,
			double seekpos, float rate,
			id3_callback_t id3Handler = nullptr,
			ptsoffset_update_t ptsOffsetUpdate = nullptr
		);
		/*************************************************************************
		 * @brief Copy constructor disabled
		 *
		 *************************************************************************/
		StreamAbstractionAAMP_HLS(const StreamAbstractionAAMP_HLS&) = delete;
		/***************************************************************************
		 * @fn ~StreamAbstractionAAMP_HLS
		 *
		 * @return void
		 ***************************************************************************/
		~StreamAbstractionAAMP_HLS();
		/*****************************************************************************
		 * @brief assignment operator disabled
		 *
		 ****************************************************************************/
		StreamAbstractionAAMP_HLS& operator=(const StreamAbstractionAAMP_HLS&) = delete;
		/***************************************************************************
		 * @fn Start
		 *
		 * @return void
		 ***************************************************************************/
		void Start() override;
		/***************************************************************************
		 * @fn Stop
		 * @param[in] clearChannelData flag indicating to full stop or temporary stop
		 * @return void
		 ***************************************************************************/
		void Stop(bool clearChannelData) override;
		/***************************************************************************
		 * @fn Init
		 *
		 * @param[in] tuneType Tune type
		 * @return bool true on success
		 ***************************************************************************/
		AAMPStatusType Init(TuneType tuneType) override;
		/***************************************************************************
		 * @fn GetStreamFormat
		 *
		 * @param[out] primaryOutputFormat video format
		 * @param[out] audioOutputFormat audio format
		 * @param[out] auxOutputFormat auxiliary audio format
		 * @param[out] subFormat subtitle format
		 * @return void
		 ***************************************************************************/
		void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subOutputFormat) override;
		/***************************************************************************
		 * @fn GetStreamPosition
		 * @brief Function to return current playing position of stream
		 *
		 * @return seek position
		 ***************************************************************************/
		double GetStreamPosition() override { return seekPosition.inSeconds(); }
		/***************************************************************************
		 * @fn GetFirstPTS
		 *
		 * @return double PTS value
		 ***************************************************************************/
		double GetFirstPTS() override;
		/***************************************************************************
		 * @fn GetMediaTrack
		 *
		 * @param[in] type TrackType input
		 * @return MediaTrack structure pointer
		 ***************************************************************************/
		MediaTrack* GetMediaTrack(TrackType type) override;
		/***************************************************************************
		 * @fn GetBWIndex
		 *
		 * @param bitrate Bitrate in bits per second
		 * @return bandwidth index
		 ***************************************************************************/
		int GetBWIndex(BitsPerSecond bitrate) override;
		/***************************************************************************
		 * @fn GetVideoBitrates
		 *
		 * @return available video bitrates
		 ***************************************************************************/
		std::vector<BitsPerSecond> GetVideoBitrates(void) override;
		/***************************************************************************
		 * @fn GetMediaCount
		 * @brief Function to get the Media count
		 *
		 * @return Number of media count
		 ***************************************************************************/
		int GetMediaCount(void) { return mMediaCount;}
		/***************************************************************************
		 * @fn FilterAudioCodecBasedOnConfig
		 *
		 * @param[in] audioFormat Audio codec type
		 * @return bool false if the audio codec type is allowed to process
		 ***************************************************************************/
		bool FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat);
		/***************************************************************************
		 * @fn SeekPosUpdate
		 *
		 * @param[in] secondsRelativeToTuneTime seek position time
		 ***************************************************************************/
		void SeekPosUpdate(double secondsRelativeToTuneTime) override;
		/***************************************************************************
		 * @fn PreCachePlaylist
		 *
		 * @return none
		 ***************************************************************************/
		void PreCachePlaylist();
		/***************************************************************************
		 *	 @fn GetBufferedDuration
		 *
		 *	 @return buffer value
		 **************************************************************************/
		double GetBufferedDuration() override;
		/***************************************************************************
		 * @fn GetLanguageCode
		 *
		 * @return Language code in string format
		 ***************************************************************************/
		std::string GetLanguageCode( int iMedia );
		/***************************************************************************
		 * @fn GetBestAudioTrackByLanguage
		 *
		 * @return int index of the audio track selected
		 ***************************************************************************/
		int GetBestAudioTrackByLanguage( void );
		/***************************************************************************
		 * @fn GetAvailableThumbnailTracks
		 *
		 * @return vector of available thumbnail tracks.
		 ***************************************************************************/
		std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
		/***************************************************************************
		 * @fn SetThumbnailTrack
		 *
		 * @param thumbIndex thumbnail index value indicating the track to select
		 * @return bool true on success.
		 ***************************************************************************/
		bool SetThumbnailTrack(int) override;
		/***************************************************************************
		 * @fn GetThumbnailRangeData
		 *
		 * @param tStart start duration of thumbnail data.
		 * @param tEnd end duration of thumbnail data.
		 * @param baseurl base url of thumbnail images.
		 * @param raw_w absolute width of the thumbnail spritesheet.
		 * @param raw_h absolute height of the thumbnail spritesheet.
		 * @param width width of each thumbnail tile.
		 * @param height height of each thumbnail tile.
		 * @return Updated vector of available thumbnail data.
		 ***************************************************************************/
		std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
		/***************************************************************************
		 * @brief Function to parse the Thumbnail Manifest and extract Tile information
		 *
		 *************************************************************************/
		std::map<std::string,double> GetImageRangeString(double*, std::string, TileInfo*, double);
		AampGrowableBuffer thumbnailManifest;	/**< Thumbnail manifest buffer holder */
		std::vector<TileInfo> indexedTileInfo;	/**< Indexed Thumbnail information */
		/***************************************************************************
		 * @brief Function to get the total number of profiles
		 *
		 ***************************************************************************/
		int GetTotalProfileCount() { return mProfileCount;}
		/***************************************************************************
		 * @fn GetAvailableVideoTracks
		 * @return list of available video tracks
		 *
		 **************************************************************************/
		std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;

		/**
		 * @fn Is4KStream
		 * @brief check if current stream have 4K content
		 * @param height - resolution of 4K stream if found
		 * @param bandwidth - bandwidth of 4K stream if found
		 * @return true on success
		 */
		virtual bool Is4KStream(int &height, BitsPerSecond &bandwidth) override;

		/**
		 * @fn UpdateFailedDRMStatus
		 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
		 * @param[in] object  - Prefetch object instance which failed
		 */
		void UpdateFailedDRMStatus(LicensePreFetchObject *object) override { }

		/**
		 * @brief Get the ABR mode.
		 *
		 * @return the ABR mode.
		 */
		ABRMode GetABRMode() override;
		//private:
		// TODO: following really should be private, but need to be accessible from callbacks

		TrackState* trackState[AAMP_TRACK_COUNT]{};	/**< array to store all tracks of a stream */
		float rate;					/**< Rate of playback  */
		int maxIntervalBtwPlaylistUpdateMs;		/**< Interval between playlist update */
		AampGrowableBuffer mainManifest;			/**< Main manifest buffer holder */
		bool allowsCache;				/**< Flag indicating if playlist needs to be cached or not */
		std::vector<HlsStreamInfo> streamInfoStore{};	/**< Store of multiple stream information */
		std::vector<MediaInfo> mediaInfoStore{};		/**< Store of multiple media within stream */

		AampTime seekPosition;				/**< Seek position for playback */
		AampTime midSeekPtsOffset;			/**< PTS offset for Mid Fragment seek  */
		int mTrickPlayFPS;				/**< Trick play frames per stream */
		bool enableThrottle;				/**< Flag indicating throttle enable/disable */
		bool firstFragmentDecrypted;			/**< Flag indicating if first fragment is decrypted for stream */
		bool mStartTimestampZero;			/**< Flag indicating if timestamp to start is zero or not (No audio stream) */
		int mNumberOfTracks;				/**< Number of media tracks.*/
		std::mutex mDiscoCheckMutex;               	/**< protect playlist discontinuity check */
		DrmInterface mDrmInterface;

		/***************************************************************************
		 * @fn ParseMainManifest
		 *
		 * @return AAMPStatusType
		 ***************************************************************************/
		AAMPStatusType ParseMainManifest();
		/***************************************************************************
		 * @fn GetPlaylistURI
		 *
		 * @param[in] trackType Track type
		 * @param[in] format stream output type
		 * @return string playlist URI
		 ***************************************************************************/
		std::string GetPlaylistURI(TrackType trackType, StreamOutputFormat* format = NULL);
		/***************************************************************************
		 * @fn StopInjection
		 *
		 * @return void
		 ***************************************************************************/
		void StopInjection(void) override;
		/***************************************************************************
		 * @fn StartInjection
		 *
		 * @return void
		 ***************************************************************************/
		void StartInjection(void) override;
		/***************************************************************************
		 * @fn IsLive
		 * @return True if both or any track in live mode
		 ***************************************************************************/
		bool IsLive();
		/***************************************************************************
		 * @fn NotifyFirstVideoPTS
		 * @param[in] pts base pts
		 * @param[in] timeScale time scale
		 * @return void
		 ***************************************************************************/
		void NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale) override;
		/**
		 * @fn StartSubtitleParser
		 * @return void
		 */
		void StartSubtitleParser() override;
		/**
		 * @fn PauseSubtitleParser
		 * @return void
		 */
		void PauseSubtitleParser(bool pause) override;
		/***************************************************************************
		 * @fn GetMediaIndexForLanguage
		 *
		 * @param[in] lang language
		 * @param[in] type track type
		 * @return int mediaInfo index of track with matching language
		 ***************************************************************************/
		int GetMediaIndexForLanguage(std::string lang, TrackType type);
		/***************************************************************************
		 * @fn GetStreamOutputFormatForTrack
		 *
		 * @param[in] type track type
		 * @return StreamOutputFormat for the audio codec selected
		 ***************************************************************************/
		StreamOutputFormat GetStreamOutputFormatForTrack(TrackType type);
		/***************************************************************************
		 * @brief  Function to get output format for audio/aux track
		 *
		 *************************************************************************/
		StreamOutputFormat GetStreamOutputFormatForAudio(void);
		/****************************************************************************
		 *	 @brief Change muxed audio track index
		 *
		 *	 @param[in] string index
		 *	 @return void
		 ****************************************************************************/
		void ChangeMuxedAudioTrackIndex(std::string& index) override;


		/***************************************************************************
		 * @brief  Function to get output format for audio/aux track
		 *
		 *************************************************************************/
		void InitiateDrmProcess();

		/***************************************************************************
		 * @brief  Function to initiate tracks
		 *
		 *************************************************************************/
		void InitTracks();


		const std::unique_ptr<aamp::MetadataProcessorIntf> & GetMetadataProcessor(StreamOutputFormat fmt);

		/***************************************************************************
		 * @fn RefreshTrack
		 *
		 * @return void
		 ***************************************************************************/
		void RefreshTrack(AampMediaType type) override;

		/***************************************************************************
		 * @fn PopulateAudioAndTextTracks
		 *
		 * @return void
		 ***************************************************************************/
		void PopulateAudioAndTextTracks();
		/***************************************************************************
		 * @fn ConfigureAudioTrack
		 *
		 * @return void
		 ***************************************************************************/
		void ConfigureAudioTrack();
		/***************************************************************************
		 * @fn SelectPreferredTextTrack
		 * @param selectedTextTrack Current PreferredTextTrack Info
		 * @return bool
		 ***************************************************************************/
		bool SelectPreferredTextTrack(TextTrackInfo& selectedTextTrack) override;

		/***************************************************************************
		 * @fn DoEarlyStreamSinkFlush
		 *
		 * @param[in] newTune true if new tune
		 * @param[in] rate playback rate
		 * @return bool true if stream should be flushed
		 ***************************************************************************/
		virtual bool DoEarlyStreamSinkFlush(bool newTune, float rate) override;

		/***************************************************************************
		 * @brief Should flush the stream sink on discontinuity or not.
		 *
		 * @return true if stream should be flushed, false otherwise
		 ***************************************************************************/
		virtual bool DoStreamSinkFlushOnDiscontinuity() override;
	protected:
		/***************************************************************************
		 * @fn GetStreamInfo
		 *
		 * @param[in] idx profileIndex
		 * @return StreamInfo for the index
		 ***************************************************************************/
		StreamInfo* GetStreamInfo(int idx) override;
	//private:
	protected:
		/***************************************************************************
		 * @fn SyncTracks
		 * @param useProgramDateTimeIfAvailable use program date time tag to sync if available
		 * @return eAAMPSTATUS_OK on success
		 ***************************************************************************/
		AAMPStatusType SyncTracks(void);
		/***************************************************************************
		 * @fn CheckDiscontinuityAroundPlaytarget
		 *
		 * @return void
		 ***************************************************************************/
		void CheckDiscontinuityAroundPlaytarget(void);
		/***************************************************************************
		 * @fn SyncTracksForDiscontinuity
		 *
		 * @return eAAMPSTATUS_OK on success
		 ***************************************************************************/
		AAMPStatusType SyncTracksForDiscontinuity();
		/***************************************************************************
		 * @fn ConfigureVideoProfiles
		 *
		 * @return void
		 ***************************************************************************/
		void ConfigureVideoProfiles();
		/***************************************************************************
		 * @fn ConfigureTextTrack
		 *
		 * @return void
		 ***************************************************************************/
		void ConfigureTextTrack();
		void SelectSubtitleTrack();

		/***************************************************************************
		 * @fn CachePlaylistThreadFunction
		 * @brief Thread function created for PreCaching playlist
		 * @return void
		 ***************************************************************************/
		void CachePlaylistThreadFunction(void);

		/* Initial capacity to reserve in the vectors to minimise reallocs */
		static const int initial_stream_info_store_capacity = 20;
		static const int initial_media_info_store_capacity	= 10;

		int segDLFailCount;		/**< Segment Download fail count */
		int segDrmDecryptFailCount;	/**< Segment Decrypt fail count */
		int mMediaCount;		/**< Number of media in the stream */
		int mProfileCount;		/**< Number of Video/Iframe in the stream */
		bool mIframeAvailable;		/**< True if iframe available in the stream */
		std::set<std::string> mLangList;/**< Available language list */
		AampTime mFirstPTS;		/**< First video PTS */

		ptsoffset_update_t mPtsOffsetUpdate;	/**< Function to use to update the PTS offset */

		std::mutex mMP_mutex;  // protects mMetadataProcessor
		 std::unique_ptr<aamp::MetadataProcessorIntf> mMetadataProcessor;
			 
};

StreamOutputFormat GetFormatFromFragmentExtension( const AampGrowableBuffer &playlist );

#endif // FRAGMENTCOLLECTOR_HLS_H
