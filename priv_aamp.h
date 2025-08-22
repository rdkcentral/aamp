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
 * @file priv_aamp.h
 * @brief Private functions and types used internally by AAMP
 */

#ifndef PRIVAAMP_H
#define PRIVAAMP_H

#include "AampProfiler.h"
#include "DrmHelper.h"
#include "DrmMediaFormat.h"
#include "DrmCallbacks.h"
#include "main_aamp.h"
#include <IPVideoStat.h>
#include "AampGrowableBuffer.h"
#include "CCTrackInfo.h"
#include <signal.h>
#include <semaphore.h>
#include <curl/curl.h>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <list>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <glib.h>
#include <cjson/cJSON.h>
#include "AampConfig.h"
#include <atomic>
#include <memory>
#include <inttypes.h>
#include <type_traits>
#include <chrono>
#include "AampEventManager.h"
#include <HybridABRManager.h>
#include "AampCMCDCollector.h"
#include "AampDefine.h"
#include "AampCurlDefine.h"
#include "AampLLDASHData.h"
#include "AampMPDPeriodInfo.h"
#include "TsbApi.h"

class AampMPDDownloader;
typedef struct _manifestDownloadConfig ManifestDownloadConfig;

class AampTSBSessionManager;
#include "ID3Metadata.hpp"
#define AAMP_SEEK_TO_LIVE_POSITION (-1)

#define MANIFEST_TEMP_DATA_LENGTH 100			/**< Manifest temp data length */
#define  AAMP_LOW_BUFFER_BEFORE_RAMPDOWN_FOR_LLD 3	/**< 3sec buffer before rampdown for lld */
#define AAMP_HIGH_BUFFER_BEFORE_RAMPUP_FOR_LLD	 4	/**< 4sec buffer before rampup for lld */
#define TIMEOUT_FOR_LLD	3				/**< 3sec network timeout for lld */
#define MANIFEST_TIMEOUT_FOR_LLD 3      /**< 3 sec timeout for manifest refresh in case of LLD*/
#define ABR_BUFFER_COUNTER_FOR_LLD 3		/** Counter for steady state rampup/rampdown for lld */

#define AAMP_USER_AGENT_MAX_CONFIG_LEN  512    /**< Max Chars allowed in aamp.cfg for user-agent */
#define SERVER_UTCTIME_DIRECT "urn:mpeg:dash:utc:direct:2014"
#define SERVER_UTCTIME_HTTP "urn:mpeg:dash:utc:http-xsdate:2014"
// MSO-specific VSS Service Zone identifier in URL
#define VSS_MARKER			"?sz="
#define VSS_MARKER_LEN			4
#define VSS_MARKER_FOG			"%3Fsz%3D"	/**<URI-encoded ?sz= */
#define VSS_VIRTUAL_STREAM_ID_KEY_STR "content:xcal:virtualStreamId"
#define VSS_VIRTUAL_STREAM_ID_PREFIX "urn:merlin:linear:stream:"
#define VSS_SERVICE_ZONE_KEY_STR "device:xcal:serviceZone"

//Low Latency DASH SERVICE PROFILE URL
#define LL_DASH_SERVICE_PROFILE "http://www.dashif.org/guidelines/low-latency-live-v5"
#define URN_UTC_HTTP_XSDATE "urn:mpeg:dash:utc:http-xsdate:2014"
#define URN_UTC_HTTP_ISO "urn:mpeg:dash:utc:http-iso:2014"
#define URN_UTC_HTTP_NTP "urn:mpeg:dash:utc:http-ntp:2014"
#define URN_UTC_HTTP_HEAD "urn:mpeg:dash:utc:http-head:2014"
#define MAX_LOW_LATENCY_DASH_CORRECTION_ALLOWED 100
#define MAX_LOW_LATENCY_DASH_RETUNE_ALLOWED 2

#define MAX_LOW_LATENCY_DASH_ABR_SPEEDSTORE_SIZE 10

/**
 * @brief Max URL log size
 */
#define MAX_URL_LOG_SIZE 960				/**< Considering "aamp_tune" and [AAMP-PLAYER] pretext */

#define CONVERT_SEC_TO_MS(_x_) (_x_ * 1000) 		/**< Convert value to sec to ms */
#define CONVERT_MS_TO_SEC(_x_) (_x_ / 1000)
#define DEFAULT_PRECACHE_WINDOW (10) 			/**< 10 mins for full precaching */
// These error codes are used internally to identify the cause of error from GetFile
#define PARTIAL_FILE_CONNECTIVITY_AAMP (130)
#define PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP (131)
#define OPERATION_TIMEOUT_CONNECTIVITY_AAMP (132)
#define PARTIAL_FILE_START_STALL_TIMEOUT_AAMP (133)
#define AAMP_MINIMUM_AUDIO_LEVEL (0) 			/**< minimum value for audio level supported */
#define AAMP_MAXIMUM_AUDIO_LEVEL (100) 			/**< maximum value for audio level supported */

#define STRBGPLAYER "BACKGROUND"
#define STRFGPLAYER "FOREGROUND"

#define MUTE_SUBTITLES_TRACKID (-1)
#define LOW_BW_TIMEOUT_FACTOR 0.4
#define DEFAULT_LOW_BW_TIMEOUT 4

/**
 * @brief Structure of X-Start HLS Tag
 */
struct HLSXStart
{
	double offset;      /**< Time offset from XStart */
	bool precise;       /**< Precise input */
};


/*
 * @brief Playback Error Type
 */
enum PlaybackErrorType
{
	eGST_ERROR_PTS,                           /**< PTS error from gstreamer */
	eGST_ERROR_UNDERFLOW,                     /**< Underflow error from gstreamer */
	eGST_ERROR_VIDEO_BUFFERING,     	  /**< Video buffering error */
	eGST_ERROR_OUTPUT_PROTECTION_ERROR,       /**< Output Protection error */
	eDASH_ERROR_STARTTIME_RESET,    	  /**< Start time reset of DASH */
	eSTALL_AFTER_DISCONTINUITY,		  /**< Playback stall after notifying discontinuity */
	eGST_ERROR_GST_PIPELINE_INTERNAL,	  /**< GstPipeline Internal Error */
	eDASH_LOW_LATENCY_MAX_CORRECTION_REACHED, /**< Low Latency Dash Max Correction Reached**/
	eDASH_LOW_LATENCY_INPUT_PROTECTION_ERROR,  /**< Low Latency Dash Input Protection error **/
	eDASH_RECONFIGURE_FOR_ENC_PERIOD /**< Retune to reconfigure pipeline for encrypted period **/
};

/**
 * @brief Tune Type
 */
enum TuneType
{
	eTUNETYPE_NEW_NORMAL,   /**< Play from live point for live streams, from start for VOD*/
	eTUNETYPE_NEW_SEEK,     /**< A new tune with valid seek position*/
	eTUNETYPE_SEEK,         /**< Seek to a position. Not a new channel, so resources can be reused*/
	eTUNETYPE_SEEKTOLIVE,   /**< Seek to live point. Not a new channel, so resources can be reused*/
	eTUNETYPE_RETUNE,       /**< Internal retune for error handling.*/
	eTUNETYPE_LAST,         /**< Use the tune mode used in last tune*/
	eTUNETYPE_NEW_END,      /**< Start playback from the end of the asset*/
	eTUNETYPE_SEEKTOEND     /**< Seek to live point. Not a new channel, so resources can be reused*/
};


/**
 * @brief Http Header Type
 */
/**
 * @brief Different reasons for bitrate change
 */
typedef enum
{
	eAAMP_BITRATE_CHANGE_BY_ABR = 0,
	eAAMP_BITRATE_CHANGE_BY_RAMPDOWN = 1,
	eAAMP_BITRATE_CHANGE_BY_TUNE = 2,
	eAAMP_BITRATE_CHANGE_BY_SEEK = 3,
	eAAMP_BITRATE_CHANGE_BY_TRICKPLAY = 4,
	eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL = 5,
	eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY = 6,
	eAAMP_BITRATE_CHANGE_BY_FOG_ABR = 7,
	eAAMP_BITRATE_CHANGE_BY_OTA = 8,
	eAAMP_BITRATE_CHANGE_BY_HDMIIN = 9,
	eAAMP_BITRATE_CHANGE_MAX = 10
} BitrateChangeReason;

/**
 * @enum AudioType
 *
 * @brief Type of audio ES for MPD
 */
enum AudioType
{
	eAUDIO_UNSUPPORTED,
	eAUDIO_UNKNOWN,
	eAUDIO_AAC,
	eAUDIO_DDPLUS,
	eAUDIO_ATMOS,
	eAUDIO_DOLBYAC3,
	eAUDIO_DOLBYAC4,
	eAUDIO_VORBIS,
	eAUDIO_OPUS
};


/**
 * @struct AsyncEventDescriptor
 * @brief Used in asynchronous event notification logic
 */
struct AsyncEventDescriptor
{
	/**
	 * @brief AsyncEventDescriptor constructor
	 */
	AsyncEventDescriptor() : event(nullptr), aamp(nullptr)
	{
	}

	/**
	 * @brief AsyncEventDescriptor destructor
	 */
	virtual ~AsyncEventDescriptor()
	{
	}

	AsyncEventDescriptor(const AsyncEventDescriptor &other) = delete;
	AsyncEventDescriptor& operator=(const AsyncEventDescriptor& other) = delete;

	AAMPEventPtr event;
	std::shared_ptr<PrivateInstanceAAMP> aamp;
};

/**
 *  @struct EventBreakInfo
 *  @brief Stores the detail about the Event break info
 */
struct EventBreakInfo
{
	std::string payload;
	std::string name;
	uint32_t duration;
	uint64_t presentationTime;
	bool isDAIEvent;     // true if the SCTE35 event is PA START or PPO START
	EventBreakInfo() : payload(), name(), duration(0), presentationTime(0), isDAIEvent(false)
	{}
	EventBreakInfo(std::string _data, std::string _name, uint64_t _presentationTime, uint32_t _dur, bool _isDAIEvent) : payload(_data), name(_name), presentationTime(_presentationTime), duration(_dur), isDAIEvent(_isDAIEvent)
	{}
};

struct DynamicDrmInfo {
	std::vector<uint8_t> keyID;
	std::map<std::string, std::string> licenseEndPoint;
	std::string customData;
	std::string authToken;
	std::string licenseResponse;
	DynamicDrmInfo() : keyID(), licenseEndPoint{}, customData(""),     authToken(""), licenseResponse()
	{
	}
};

class Id3CallbackData;

/**
 * @brief Class for Timed Metadata
 */
class TimedMetadata
{
public:

	/**
	 * @brief TimedMetadata Constructor
	 */
	TimedMetadata() : _timeMS(0), _name(""), _content(""), _id(""), _durationMS(0) {}

	/**
	 * @brief TimedMetadata Constructor
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] name - Metadata name
	 * @param[in] content - Metadata content
	 */
	TimedMetadata(long long timeMS, std::string name, std::string content, std::string id, double durMS) : _timeMS(timeMS), _name(name), _content(content), _id(id), _durationMS(durMS) {}

public:
	long long _timeMS;       /**< Time in milliseconds */
	std::string _name;       /**< Metadata name */
	std::string _content;    /**< Metadata content */
	std::string _id;         /**< Id of the timedMetadata. If not available an Id will bre created */
	double      _durationMS; /**< Duration in milliseconds */
};


/**
 * @brief Class for Content gap information
 */
class ContentGapInfo
{
public:

	/**
	 * @brief ContentGapInfo Constructor
	 */
	ContentGapInfo() : _timeMS(0), _id(""), _durationMS(-1), _complete(false) {}

	/**
	 * @brief ContentGapInfo Constructor
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] id - Content gap ID
	 * @param[in] durMS - Total duration of gap identified
	 */
	ContentGapInfo(long long timeMS, std::string id, double durMS) : _timeMS(timeMS), _id(id), _complete(false), _durationMS(durMS)
	{
		if(durMS > 0)
		{
			_complete = true;
		}
	}

public:
	long long _timeMS;       /**< Time in milliseconds */
	std::string _id;         /**< Id of the content gap information. (period ID of new dash period after gap) */
	double      _durationMS; /**< Duration in milliseconds */
	bool _complete;	         /**< Flag to indicate whether gap info is complete or not */
};


/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int(*IdleTask)(void* arg);

/**
 * @brief Function pointer for the destroy task
 *
 * @param[in] arg - Arguments
 *
 */
typedef void(*DestroyTask)(void * arg);

/**
 * @brief To store Set Cookie: headers and X-Reason headers in HTTP Response
 */
struct httpRespHeaderData {
	httpRespHeaderData() : type(0), data("")
	{
	}
	int type;             /**< Header type */
	std::string data;     /**< Header value */
};

/**
 * @struct ThumbnailData
 * @brief Holds the Thumbnail information
 */
struct ThumbnailData {
	ThumbnailData() : url(""), x(0), y(0), t(0.0), d(0.0)
	{
	}
	std::string url; /**<  url of tile image (may be relative or absolute path) */
	double t; /**<  presentation time for this thumbnail */
	double d; /**< time duration of this tile */
	int x;    /**< x coordinate of thumbnail within tile */
	int y;    /**< y coordinate of Thumbnail within tile */
};

/**
 * @struct SpeedCache
 * @brief Stores the information for cache speed
 */
struct SpeedCache
{
    long last_sample_time_val;
    long prev_dlnow;
    long prevSampleTotalDownloaded;
    long totalDownloaded;
    long speed_now;
    long start_val;
    bool bStart;

    double totalWeight;
    double weightedBitsPerSecond;
    std::vector< std::pair<double,long> > mChunkSpeedData;

    SpeedCache() : last_sample_time_val(0), prev_dlnow(0), prevSampleTotalDownloaded(0), totalDownloaded(0), speed_now(0), start_val(0), bStart(false) , totalWeight(0), weightedBitsPerSecond(0), mChunkSpeedData()
    {
    }
};


/**
 * @brief To store video rectangle properties
 */
struct videoRect {
   int horizontalPos;
   int verticalPos;
   int width;
   int height;
};

/**
 * @class AudioTrackTuple
 * @brief Class to hold audio information like lang, codec, bitrate,etc
 */
class AudioTrackTuple
{
	public:
		std::string language;
		std::string rendition;
		std::string codec;
		BitsPerSecond bitrate;
		unsigned int channel;

	public:
		AudioTrackTuple(): language(""),rendition(""),codec(""),bitrate(0), channel(0){}

		void setAudioTrackTuple(std::string language="",  std::string rendition="", std::string codec="", unsigned int channel=0)
		{
			this->language = language;
			this->rendition = rendition;
			this->codec = codec;
			this->channel = channel;
			this->bitrate = 0;
		}

		void clear(void)
		{
			this->language = "";
			this->rendition = "";
			this->codec = "";
			this->bitrate = 0;
			this->channel = 0;
		}
};

/**
 *	\Class attrNameData
 * 	\brief	local class to hold DRM information
 */
class attrNameData
{
public:
	std::string attrName;
	bool isProcessed;
	attrNameData() : attrName(""),isProcessed(false)
	{
	}

	attrNameData(std::string argument) : attrName(argument), isProcessed(false)
	{
	}

	bool operator==(const attrNameData& rhs) const { return (this->attrName == rhs.attrName); }
};

/**
 * @brief To have hostname mapped curl handles
 */
typedef struct eCurlHostMap
{
	CURL *curl;
	std::string hostname;
	bool isRemotehost;
	bool redirect;

	eCurlHostMap():curl(NULL),hostname(""),isRemotehost(true),redirect(true)
	{}

	//Disabled
	eCurlHostMap(const eCurlHostMap&) = delete;
	eCurlHostMap& operator=(const eCurlHostMap&) = delete;
}eCurlHostMapStruct;

/**
 * @brief Struct to store parsed url hostname & its type
 */
typedef struct AampUrlInfo
{
	std::string hostname;
	bool isRemotehost;

	AampUrlInfo():hostname(""),isRemotehost(true)
	{}

	//Disabled
	AampUrlInfo(const AampUrlInfo&) = delete;
	AampUrlInfo& operator=(const AampUrlInfo&) = delete;
}AampURLInfoStruct;

typedef enum
{
	PROFILE_BLACKLIST_DRM_FAILURE,
	PROFILE_BLACKLIST_INIT_FAILURE

} eBlacklistProfileReason;

typedef struct BlacklistProfileInfo_t
{
	// Extend for HLS and other stream formats
	std::string mPeriodId;
	uint32_t mAdaptationSetIdx;
	eBlacklistProfileReason mReason;
} StreamBlacklistProfileInfo;

/**
 * @}
 */

class AampCacheHandler;

class AampDRMLicenseManager;
/**
 * @brief
 *
 */
class SegmentInfo_t;

/**
 * @brief Class representing the AAMP player's private instance, which is not exposed to outside world.
 */
class PrivateInstanceAAMP : public DrmCallbacks, public std::enable_shared_from_this<PrivateInstanceAAMP>
{
private:
	enum AAMP2ReceiverMsgType
	{
	    E_AAMP2Receiver_TUNETIME,   /**< Tune time Message */
	    E_AAMP2Receiver_EVENTS,	/**< Aamp Events to receiver */
	    E_AAMP2Receiver_MsgMAX	/**< Max message to receiver */
	};
	// needed to ensure matching structure alignment in receiver
	typedef struct __attribute__((__packed__)) _AAMP2ReceiverMsg
	{
	    unsigned int type;
	    unsigned int length;
	    char data[1];
	}AAMP2ReceiverMsg;

	#define AAMP2ReceiverMsgHdrSz (sizeof(AAMP2ReceiverMsg)-1)

	//The position previously reported by ReportProgress() (i.e. the position really sent, using SendEvent())
	double mReportProgressPosn;
	long long mLastTelemetryTimeMS;
	std::chrono::system_clock::time_point m_lastSubClockSyncTime;
	std::shared_ptr<TSB::Store> mTSBStore; /**< Local TSB Store object */
	void _SanitizeLanguageList(std::vector<std::string>& languages) const;
public:
	/**
	 * @brief Get profiler bucket type
	 *
	 * @param[in] mediaType - Media type. eg: Video, Audio, etc
	 * @param[in] isInitializationSegment - Initialization segment or not
	 * @return Bucket type
	 */
	ProfilerBucketType _GetProfilerBucketForMedia(AampMediaType mediaType, bool isInitializationSegment)
	{
		switch (mediaType)
		{
		case eMEDIATYPE_SUBTITLE:
			return isInitializationSegment ? PROFILE_BUCKET_INIT_SUBTITLE : PROFILE_BUCKET_FRAGMENT_SUBTITLE;
		case eMEDIATYPE_VIDEO:
			return isInitializationSegment ? PROFILE_BUCKET_INIT_VIDEO : PROFILE_BUCKET_FRAGMENT_VIDEO;
		case eMEDIATYPE_AUDIO:
		default:
			return isInitializationSegment ? PROFILE_BUCKET_INIT_AUDIO : PROFILE_BUCKET_FRAGMENT_AUDIO;
		}
	}

	/**
	 * @fn Tune
	 *
	 * @param[in] url - Asset URL
	 * @param[in] autoPlay - Start playback immediately or not
	 * @param[in] contentType - Content Type
	 * @param[in] bFirstAttempt - External initiated tune
	 * @param[in] bFinalAttempt - Final retry/attempt.
	 * @param[in] audioDecoderStreamSync - Enable or disable audio decoder stream sync,
	 *                set to 'false' if audio fragments come with additional padding at the end
	 *
	 * @param[in] refreshManifestUrl -
	 * @param[in] mpdStitchingMode -
	 * @param[in] sid - Session ID defined by the player
	 * @param[in] manifestData - preprocessed manifest provided by application
	 * @return void
	 */
	void _Tune(const char *url,
				bool autoPlay,
				const char *contentType = NULL,
				bool bFirstAttempt = true,
				bool bFinalAttempt = false,
				const char *sessionUUID = NULL,
				bool audioDecoderStreamSync = true,
				const char *refreshManifestUrl = NULL,
				int mpdStitchingMode = 0,
				std::string sid = {},
				const char *manifestData = NULL
				);

	/**
	 * @brief API Used to reload TSB with new session
	 *
	 * @return void
	 */
	void _ReloadTSB();

	/**
	 * @fn TuneHelper
	 * @param[in] tuneType - Type of tuning. eg: Normal, trick, seek to live, etc
	 * @param[in] seekWhilePaused - Set true if want to keep in Paused state after
	 *              seek for tuneType = eTUNETYPE_SEEK or eTUNETYPE_SEEKTOLIVE
	 * @return void
	 */
	void _TuneHelper(TuneType tuneType, bool seekWhilePaused = false);

	/**
	 * @fn TeardownStream
	 *
	 * @param[in] newTune - true if operation is a new tune
	 * @param[in] newTune - true if downloads need to be disabled
	 * @return void
	 */
	void _TeardownStream( bool newTune, bool disableDownloads = false );

	/**
	 * @fn SendMessageOverPipe
	 *
	 * @param[in] str - Pointer to the message
	 * @param[in] nToWrite - Number of bytes in the message
	 * @return void
	 */
	void _SendMessageOverPipe(const char *str,int nToWrite);

	/**
	 *   @fn GetLangCodePreference
	 *   @return enum type
	 */
	LangCodePreference _GetLangCodePreference() const;

	/**
	 * @fn SetupPipeSession
	 *
	 * @return Success/Failure
	 */
	bool _SetupPipeSession();

	/**
	 * @fn ClosePipeSession
	 *
	 * @return void
	 */
	void _ClosePipeSession();

	/**
	 * @fn SendMessage2Receiver
	 *
	 * @param[in] type - Message type
	 * @param[in] data - Message data
	 * @return void
	 */
	void _SendMessage2Receiver(AAMP2ReceiverMsgType type, const char *data);

	/**
	 * @fn PausePipeline
	 *
	 * @param[in] pause - true for pause and false for play
	 * @param[in] forceStopGstreamerPreBuffering - true for disabling buffer-in-progress
	 * @return true on success
	 */
	bool _PausePipeline(bool pause, bool forceStopGstreamerPreBuffering);

	/**
	 * @fn mediaType2Bucket
	 *
	 * @param[in] mediaType - Media filetype
	 * @return Profiler bucket type
	 */
	ProfilerBucketType _mediaType2Bucket(AampMediaType mediaType);

       /**
         * @brief to set the vod-tune-event according to the player
         *
         * @param[in] tuneEventType
         * @return void
         */
	void _SetTuneEventConfig( TunedEventConfig tuneEventType);
	TunedEventConfig _GetTuneEventConfig(bool isLive);

        /**
         * @fn UpdatePreferredAudioList
         *
         * @return void
         */
	void _UpdatePreferredAudioList();

	/**
	 * @fn EnableMediaDownloads
	 *
	 * @param[in] AampMediaType - playlist type
	 * @return void
	 */
	void _EnableMediaDownloads(AampMediaType type);

	/**
	 * @fn DisableMediaDownloads
	 *
	 * @param[in] AampMediaType - playlist type
	 * @return void
	 */
	void _DisableMediaDownloads(AampMediaType type);

	/**
	 * @fn EnableAllMediaDownloads
	 *
	 * @return void
	 */
	void _EnableAllMediaDownloads();

	/**
	 * @fn ReplaceKeyIDPsshData
	 * @param initialization data input
	 * @param initialization data input size
	 * @param [out] output data size
	 * @retval Output data pointer
	 */
	unsigned char* _ReplaceKeyIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength);

	/**
	 * @fn UpdateBufferBasedOnLiveOffset - to modify Min/Max buffer based on liveoffset value
	 *
	 * @return void
	 */
	void _UpdateBufferBasedOnLiveOffset();

	/**
	 * @fn GetCustomHeaders - get CUrl headers for a media type request
	 *
	 * Free the returned list of CUrl headers using curl_slist_free_all.
	 *
	 * @param[in] mediaType Media type
	 * @return list of CUrl headers
	 */
	struct curl_slist* _GetCustomHeaders(AampMediaType mediaType);

	/**
	 * @fn SetDiscontinuityParam - to mark the discontinuity and save the details
	 * @return void
	 */
	void _SetDiscontinuityParam();

	/**
	 * @fn SetLatencyParam - to mark the latency details
	 * @param latency - latency value
	 * @param buffer - buffer
	 * @param playbackRate - playback Rate
	 * @param bw - bandwidth
	 * @return void
	 */
	void _SetLatencyParam(double latency, double buffer, double playbackRate, double bw);

	/**
	 * @fn SetLLDLowBufferParam - to mark the lld low buff details
	 * @param latency - latency value
	 * @param buff - buffer
	 * @param rate - playback rate
	 * @param bw - bandwidth
	 * @param buffLowCount - Low buffer hit count
	 * @return void
	 */
	void _SetLLDLowBufferParam(double latency, double buff, double rate, double bw, double buffLowCount);

	/**
	 * @fn GetTSBStore - Get the TSB Store Object
	 * @param config - TSB config
	 * @param logger -  Logger function
	 * @param level - Log Level
	 * @return TSB::Store pointer
	 */
	std::shared_ptr<TSB::Store> _GetTSBStore(const TSB::Store::Config& config, TSB::LogFunction logger, TSB::LogLevel level);

	/**
	 * @brief Get if pipeline reconfigure required for elementary stream type change status (from stream abstraction)
	 * @return true if audio codec has changed
	 */
	bool _ReconfigureForCodecChange();

	/**
	* @brief Function pointer passed as argument to AampMPDDownloader class. This function is invoked to read the preprocessed manifest provided by application.
	*  Also it generate error event if preprocessed manifest is not available.
	*
	* @return modified manifest data
	*/
	std::string _SendManifestPreProcessEvent();

	/**
	 * @brief This function is invoked by application with the available preprocessed manifest information
	 * This function is invoked continuously when ever there is an update in manifest
	 */
	void _updateManifest(const char *manifestData);

	bool mDiscontinuityFound;
	int mTelemetryInterval;
	std::vector< std::pair<long long,long> > mAbrBitrateData;

	std::recursive_mutex mLock;
	std::recursive_mutex mParallelPlaylistFetchLock; 	/**< mutex lock for parallel fetch */
	std::thread  mRateCorrectionThread;     /**< Rate correction thread Id **/

	class StreamAbstractionAAMP *mpStreamAbstractionAAMP; /**< HLS or MPD collector */
	class CDAIObject *mCdaiObject;      		/**< Client Side DAI Object */
	std::queue<AAMPEventPtr> mAdEventsQ;   		/**< A Queue of Ad events */
	std::mutex mAdEventQMtx;            		/**< Add events' queue protector */
	bool mInitSuccess;				/**< TODO: Need to replace with player state */
	StreamOutputFormat mVideoFormat;
	StreamOutputFormat mAudioFormat;
	StreamOutputFormat mPreviousAudioType; 		/**< Used to maintain previous audio type of HLS playback */
	StreamOutputFormat mAuxFormat;
	StreamOutputFormat mSubtitleFormat{FORMAT_UNKNOWN};
	std::condition_variable_any mDownloadsDisabled;
	bool mDownloadsEnabled;
	std::map<AampMediaType, bool> mMediaDownloadsEnabled; /* Used to enable/Disable individual mediaType downloads */
	HybridABRManager mhAbrManager;                 /**< Pointer to Hybrid abr manager*/
	ProfileEventAAMP profiler;
	bool licenceFromManifest;
	AudioType previousAudioType; 			/**< Used to maintain previous audio type */

	CURL *curl[eCURLINSTANCE_MAX];
	eCurlHostMapStruct *curlhost[eCURLINSTANCE_MAX];
	CURLSH* mCurlShared;

	// To store Set Cookie: headers and X-Reason headers in HTTP Response
	httpRespHeaderData httpRespHeaders[eCURLINSTANCE_MAX];
	//std::string cookieHeaders[MAX_CURL_INSTANCE_COUNT]; //To store Set-Cookie: headers in HTTP response
	std::string  mManifestUrl;
	std::string mTunedManifestUrl;
	std::string mTsbSessionRequestUrl;
	std::string mSchemeIdUriDai;
	std::string mMPDStichRefreshUrl;
	std::string mProvidedManifestFile;              // provided manifest data
	MPDStichOptions	mMPDStichOption;
	AampURLInfoStruct mOrigManifestUrl;					/**< Original Manifest URl */

	bool isPreferredDRMConfigured;
	bool mIsWVKIDWorkaround;            			/**< Widevine KID workaround flag */
	int mPreCacheDnldTimeWindow;				/**< Stores PreCaching timewindow */
	bool mbDownloadsBlocked;
	bool streamerIsActive;
	bool mFogTSBEnabled;
	bool mIscDVR;
	double mLiveOffset;
	double mLiveOffsetDrift;               /**< allowed drift value from live offset configured **/
	int mNetworkTimeoutMs;
	int mManifestTimeoutMs;
	int mPlaylistTimeoutMs;
	bool mAsyncTuneEnabled;
	long mNetworkBandwidth;
	std::string mTsbType;
	int mTsbDepthMs;
	int mDownloadDelay;
	/**
	 * @brief A readonly, validatable position value.
	 */
	template<typename TPOSITION> class PositionInfo
	{
		private:
		TPOSITION mPosition;
		long long mUpdateTime;
		double mSeekPosSeconds; //copy of seek_pos_seconds
		bool mIsPopulated;  //true if data is real, false if default values

		public:
		PositionInfo():mPosition(-1.0), mUpdateTime(0), mSeekPosSeconds(-1), mIsPopulated(false){}
		PositionInfo(TPOSITION Pos, double SeekPosSeconds):mPosition(Pos), mUpdateTime(aamp_GetCurrentTimeMS()), mSeekPosSeconds(SeekPosSeconds), mIsPopulated(true){}

		/**
		 * @brief The stored position value, may be invalid, check using isPositionValid()
		 */
		TPOSITION getPosition() const {return mPosition;}

		/**
		 * @brief The timestamp at which the position in this object was updated (0 by default)
		 */
		long long getUpdateTime() const {return mUpdateTime;};

		/**
		 * @brief For objects containing real data (check using isPopulated()) this
		 * returns the number of milliseconds since the object was created
		 */
		int getTimeSinceUpdateMs() const
		{
			return (int)(aamp_GetCurrentTimeMS() - getUpdateTime());
		}

		/**
		 * @brief seek_pos_seconds value supplied when this object was created (-1 default)
		 */
		double getSeekPositionSec() const {return mSeekPosSeconds;}

		/**
		 * @brief false if the object contains default data
		 */
		bool isPopulated() const {return mIsPopulated;}

		/**
		 * @brief Returns true if the value returned by Position() is valid
		 */
		bool isPositionValid(const double LatestSeekPosSeconds) const
		{
			constexpr double SEEK_POS_SECONDS_TOLERANCE = 0.01;
			return (
				isPopulated() &&
				((std::abs(getSeekPositionSec() - LatestSeekPosSeconds)<SEEK_POS_SECONDS_TOLERANCE)) &&
				(0.0<=getPosition())
			);
		}
	};

	/**
	 * @brief A standard way of storing positions with associated data for validation purposes
	 */
	template<typename TPOSITIONCACHE> class PositionCache
	{
		PositionInfo<TPOSITIONCACHE> mInfo;
		std::mutex mMutex;

		public:
		PositionCache():mInfo(), mMutex(){}

		/**
		 * @brief Update the stored position information
		 */
		void Update(TPOSITIONCACHE Pos, double SeekPosSeconds)
		{
			std::lock_guard<std::mutex>lock(mMutex);
			mInfo = PositionInfo<TPOSITIONCACHE>{Pos, SeekPosSeconds};
		}

		/**
		 * @brief Retrieve the stored position information
		 */
		PositionInfo<TPOSITIONCACHE> GetInfo()
		{
			std::lock_guard<std::mutex>lock(mMutex);
			return mInfo;
		}

		/**
		 * @brief Explicitly set the cache to an invalid state
		 */
		void Invalidate()
		{
			std::lock_guard<std::mutex>lock(mMutex);
			mInfo = PositionInfo<TPOSITIONCACHE>{};
		}
	};
	PositionCache<long long> mPrevPositionMilliseconds;
	std::mutex mGetPositionMillisecondsMutexHard;	//limit (with lock()) access to GetPositionMilliseconds(), & mGetPositionMillisecondsMutexSoft
	std::mutex mGetPositionMillisecondsMutexSoft;   //detect (with trylock()) where mGetPositionMillisecondsMutexHard would have deadlocked if it was the sole mutex
	volatile std::atomic <long long> mPausePositionMilliseconds;	/**< Requested pause position, can be 0 or more, or AAMP_PAUSE_POSITION_INVALID_POSITION */
	MediaFormat mMediaFormat;
	double seek_pos_seconds; 				/**< indicates the playback position at which most recent playback activity began */
	float rate; 						/**< most recent (non-zero) play rate for non-paused content */
	float playerrate;
	bool mSetPlayerRateAfterFirstframe;
	bool pipeline_paused; 					/**< true if pipeline is paused */
	bool mbNewSegmentEvtSent[AAMP_TRACK_COUNT];

	char mLanguageList[MAX_LANGUAGE_COUNT][MAX_LANGUAGE_TAG_LENGTH]; /**< list of languages in stream */
	int mCurrentLanguageIndex; 				/**< Index of current selected lang in mLanguageList, this is used for VideoStat event data collection */
	int  mMaxLanguageCount;
	std::string preferredLanguagesString;   		/**< unparsed string with preferred languages in format "lang1,lang2,.." */
	std::vector<std::string> preferredLanguagesList; 	/**< list of preferred languages from most-preferred to the least */
	std::string preferredRenditionString; 			/**< unparsed string with preferred renditions in format "rendition1,rendition2,.." */
	std::vector<std::string> preferredRenditionList;	/**< list of preferred rendition from most-preferred to the least */
	std::string preferredLabelsString; 			/**< unparsed string with preferred labels in format "lang1,lang2,.." */
	std::vector<std::string> preferredLabelList;	 	/**< list of preferred labels from most-preferred to the least */
	std::string preferredTypeString; 			/**< unparsed string with preferred accessibility type */
	std::string preferredCodecString; 			/**< unparsed string with preferred codecs in format "codec1,codec2,.." */
	std::vector<std::string> preferredCodecList;            /**<String array to store codec preference */
	std::string preferredNameString;			/**< unparsed string with preferred name of track */
	std::string preferredTextNameString; 		/**< unparsed string with preferred name of text track */
	std::string preferredTextLanguagesString; 		/**< unparsed string with preferred languages in format "lang1,lang2,.." */
	std::vector<std::string> preferredTextLanguagesList;	/**< list of preferred text languages from most-preferred to the least*/
	std::string preferredTextRenditionString; 		/**< String value for rendition */
	std::string preferredTextTypeString; 			/**< String value for text type */
	std::string preferredTextLabelString; 			/**< String value for text type */
	std::string preferredInstreamIdString;			/**< String value for instreamId */
	std::vector<struct DynamicDrmInfo> vDynamicDrmData;
	Accessibility  preferredTextAccessibilityNode; 		/**< Preferred Accessibility Node for Text */
	Accessibility  preferredAudioAccessibilityNode; 	/**< Preferred Accessibility Node for Audio  */
	AudioTrackTuple mAudioTuple;				/**< Deprecated **/
	VideoZoomMode zoom_mode;
	bool video_muted; /**< true iff video plane is logically muted */
	bool subtitles_muted; /**< true iff subtitle plane is logically muted */
	int audio_volume;
	std::vector<std::string> subscribedTags;
	std::vector<TimedMetadata> timedMetadata;
	std::vector<TimedMetadata> timedMetadataNew;
	std::vector<ContentGapInfo> contentGaps;
	std::vector<std::string> manifestHeadersNeeded;
	std::vector<BitsPerSecond>bitrateList;
	std::map<std::string, std::string> httpHeaderResponses;
	bool mIsIframeTrackPresent;				/**< flag to check iframe track availability*/

	bool IsTuneTypeNew; 					/**< Flag for the eTUNETYPE_NEW_NORMAL */
	std::condition_variable waitforplaystart;    			/**< Signaled after playback starts */
	std::mutex mMutexPlaystart;			/**< Mutex associated with playstart */
	long long trickStartUTCMS;
	double durationSeconds;
	double culledSeconds;
	double culledOffset;
        double mProgramDateTime;
	std::vector<PeriodInfo> mMPDPeriodsInfo;
	float maxRefreshPlaylistIntervalSecs;
	EventListener* mEventListener;
	long long prevFirstPeriodStartTime;

	//updated by ReportProgress() and used by PlayerInstanceAAMP::SetRateInternal() to update seek_pos_seconds
	PositionCache<double> mNewSeekInfo;

	long long mAdPrevProgressTime;
	uint32_t mAdCurOffset;					/**< Start position in percentage */
	uint32_t mAdDuration;
	uint64_t mAdAbsoluteStartTime;			/**< Start time of Ad */
	std::string mAdProgressId;
	bool discardEnteringLiveEvt;
	bool mIsRetuneInProgress;
	std::condition_variable_any mCondDiscontinuity;
	guint mDiscontinuityTuneOperationId;
	bool mIsVSS;       					/**< Indicates if stream is VSS, updated during Tune */
	long curlDLTimeout[eCURLINSTANCE_MAX]; 			/**< To store download timeout of each curl instance*/
	std::string mSubLanguage;
	std::vector<std::string> preferredSubtitleLanguageVctr;	/**< list of preferred text languages from most-preferred to the least*/
	bool mPlayerPreBuffered;	     			/**< Player changed from BG to FG */
	int mPlayerId;
	int mDrmDecryptFailCount;				/**< Sets retry count for DRM decryption failure */

	int mCurrentAudioTrackId;				/**< Current audio  track id read from trak box of init fragment */
	int mCurrentVideoTrackId;				/**< Current video track id read from trak box of init fragment */
	bool mIsTrackIdMismatch;				/**< Indicate track_id mismatch in the trak box between periods */

	bool mIsDefaultOffset; 					/**< Playback offset is not specified and we are using the default value/behavior */
	bool mEncryptedPeriodFound;				/**< Will be set if an encrypted pipeline is found while pipeline is clear*/
	bool mPipelineIsClear;					/**< To keep the status of pipeline (whether configured for clear or not)*/

	std::vector <attrNameData> aesCtrAttrDataList; 		/**< Queue to hold the values of DRM data parsed from manifest */
	std::mutex drmParserMutex; 			/**< Mutex to lock DRM parsing logic */
	bool fragmentCdmEncrypted; 				/**< Indicates CDM protection added in fragments **/
	std::thread mPreCachePlaylistThreadId;
	bool mbPlayEnabled;					/**< Send buffer to pipeline or just cache them */
	std::thread createDRMSessionThreadID; 			/**< thread ID for DRM session creation */
	bool drmSessionThreadStarted; 				/**< flag to indicate the thread is running on not */
	AampDRMLicenseManager *mDRMLicenseManager;
	int mPlaylistFetchFailError;				/**< To store HTTP error code when playlist download fails */
	bool mAudioDecoderStreamSync; 				/**<  Flag to set or clear 'stream_sync_mode' property
	                                				in gst brcmaudiodecoder, default: True */
	std::string mSessionToken; 				/**< Field to set session token for player */
	bool midFragmentSeekCache;    				/**< To find if cache is updated when seeked to mid fragment boundary */
	bool mDisableRateCorrection;             /**< Disable live latency correction when user pause or seek the playback **/
	bool mAbortRateCorrection;               /**< Flag to abort rate correction thread **/
	bool mAutoResumeTaskPending;

	std::string mTsbRecordingId; 				/**< Recording ID of current TSB */
	int mthumbIndexValue;

	PausedBehavior mPausedBehavior;				/**< Player paused state behavior for linear */
	bool mJumpToLiveFromPause;				/**< Flag used to jump to live position from paused position */
	bool mSeekFromPausedState; 				/**< Flag used to seek to live/culled position from SetRate() */
	int mDisplayWidth; 					/**< Display resolution width */
	int mDisplayHeight; 					/**< Display resolution height */
	bool mProfileCappedStatus; 				/**< Profile capped status by resolution or bitrate */
	double mProgressReportOffset; 				/**< Offset time for progress reporting */
	double mProgressReportAvailabilityOffset; 	/**< Offset time for progress reporting from availability start */
	double mAbsoluteEndPosition; 				/**< Live Edge position for absolute reporting */
	double mFirstFragmentTimeOffset;			/**< Offset time for first fragment injected */
	AampConfig *mConfig;
	long mDiscStartTime;					/**< start time of discontinuity */
	bool mRateCorrectionDelay;				/**<Disable live latency correction when discontinuity is playing */

	bool mbUsingExternalPlayer; 				/**<Playback using external players eg:OTA, HDMIIN,Composite*/

    bool mbDetached;					/**< Flag to denote setRate call if that called after detach */
	bool mbSeeked; 						/**< Flag to indicate play after seek */

	double mNextPeriodDuration; 				/**< Keep Next Period duration  */
	double mNextPeriodStartTime; 				/**< Keep Next Period Start Time  */
	double mNextPeriodScaledPtoStartTime; 			/**< Keep Next Period Start Time as per PTO  */

	std::condition_variable mRateCorrectionWait;	/**< Conditional variable for signaling timed wait for rate correction*/
	std::mutex mRateCorrectionTimeoutLock;				/**< Rate correction thread mutex for conditional timed wait*/
	double mCorrectionRate;                          /**< Variable to store correction rate **/
	bool mIsEventStreamFound;				/**< Flag to indicate event stream entry in any of period */

	bool mIsFakeTune;

	double mOffsetFromTunetimeForSAPWorkaround; 		/**< current playback position in epoch */
	bool mLanguageChangeInProgress;
	bool mAampTsbLanguageChangeInProgress;     /**< set during AAMP TSB language change */
	int mSupportedTLSVersion;    				/**< ssl/TLS default version */
	std::string mFailureReason;   				/**< String to hold the tune failure reason  */
	long long mTimedMetadataStartTime;			/**< Start time to report TimedMetadata   */
	long long mTimedMetadataDuration;
	bool playerStartedWithTrickPlay; 			/**< To indicate player switch happened in trickplay rate */
	bool userProfileStatus; 				/**< Select profile based on user list*/
	bool mApplyCachedVideoMute;				/**< To apply video mute() operations if it has been cached due to tune in progress */
	std::vector<uint8_t> mcurrent_keyIdArray;		/**< Current KeyID for DRM license */
	DynamicDrmInfo mDynamicDrmDefaultconfig;		/**< Init drmConfig stored as default config */
	std::vector<std::string> mDynamicDrmCache;
	std::recursive_mutex mDynamicDrmUpdateLock;
	std::condition_variable_any mWaitForDynamicDRMToUpdate;
	bool mAudioComponentCount;
	bool mVideoComponentCount;
	bool mAudioOnlyPb;
	double mSubtitleDelta;
	double mAudioDelta;					/** To indicate audio playlist delta */
	bool mVideoOnlyPb;					/**< To indicate Video Only Playback */
	int mCurrentAudioTrackIndex;				/**< Keep current selected audio track index */
	int mCurrentTextTrackIndex;				/**< Keep current selected text track index*/
	double mLLActualOffset;				/**< Actual Offset After Seeking in LL Mode*/
	bool mIsStream4K;                  /**< Identify whether live playing stream is 4K or not; reset on every retune*/
	bool mIsInbandCC;                   /** Indicate inband cc or out of band cc is selected*/
	std::string mFogDownloadFailReason; /** Identify Fog Manifest Download Failure Reason*/
	int mBufferFor4kRampup; 		    /** Max Buffer for rampup used for 4k stream */
	int mBufferFor4kRampdown; 	    /** Min Buffer for rampdown used for 4k Stream */
	bool mIsLoggingNeeded;               /** Enable/disable logging for LLD based on buffer */
	double mLiveEdgeDeltaFromCurrentTime;	/**< The delta between Live edge and current time (calculated at tune time after processing the manifest) */
	double mTrickModePositionEOS;			/**< Position at which EOS will be raised during trick mode playback */
	AampCMCDCollector *mCMCDCollector;

	std::string seiTimecode; /**< SEI Timestamp information from Westeros */

	static bool mTrackGrowableBufMem; /**< GROWABLE BUFFER Debug is enabled or not */
	static bool mSubtecCCEnabled;	/**< To identify SUBTEC_CC is enabled or not */
	// ID3 metadata
	aamp::id3_metadata::MetadataCache mId3MetadataCache; /**< Metadata cache object for the JS event */

	bool mIsFlushFdsInCurlStore;	/**< Mark to clear curl store instance in case of playback stopped due to download Error */
	bool mIsFlushOperationInProgress;		/**< Flag to indicate pipeline flush operation is going on */

	/**
	 * @fn ProcessID3Metadata
	 *
	 * @param[in] segment - fragment
	 * @param[in] size - fragment size
	 * @param[in] type - AampMediaType
	 */
	void _ProcessID3Metadata(char *segment, size_t size, AampMediaType type, uint64_t timestampOffset = 0);

	/**
	 * @fn ReportID3Metadata
	 *
	 * @param[in] mediaType - Media type
	 * @param[in] ptr - ID3 metadata pointer
	 * @param[in] len - Metadata length
	 * @param[in] schemeIdURI - schemeID URI
	 * @param[in] id3Value - value from id3 metadata
	 * @param[in] presTime - presentationTime
	 * @param[in] id3ID - id from id3 metadata
	 * @param[in] eventDur - event duration
	 * @param[in] tScale - timeScale
	 * @param[in] tStampOffset - timestampOffset
	 * @return void
	 */
	void _ReportID3Metadata(AampMediaType mediaType, const uint8_t* ptr, size_t len,
		const char* schemeIdURI = NULL, const char* id3Value = NULL, uint64_t presTime = 0,
		uint32_t id3ID = 0, uint32_t eventDur = 0, uint32_t tScale = 0, uint64_t tStampOffset = 0);

	/**
	 * @fn ReportID3Metadata
	 *
	 * @param[in] mediaType - Media type
	 * @param[in] data - Vector containing the metadata
	 * @param[in] schemeIdURI - schemeID URI
	 * @param[in] id3Value - value from id3 metadata
	 * @param[in] presTime - presentationTime
	 * @param[in] id3ID - id from id3 metadata
	 * @param[in] eventDur - event duration
	 * @param[in] tScale - timeScale
	 * @param[in] tStampOffset - timestampOffset
	 * @return void
	 */
	void _ReportID3Metadata(AampMediaType mediaType, std::vector<uint8_t> data,
		const char* schemeIdURI = NULL, const char* id3Value = NULL, uint64_t presTime = 0,
		uint32_t id3ID = 0, uint32_t eventDur = 0, uint32_t tScale = 0, uint64_t tStampOffset = 0);

    /**
     * @brief Verifies the ID3 packet content and prepares the data for the JS event
     *
     * @param mediaType - Media type
     * @param[in] ptr - ID3 metadata pointer
     * @param[in] len - Metadata packet length
     * @param info - Stream current data
     */
    void _ID3MetadataHandler(AampMediaType mediaType, const uint8_t* ptr, size_t len, const SegmentInfo_t & info, const char * scheme_uri);

	/**
	 * @fn CurlInit
	 *
	 * @param[in] startIdx - Start index of the curl instance
	 * @param[in] instanceCount - Instance count
	 * @param[in] proxyName - proxy to be applied for curl connection
	 * @return void
	 */
	void _CurlInit(AampCurlInstance startIdx, unsigned int instanceCount=1, std::string proxyName="");

	/**
	 *   @fn SetTunedManifestUrl
	 *   @param[in] isrecordedUrl - flag to check for recordedurl in Manifest
	 */
	void _SetTunedManifestUrl(bool isrecordedUrl = false);

	/**
	 *   @fn GetTunedManifestUrl
	 *   @param[out] manifestUrl - for VOD and recordedUrl for FOG enabled
	 */
	const char *_GetTunedManifestUrl();

	/**
	 * @fn SetCurlTimeout
	 *
	 * @param[in] timeout - maximum time  in seconds curl request is allowed to take
	 * @param[in] instance - index of curl instance to which timeout to be set
	 * @return void
	 */
	void _SetCurlTimeout(long timeout, AampCurlInstance instance);

	/**
	 * @brief Set manifest curl timeout
	 *
	 * @param[in] timeout - Timeout value in ms
	 * @return void
	 */
	void _SetManifestCurlTimeout(long timeout);

	/**
	 * @fn StoreLanguageList
	 *
	 * @param[in] langlist - Vector of languages
	 * @return void
	 */
	void _StoreLanguageList(const std::set<std::string> &langlist);

	/**
	 * @fn IsAudioLanguageSupported
	 *
	 * @param[in] checkLanguage - Language to be checked
	 * @return True or False
	 */
	bool _IsAudioLanguageSupported (const char *checkLanguage);
	/**
	 * @fn LicenseRenewal
	 *
	 * @param[in] drmHelper - Current drm helper
	 * @param[in] userData - DrmSession data
	 * @return void
	 */
	void LicenseRenewal(DrmHelperPtr drmHelper,void* userData) override;
	/**
	 * @fn CurlTerm
	 *
	 * @param[in] startIdx - First index
	 * @param[in] instanceCount - Instance count
	 * @return void
	 */
	void _CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount=1);

	/**
	 * @fn GetPlaylistCurlInstance
	 * Considers parallel download to decide the curl instance
	 * @param[in] AampMediaType  - type of playlist
	 * @param[in] IsInitDnld - Init or Refresh download
	 */
	AampCurlInstance _GetPlaylistCurlInstance(AampMediaType type, bool IsInitDnld=true);

	/**
	 * @fn GetFile
	 *
	 * @param [in] bucketType profiling bucket
	 * @param[in] remoteUrl media file to download
	 * @param[in] mediaType
	 * @param[out] buffer receives downloaded bytes on success
	 * @param[out] effectiveUrl - Final URL after HTTP redirection
	 * @param[out] http_error - HTTP error code
	 * @param[out] downloadTime
	 * @param[in] range - Byte range
	 * @param[in] curlInstance - Curl instance to be used
	 * @param[in] resetBuffer - Flag to reset the out buffer
	 * @param[in] bitrate
	 * @param[out] fogError
	 * @param[in] fragmentDurationS
	 * @param[in] maxInitDownloadTimeMS - Max time to retry init segment downloads if AAMP TSB is enabled, 0 otherwise
	 * @return true iff successful
	 */
	bool _GetFile( std::string remoteUrl, AampMediaType mediaType, AampGrowableBuffer *buffer, std::string& effectiveUrl, int *http_error = NULL, double *downloadTime = NULL, const char *range = NULL, unsigned int curlInstance = 0, bool resetBuffer = true, BitsPerSecond *bitrate = NULL,  int * fogError = NULL, double fragmentDurationS = 0, ProfilerBucketType bucketType=PROFILE_BUCKET_TYPE_COUNT, int maxInitDownloadTimeMS = 0);

	/**
	 * @fn getUUID
	 *
	 * @param[out] string - TraceUUID
	 */
	const std::string & _GetTraceUUID() const { return mTraceUUID; }

	/**
	 * @fn SetSessionId
	 * @brief Sets the session id
	 * @param[in] sid - Session ID
	 *
	*/
	void _SetSessionId(std::string sid);

	/**
	 * @fn GetSessionId
	 *
	 * @brief Returns the value of the current session id
	 *
	 * @returns The current session ID
	*/
	const std::string & _GetSessionId() const
	{
		// AAMPLOG_INFO(" DBG :: Session ID is read.");
		return mSessionId;
	}

	/**
	 * @fn GetOnVideoEndSessionStatData
	 *
	 * @param[out] buffer - Pointer to the output buffer
	 */
	void _GetOnVideoEndSessionStatData(std::string &data);

	/**
	 * @fn LoadIDX
	 *
	 * @param[in] bucketType - Bucket type of the profiler
	 * @param[in] fragmentUrl - Fragment URL
	 * @param[out] buffer - Pointer to the output buffer
	 * @param[out] len - Content length
	 * @param[in] curlInstance - Curl instance to be used
	 * @param[in] range - Byte range
	 * @param[in] mediaType - File type
	 * @param[out] fogError - Error from FOG
	 * @return void
	 */
	void _LoadIDX( ProfilerBucketType bucketType, std::string fragmentUrl, std::string& effectiveUrl,  AampGrowableBuffer *idx, unsigned int curlInstance = 0, const char *range = NULL,int * http_code = NULL, double *downloadTime = NULL, AampMediaType mediaType = eMEDIATYPE_MANIFEST,int * fogError = NULL);

	/**
	 * @fn EndOfStreamReached
	 *
	 * @param[in] mediaType - Media type
	 * @return void
	 */
	void _EndOfStreamReached(AampMediaType mediaType);

	/**
	 * @brief Clip ended
	 *
	 * @param[in] mediaType - Media type
	 * @return void
	 */
	void _EndTimeReached(AampMediaType mediaType);

	/**
	 * @brief Insert ad content
	 *
	 * @param[in] url - Ad url
	 * @param[in] positionSeconds - Ad start position in seconds
	 * @return void
	 */
	void _InsertAd(const char *url, double positionSeconds);

	/**
	 * @fn AddEventListener
	 *
	 * @param[in] eventType - Event type
	 * @param[in] eventListener - Event handler
	 * @return void
	 */
	void _AddEventListener(AAMPEventType eventType, EventListener* eventListener);

	/**
	 * @fn RemoveEventListener
	 *
	 * @param[in] eventType - Event type
	 * @param[in] eventListener - Event handler
	 * @return void
	 */
	void _RemoveEventListener(AAMPEventType eventType, EventListener* eventListener);
	/**
	 * @fn IsEventListenerAvailable
	 *
	 * @param[in] eventType - Event type
	 * @return void
	 */
	bool _IsEventListenerAvailable(AAMPEventType eventType);


	/**
	 * @fn SendErrorEvent
	 *
	 * @param[in] tuneFailure - Reason of error
	 * @param[in] description - Optional description of error
	 * @return void
	 */
	void _SendErrorEvent(AAMPTuneFailure tuneFailure, const char *description = NULL, bool isRetryEnabled = true, int32_t secManagerClassCode = -1, int32_t secManagerReasonCode = -1, int32_t secClientBusinessStatus = -1,const std::string &responseData = "undefined");

	/**
	 * @fn SendDRMMetaData
	 * @param e DRM metadata event
	 */
	void _SendDRMMetaData(DrmMetaDataEventPtr e);

	/**
	 * @fn SendDrmErrorEvent
	 * @param[in] event aamp event struck which holds the error details and error code(http, curl or secclient).
	 * @param[in] isRetryEnabled drm retry enabled
	 */
	void _SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled);

	/**
	 * @fn SendDownloadErrorEvent
	 *
	 * @param[in] tuneFailure - Reason of error
	 * @param[in] error_code - HTTP error code/ CURLcode
	 * @return void
	 */
	void _SendDownloadErrorEvent(AAMPTuneFailure tuneFailure,int error_code);

	/**
	 * @fn SendAnomalyEvent
	 *
	 * @param[in] type - severity of message
	 * @param[in] format - format string
	 * args [in]  - multiple arguments based on format
	 * @return void
	 */
	void _SendAnomalyEvent(AAMPAnomalyMessageType type, const char* format, ...);

	/**
	 * @fn SendBufferChangeEvent
	 *
	 * @param[in] bufferingStopped- Flag to indicate buffering stopped.Underflow = True
	 * @return void
	 */
	void _SendBufferChangeEvent(bool bufferingStopped=false);

	/**
	 * @fn SendTuneMetricsEvent
	 *
	 * @param[in] timeMetricData- Providing the Tune Timemetric info as an event
	 * @return void
	 */
	void _SendTuneMetricsEvent(std::string &timeMetricData);

	/* Buffer Under flow status flag, under flow Start(buffering stopped) is true and under flow end is false*/
	bool mBufUnderFlowStatus;
	bool _GetBufUnderFlowStatus() { return mBufUnderFlowStatus; }
	void _SetBufUnderFlowStatus(bool statusFlag) { mBufUnderFlowStatus = statusFlag; }
	void _ResetBufUnderFlowStatus() { mBufUnderFlowStatus = false;}

	/**
	 * @fn SendEvent
	 *
	 * @param[in] eventData - Event data
	 * @param[in] eventMode - Sync/Async/Default mode(decided based on AsyncTuneEnabled/SourceId
	 * @return void
	 */

	void _SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode=AAMP_EVENT_DEFAULT_MODE);

	/**
	 * @fn NotifySpeedChanged
	 *
	 * @param[in] rate - New speed
	 * @param[in] changeState - true if state change to be done, false otherwise (default = true)
	 * @return void
	 */
	void _NotifySpeedChanged(float rate, bool changeState = true);

	/**
	 * @fn NotifyBitRateChangeEvent
	 *
	 * @param[in] bitrate - New bitrate
	 * @param[in] reason - Bitrate change reason
	 * @param[in] width - Video width
	 * @param[in] height - Video height
 	 * @param[in] framerate - FRAME-RATE from manifest
	 * @param[in] GetBWIndex - Flag to get the bandwidth index
	 * @return void
	 */
	void _NotifyBitRateChangeEvent(BitsPerSecond bitrate, BitrateChangeReason reason, int width, int height, double framerate, double position, bool GetBWIndex = false, VideoScanType scantype = eVIDEOSCAN_UNKNOWN, int aspectRatioWidth = 0, int aspectRatioHeight = 0);

	/**
	 * @fn NotifyEOSReached
	 *
	 * @return void
	 */
	void _NotifyEOSReached();

	/**
	 * @fn NotifyOnEnteringLive
	 *
	 * @return void
	 */
	void _NotifyOnEnteringLive();

	/**
	 * @fn getLastInjectedPosition
	 *
	 * @return last injected position
	 */
	double _getLastInjectedPosition();
	/**
	 * @brief Get persisted profile index
	 *
	 * @return Profile index
	 */
	int _GetPersistedProfileIndex() {return mPersistedProfileIndex;}

	/**
	 * @brief Set persisted profile index
	 *
	 * @param[in] profile - Profile index
	 * @return void
	 */
	void _SetPersistedProfileIndex(int profile){mPersistedProfileIndex = profile;}

	/**
	 * @brief Set persisted bandwidth
	 *
	 * @param[in] bandwidth - Bandwidth in bps
	 * @return void
	 */
	void _SetPersistedBandwidth(long bandwidth) {mAvailableBandwidth = bandwidth;}

	/**
	 * @brief Get persisted bandwidth
	 *
	 * @return Bandwidth
	 */
	long _GetPersistedBandwidth(){return mAvailableBandwidth;}

	/**
	 * @fn UpdateDuration
	 *
	 * @param[in] seconds - Duration in seconds
	 * @return void
	 */
	void _UpdateDuration(double seconds);

	/**
	 * @fn SetFlushCurlStoreFdToClose
	 *
	 * @param[in] isFlush - true if fds need to be deleted else false
	 * @return void
	 */
	void _SetFlushFdsNeededInCurlStore(bool isFlush)
	{
		mIsFlushFdsInCurlStore=isFlush;
	}

	/**
	 * @fn UpdateCullingState
	 *
	 * @param[in] culledSeconds - Seconds to be culled
	 * @return void
	 */
	void _UpdateCullingState(double culledSeconds);

	/**
	 *   @fn UpdateRefreshPlaylistInterval
	 *
	 *   @param[in]  maxIntervalSecs - Interval in seconds
	 *   @return void
	 */
	void _UpdateRefreshPlaylistInterval(float maxIntervalSecs);

	/**
	*   @fn GetVideoPTS
	*   @return long long - Video PTS
	*/
	long long _GetVideoPTS();

	/**
	 *   @fn ReportProgress
 	 *   @param[in]  sync - Flag to indicate that event should be synchronous
	 *   @param[in]  beginningOfStream - Flag to indicate if the progress reporting is for the Beginning Of Stream
	 *   @return void
	 */
	void _ReportProgress(bool sync = true, bool beginningOfStream = false);
	/**
	 *   @fn WakeupLatencyCheck
	 *   @return void
	 */
	void _WakeupLatencyCheck();
	/**
	 *   @fn TimedWaitForLatencyCheck
	 *   @param [in] timeInMs - Time in milliseconds
	 *   @return void
	 */
	void _TimedWaitForLatencyCheck(int timeInMs);
	/**
	 *   @fn StartRateCorrectionWorkerThread
	 *   @return void
	 */
	void _StartRateCorrectionWorkerThread(void);

	/**
	 *   @fn StopRateCorrectionWorkerThread
	 *   @return void
	 */
	void _StopRateCorrectionWorkerThread(void);

	/**
	 *   @fn RateCorrectionWorkerThread
	 *   @return void
	 */
	void _RateCorrectionWorkerThread(void);

	/**
	 *   @fn ReportAdProgress
	 *   @param[in]  positionMs - Position value in milliseconds
	 *   @return void
	 */
	void _ReportAdProgress(double positionMs = -1);

	/**
	 *   @fn GetDurationMs
	 *
	 *   @return Duration in ms.
	 */
	long long _GetDurationMs(void);

	/**
	 *   @fn DurationFromStartOfPlaybackMs
	 *
	 *   @return Duration in ms.
	 */
	long long _DurationFromStartOfPlaybackMs(void);

	/**
	 *   @fn GetPositionMs
	 *
	 *   @return Position in ms.
	 */
	long long _GetPositionMs(void);

	/**
	 *   @brief Lock GetPositionMilliseconds() returns true if successful
	 */
	bool _LockGetPositionMilliseconds();

	/**
	 *   @brief Unlock GetPositionMilliseconds()
	 */
	void _UnlockGetPositionMilliseconds();

	long long _GetPositionRelativeToSeekMilliseconds(long long rate, long long trickStartUTCMS);
	long long _GetPositionRelativeToSeekMilliseconds(void){return _GetPositionRelativeToSeekMilliseconds(rate, trickStartUTCMS);;}
	double _GetPositionRelativeToSeekSeconds(void){return static_cast<double>(_GetPositionRelativeToSeekMilliseconds())/1000.0;;}

	/**
	 *   @fn GetPositionMilliseconds
	 *
	 *   @return Position in ms.
	 */
	long long _GetPositionMilliseconds(void);

	/**
	 *   @fn GetPositionSeconds
	 *
	 *   @return Position in seconds.
	 */
	double _GetPositionSeconds(void)
	{
		return static_cast<double>(_GetPositionMilliseconds())/1000.00;
	}

	/**
	 *   @fn SendStreamCopy
	 *
	 *   @param[in]  mediaType - Type of the media.
	 *   @param[in]  ptr - Pointer to the buffer.
	 *   @param[in]  len - Buffer length.
	 *   @param[in]  fpts - Presentation Time Stamp.
	 *   @param[in]  fdts - Decode Time Stamp
	 *   @param[in]  fDuration - Buffer duration.
	 *   @return True if the fragment has been successfully injected into gstreamer pipeline
	 */
	bool _SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration);

	/**
	 *   @fn SendStreamTransfer
	 *
	 *   @param[in]  mediaType - Type of the media.
	 *   @param[in]  buffer - Pointer to the AampGrowableBuffer.
	 *   @param[in]  fpts - Presentation Time Stamp.
	 *   @param[in]  fdts - Decode Time Stamp
	 *   @param[in]  fDuration - Buffer duration.
	 *   @param[in]  fragmentPTSoffset - Offset PTS
	 *   @param[in]  initFragment - flag for buffer type (init, data)
	 *   @param[in]  discontinuity - flag for discontinuity
	 *   @return void
	 */
	void _SendStreamTransfer(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment = 0, bool discontinuity = false);

	/**
	 * @fn IsLive
	 *
	 * @return True if stream is live, False if not
	 */
	bool _IsLive(void);

	/**
         * @fn IsAudioPlayContextCreationSkipped
         *
         * @return True or False
         */
	bool _IsAudioPlayContextCreationSkipped(void);

	/**
	 * @fn IsLiveStream
	 *
	 * @return True if first tuned manifest is live, False otherwise
	 */
	bool _IsLiveStream(void);

	/**
	 * @fn Stop
	 *
	 * @return void
	 */
	void _Stop( bool isDestructing = false );

	/**
	 * @brief Checking whether TSB enabled or not
	 *
	 * @return True or False
	 */
	bool _IsFogTSBSupported() { return mFogTSBEnabled;}

	/**
	 * @brief Checking whether CDVR in progress
	 *
	 * @return True or False
	 */
	bool _IsInProgressCDVR() {return (_IsLive() && _IsCDVRContent());}

	/**
	 * @brief Checking whether fog is giving uninterrupted TSB
	 *
	 * @return True or False
	 */
	bool _IsUninterruptedTSB() {return (_IsFogTSBSupported() && !_IsLive());}

	/**
	 * @brief Checking whether CDVR Stream or not
	 *
	 * @return True or False
	 */
	bool _IsCDVRContent() { return (mContentType==ContentType_CDVR || mIscDVR);}
	/**
	 * @brief Checking whether OTA content or not
 	 *
	 * @return True or False
	 */
	bool _IsOTAContent() { return (mContentType==ContentType_OTA);}
	/**
	 * @brief Checking whether EAS content or not
	 *
	 * @return True or False
	 */
	bool _IsEASContent() { return (mContentType==ContentType_EAS);}

	bool _IsIVODContent() { return (mContentType==ContentType_IVOD);}
	/**
	 * @fn ReportTimedMetadata
	 */
	void _ReportTimedMetadata(bool init=false);
	/**
	 * @fn ReportTimedMetadata
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] szName - Metadata name
	 * @param[in] szContent - Metadata content
	 * @param[in] nb - ContentSize
	 * @param[in] bSyncCall - Sync /Async Event reporting
	 * @param[in] id - Identifier of the TimedMetadata
	 * @param[in] durationMS - Duration in milliseconds
	 * @return void
	 */
	void _ReportTimedMetadata(long long timeMS, const char* szName, const char* szContent, int nb, bool bSyncCall=false,const char* id = "", double durationMS = -1);
	/**
	 * @fn SaveNewTimedMetadata
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] szName - Metadata name
	 * @param[in] szContent - Metadata content
	 * @param[in] nb - ContentSize
	 * @param[in] id - Identifier of the TimedMetadata
	 * @param[in] durationMS - Duration in milliseconds
	 * @return void
	 */
	void _SaveNewTimedMetadata(long long timeMS, const char* szName, const char* szContent, int nb, const char* id = "", double durationMS = -1);

	/**
	 * @fn SaveTimedMetadata
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] szName - Metadata name
	 * @param[in] szContent - Metadata content
	 * @param[in] nb - ContentSize
	 * @param[in] id - Identifier of the TimedMetadata
	 * @param[in] durationMS - Duration in milliseconds
	 * @return void
	 */
	void _SaveTimedMetadata(long long timeMS, const char* szName, const char* szContent, int nb, const char* id = "", double durationMS = -1);

	/**
	 * @fn ReportBulkTimedMetadata
	 *
	 * @return void
	 */
	void _ReportBulkTimedMetadata();

	/**
	 * @fn ReportContentGap
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] id - Identifier of the TimedMetadata
	 * @param[in] durationMS - Duration in milliseconds
	 * @return void
	 */
	void _ReportContentGap(long long timeMS, std::string id, double durationMS = -1);

	/**
	 * @fn interruptibleMsSleep
	 *
	 * @param[in] timeInMs timeout in milliseconds
	 * @return void
	 */
	void _interruptibleMsSleep(int timeInMs);

	/**
	 * @brief Check if downloads are enabled
	 *
	 * @return true if downloads are enabled
	 */
	bool _DownloadsAreEnabled(void);

	/**
	 * @fn StopDownloads
	 * @return void
	 */
	void _StopDownloads();

	/**
 	 * @fn ResumeDownloads
	 *
	 * @return void
	 */
	void _ResumeDownloads();

	/**
	 * @fn StopTrackDownloads
	 *
	 * @param[in] type Media type
	 * @return void
	 */
	void _StopTrackDownloads(AampMediaType type);

	/**
 	 * @fn ResumeTrackDownloads
	 *
	 * @param[in] type Media type
	 * @return void
	 */
	void _ResumeTrackDownloads(AampMediaType type);

	/**
	 *   @fn BlockUntilGstreamerWantsData
	 *
	 *   @param[in] cb - Callback helping to perform additional tasks, if gst doesn't need extra data
	 *   @param[in] periodMs - Delay between callbacks
	 *   @param[in] track - Track id
	 *   @return void
	 */
	void _BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track);

	/**
	 *   @fn LogTuneComplete
	 *
	 *   @return void
	 */
	void _LogTuneComplete(void);

	/**
	*   @brief Additional log entries to assist with tune failure diagnostics
	*
	*   @return void
	*/
	void _AdditionalTuneFailLogEntries();

	/**
     * @fn TuneFail
     *
	 *   @param[in] Fail - Tune fail status
     *   @return void
     */
    void _TuneFail(bool fail);

	/**
	 *   @fn LogFirstFrame
	 *
	 *   @return void
	 */
	void _LogFirstFrame(void);

	/**
	 *   @fn ResetProfileCache
	 *
	 *   @return void
	 */
	void _ResetProfileCache(void);

	void _ActivatePlayer();

	/**
	 *   @fn LogPlayerPreBuffered
	 *
	 *   @return void
	 */
    void _LogPlayerPreBuffered(void);

	/**
	 *   @fn LogDrmInitComplete
	 *
	 *   @return void
	 */
	void _LogDrmInitComplete(void);

	/**
	 *   @fn LogDrmDecryptBegin
	 *
	 *   @param[in] bucketType - Bucket Id
	 *   @return void
	 */
	void _LogDrmDecryptBegin( ProfilerBucketType bucketType );

	/**
	 *   @fn LogDrmDecryptEnd
	 *
	 *   @param[in] bucketType profiler bucket type
	 *   @return void
	 */
	void _LogDrmDecryptEnd( int bucketType );

	/**
	 *   @brief Get manifest URL
	 *
	 *   @return Manifest URL
	 */
	std::string& _GetManifestUrl(void)
	{
		return mManifestUrl;
	}

	/**
	 *   @brief Get DRM init data obtained from manifest URL (if present)
	 *
	 *   @return DRM init data
	 */
	std::string& _GetDrmInitData(void)
	{
		return mDrmInitData;
	}

	/**
	 *   @brief Set manifest URL
	 *
	 *   @param[in] url - Manifest URL
	 *   @return void
	 */
	void _SetManifestUrl(const char *url)
	{
		mManifestUrl.assign(url);
	}

	/**
	 *   @fn NotifyFirstFrameReceived
	 *
	 *   @param[in] ccDecoderHandle - handle for closed caption decoder
	 *   @return void
	 */
	void _NotifyFirstFrameReceived(unsigned long ccDecoderHandle);

	/**
	 *   @fn InitializeCC
	 *
	 *   @param[in] decoderHandle - handle for closed caption decoder
	 *   @return void
	 */
	void _InitializeCC(unsigned long decoderHandle);

	/**
	 *   @brief GStreamer operation start
	 *
	 *   @return void
	 */
	void _SyncBegin(void);

	/**
	 * @fn SyncEnd
	 *
	 * @return void
	 */
	void _SyncEnd(void);

	/**
	 * @fn GetSeekBase
	 *
	 * @return Position in seconds
	 */
	double _GetSeekBase(void);

	/**
	 * @fn ResetCurrentlyAvailableBandwidth
	 *
	 * @param[in] bitsPerSecond - bps
	 * @param[in] trickPlay		- Is trickplay mode
	 * @param[in] profile		- Profile id.
	 * @return void
	 */
	void _ResetCurrentlyAvailableBandwidth(long bitsPerSecond,bool trickPlay,int profile=0);

	/**
	 * @fn GetCurrentlyAvailableBandwidth
	 */
	BitsPerSecond _GetCurrentlyAvailableBandwidth(void);

	/**
	 * @fn DisableDownloads
	 *
	 * @return void
	 */
	void _DisableDownloads(void);

	/**
	 * @fn EnableDownloads
	 *
	 * @return void
	 */
	void _EnableDownloads(void);

	/**
	 *   @brief Register event listener
	 *
	 *   @param[in] type - Event type
	 *   @param[in] listener - Handle to event listener
	 *   @return void
	 */
	void _RegisterEvent(AAMPEventType type, EventListener* listener)
	{
		mEventManager->AddEventListener(type, listener);
	}

	/**
	 *   @brief Register all event listeners
	 *
	 *   @param[in] eventListener - Handle to event listener
	 *   @return void
	 */
	void _RegisterAllEvents(EventListener* eventListener)
	{
		mEventManager->AddListenerForAllEvents(eventListener);
	}

	/**
	 *   @brief UnRegister event listener
	 *
	 *   @param[in] eventListener - Handle to event listener
	 *   @return void
	 */
	void _UnRegisterEvents(EventListener* eventListener)
	{
		mEventManager->RemoveListenerForAllEvents(eventListener);
	}

	/**
	 *   @fn ScheduleRetune
	 *
	 *   @param[in] errorType - Current error type
	 *   @param[in] trackType - Video/Audio
	 *   @return void
	 */
	void _ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull = false);

	/**
	 * @brief PrivateInstanceAAMP Constructor
	 */
	PrivateInstanceAAMP(AampConfig *config=NULL);

	/**
	 * @fn ~PrivateInstanceAAMP
	 */
	~PrivateInstanceAAMP();

	/**
 	 * @brief Copy constructor disabled
	 *
	 */
	PrivateInstanceAAMP(const PrivateInstanceAAMP&) = delete;

	/**
 	 * @brief assignment operator disabled
	 *
	 */
	PrivateInstanceAAMP& operator=(const PrivateInstanceAAMP&) = delete;

	/**
	 *   @fn UpdateVideoRectangle
     	 *   @param[in] x - Left
     	 *   @param[in] y - Top
     	 *   @param[in] w - Width
     	 *   @param[in] h - Height
     	 *   @return void
	 */
	void _UpdateVideoRectangle(int x, int y, int w, int h);
	/**
	 *   @fn SetVideoRectangle
	 *
	 *   @param[in] x - Left
	 *   @param[in] y - Top
	 *   @param[in] w - Width
	 *   @param[in] h - Height
	 *   @return void
	 */
	void _SetVideoRectangle(int x, int y, int w, int h);

	/**
	 *   @fn Discontinuity
	 *
	 *   @param[in] track - Media type
	 *   @param[in] setDiscontinuityFlag if true then no need to call mStreamSink->Discontinuity(), set only the discontinuity processing flag.
	 *   @return true if discontinuity is handled.
	 */
	bool _Discontinuity(AampMediaType track, bool setDiscontinuityFlag = false);

	/**
	 *    @fn SetTrackDiscontinuityIgnoredStatus
	 *
	 *    @return void
	 */
	void _SetTrackDiscontinuityIgnoredStatus(AampMediaType track);

	/**
	 *    @fn IsDiscontinuityIgnoredForOtherTrack
	 *
	 *    @return true - if the discontinuity already ignored.
	 */
	bool _IsDiscontinuityIgnoredForOtherTrack(AampMediaType track);

	/**
	 *    @fn IsDiscontinuityIgnoredForCurrentTrack
	 *
	 *    @return true - if the discontinuity already ignored.
	 */
	bool _IsDiscontinuityIgnoredForCurrentTrack(AampMediaType track);

	/**
	 *    @fn ResetTrackDiscontinuityIgnoredStatus
	 *
	 *    @return void
	 */
	void _ResetTrackDiscontinuityIgnoredStatus(void);

	/**
	 *    @fn ResetTrackDiscontinuityIgnoredStatusForTrack
	 *
	 *    @return void
	 */
	void _ResetTrackDiscontinuityIgnoredStatusForTrack(AampMediaType track);

	/**
	 *   @fn ResetDiscontinuityInTracks
	 *
	 *   @return void
	 */
	void _ResetDiscontinuityInTracks();

	/**
	 *   @fn SetVideoZoom
	 *
	 *   @param[in] zoom - Video zoom mode
	 *   @return void
	 */
	void _SetVideoZoom(VideoZoomMode zoom);

	/**
	 *   @fn SetVideoMute
	 *
	 *   @param[in] muted - muted or unmuted
	 *   @return void
	 */
	void _SetVideoMute(bool muted);

	/**
	 *   @brief Set subtitle mute state
	 *
	 *   @param[in] muted - muted or unmuted
	 *   @return void
	 */
	void _SetSubtitleMute(bool muted);

	/**
	 *   @brief Set audio volume
	 *
	 *   @param[in] volume - Volume level Minimum 0, maximum 100
	 *   @return void
	 */
	void _SetAudioVolume(int volume);

	/**
	 *   @fn SetState
	 *
	 *   @param[in] state - New state
	 *   @return void
	 */
	void _SetState(AAMPPlayerState state);

	/**
	 *   @fn GetState
	 *
	 *  @return current logical player state, i.e. eSTATE_PLAYING, eSTATE_IDLE
	 */
	AAMPPlayerState _GetState();

	/**
     	 *   @fn AddHighIdleTask
 	 *   @param[in] task - Task function pointer
	 *   @param[in] arg - passed as parameter during idle task execution
	 *
	 *   @return void
	 */
	static gint _AddHighIdleTask(IdleTask task, void* arg,DestroyTask dtask=NULL);

	/**
	 *   @fn IsSinkCacheEmpty
	 *
	 *   @param[in] mediaType - Audio/Video
	 *   @return true: empty, false: not empty
	 */
	bool _IsSinkCacheEmpty(AampMediaType mediaType);

	/**
	 * @fn ResetEOSSignalledFlag
	 */
	void _ResetEOSSignalledFlag();

	/**
	 *   @fn NotifyFragmentCachingComplete
	 *
	 *   @return void
	 */
	void _NotifyFragmentCachingComplete();

	/**
	 *   @fn SendTunedEvent
	 *
	 *   @param[in] isSynchronous - send event synchronously or not
	 *   @return success or failure
     	 *   @retval true if event is scheduled, false if discarded
	 */
	bool _SendTunedEvent(bool isSynchronous = true);

	/**
	 *   @fn SendVideoEndEvent
	 *
	 *   @return success or failure
	 */
	bool _SendVideoEndEvent();

	/**
	 *   @fn IsFragmentCachingRequired
	 *
	 *   @return true if required or ongoing, false if not needed
	 */
	bool _IsFragmentCachingRequired();

	/**
	 *   @fn GetPlayerVideoSize
	 *
	 *   @param[out] w - Width
	 *   @param[out] h - Height
	 *   @return void
	 */
	void _GetPlayerVideoSize(int &w, int &h);

	/**
	 *   @fn SetCallbackAsPending
	 *
	 *   @param[in] id - Callback id.
	 *   @return void
	 */
	void _SetCallbackAsPending(guint id);

	/**
	 *   @fn SetCallbackAsDispatched
	 *
	 *   @param[in] id - Callback id.
	 *   @return void
	 */
	void _SetCallbackAsDispatched(guint id);

	/**
	 *   @fn AddCustomHTTPHeader
	 *
	 *   @param[in] headerName  - Header name
	 *   @param[in] headerValue - Header value
	 *   @param[in] isLicenseHeader - true if header is for a license request
	 *   @return void
	 */
	void _AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader);

	/**
	 *   @brief Set license server URL
	 *
	 *   @param[in] url - server URL
	 *   @param[in] drmType - DRM type (PR/WV) for which the URL has to be used, global by default
	 *   @return void
	 */
	void _SetLicenseServerURL(const char* url, DRMSystems drmType = eDRM_MAX_DRMSystems);

	/**
	 *   @brief Set Preferred DRM.
	 *
	 *   @param[in] drmType - Preferred DRM type
	 *   @return void
	 */
	void _SetPreferredDRM(DRMSystems drmType);

	/**
	 *   @fn GetPreferredDRM
	 *
	 *   @return Preferred DRM type
	 */
	DRMSystems _GetPreferredDRM();

	/**
	 *   @fn FoundEventBreak
	 *
	 *   @param[in] adBreakId Adbreak's unique identifier.
	 *   @param[in] startMS Break start time in milliseconds.
	 *   @param[in] brInfo EventBreakInfo object.
	 */
	void _FoundEventBreak(const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo);

	/**
	 *   @fn SetAlternateContents
	 *
	 *   @param[in] adBreakId Adbreak's unique identifier.
	 *   @param[in] adId Individual Ad's id
	 *   @param[in] url Ad URL
	 */
	void _SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url);

	/**
	 *   @fn SendAdResolvedEvent
	 *
	 *   @param[in] adId Ad's unique identifier.
	 *   @param[in] status Manifest status (success/Failure)
	 *   @param[in] startMS Ad playback start time in milliseconds
	 *   @param[in] durationMs Ad's duration in milliseconds
	 */
	void _SendAdResolvedEvent(const std::string &adId, bool status, uint64_t startMS=0, uint64_t durationMs=0);

	/**
	 *   @fn SendAdReservationEvent
	 *
	 *   @param[in] type - Event type
	 *   @param[in] adBreakId - Reservation Id
	 *   @param[in] position - Event position in terms of channel's timeline
	 *   @param[in] absolutePositionMs - Event absolute position
	 *   @param[in] immediate - Send it immediate or not
	 */
	void _SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, uint64_t absolutePositionMs, bool immediate=false);

	/**
	 *   @fn SendAdPlacementEvent
	 *
	 *   @param[in] type - Event type
	 *   @param[in] adId - Placement Id
	 *   @param[in] position - Event position wrt to the corresponding adbreak start
	 *   @param[in] absolutePositionMs - Event absolute position
	 *   @param[in] adOffset - Offset point of the current ad
	 *   @param[in] adDuration - Duration of the current ad
	 *   @param[in] immediate - Send it immediate or not
	 *   @param[in] error_code - Error code (in case of placement error)
	 */
	void _SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset, uint32_t adDuration, bool immediate=false, long error_code=0);

	/**
	 *   @brief Send stalled events to listeners
	 *
	 *   @return void
	 */
	void _SendStalledErrorEvent();

	/**
	 *   @fn IsDiscontinuityProcessPending
	 *
	 *   @return true if discontinuity processing is pending
	 */
	bool _IsDiscontinuityProcessPending();

	/**
	 *   @fn ProcessPendingDiscontinuity
	 */
	bool _ProcessPendingDiscontinuity();

	/**
	 *   @fn NotifyFirstBufferProcessed
	 *
	 *   @param[in] videoRectangle - video rectangle string in format "x,y,w,h";
	 *                               an empty string will be treated as "0,0,0,0".
	 *   @return void
	 */
	void _NotifyFirstBufferProcessed(const std::string& videoRectangle);

	/**
	 * @fn UpdateSubtitleTimestamp
	 */
	void _UpdateSubtitleTimestamp();

	/**
	 * @fn PauseSubtitleParser
	 *
	 */
	void _PauseSubtitleParser(bool pause);

	/**
	 *  @fn ResetTrickStartUTCTime
	 *
	 *  @return void
	 */
	void _ResetTrickStartUTCTime();

	/**
	 *   @fn getStreamType
	 *
	 *   @return Stream type
	 */
	int _getStreamType();

	/**
	 *   @fn GetMediaFormatTypeEnum
	 *
	 *   @return eMEDIAFORMAT
	 */
	MediaFormat _GetMediaFormatTypeEnum() const;

	/**
	 *   @fn getStreamTypeString
	 *
	 *   @return Stream type as string
	 */
	std::string _getStreamTypeString();

	/**
	 *   @fn GetCurrentDRM
	 *
	 *   @return current drm helper
	 */
	DrmHelperPtr  _GetCurrentDRM();

	/**
	 *   @fn GetPreferredAudioProperties
	 *
	 *   @return json string with preference data
	 */
	std::string _GetPreferredAudioProperties();

	/**
	 *   @fn GetPreferredTextProperties
	 */
	std::string _GetPreferredTextProperties();

	/**
	 *   @brief Set DRM type
	 *
	 *   @param[in] drm - New DRM type
	 *   @return void
	 */
	void _setCurrentDrm(DrmHelperPtr drm) { mCurrentDrm = drm; }

	/**
	 * @fn GetMoneyTraceString
	 * @param[out] customHeader - Generated moneytrace is stored
	 *
	 * @return void
	 */
	void _GetMoneyTraceString(std::string &) const;

	/**
	 *   @fn NotifyFirstFragmentDecrypted
	 *
	 *   @return void
	 */
	void _NotifyFirstFragmentDecrypted();

	/**
	 *   @fn GetFirstPTS
	 *
	 *   @return PTS of first sample
	 */
	double _GetFirstPTS();

	/**
	 *   @fn GetMidSeekPosOffset
	 *
	 *   @return PTS offset for mid fragment seek
	 */
	double _GetMidSeekPosOffset();

	/**
	 *   @fn IsLiveAdjustRequired
	 *
	 *   @return False if the content is either vod/ivod/cdvr/ip-dvr/eas
	 */
	bool _IsLiveAdjustRequired();

	/**
	 * @fn SendHTTPHeaderResponse
	 *
	 */
	void _SendHTTPHeaderResponse();

	/**
	 *   @fn SendMediaMetadataEvent
	 *
	 */
	void _SendMediaMetadataEvent(void);

	/**
	 *   @fn SendSupportedSpeedsChangedEvent
	 *
	 *   @param[in] isIframeTrackPresent - indicates if iframe tracks are available in asset
	 */
	void _SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent);

	/**
	 *   @fn SendBlockedEvent
	 *
	 *   @param[in] reason - Blocked Reason
	 */
	void _SendBlockedEvent(const std::string & reason, const std::string currentLocator = "");

	/**
	 *   @fn SendWatermarkSessionUpdateEvent
	 *
	 *   @param[in] sessionHandle - Handle used to track and manage session
	 *   @param[in] status - Status of the watermark session
	 *   @param[in] system - Watermarking protection provider
	 */
	void _SendWatermarkSessionUpdateEvent(uint32_t sessionHandle, uint32_t status, const std::string &system);

	/**
	 *   @fn IsTuneCompleted
	 *
	 *   @return true, if tune completed.
	 */
	bool _IsTuneCompleted();

	/**
	 *   @brief Check if ABR enabled for this playback session.
	 *
	 *   @return true if ABR enabled.
	 */
	bool _CheckABREnabled(void) { return ISCONFIGSET_PRIV(eAAMPConfig_EnableABR); }

	/**
 	 *    @fn GetThumbnails
	 *
	 *    @return string with Thumbnail information.
	 */
	std::string _GetThumbnails(double start, double end);
	/**
	 *    @fn GetThumbnailTracks
	 *
	 *    @return string with thumbnail track information.
	 */
	std::string _GetThumbnailTracks();
	/**
	 *   @brief Get preferred bitrate for video.
	 *
	 *   @return preferred bitrate.
	 */
	long _GetVideoBitrate();

	/**
	 *   @fn GetNetworkProxy
	 *
	 *   @return Network proxy URL, if exists.
	 */
	std::string _GetNetworkProxy();

	/**
	 *   @fn GetLicenseReqProxy
	 *
	 *   @return proxy to use for license request
	 */
	std::string _GetLicenseReqProxy();

	/**
	 *   @brief Set is Live flag, Current manifest type
	 *
	 *   @param[in] isLive - is Live flag
	 *   @return void
	 */
	void _SetIsLive(bool isLive)  {mIsLive = isLive; }

	/**
	 *   @brief Set is Audio play context is skipped, due to Audio HLS file is ES Format type.
	 *
	 *   @param[in] isAudioContextSkipped - is audio context creation skipped.
	 *   @return void
	 */
	void _SetAudioPlayContextCreationSkipped( bool isAudioContextSkipped ) { mIsAudioContextSkipped = isAudioContextSkipped; }

	/**
	 *   @brief Set isLiveStream flag, This keeps the history of first tuned manifest type
	 *
	 *   @param[in] isLiveStream - is Live stream flag
	 *   @return void
	 */
	void _SetIsLiveStream(bool isLiveStream)  {mIsLiveStream = isLiveStream; }

	/**
	 *   @fn SignalTrickModeDiscontinuity
	 *
	 *   @return void
	 */
	void _SignalTrickModeDiscontinuity();

	/**
	 *   @brief  pass service zone, extracted from locator &sz URI parameter
	 *   @return std::string
	 */
	std::string _GetServiceZone() const{ return mServiceZone; }

	/**
	 *   @brief  pass virtual stream ID
	 *   @return std::string
	 */
	std::string _GetVssVirtualStreamID() const{ return mVssVirtualStreamId; }

	/**
	 *   @brief  set virtual stream ID, extracted from manifest
	 */
	void _SetVssVirtualStreamID(std::string streamID) { mVssVirtualStreamId = streamID;}

	/**
	 *   @brief getTuneType Function to check what is the tuneType
	 *  @return Bool TuneType
	 */
	TuneType _GetTuneType()  { return mTuneType; }

	/**
	 *   @brief IsNewTune Function to check if tune is New tune or retune
	 *
	 *   @return Bool True on new tune
	 */
	bool _IsNewTune()  { return ((eTUNETYPE_NEW_NORMAL == mTuneType) || (eTUNETYPE_NEW_SEEK == mTuneType) || (eTUNETYPE_NEW_END == mTuneType)); }

	/**
	 *   @brief IsFirstRequestToFog Function to check first request to fog
	 *
	 *   @return true if first request to fog
	 */
	bool _IsFirstRequestToFog()  { return mIsFirstRequestToFOG; }

	/**
	 *   @fn IsMuxedStream
	 *
	 *   @return true if current stream is muxed
	 */
	bool _IsMuxedStream();

	/**
	 * @fn StopTrackInjection
	 *
	 * @param[in] type Media type
	 * @return void
	 */
	void _StopTrackInjection(AampMediaType type);

	/**
	 * @fn ResumeTrackInjection
	 *
	 * @param[in] type Media type
	 * @return void
	 */
	void _ResumeTrackInjection(AampMediaType type);

	/**
	 *   @fn NotifyFirstVideoPTS
	 *
	 *   @param[in]  pts - pts value
	 *   @param[in]  timeScale - time scale (default 90000)
	 */
	void _NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale = 90000);

	/**
	 *   @fn SendVTTCueDataAsEvent
	 *
	 *   @param[in]  cue - vtt cue object
	 */
	void _SendVTTCueDataAsEvent(VTTCue* cue);

	/**
	 *   @fn IsSubtitleEnabled
	 *
	 *   @return bool - true if subtitles are enabled
	 */
	bool _IsSubtitleEnabled(void);

	/**
	 *   @fn WebVTTCueListenersRegistered
	 *
	 *   @return bool - true if listeners are registered
	 */
	bool _WebVTTCueListenersRegistered(void);

	/**   @fn UpdateVideoEndMetrics
	 *
	 *   @param[in]  mediaType - AampMediaType ( Manifest/Audio/Video etc )
	 *   @param[in]  bitrate - bitrate ( bits per sec )
	 *   @param[in]  curlOrHTTPCode - download curl or http error
	 *   @param[in]  strUrl :  URL in case of failures
	 *   @param[in] manifestData : Manifest info to be updated to partner apps
	 *   @return void
	 */
	void _UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData = NULL);

	/**
	 *   @fn UpdateVideoEndProfileResolution
	 *   @param[in]  mediaType - AampMediaType ( Manifest/Audio/Video etc )
	 *   @param[in]  bitrate - bitrate ( bits per sec )
	 *   @param[in]  width - Frame width
	 *   @param[in]  height - Frame Height
	 *   @return void
	 */
	void _UpdateVideoEndProfileResolution(AampMediaType mediaType, BitsPerSecond bitrate, int width, int height);

	/**
	 *   @fn UpdateVideoEndTsbStatus
	 *
	 *   @param[in]  btsbAvailable - true if TSB supported
	 *   @return void
	 */
	void _UpdateVideoEndTsbStatus(bool btsbAvailable);

	/**
	 *   @fn UpdateProfileCappedStatus
	 *
	 *   @return void
	 */
	void _UpdateProfileCappedStatus(void);

	/**
	 *   @fn UpdateVideoEndMetrics
	 *
	 *   @param[in]  mediaType - AampMediaType ( Manifest/Audio/Video etc )
 	 *   @param[in]  bitrate - bitrate ( bits per sec )
	 *   @param[in]  curlOrHTTPCode - download curl or http error
	 *   @param[in]  strUrl :  URL in case of failures
	 *   @param[in] keyChanged : if DRM key changed then it is set to true
	 *   @param[in] isEncrypted : if fragment is encrypted then it is set to true
	 *   @param[in] manifestData : Manifest info to be updated to partner apps
	 *   @return void
	 */
	void _UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration,double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData = NULL);

	/**
	 *   @fn UpdateVideoEndMetrics
	 *
	 *   @param[in]  mediaType - AampMediaType ( Manifest/Audio/Video etc )
	 *   @param[in]  bitrate - bitrate ( bits per sec )
	 *   @param[in]  curlOrHTTPCode - download curl or http error
	 *   @param[in]  strUrl - URL in case of failures
	 *   @return void
	 */
	void _UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime);


	/**
	 *   @fn UpdateVideoEndMetrics
	 *
	 *   @param[in] info - abr info
	 *   @return void
	 */
	void _UpdateVideoEndMetrics(AAMPAbrInfo & info);

	/**
	 *   @fn UpdateVideoEndMetrics
	 *
	 *   @param[in] adjustedRate - new rate after correction
	 *   @return void
	 */
	void _UpdateVideoEndMetrics(double adjustedRate);

	/**
	 *   @brief To check if current asset is DASH or not
	 *
	 *   @return bool - true if its DASH asset
	 */
	bool _IsDashAsset(void) { return (mMediaFormat==eMEDIAFORMAT_DASH); }

	/**
	 *   @fn CheckForDiscontinuityStall
	 *   @param[in] mediaType stream type
	 */
	void _CheckForDiscontinuityStall(AampMediaType mediaType);

	/**
	 *   @fn NotifyVideoBasePTS
	 *
	 *   @param[in]  pts - base pts value
	 */
	void _NotifyVideoBasePTS(unsigned long long basepts, unsigned long timeScale = 90000);

	/**
	 *   @fn GetCustomLicenseHeaders
	 *
	 *   @param[out] headers - map of headers
	 */
	void _GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders);

	/**
	 *   @brief Set async tune configuration for EventPriority
	 *
	 *   @param[in] bValue - true if async tune enabled
	 *   @return void
	 */
	void _SetEventPriorityAsyncTune(bool bValue);

	/**
	 *   @fn GetAsyncTuneConfig
	 *
	 *   @return bool - true if async tune enabled
	 */
	bool _GetAsyncTuneConfig();

	/**
	 * @brief Flush the stream sink
	 * @param[in]  position - playback position
	 */
	void _FlushTrack(AampMediaType type,double pos);

	/**
	 *   @fn FlushStreamSink
	 *
	 *   @param[in] position - position to which we seek after flush
	 *   @param[in] rate - playback rate
	 *   @return void
	 */
	void _FlushStreamSink(double position, double rate);

	/**
	 *   @fn GetAvailableVideoTracks
	 *
	 *   @return std::string JSON formatted list of video tracks
	 */
	std::string _GetAvailableVideoTracks();

	/**
	 *   @fn SetVideoTracks
	 *   @param[in] bitrateList bitrate list
	 *
	 *   @return void
	 */
	void _SetVideoTracks(std::vector<BitsPerSecond> bitrateList);

	/**
	 *   @fn GetAvailableAudioTracks
	 *
	 *   @return std::string JSON formatted list of audio tracks
	 */
	std::string _GetAvailableAudioTracks(bool allTrack=false);

	/**
	 *   @fn GetAvailableTextTracks
	 *
	 *   @return std::string JSON formatted list of text tracks
	 */
	std::string _GetAvailableTextTracks(bool alltrack=false);

	/**
	 * @fn SetPreferredTextLanguages
	 *
	 * @brief set preferred Audio Language properties like language, rendition, type, codec, and Label
	 * @param - language list
	 * @return void
	 */
	void _SetPreferredTextLanguages(const char *param );

	/*
	 *   @fn GetVideoRectangle
	 *
	 *   @return current video co-ordinates in x,y,w,h format
	 */
	std::string _GetVideoRectangle();
	/**
	 *   @fn SetPreCacheDownloadList
	 *   @param[in] dnldListInput Playlist Download list
	 *
	 *   @return void
	 */
	void _SetPreCacheDownloadList(PreCacheUrlList &dnldListInput);
	/**
	 *   @fn PreCachePlaylistDownloadTask
	 *
	 *   @return void
	 */
	void _PreCachePlaylistDownloadTask();

	/**
	 *   @fn SetAppName
	 *
	 *   @return void
	 */
	void _SetAppName(std::string name);

	/**
	 *   @fn GetAppName
	 *
	 *   @return string application name
	 */
	std::string _GetAppName();

	/**
	 *   @fn SendId3MetadataEvent
	 *
	 *   @param[in] id3Metadata ID3 metadata
	 */
	void _SendId3MetadataEvent(aamp::id3_metadata::CallbackData* id3Metadata);

	/**
	 * @fn TrackDownloadsAreEnabled
	 *
	 * @param[in] type Media type
	 * @return bool true if track can inject data, false otherwise
	 */
	bool _TrackDownloadsAreEnabled(AampMediaType type);

	/**
	 * @fn StopBuffering
	 *
	 * @param[in] forceStop - stop buffering forcefully
	 * @return void
	 */
	void _StopBuffering(bool forceStop);
	/**
	 *   @fn IsPlayEnabled
	 *
	 *   @return true if autoplay enabled
	 */
	bool _IsPlayEnabled();

	/**
	 * @fn detach
	 *
	 */
	void _detach();
	/*
	 *	 @brief Get Access Attribute flag for VSS
	 *
	 *	 @return true / false
	 */
	bool _GetEnableAccessAttributesFlag() const { return ISCONFIGSET_PRIV(eAAMPConfig_EnableAccessAttributes); }

	/**
	 * @fn getAampCacheHandler
	 *
	 * @return Pointer to AampCacheHandler
	 */
	AampCacheHandler * _getAampCacheHandler();

	/**
	 * @fn GetMaximumBitrate
	 * @return maximum bitrate value
	 */
	BitsPerSecond _GetMaximumBitrate();

	/**
	 * @fn GetMinimumBitrate
	 * @return minimum bitrate value
	 */
	BitsPerSecond _GetMinimumBitrate();

	/**
	 * @fn GetDefaultBitrate
	 * @return default bitrate value
	 */
	BitsPerSecond _GetDefaultBitrate();

	/**
	* @fn GetDefaultBitrate4K
	* @return default bitrate 4K value
	*/
	BitsPerSecond _GetDefaultBitrate4K();

	/**
	 * @fn GetIframeBitrate
	 * @return default iframe bitrate value
	 */
	BitsPerSecond _GetIframeBitrate();

	/**
	 * @fn GetIframeBitrate4K
	 * @return default iframe bitrate 4K value
	 */
	BitsPerSecond _GetIframeBitrate4K();

	/**
	 *   @fn GetInitialBufferDuration
	 *
	 *   @return void
	 */
	int _GetInitialBufferDuration();

	/* DrmCallbacks implementation */
	/**
	 *   @fn individualization
	 *
	 *   @param[in] payload - individualization payload
	 *   @return void
	 */
	void Individualization(const std::string& payload) override;

	/* End DrmCallbacks implementation */

	/**
	 *   @fn SetContentType
	 *
	 *   @param[in]  contentType - Content type
	 *   @return void
	 */
	void _SetContentType(const char *contentType);
	/**
	 *   @fn GetContentType
	 *   @return ContentType
	 */
	ContentType _GetContentType() const;

	/**
	 *   @brief Assign the correct mediaFormat by parsing the url
	 *   @param[in] url - manifest url
	 *   @return MediaFormatType
	 */

	MediaFormat _GetMediaFormatType(const char *url);

	/**
	 * @fn GetLicenseServerUrlForDrm
	 *
	 * @param[in] type DRM type
	 * @return license server url
	 */
	std::string _GetLicenseServerUrlForDrm(DRMSystems type);

	/**
	 *   @fn SetStateBufferingIfRequired
	 *
	 *   @return bool - true if has been set
	 */
	bool _SetStateBufferingIfRequired();

	/**
	 *   @fn IsFirstVideoFrameDisplayedRequired
	 *
	 *   @return bool - true if required
	 */
	bool _IsFirstVideoFrameDisplayedRequired();

	/**
	 *   @fn NotifyFirstVideoFrameDisplayed
	 *
	 *   @return void
	 */
	void _NotifyFirstVideoFrameDisplayed();

	/**
	 *   @brief Set audio track
	 *
	 *   @param[in] trackId - index of audio track in available track list
	 *   @return void
	 */
	void _SetAudioTrack(int trackId);

	/**
	 *   @fn GetAudioTrack
	 *
	 *   @return int - index of current audio track in available track list
	 */
	int _GetAudioTrack();

	/**
	 *   @fn GetAudioTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string _GetAudioTrackInfo();

	/**
	 *   @fn SetTextTrack
	 *
	 *   @param[in] trackId - index of text track in available track list
	 *   @param[in] data - subtitle data from application
	 *   @return void
	 */
	void _SetTextTrack(int trackId, char *data=NULL);

	/**
	 *   @fn GetTextTrack
	 *
	 *   @return int - index of current text track in available track list
	 */
	int _GetTextTrack();

	/**
	 *   @fn GetTextTrackInfo
	 *
	 *   @return int - index of current audio track in available track list
	 */
	std::string _GetTextTrackInfo();

	/**
	 *   @fn SetCCStatus
	 *
	 *   @param[in] enabled - true for CC on, false otherwise
	 *   @return void
	 */
	void _SetCCStatus(bool enabled);

	/**
	 * @brief Updates the provided vector of CCTrackInfo with data from a vector of TextTrackInfo.
	 *
	 * @param textTracksCopy A vector of TextTrackInfo objects to be processed.
	 * @param updatedTextTracks A vector of CCTrackInfo objects to be updated.
	 */
	void _UpdateCCTrackInfo(const std::vector<TextTrackInfo>& textTracksCopy, std::vector<CCTrackInfo>& updatedTextTracks);

	/**
	 *   @fn GetCCStatus
	 *
	 *   @return bool- true/false(OFF/ON)
	 */
	bool _GetCCStatus(void);

	/**
	 *   @fn RefreshSubtitles
	 *
	 *   @return void
	 */
	void _RefreshSubtitles();

	/**
	 *   @fn NotifyAudioTracksChanged
	 *
	 *   @return void
	 */
	void _NotifyAudioTracksChanged();

	/**
	 *   @fn NotifyTextTracksChanged
	 *
	 *   @return void
	 */
	void _NotifyTextTracksChanged();

	/**
	 *   @brief Set preferred text track
	 *   Required to persist across trickplay or other operations
	 *
	 *   @param[in] track - text track info object
	 *   @return void
	 */
	void _SetPreferredTextTrack(const TextTrackInfo track) { mPreferredTextTrack = track; }

	/**
	 *   @brief Get preferred text track
	 *
	 *   @return TextTrackInfo - preferred text track object
	 */
	const TextTrackInfo &_GetPreferredTextTrack() { return mPreferredTextTrack; }

	/**
	 *   @fn SetTextStyle
	 *
	 *   @param[in] options - JSON formatted style options
	 *   @return void
	 */
	void _SetTextStyle(const std::string &options);

	/**
	 *   @fn GetTextStyle
	 *
	 *   @return std::string - JSON formatted style options
	 */
	std::string _GetTextStyle();

	/**
	 *   @fn IsActiveInstancePresent
	 *
	 *   @return bool true if available
	 */
	static bool _IsActiveInstancePresent();

	/**
	 *   @brief Return BasePTS - for non-HLS/TS streams this will be zero
	 *
	 *   @return unsigned long long mVideoBasePTS
	 */
	unsigned long long _GetBasePTS() { return mVideoBasePTS; }

	/**
	 *   @brief Set the session Token for player
	 *
	 *   @param[in] sessionToken - sessionToken in string format
	 *   @return void
	 */
	void _SetSessionToken(std::string &sessionToken);

	/**
	 *   @fn PipelineValid
	 *
	 *   @param[in] track - media type
	 *   @return bool
	 */
	bool _PipelineValid(AampMediaType track);

	/**
	 *   @fn SetStreamFormat
	 *
	 *   @param[in] videoFormat - video stream format
	 *   @param[in] audioFormat - audio stream format
	 *   @param[in] auxFormat - aux stream format
	 *   @return void
	 */
	void _SetStreamFormat(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat,  StreamOutputFormat auxFormat);

	/**
	 *   @fn IsAudioOrVideoOnly
	 *
	 *   @param[in] videoFormat - video stream format
	 *   @param[in] audioFormat - audio stream format
	 *   @param[in] auxFormat - aux stream format
	 *   @return bool
	 */
	bool _IsAudioOrVideoOnly(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat);

	/**
	 *   @fn DisableContentRestrictions
	 *   @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 *   @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 *   @param[in] eventChange - disable restriction handling till next program event boundary
	 */
	void _DisableContentRestrictions(long grace=0, long time=-1, bool eventChange=false);

	/**
	 *   @fn EnableContentRestrictions
	 *   @return void
	 */
	void _EnableContentRestrictions();

	/**
	 *   @brief Get config for ABR profile persistence over Seek/Audio Chg
	 *
	 *   @return bool - true if enabled
	 */
	bool _IsBitRatePersistedOverSeek() { return ISCONFIGSET_PRIV(eAAMPConfig_PersistentBitRateOverSeek); }

	/**
	 *   @fn SetPreferredLanguages
	 *   @param[in] languageList - string with comma-delimited language list in ISO-639
	 *             from most to least preferred. Set NULL to clear current list.
	 *   @param[in] preferredRendition  - preferred rendition from role
	 *   @param[in] preferredType -  preferred accessibility type
	 *   @param[in] codecList  - preferred codec list
	 *   @param[in] labelList  - preferred label list
	 *   @param[in] accessibilityItem - preferred accessibilityNode with scheme id and value
	 *   @param[in] preferredName - preferred name of track
	 *   @return void
	 */
	void _SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList, const Accessibility *accessibilityItem = NULL, const char *preferredName = NULL);

	/**
	 *   @brief Set the scheduler instance to schedule tasks
	 *
	 *   @param[in] instance - schedule instance
	 */
	void _SetScheduler(AampScheduler *instance) { mScheduler = instance; }

	/**
	 *   @fn ScheduleAsyncTask
	 *
	 *   @param[in] task - Task
	 *   @param[in] arg - Arguments
	 *   @return int - task id
	 */
	int _ScheduleAsyncTask(IdleTask task, void *arg, std::string taskName="");

	/**
	 *   @fn RemoveAsyncTask
	 *
	 *   @param[in] taskId - task id
	 *   @return bool - true if removed, false otherwise
	 */
	bool _RemoveAsyncTask(int taskId);

	/**
	 *   @fn AcquireStreamLock
	 *
	 *   @return void
	 */
	void _AcquireStreamLock();

	/**
	 *   @fn TryStreamLock
	 *
	 *   @return True if it could I acquire it successfully else false
	 */
	bool _TryStreamLock();

	/**
	 *   @fn ReleaseStreamLock
	 *
	 *   @return void
	 */
	void _ReleaseStreamLock();

	/**
	 *  @fn UpdateLiveOffset
	 *
	 */
	void _UpdateLiveOffset();

	/**
	 *   @fn IsAuxiliaryAudioEnabled
	 *
	 *   @return bool - true if aux audio is enabled
	 */
	bool _IsAuxiliaryAudioEnabled(void);

	/**
	 *   @brief Set auxiliary language
	 *
	 *   @param[in] language - auxiliary language
	 *   @return void
	 */
	void _SetAuxiliaryLanguage(const std::string &language) { mAuxAudioLanguage = language; }

	/**
	 *   @brief Get auxiliary language
	 *
	 *   @return std::string auxiliary audio language
	 */
	std::string _GetAuxiliaryAudioLanguage() { return mAuxAudioLanguage; }

	/**
	 *   @fn GetPauseOnFirstVideoFrameDisp
	 *   @return bool
	 */
	bool _GetPauseOnFirstVideoFrameDisp(void);

	/**
	 *   @fn SetLLDashServiceData
	 *   @param[in] stAampLLDashServiceData - Low Latency Service Data from MPD
	 *   @return void
	 */
	void _SetLLDashServiceData(AampLLDashServiceData &stAampLLDashServiceData);

	/**
	 *   @fn GetLLDashServiceData
	 *
	 *   @return AampLLDashServiceData*
	 */
	AampLLDashServiceData* _GetLLDashServiceData(void);

	/**
	 *   @fn SetVidTimeScale
	 *
	 *   @param[in] vidTimeScale - vidTimeScale value
	 *   @return void
	 */
	void _SetVidTimeScale(uint32_t vidTimeScale);

	/**
	 *   @fn GetVidTimeScale
	 *
	 *   @return uint32_t
	 */
	uint32_t _GetVidTimeScale(void);

	/**
	 *   @fn SetAudTimeScale
	 *
	 *   @param[in] audTimeScale - audTimeScale Value
	 *   @return void
	 */
	void _SetAudTimeScale(uint32_t audTimeScale);

	/**
	 *   @fn GetAudTimeScale
	 *
	 *   @return uint32_t
	 */
	uint32_t  _GetAudTimeScale(void);

	/**
	 * @brief Sets Subtitle TimeScale
	 * @param[in] subTimeScale - Subtitle TimeScale
	 */
	void _SetSubTimeScale(uint32_t subTimeScale);

	/**
	 * @brief Gets Subtitle TimeScale
	 * @return uint32_t - Subtitle TimeScale
	 */
	uint32_t _GetSubTimeScale(void);

	/**
	 *   @fn SetLLDashSpeedCache
	 *
	 *   @param[in] speedCache - Speed Cache
	 *   @return void
	 */
	void _SetLLDashSpeedCache(struct SpeedCache &speedCache);

	/**
	 *   @fn GetLLDashSpeedCache
	 *
	 *   @return struct SpeedCache speedCache*
	 */
	struct SpeedCache * _GetLLDashSpeedCache();

	 /**
	  *   @brief Sets Low latency play rate
	  *
	  *   @param[in] rate - playback rate to set
	  *   @return void
	  */
	void _SetLLDashCurrentPlayBackRate(double rate) { mLLDashCurrentPlayRate = rate; }

	/**
	 *   @brief Gets Low Latency current play back rate
	 *
	 *   @return double
	 */
	double _GetLLDashCurrentPlayBackRate(void);

	/**
	 *   @brief Turn off/on the player speed correction for Low latency Dash
	 *
	 *   @param[in] state - true or false
	 *   @return void
	 */
	void _SetLLDashAdjustSpeed(bool state) { bLLDashAdjustPlayerSpeed = state; }

	/**
	 *   @brief Gets the state of the player speed correction for Low latency Dash
	 *
	 *   @return double
	 */
	bool _GetLLDashAdjustSpeed(void);

	/**
	 *   @brief Set iframe extraction enabled or not
	 *
	 *   @return void
	 */
	void _SetIsIframeExtractionEnabled(bool enable)
	{
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_EnableIFrameTrackExtract,enable);
	}

	/**
	 *   @brief Set LL-DASH chunk mode enabled or not.
	 *
	 *   @param true for LL-DASH chunk mode, default false - which is non chunk mode
	 *
	 *   @return void
	 */
	void _SetLLDashChunkMode(bool enable);

	/**
	 *   @brief Get the status of LL-DASH chunk mode.
	 *
	 *   @return true if LL-DASH chunk mode is enabled, false otherwise.
	 */
	bool _GetLLDashChunkMode();
    
	/**
	 *   @brief Is iframe extraction enabled
	 *
	 *   @return bool
	 */
	bool _IsIframeExtractionEnabled()
	{
		return ISCONFIGSET_PRIV(eAAMPConfig_EnableIFrameTrackExtract) ;
	}

	/**
	 *   @fn GetLiveOffsetAppRequest
	 *   @return bool
	 */
	bool _GetLiveOffsetAppRequest();

	/**
	 *     @fn SetLiveOffsetAppRequest
	 *     @param[in] LiveOffsetAppRequest - flag
	 *     @return void
	 */
	void _SetLiveOffsetAppRequest(bool LiveOffsetAppRequest);

	/**
	 *     @fn GetLowLatencyServiceConfigured
	 *     @return bool
	 */
	bool _GetLowLatencyServiceConfigured();

	/**
	 *     @fn SetLowLatencyServiceConfigured
	 *     @param[in] bConfig - bool flag
	 *     @return void
	 */
	void _SetLowLatencyServiceConfigured(bool bConfig);

	/**
	 *     @fn GetCurrentLatency
	 *
	 *     @return long
	 */
	long _GetCurrentLatency();

	/**
	 *     @fn SetCurrentLatency
	 *     @param[in] currentLatency - Current latency to set
	 *     @return void
	 */
	void _SetCurrentLatency(long currentLatency);

	/**
	 *     @brief Get Media Stream Context
	 *     @param[in] type AampMediaType
	 *     @return MediaStreamContext*
	 */
	class MediaStreamContext* _GetMediaStreamContext(AampMediaType type);

	/**
	 * @fn Run the thread loop monitoring for requested pause position
	 */
	void _RunPausePositionMonitoring(void);

	/**
	 * @fn Start monitoring for requested pause position
	 * @param[in] pausePositionMilliseconds - The position to pause at, must not be negative
	 */
	void _StartPausePositionMonitoring(long long pausePositionMilliseconds);

	/**
	 * @fn Stop monitoring for requested pause position
	 * @param[in] reason - Reason why the pause position monitoring is stopped
	 */
	void _StopPausePositionMonitoring(std::string reason);

	/**
	 * @fn WaitForDiscontinuityProcessToComplete
	 */
	void _WaitForDiscontinuityProcessToComplete(void);

	/**
	 * @fn UnblockWaitForDiscontinuityProcessToComplete
	 */
	void _UnblockWaitForDiscontinuityProcessToComplete(void);

	/**
	 * @fn CompleteDiscontinuityDataDeliverForPTSRestamp
	 */
	void _CompleteDiscontinuityDataDeliverForPTSRestamp(AampMediaType type);

	/**
	 * @brief Set Discontinuity handling period change marked flag
	 * @param[in] value Period change marked flag
	 */
	void _SetIsPeriodChangeMarked(bool value);

	/**
	 * @brief Get Discontinuity handling period change marked flag
	 * @return Period change marked flag
	 */
	bool _GetIsPeriodChangeMarked();

	/**
	 * @fn GetLicenseCustomData
	 *
	 * @return Custom data string
	 */
	std::string _GetLicenseCustomData();

	/**
	*     @fn GetPeriodDurationTimeValue
	*     @return double
	*/
	double _GetPeriodDurationTimeValue(void);

	/**
	 *     @fn GetPeriodStartTimeValue
	 *     @return double
	 */
	double _GetPeriodStartTimeValue(void);

	/**
	 *     @fn GetPeriodScaledPtoStartTime
	 *     @return double
	 */
	double _GetPeriodScaledPtoStartTime(void);

	/**
	 *    @fn LoadFogConfig
	 *    return long error code
	 */
	long _LoadFogConfig(void);

	/**
	* @brief To pass player config to aampabr
	* @fn LoadAampAbrConfig
	* return none
	*/
	void _LoadAampAbrConfig(void);

	/**
	* @brief To pass player config to TSB Handler
	* @fn LoadLocalTSBConfig
	* return none
	*/
	void _LoadLocalTSBConfig(void);

	/**
	 * @fn CreateTsbSessionManager
	 * @brief Create a new TSB Session Manager
	 * The new session manager will be created only for DASH linear content.
	 * If one already exists it will be destroyed (wiping the content of the TSB) and a new one created.
	 */
	void _CreateTsbSessionManager();

	/**
	 *    @brief To increment gaps between periods for dash
	 *    return none
	 */
	void _IncrementGaps();

	/**
 	 *     @fn GetPlaybackStats
 	 *     @return the json string representing the playback stats
 	 */
	std::string _GetPlaybackStats();

	/**
	 *     @fn GetCurrentAudioTrackId
	 */
	int _GetCurrentAudioTrackId(void);

	/**
	 * @fn HandleSSLWriteCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param userdata CurlCallbackContext pointer
	 * @retval size consumed or 0 if interrupted
	 */
	size_t _HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata );

	/**
	 * @fn HandleSSLProgressCallback
	 *
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 * @retval negative value to abort, zero otherwise
	 */
	int _HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow );

	/**
	 * @fn HandleSSLHeaderCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param user_data  CurlCallbackContext pointer
	 * @retval returns size * nmemb
	 */
	size_t _HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* userdata );

	void _UpdateUseSinglePipeline();

	/**
	 * @brief To update the max DASH DRM sessions supported in AAMP
	 */
	void _UpdateMaxDRMSessions();

	/**
	 * @brief To add profile to blacklisted profile list
	 */
	void _AddToBlacklistedProfiles(const StreamBlacklistProfileInfo &info)
	{
		mBlacklistedProfiles.push_back(info);
	}

	/**
	 * @brief To get the blacklisted profiles
	 */
	const std::vector<StreamBlacklistProfileInfo>& _GetBlacklistedProfiles()
	{
		return mBlacklistedProfiles;
	}

	/**
	 *     @fn GetHarvestRemainingFragmentCount
	 *     @return int of remaining fragments to be harvested. 0 when harvest is complete.
	 */
	int _GetHarvestRemainingFragmentCount()
	{
		return mHarvestCountLimit;
	}

	AampMPDDownloader *_GetMPDDownloader()
	{
		return mMPDDownloaderInstance;
	}

	/**
	 * @fn GetVideoPlaybackQuality
	 * @return video playback quality data string.
	 */
	std::string _GetVideoPlaybackQuality(void);

	/**
	 * @brief Get last downloaded manifest file for DASH
	 */
	void _GetLastDownloadedManifest(std::string& manifestBuffer);

	 /**
	 * @fn IsGstreamerSubsEnabled
	 *
	 * @return bool - true if gstsubtec flag enabled and vttcueevent listener not registered.
	 */
	bool _IsGstreamerSubsEnabled(void);

	/**
	 * @brief Signal the clock to subtitle module
	 * @return - true indicating successful operation in sending the clock update
	 */
	bool _SignalSubtitleClock( void );

	/**
	 * @brief Apply CC/Subtitle mute but preserve the original status
	 * This function should be called after acquiring StreamLock
	 * @param[in] muted true if CC/Subtitle is to be muted, false otherwise
	 */
	void _CacheAndApplySubtitleMute(bool muted);

	/**
	  * @fn ReleaseDynamicDRMToUpdateWait
	  *
	  * @param Void
	  */
	void _ReleaseDynamicDRMToUpdateWait();

	/**
	  * @fn GetLiveOffsetMs
	  *
	  * @param Void
	  * @return double, live offset value in ms
	  */
	double _GetLiveOffsetMs() { return mLiveOffset * 1000; }

	/**
	  * @fn GetStreamPositionMs
	  *
	  * @return double, current position in the stream
	  */
	double _GetStreamPositionMs();

	/**
	  * @fn IsAtLivePoint
	  *
	  * @param Void
	  * @return bool, true if stream is playing at live point
	  */
	bool _IsAtLivePoint();

	 /*
	  * @brief Get the TSB Session manager instance
	  * @return AampTSBSessionManager instance
	  */
	AampTSBSessionManager *_GetTSBSessionManager();

	/**
	 * @brief Set AAMP local TSB flag
	 */
	void _SetLocalAAMPTsb(bool value)
	{
		mLocalAAMPTsb = value;
		AAMPLOG_INFO("Local AAMP TSB %d", mLocalAAMPTsb);
	}

	/**
	 * @brief Is AAMP local TSB enabled/disabled
	 */
	bool _IsLocalAAMPTsb()
	{
		return mLocalAAMPTsb;
	}

	/**
	 * @brief Is AAMP local TSB enabled/disabled from config
	 */
	bool _IsLocalAAMPTsbFromConfig()
	{
		return mLocalAAMPTsbFromConfig;
	};

	/**
	 * @brief Set AAMP local TSB injection flag
	 */
	void _SetLocalAAMPTsbInjection(bool value);

	/**
	 * @brief Is AAMP local TSB injection enabled/disabled
	 */
	bool _IsLocalAAMPTsbInjection();

	/**
	 * @brief Clear Local AAMP TSB injection flag if there are no media tracks playing from TSB
	 */
	void _UpdateLocalAAMPTsbInjection();

	/**
	 * @brief Increase Buffer value dynamically according to Max Profile Bandwidth to accommodate Larger Buffers
	 */
	void _IncreaseGSTBufferSize();

	/**
	 * @brief Set to pause on next playback start
	 * @param[in] enable - Flag to set whether enabled
	 */
	void _SetPauseOnStartPlayback(bool enable);

	/**
	 * @brief Send MonitorAvEvent
	 * @param[in] status - Current MonitorAV status
	 * @param[in] videoPositionMS - video position in milliseconds
	 * @param[in] audioPositionMS - audio position in milliseconds
	 * @param[in] timeInStateMS - time in state in milliseconds
	 * @param[in] droppedFrames - dropped frames count
	 */
	void _SendMonitorAvEvent(const std::string &status, int64_t videoPositionMS, int64_t audioPositionMS, uint64_t timeInStateMS, uint64_t droppedFrames);

	/**
	 * @brief Determines if decrypt should be called on clear samples
	 * @return Flag to indicate if should decrypt
	 */
	bool _isDecryptClearSamplesRequired();

	/**
	 * @fn getStringForErrorType
	 * @brief Retrieves a human-readable error string for a given playback error type.
	 *
	 * @param[in] errorType - Errortype of PlaybackErrorType enum.
	 * @return A constant character pointer to the error string corresponding to the provided error type.
	 */
	const char* _getStringForPlaybackError(PlaybackErrorType errorType);

    /**
	 *	@fn CalculateTrickModePositionEOS
	 *		- this function only works for (rate > 1) - see priv_aamp.cpp
	 *	@return void
	 */
	void _CalculateTrickModePositionEOS(void);

	/**
	 * @fn GetLivePlayPosition
	 *
	 * @brief Get current live play stream position.
	 * This is the live edge of the stream minus a configurable offset.
	 *
	 * @retval current live play position of the stream in seconds.
	 */
	 double _GetLivePlayPosition(void);
	
	/**
	 * @fn GetFormatPositionOffsetInMSecs
	 * @brief API to get the offset value in msecs for the position values to be reported.
	 * @return Offset value in msecs
	 */
	double _GetFormatPositionOffsetInMSecs();

protected:

	/**
	 *   @fn IsWideVineKIDWorkaround
	 *
	 *   @param[in] url - url info
	 *   @return true/false
	 */
	bool _IsWideVineKIDWorkaround(const std::string url);

	/**
	 *   @brief Load the configuration lazily
	 *
	 *   @return void
	 */
	void _LazilyLoadConfigIfNeeded(void);

	/**
	 *   @fn ExtractServiceZone
	 *   @param  url - stream url with vss service zone info as query string
	 *   @return std::string
	 */
	void _ExtractServiceZone(std::string url);

	/**
	 *   @brief Schedule Event
	 *
	 *   @param[in]  e - Pointer to the event descriptor
	 *   @return void
	 */
	void _ScheduleEvent(struct AsyncEventDescriptor* e);

	/**
	 * @fn DeliverAdEvents
	 *
	 * @param[in] immediate - flag to deliver ad events immediately
	 * @param[in] positionMs - position in milliseconds
	 *
	 * @return void
	 */
	void _DeliverAdEvents(bool immediate=false, double positionMs = -1);

	/**
	 *   @fn GetContentTypString
	 *
	 *   @return string
	 */
	std::string _GetContentTypString();

	/**
	 *   @fn NotifySinkBufferFull
	 *
	 *   @return void
 	 */
	void _NotifySinkBufferFull(AampMediaType type);
	/**
	 * @fn ExtractDrmInitData
	 */
	const std::tuple<std::string, std::string> _ExtractDrmInitData(const char *url);

	/**
	 *   @brief Set local configurations to variables
	 *
	 *   @return void
	 */
	void _ConfigureWithLocalOptions();

	/**
	 *   @brief Check if discontinuity processed in all tracks
	 *
	 *   @return true if discontinuity processed in all track
	 */
	bool _DiscontinuitySeenInAllTracks();

	/**
	 *   @fn DiscontinuitySeenInAnyTracks
	 *
	 *   @return true if discontinuity processed in any track
	 */
	bool _DiscontinuitySeenInAnyTracks();

	/**
	 *   @fn HasSidecarData
	 *
	 *   @return true if sidecar data available
	 */
	bool _HasSidecarData();

	/**
	 *   @fn GetCurlInstanceForURL
	 *
	 *   @return Get curl instance for the url from curlstore
	 */
	CURL *_GetCurlInstanceForURL(std::string &remoteUrl,unsigned int curlInstance) ;

	/**
	 * @fn prepareManifestDownloadConfig
	 *
	 * @return shared ptr of ManifestDownloadConfig
	 */
	std::shared_ptr<ManifestDownloadConfig> _prepareManifestDownloadConfig();

	/**
	 * @brief  Updates the PTS offset with the given value
	 *
	 * @param value New value of the PTS offset
	 */
	void _UpdatePTSOffsetFromTune(double value, bool is_set = false);

	/**
	 * @brief Provides the value of the currently cached PTS offset from tune
	 *
	 * @return The current value of the PTS cache
	 */
	double _GetPTSOffsetFromTune() const { return m_PTSOffsetFromTune.load(); }

	/**
	 * @brief Notify reached paused when starting playback into paused state
	 *
	 */
	void _NotifyPauseOnStartPlayback(void);

	/**
	 *   @brief Get output format of stream.
	 *
	 *   @param[out]  primaryOutputFormat - format of primary track
	 *   @param[out]  audioOutputFormat - format of audio track
	 *   @param[out]  auxAudioOutputFormat - format of aux audio track
	 *   @param[out]  subtitleOutputFormat - format of subtitle  track
	 *   @return void
	 */
	void _GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat);

	std::mutex mPausePositionMonitorMutex;				// Mutex lock for PausePosition condition variable
	std::condition_variable mPausePositionMonitorCV;	// Condition Variable to signal to stop PausePosition monitoring
    std::thread mPausePositionMonitoringThreadID;			// Thread Id of the PausePositionMonitoring thread
	bool mPausePositionMonitoringThreadStarted;			// Flag to indicate PausePositionMonitoring thread started
	TuneType mTuneType;
	int m_fd;
	bool mIsLive;				// Flag to indicate manifest type.
	bool mIsLiveStream;			// Flag to indicate stream type, keeps history if stream was live earlier.
	bool mIsAudioContextSkipped;		// Flag to indicate Audio playcontext creation is skipped.
	bool mLogTune;				//Guard to ensure sending tune  time info only once.
	bool mFirstProgress;				//Log first progress event.
	bool mTuneCompleted;
	bool mFirstTune;			//To identify the first tune after load.
	int mfirstTuneFmt;			//First Tune Format HLS(0) or DASH(1)
	int  mTuneAttempts;			//To distinguish between new tune & retries with redundant over urls.
	long long mPlayerLoadTime;
	std::atomic<AAMPPlayerState> mState;  //Changed to atomic as there are cross thread accesses.
	long long lastUnderFlowTimeMs[AAMP_TRACK_COUNT];
	bool mbTrackDownloadsBlocked[AAMP_TRACK_COUNT];
	DrmHelperPtr mCurrentDrm;
	int  mPersistedProfileIndex;
	long mAvailableBandwidth;
	bool mProcessingDiscontinuity[AAMP_TRACK_COUNT];
	bool mIsDiscontinuityIgnored[AAMP_TRACK_COUNT];
	bool mDiscontinuityTuneOperationInProgress;
	ContentType mContentType;
	bool mTunedEventPending;
	bool mSeekOperationInProgress;
	bool mTrickplayInProgress;
	std::map<guint, bool> mPendingAsyncEvents;
	std::unordered_map<std::string, std::vector<std::string>> mCustomHeaders;
	bool mIsFirstRequestToFOG;
	// VSS license parameters
	std::string mServiceZone;         	/**< part of url */
	std::string  mVssVirtualStreamId; 	/**< part of manifest file */
	std::string mPlaybackMode;        	/**< linear or VOD or any other type */
	bool mTrackInjectionBlocked[AAMP_TRACK_COUNT];
	CVideoStat * mVideoEnd;
	std::string  mTraceUUID;          	/**< Trace ID unique to tune */
	double mTimeToTopProfile;
	double mTimeAtTopProfile;
	unsigned long long mVideoBasePTS;
	double mPlaybackDuration; 		/**< Stores Total of duration of VideoDownloaded, it is not accurate playback duration but best way to find playback duration */
	std::unordered_map<std::string, std::vector<std::string>> mCustomLicenseHeaders;
	std::string mAppName;
	PreCacheUrlList mPreCacheDnldList;
	bool mProgressReportFromProcessDiscontinuity; /** flag denotes if progress reporting is in execution from ProcessPendingDiscontinuity*/
	AampEventManager *mEventManager;
	AampCacheHandler *mAampCacheHandler;

	int mMinInitialCacheSeconds; 		/**< Minimum cached duration before playing in seconds*/
	std::string mDrmInitData; 		/**< DRM init data from main manifest URL (if present) */
	bool mFragmentCachingRequired; 		/**< True if fragment caching is required or ongoing */
	std::recursive_mutex mFragmentCachingLock; 	/**< To sync fragment initial caching operations */
	bool mPauseOnFirstVideoFrameDisp; 	/**< True if pause AAMP after displaying first video frame */
//	AudioTrackInfo mPreferredAudioTrack; 	/**< Preferred audio track from available tracks in asset */
	TextTrackInfo mPreferredTextTrack; 	/**< Preferred text track from available tracks in asset */
	bool mFirstVideoFrameDisplayedEnabled; 	/**< Set True to enable call to NotifyFirstVideoFrameDisplayed() from Sink */
	unsigned int mManifestRefreshCount; 	/**< counter which keeps the count of manifest/Playlist success refresh */

	guint mAutoResumeTaskId;		/**< handler id for auto resume idle callback */
	AampScheduler *mScheduler; 		/**< instance to schedule async tasks */
	std::recursive_mutex mEventLock; 		/**< lock for operation on mPendingAsyncEvents */
	int mEventPriority; 			/**< priority for async events */
	std::recursive_mutex mStreamLock; 		/**< Mutex for accessing mpStreamAbstractionAAMP */
	int mHarvestCountLimit;			/**< Harvest count */
	int mHarvestConfig;			/**< Harvest config */
	std::string mAuxAudioLanguage; 		/**< auxiliary audio language */
	int mCCId;
	AampLLDashServiceData mAampLLDashServiceData; /**< Low Latency Service Configuration Data */
	bool bLowLatencyServiceConfigured;
	bool bLLDashAdjustPlayerSpeed;
	double mLLDashCurrentPlayRate; 		/**<Low Latency Current play Rate */
	uint32_t vidTimeScale;
	uint32_t audTimeScale;
	uint32_t subTimeScale;
	struct SpeedCache speedCache;
	bool bLowLatencyStartABR;
	bool mLiveOffsetAppRequest;
	long mCurrentLatency;
	bool mApplyVideoRect; 			/**< Status to apply stored video rectangle */
	bool mApplyContentRestriction;		/**< Status to apply content restriction */
	videoRect mVideoRect;
	std::unique_ptr<char[]> mData;
	std::string mTextStyle;
	std::vector<StreamBlacklistProfileInfo> mBlacklistedProfiles;
	//std::vector<ProfilerBucketType> cachedMediaBucketTypes;
	AampMPDDownloader *mMPDDownloaderInstance;

	std::atomic<double> m_PTSOffsetFromTune {0.}; /**< PTS offset cache from Tune, is reset only on a `Stop` */

	std::mutex  mDiscoCompleteLock; 					/**< Lock the period jump if discontinuity already in progress */
	std::condition_variable mWaitForDiscoToComplete;	/**< Conditional wait for period jump */
	bool mIsPeriodChangeMarked; 						/**< Mark if a period change occurred */

	std::string mSessionId;		/**< ID of the current session as set by the player */
	AampTSBSessionManager *mTSBSessionManager;
	bool mLocalAAMPInjectionEnabled;					/**< Injecting segments from AAMP Local TSB */
	bool mLocalAAMPTsb;									/**< AAMP Local TSB enabled for the current channel
															(localTSBEnabled and enablePTSReStamp enabled, and playing linear DASH content) */
	bool mbPauseOnStartPlayback;						/**< Start playback in paused state */

	std::mutex mPreProcessLock;
	bool mIsChunkMode;		/** LLD ChunkMode */
	bool mLocalAAMPTsbFromConfig;						/**< AAMP TSB enabled in the configuration, regardless of the current channel */

private:
	void _SetCMCDTrackData(AampMediaType mediaType);
	std::vector<float> _getSupportedPlaybackSpeeds(void);
	bool _IsFogUrl(const char *mainManifestUrl);
};
#endif // PRIVAAMP_H
