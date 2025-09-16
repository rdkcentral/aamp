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

#ifndef __AAMP_DEFINE_H__
#define __AAMP_DEFINE_H__

/**
 * @file AampDefine.h
 * @brief Macros for Aamp
 */

#include <limits.h>

#define AAMP_CFG_PATH "/opt/aamp.cfg"
#define AAMP_JSON_PATH "/opt/aampcfg.json"

#define AAMP_VERSION "7.08"
#define AAMP_TUNETIME_VERSION 6

//Stringification of Macro : use two levels of macros
#define MACRO_TO_STRING(s) X_STR(s)
#define X_STR(s) #s

#define GST_VIDEOBUFFER_SIZE_BYTES_BASE 5242880
#define GST_AUDIOBUFFER_SIZE_BYTES_BASE 512000

#define GST_VIDEOBUFFER_SIZE_BYTES (GST_VIDEOBUFFER_SIZE_BYTES_BASE*3)
#define GST_AUDIOBUFFER_SIZE_BYTES (GST_AUDIOBUFFER_SIZE_BYTES_BASE*3)

#define GST_BW_TO_BUFFER_FACTOR 0.80			/**< Bandwidth to buffer factor to calculate new GST Buffer Size to accommodate larger Video Buffers*/
#define GST_VIDEOBUFFER_SIZE_MAX_BYTES 26214400			/**< 25*1024*1024 , Upper limit for HiFi Content */

#define DEFAULT_ENCODED_CONTENT_BUFFER_SIZE (512*1024)		/**< 512KB buffer is allocated for a content encoded curl download to minimize buffer reallocation*/
#define MAX_PTS_ERRORS_THRESHOLD 20
#define DEFAULT_PTS_ERRORS_THRESHOLD 4
#define DEFAULT_WAIT_TIME_BEFORE_RETRY_HTTP_5XX_MS (1000)  	/**< Wait time in milliseconds before retry for 5xx errors */
#define MAX_PLAYLIST_CACHE_SIZE    (3*1024) 			/**< Approx 3MB -> 2 video profiles + one audio profile + one iframe profile, 500-700K MainManifest */

#define DEFAULT_ABR_CACHE_LIFE 5000                 		/**< Default ABR cache life  in milliseconds*/
#define DEFAULT_ABR_OUTLIER 5000000                 		/**< ABR outlier: 5 MB */
#define DEFAULT_ABR_SKIP_DURATION 6          		        /**< Initial skip duration of ABR - 6 sec */
#define DEFAULT_ABR_NW_CONSISTENCY_CNT 2            		/**< ABR network consistency count */
#define DEFAULT_BUFFER_HEALTH_MONITOR_DELAY 10
#define DEFAULT_BUFFER_HEALTH_MONITOR_INTERVAL 5
#define DEFAULT_ABR_CACHE_LENGTH 3                  		/**< Default ABR cache length */
#define DEFAULT_ABR_BUFFER_COUNTER 4				/**< Default ABR Buffer Counter */
#define DEFAULT_REPORT_PROGRESS_INTERVAL 1     			/**< Progress event reporting interval: 1sec */
#define DEFAULT_PROGRESS_LOGGING_DIVISOR 4			/**< Divisor of progress logging frequency to print logging */
#define DEFAULT_LICENSE_REQ_RETRY_WAIT_TIME 500			/**< Wait time in milliseconds before retrying for DRM license */
#define MIN_LICENSE_KEY_ACQUIRE_WAIT_TIME 500			/**<minimum wait time in milliseconds for DRM license to ACQUIRE */
#define DEFAULT_LICENSE_KEY_ACQUIRE_WAIT_TIME 5000		/**< Wait time in milliseconds for DRM license to ACQUIRE  */
#define MAX_LICENSE_ACQ_WAIT_TIME 12000  			/**< 12 secs Increase from 10 to 12 sec */
#define DEFAULT_INIT_BITRATE     2500000            		/**< Initial bitrate: 2.5 mb - for non-4k playback */
#define DEFAULT_BITRATE_OFFSET_FOR_DOWNLOAD 500000		/**< Offset in bandwidth window for checking buffer download expiry */
#define DEFAULT_INIT_BITRATE_4K 13000000            		/**< Initial bitrate for 4K playback: 13mb ie, 3/4 profile */
#define AAMP_LIVE_OFFSET 15             			/**< Live offset in seconds */
#define AAMP_DEFAULT_LIVE_OFFSET_DRIFT (1)          /**< Default value of allowed live offset drift **/
#define AAMP_DEFAULT_PLAYBACK_OFFSET -99999            		/**< default 'unknown' offset value */
#define AAMP_CDVR_LIVE_OFFSET 30        			/**< Live offset in seconds for CDVR hot recording */
#define MIN_DASH_DRM_SESSIONS 3
#define DEFAULT_DRM_NETWORK_TIMEOUT 5                           /** < default value for drmNetworkTimeout  - 5 sec */
#define DEFAULT_CACHED_FRAGMENTS_PER_TRACK  4       		/**< Default cached fragments per track */
#define TRICKPLAY_VOD_PLAYBACK_FPS 4            		/**< Frames rate for trickplay from CDN server */
#define TRICKPLAY_LINEAR_PLAYBACK_FPS 8                		/**< Frames rate for trickplay from TSB */
#define DEFAULT_DOWNLOAD_RETRY_COUNT (1)			/**< max download failure retry attempt count */
#define DEFAULT_FRAGMENT_DOWNLOAD_502_RETRY_COUNT (1) /**< max fragment download failure retry attempt count for 502 error */
#define MANIFEST_DOWNLOAD_502_RETRY_COUNT (10) /**< max manifest download failure retry attempt count for 502 error */
#define DEFAULT_DISCONTINUITY_TIMEOUT 3000          		/**< Default discontinuity timeout after cache is empty in MS */
#define CURL_FRAGMENT_DL_TIMEOUT 10L    			/**< Curl timeout for fragment download */
#define DEFAULT_STALL_ERROR_CODE (7600)             		/**< Default stall error code: 7600 */
#define DEFAULT_STALL_DETECTION_TIMEOUT (10000)     		/**< Stall detection timeout: 10000 millisec */
#define DEFAULT_MINIMUM_INIT_CACHE_SECONDS  0        		/**< Default initial cache size of playback */
#define DEFAULT_MAXIMUM_PLAYBACK_BUFFER_SECONDS 30   		/**< Default maximum playback buffer size */
#define DEFAULT_TIMEOUT_FOR_SOURCE_SETUP (1000) 		/**< Default timeout value in milliseconds */
#define MAX_SEG_DRM_DECRYPT_FAIL_COUNT 10           		/**< Max segment decryption failures to identify a playback failure. */
#define MAX_SEG_INJECT_FAIL_COUNT 10                		/**< Max segment injection failure to identify a playback failure. */
#define AAMP_USERAGENT_BASE_STRING	"Mozilla/5.0 (Linux; x86_64 GNU/Linux) AppleWebKit/601.1 (KHTML, like Gecko) Version/8.0 Safari/601.1 WPE"	/**< Base User agent string,it will be appended with AAMP_USERAGENT_SUFFIX */
#define AAMP_USERAGENT_SUFFIX		" AAMP/" AAMP_VERSION    /**< Version string of AAMP Player */
#define AAMP_USERAGENT_STRING AAMP_USERAGENT_BASE_STRING AAMP_USERAGENT_SUFFIX
#define DEFAULT_AAMP_ABR_THRESHOLD_SIZE (6000)			/**< aamp abr threshold size */
#define DEFAULT_PREBUFFER_COUNT (2)			/**< Count of video segments to be downloaded until play state */
#define AAMP_LOW_BUFFER_BEFORE_RAMPDOWN 6 		/**< 6sec buffer before rampdown */
#define AAMP_HIGH_BUFFER_BEFORE_RAMPUP  10 		/**< 10sec buffer before rampup */
#define MAX_DASH_DRM_SESSIONS 30
#define MAX_AD_SEG_DOWNLOAD_FAIL_COUNT 2            		/**< Max Ad segment download failures to identify as the ad playback failure. */
#define FRAGMENT_DOWNLOAD_WARNING_THRESHOLD 2000    		/**< MAX Fragment download threshold time in Msec*/
#define BITRATE_ALLOWED_VARIATION_BAND 100000       		/**< NW BW change beyond this will be ignored */
#define MAX_DIFF_BETWEEN_PTS_POS_MS (3600*1000)
#define MAX_SEG_DOWNLOAD_FAIL_COUNT 10              		/**< Max segment download failures to identify a playback failure. */
#define MAX_DOWNLOAD_DELAY_LIMIT_MS 30000
#define MAX_ERROR_DESCRIPTION_LENGTH 128
#define MAX_ANOMALY_BUFF_SIZE   256
#define MAX_WAIT_TIMEOUT_MS	200				/**< Max Timeout duration for wait until cache is available to inject next*/
#define MAX_INIT_FRAGMENT_CACHE_PER_TRACK  5       		/**< Max No Of cached Init fragments per track */
#define MIN_SEG_DURATION_THRESHOLD	(0.25)			/**< Min Segment Duration threshold for pushing to pipeline at period End*/
#define MAX_CURL_SOCK_STORE		10			/**< Maximum no of host to be maintained in curl store*/
#define DEFAULT_AD_FULFILLMENT_TIMEOUT 2000	/**< Default Ad fulfillment timeout in milliseconds */
#define MAX_AD_FULFILLMENT_TIMEOUT 5000	/**< Max Ad fulfillment timeout in milliseconds */

#define AAMP_TRACK_COUNT 4		/**< internal use - audio+video+sub+aux track */
#define DEFAULT_CURL_INSTANCE_COUNT (AAMP_TRACK_COUNT + 1) /**< One for Manifest/Playlist + Number of tracks */
#define AAMP_DRM_CURL_COUNT 4		/**< audio+video+sub+aux track DRMs */
//#define CURL_FRAGMENT_DL_TIMEOUT 10L	/**< Curl timeout for fragment download */
#define DEFAULT_PLAYLIST_DL_TIMEOUT 10L	/**< Curl timeout for playlist download */
#define DEFAULT_CURL_TIMEOUT 5L		/**< Default timeout for Curl downloads */
#define DEFAULT_CURL_CONNECTTIMEOUT 3L	/**< Curl socket connection timeout */
#define EAS_CURL_TIMEOUT 3L		/**< Curl timeout for EAS manifest downloads */
#define EAS_CURL_CONNECTTIMEOUT 2L      /**< Curl timeout for EAS connection */
#define DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS (6*1000)   /**< Interval between playlist refreshes */
#define DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS 3000
#define MAX_DELAY_BETWEEN_MPD_UPDATE_MS (6000)
#define MIN_DELAY_BETWEEN_MPD_UPDATE_MS (500) // 500mSec
#define SAFE_LATENCY_VALUE_FOR_SLOW_REFRESH (20000) // 20 sec
#define MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (6000)
#define MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS (500) // 500mSec
#define MIN_DELAY_BETWEEN_MANIFEST_UPDATE_FOR_502_MS (1000) // 1000mSec
#define STEADYSTATE_RAMPDOWN_DELTA 2000000 //2000 kbps
#define DEFAULT_TELEMETRY_REPORT_INTERVAL (300) 	/**< time interval for the telemetry reporting 300sec*/
#define MIN_MONITOR_AVSYNC_POSITIVE_DELTA_MS 1 /*< minimum positive delta to trigger AVSync reporting */
#define MAX_MONITOR_AVSYNC_POSITIVE_DELTA_MS 10000 /*< maximum positive delta to trigger AVSync reporting */
#define DEFAULT_MONITOR_AVSYNC_POSITIVE_DELTA_MS 100 /*< default positive delta to trigger AVSync reporting */
#define MIN_MONITOR_AVSYNC_NEGATIVE_DELTA_MS -10000 /*< minimum negative delta to trigger AVSync reporting */
#define MAX_MONITOR_AVSYNC_NEGATIVE_DELTA_MS -1 /*< maximum negative delta to trigger AVSync reporting */
#define DEFAULT_MONITOR_AVSYNC_NEGATIVE_DELTA_MS -100 /*< default negative delta to trigger AVSync reporting */
#define MIN_MONITOR_AV_JUMP_THRESHOLD_MS 1 	/**< minimum  jump threshold to trigger MonitorAV reporting */
#define MAX_MONITOR_AV_JUMP_THRESHOLD_MS 10000 	/**< maximum jump threshold to trigger MonitorAV reporting */
#define DEFAULT_MONITOR_AV_JUMP_THRESHOLD_MS 100 	/**< default jump threshold to MonitorAV reporting */
#define DEFAULT_MONITOR_AV_REPORTING_INTERVAL 1000 /**< time interval in ms for MonitorAV reporting */

// We can enable the following once we have a thread monitoring video PTS progress and triggering subtec clock fast update when we detect video freeze. Disabled it for now for brute force fast refresh..
//#define SUBTEC_VARIABLE_CLOCK_UPDATE_RATE   /* enable this to make the clock update rate dynamic*/
#ifdef SUBTEC_VARIABLE_CLOCK_UPDATE_RATE
 #define INITIAL_SUBTITLE_CLOCK_SYNC_INTERVAL_MS (500)     /**< default time interval for the subtitle clock sync 500ms*/
 #define DEFAULT_SUBTITLE_CLOCK_SYNC_INTERVAL_S  (30)      /**< default time interval for the subtitle clock sync 30sec*/
#else
 #define DEFAULT_SUBTITLE_CLOCK_SYNC_INTERVAL_S    (1)     /**< default time interval for the subtitle clock sync 1sec*/
#endif
#define SUBTITLE_CLOCK_ASSUMED_PLAYSTATE_TIME_MS (20000) /**< period after channel change/seek where we try to sync the subtitle clock quickly, before giving up and falling to slower rate */

#define DEFAULT_THUMBNAIL_TILE_ROWS 1		/**< default number of rows for thumbnail if not present in manifest*/
#define DEFAULT_THUMBNAIL_TILE_COLUMNS 1	/**< default number of columns for thumbnail if not present in manifest*/
#define DEFAULT_THUMBNAIL_TILE_DURATION 10.0f	/**< default tile duration of thumbnail if not present in manifest in seconds*/


#define AAMP_NORMAL_PLAY_RATE		1
#define AAMP_SLOWMOTION_RATE        0.5
#define AAMP_RATE_PAUSE			0
#define AAMP_RATE_INVALID		INT_MAX

// Defines used for PauseAt functionality
#define AAMP_PAUSE_POSITION_POLL_PERIOD_MS		(250)
#define AAMP_PAUSE_POSITION_INVALID_POSITION	(-1)

#define STRLEN_LITERAL(STRING) (sizeof(STRING)-1)
#define STARTS_WITH_IGNORE_CASE(STRING, PREFIX) (0 == strncasecmp(STRING, PREFIX, STRLEN_LITERAL(PREFIX)))

#define MAX_GST_VIDEO_BUFFER_BYTES			(GST_VIDEOBUFFER_SIZE_BYTES)
#define MAX_GST_AUDIO_BUFFER_BYTES			(GST_AUDIOBUFFER_SIZE_BYTES)

#define DEFAULT_LATENCY_MONITOR_DELAY			9					/**< Latency Monitor Delay */
#define DEFAULT_LATENCY_MONITOR_INTERVAL		6					/**< Latency monitor Interval */
#define DEFAULT_MIN_LOW_LATENCY			3					/**< min Default Latency */
#define DEFAULT_MAX_LOW_LATENCY			9					/**< max Default Latency */
#define DEFAULT_TARGET_LOW_LATENCY			6					/**< Target Default Latency */
#define DEFAULT_MIN_RATE_CORRECTION_SPEED		0.97f		/**< min Rate correction speed */
#define DEFAULT_MAX_RATE_CORRECTION_SPEED		1.03f		/**< max Rate correction speed */
#define DEFAULT_NORMAL_RATE_CORRECTION_SPEED    1.00f	   	/**< Live Catchup Normal play rate */
#define AAMP_LLD_LATENCY_MONITOR_INTERVAL 		(1)   		/**< Latency monitor interval for LLD*/
#define AAMP_LLD_MINIMUM_CACHE_SEGMENTS 		(2)     	/**< Number of segments to be cached minimum before rate change*/
#define AAMP_LLD_LOW_BUFF_CHECK_COUNT           (4)         /**< Count to confirm low buffer state for LLD stream playback; 4 sec to ABR; So Allow ABR first*/
#define DEFAULT_MIN_BUFFER_LOW_LATENCY          (2.0f)      /**< Default minimum buffer for Low latency stream*/
#define DEFAULT_TARGET_BUFFER_LOW_LATENCY       (4.0f)      /**< Default minimum buffer for Low latency stream*/
#define DEFAULT_ALLOWED_DELAY_LOW_LATENCY       (2.5f)      /**< Default allowed server delay for Low latency stream*/

#define AAMP_BUFFER_MONITOR_GREEN_THRESHOLD 4               /**< 2 fragments for MSO specific linear streams. */
#define AAMP_BUFFER_MONITOR_GREEN_THRESHOLD_LLD 1           /**< LLD 1 sec minimum buffer to alert */

#define AAMP_FOG_TSB_URL_KEYWORD "tsb?" /**< AAMP expect this keyword in URL to identify it is FOG url */

#define DEFAULT_INITIAL_RATE_CORRECTION_SPEED 1.000001f	/**< Initial rate correction speed to avoid audio drop */
#define DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK	20					/**< Default cached fragment chunks per track */
#define DEFAULT_ABR_CHUNK_CACHE_LENGTH			10					/**< Default ABR chunk cache length */
#define DEFAULT_AAMP_ABR_CHUNK_THRESHOLD_SIZE		(DEFAULT_AAMP_ABR_THRESHOLD_SIZE)	/**< aamp abr Chunk threshold size */
#define DEFAULT_ABR_CHUNK_SPEEDCNT			10					/**< Chunk Speed Count Store Size */
#define DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE		100					/**< Duration(ms) to check Chunk Speed */
#define AAMP_LLDABR_MIN_BUFFER_VALUE			0.5f                  /** 0.5 sec */
#define DEFAULT_ABR_BYTES_TRANSFERRED_FOR_ESTIMATE	(512 * 1024)				/**< 512K */
#define MAX_MDAT_NOT_FOUND_COUNT			500					/**< Max MDAT not found count*/
#define DEFAULT_CONTENT_PROTECTION_DATA_UPDATE_TIMEOUT	5000					/**< Default Timeout for Content Protection Data Update on Dynamic Key Rotation */

// Player configuration for Fog download
#define FOG_MAX_CONCURRENT_DOWNLOADS			4					/**< Max concurrent downloads in Fog*/

#define AAMP_MAX_EVENT_PRIORITY (-70) 	/**< Maximum allowed priority value for events */
#define AAMP_TASK_ID_INVALID 0

// weights used for audio/subtitle track-selection heuristic
#define AAMP_LANGUAGE_SCORE 1000000000ULL  /**< Top priority:  matching language **/
#define AAMP_SCHEME_ID_SCORE 100000000ULL  /**< 2nd priority to scheme id matching **/
#define AAMP_LABEL_SCORE 10000000ULL       /**< 3rd priority to  label matching **/
#define AAMP_ROLE_SCORE 1000000ULL         /**< 4th priority to role/rendition matching **/
#define AAMP_TYPE_SCORE 100000ULL          /**< 5th priority to type matching **/
#define AAMP_CODEC_SCORE 1000ULL           /**< Lowest priority: matching codec **/
#define THRESHOLD_TOIGNORE_TINYPERIOD 500  /**<in milliseconds**/


// LLD TSB Defaults
#define DEFAULT_MIN_TSB_STORAGE_FREE_PERCENTAGE 10	// Percentage of free space in TSB 
#define DEFAULT_MAX_TSB_STORAGE_MB				10*1024	// 10 GiB 
#ifdef USE_TSBCONFIG_FOR_HYBRID		// for devices with more generous storage mounted at /opt/data 
#define DEFAULT_TSB_DURATION 3600
#define DEFAULT_TSB_LOCATION "/opt/data/fog/aamp"
#else
#define DEFAULT_TSB_DURATION 1500
#define DEFAULT_TSB_LOCATION "/tmp/data/fog/aamp"
#endif
#define FIRST_PLAYER_INSTANCE_ID (0) /** Indicate fist player Id */

#define MAX_SESSION_ID_LENGTH 128                                /**<session id string length */

#define PLAYER_NAME "aamp" 

/**
 * @brief Enumeration for TUNED Event Configuration
 */
enum TunedEventConfig
{
        eTUNED_EVENT_ON_PLAYLIST_INDEXED,           /**< Send TUNED event after playlist indexed*/
        eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED,   /**< Send TUNED event after first fragment decryption*/
        eTUNED_EVENT_ON_GST_PLAYING,                /**< Send TUNED event on gstreamer's playing event*/
        eTUNED_EVENT_MAX
};

/**
 * @brief Enumeration for Paused state behavior
 */
enum PausedBehavior
{
	ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE,            /**< automatically begin playback from eldest portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_IMMEDIATE,                /**< automatically jump to live*/
	ePAUSED_BEHAVIOR_AUTOPLAY_DEFER,                /**< video remains paused indefinitely till play() call, resume playback from new start portion of live window*/
	ePAUSED_BEHAVIOR_LIVE_DEFER,                    /**< video remains paused indefinitely till play() call, resume playback from live position*/
	ePAUSED_BEHAVIOR_MAX
};

/**
 * @brief AAMP Config Ownership values
 */

typedef enum
{
	AAMP_DEFAULT_SETTING            = 0,        /**< Lowest priority */
	AAMP_OPERATOR_SETTING           = 1,
	AAMP_STREAM_SETTING             = 2,
	AAMP_APPLICATION_SETTING        = 3,
	AAMP_TUNE_SETTING        	= 4,
	AAMP_DEV_CFG_SETTING            = 5,
	AAMP_CUSTOM_DEV_CFG_SETTING     = 6,		/**< Highest priority */
	AAMP_MAX_SETTING
}ConfigPriority;

/**
 * @brief Latency status
 */
enum LatencyStatus
{
	LATENCY_STATUS_UNKNOWN=-1,     /**< The latency is Unknown */
	LATENCY_STATUS_MIN,            /**< The latency is within range but less than minimum latency */
	LATENCY_STATUS_THRESHOLD_MIN,  /**< The latency is within range but less than target latency but greater than minimum latency */
	LATENCY_STATUS_THRESHOLD,      /**< The latency is equal to given latency from mpd */
	LATENCY_STATUS_THRESHOLD_MAX,  /**< The latency is more that target latency but less than maximum latency */
	LATENCY_STATUS_MAX             /**< The latency is more than maximum latency */
};

/**
 * @brief AAMP Function return values
 */
enum AAMPStatusType
{
	eAAMPSTATUS_OK,					/**< Aamp Status ok */
	eAAMPSTATUS_FAKE_TUNE_COMPLETE,			/**< Fake tune completed */
	eAAMPSTATUS_GENERIC_ERROR,			/**< Aamp General Error */
	eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR,		/**< Manifest download failed */
	eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR,	/**< Video download failed */
	eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR,	/**< Audio download failed */
	eAAMPSTATUS_MANIFEST_PARSE_ERROR,		/**< Manifest parse failed */
	eAAMPSTATUS_MANIFEST_CONTENT_ERROR,		/**< Manifest content is unknown or Error */
	eAAMPSTATUS_MANIFEST_INVALID_TYPE,		/**< Invalid manifest type */
	eAAMPSTATUS_PLAYLIST_PLAYBACK,			/**< Playlist play back happening */
	eAAMPSTATUS_SEEK_RANGE_ERROR,			/**< Seek position range invalid */
	eAAMPSTATUS_TRACKS_SYNCHRONIZATION_ERROR,	/**< Audio video track synchronization Error */
	eAAMPSTATUS_INVALID_PLAYLIST_ERROR,		/**< Playlist discontinuity mismatch*/
	eAAMPSTATUS_UNSUPPORTED_DRM_ERROR		/**< Unsupported DRM */
};


/**
 *
 * @enum UTC TIMING
 *
 */
enum UtcTiming
{
    eUTC_HTTP_INVALID,
    eUTC_HTTP_XSDATE,
    eUTC_HTTP_ISO,
    eUTC_HTTP_NTP
};

/**
 * @brief MPD Stich Options
 */
enum MPDStichOptions
{
	OPT_1_FULL_MANIFEST_TUNE = 0,     /**< Tune with full manifest URL and stich small content manifest */
	OPT_2_SMALL_MANIFEST_TUNE = 1,     /**< Tune with small manifest URL and stich full content manifest */
};

enum DiagnosticsOverlayOptions
{
	eDIAG_OVERLAY_NONE = 0,       // No diagnostics overlay
	eDIAG_OVERLAY_MINIMAL = 1,    // Shows overlay widget
	eDIAG_OVERLAY_EXTENDED = 2    // Shows overlay widget + anomaly widget
};

/**
 * @brief Enumeration for Absolute Progress Reporting Format
 */
enum AbsoluteProgressReportFormat
{
    eABSOLUTE_PROGRESS_EPOCH,                       /**< Report epoch value including AvailabilityStartTime*/
    eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START,  /**< Exclude AvailabilityStartTime from progress reporting for linear*/
    eABSOLUTE_PROGRESS_MAX
};

enum EOSInjectionModeCode
{
	/* EOS events are only injected into the gstreamer pipeline by
	 * AAMPGstPlayer::EndOfStreamReached() & AAMPGstPlayer::Discontinuity().*/
	EOS_INJECTION_MODE_NO_EXTRA,

	/* In addition to the EOS_INJECTION_MODE_NO_EXTRA cases
	 * EOS is injected in AAMPGstPlayer::Stop() prior to setting the state to null.*/
	EOS_INJECTION_MODE_STOP_ONLY,
};

#endif

