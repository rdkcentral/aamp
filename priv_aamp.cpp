/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file priv_aamp.cpp
 * @brief Advanced Adaptive Media Player (AAMP) PrivateInstanceAAMP impl
 */
#include "isobmffprocessor.h"
#include "priv_aamp.h"
#include "AampJsonObject.h"
#include "isobmffbuffer.h"
#include "AampConstants.h"
#include "AampCacheHandler.h"
#include "AampUtils.h"
#include "PlayerExternalsInterface.h"
#include "iso639map.h"
#include "fragmentcollector_mpd.h"
#include "admanager_mpd.h"
#include "fragmentcollector_hls.h"
#include "fragmentcollector_progressive.h"
#include "MediaStreamContext.h"
#include "hdmiin_shim.h"
#include "compositein_shim.h"
#include "ota_shim.h"
#include "rmf_shim.h"
#include "_base64.h"
#include "base16.h"
#include "aampgstplayer.h"
#include "AampStreamSinkManager.h"
#include "SubtecFactory.hpp"
#include "AampGrowableBuffer.h"

#include "PlayerCCManager.h"
#include "AampDRMLicPreFetcher.h"
#include "AampDRMLicManager.h"

#ifdef AAMP_TELEMETRY_SUPPORT
#include <AampTelemetry2.hpp>
#endif //AAMP_TELEMETRY_SUPPORT

#include "ID3Metadata.hpp"
#include "AampSegmentInfo.hpp"

#include "AampCurlStore.h"

#include <iomanip>
#include <unordered_set>

#include <sys/time.h>
#include <cmath>
#include <regex>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <string.h>
#include "AampCurlDownloader.h"
#include "AampMPDDownloader.h"
#include <sched.h>
#include "AampTSBSessionManager.h"
#include "SocUtils.h"
#include "AuthTokenErrors.h"

#define LOCAL_HOST_IP       "127.0.0.1"
#define AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS (20*1000LL)
#define AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS (AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS/10)

//Description size
#define MAX_DESCRIPTION_SIZE 128

//Stringification of Macro :  use two levels of macros
#define MACRO_TO_STRING(s) X_STR(s)
#define X_STR(s) #s

// Uncomment to test GetMediaFormatType without locator inspection
#define TRUST_LOCATOR_EXTENSION_IF_PRESENT

#define VALIDATE_INT(param_name, param_value, default_value)        \
	if ((param_value <= 0) || (param_value > INT_MAX))  { \
		AAMPLOG_WARN("Parameter '%s' not within INTEGER limit. Using default value instead.", param_name); \
		param_value = default_value; \
	}

#define VALIDATE_LONG(param_name, param_value, default_value)        \
	if ((param_value <= 0) || (param_value > LONG_MAX))  { \
		AAMPLOG_WARN("Parameter '%s' not within LONG INTEGER limit. Using default value instead.", param_name); \
		param_value = default_value; \
	}

#define VALIDATE_DOUBLE(param_name, param_value, default_value)        \
	if ((param_value <= 0) || (param_value > DBL_MAX))  { \
		AAMPLOG_WARN("Parameter '%s' not within DOUBLE limit. Using default value instead.", param_name); \
		param_value = default_value; \
	}

#define FOG_REASON_STRING			"Fog-Reason:"
#define CURLHEADER_X_REASON			"X-Reason:"
#define BITRATE_HEADER_STRING		"X-Bitrate:"
#define CONTENTLENGTH_STRING		"Content-Length:"
#define SET_COOKIE_HEADER_STRING	"Set-Cookie:"
#define LOCATION_HEADER_STRING		"Location:"
#define CONTENT_ENCODING_STRING		"Content-Encoding:"
#define FOG_RECORDING_ID_STRING		"Fog-Recording-Id:"
#define CAPPED_PROFILE_STRING 		"Profile-Capped:"
#define TRANSFER_ENCODING_STRING		"Transfer-Encoding:"

/**
 * @struct gActivePrivAAMP_t
 * @brief Used for storing active PrivateInstanceAAMPs
 */
struct gActivePrivAAMP_t
{
	PrivateInstanceAAMP* pAAMP;
	bool reTune;
	int numPtsErrors;
};

static std::list<gActivePrivAAMP_t> gActivePrivAAMPs = std::list<gActivePrivAAMP_t>();
static std::mutex gMutex;
static std::condition_variable gCond;

static int PLAYERID_CNTR = 0;

static const char* strAAMPPipeName = "/tmp/ipc_aamp";

static bool activeInterfaceWifi = false;

std::shared_ptr<PlayerExternalsInterface> pPlayerExternalsInterface = NULL;

static unsigned int ui32CurlTrace = 0;

/**
 * @struct CurlCbContextSyncTime
 * @brief context during curl callbacks
 */
struct CurlCbContextSyncTime
{
	PrivateInstanceAAMP *aamp;
	AampGrowableBuffer *buffer;

	CurlCbContextSyncTime() : aamp(NULL), buffer(NULL){}
	CurlCbContextSyncTime(PrivateInstanceAAMP *_aamp, AampGrowableBuffer *_buffer) : aamp(_aamp),buffer(_buffer){}
	~CurlCbContextSyncTime() {}

	CurlCbContextSyncTime(const CurlCbContextSyncTime &other) = delete;
	CurlCbContextSyncTime& operator=(const CurlCbContextSyncTime& other) = delete;
};

/**
 * @struct TuneFailureMap
 * @brief  Structure holding aamp tune failure code and corresponding application error code and description
 */
struct TuneFailureMap
{
    AAMPTuneFailure tuneFailure;    /**< Failure ID */
    int code;                       /**< Error code */
    const char* description;        /**< Textual description */
};

static TuneFailureMap tuneFailureMap[] =
{
	{AAMP_TUNE_INIT_FAILED, 10, "AAMP: init failed"}, //"Fragmentcollector initialization failed"
	{AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR, 10, "AAMP: init failed (unable to download manifest)"},
	{AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR, 10, "AAMP: init failed (manifest missing tracks)"},
	{AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR, 10, "AAMP: init failed (corrupt/invalid manifest)"},
	{AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR, 10, "AAMP: init failed (unable to download video playlist)"},
	{AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR, 10, "AAMP: init failed (unable to download audio playlist)"},
	{AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR, 10, "AAMP: init failed (unsynchronized tracks)"},
	{AAMP_TUNE_MANIFEST_REQ_FAILED, 10, "AAMP: Manifest Download failed"}, //"Playlist refresh failed"
	{AAMP_TUNE_AUTHORIZATION_FAILURE, 40, "AAMP: Authorization failure"},
	{AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, 10, "AAMP: fragment download failures"},
	{AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, 10, "AAMP: init fragment download failed"},
	{AAMP_TUNE_UNTRACKED_DRM_ERROR, 50, "AAMP: DRM error untracked error"},
	{AAMP_TUNE_DRM_INIT_FAILED, 50, "AAMP: DRM Initialization Failed"},
	{AAMP_TUNE_DRM_DATA_BIND_FAILED, 50, "AAMP: InitData-DRM Binding Failed"},
	{AAMP_TUNE_DRM_SESSIONID_EMPTY, 50, "AAMP: DRM Session ID Empty"},
	{AAMP_TUNE_DRM_CHALLENGE_FAILED, 50, "AAMP: DRM License Challenge Generation Failed"},
	{AAMP_TUNE_LICENCE_TIMEOUT, 50, "AAMP: DRM License Request Timed out"},
	{AAMP_TUNE_LICENCE_REQUEST_FAILED, 50, "AAMP: DRM License Request Failed"},
	{AAMP_TUNE_INVALID_DRM_KEY, 50, "AAMP: Invalid Key Error, from DRM"},
	{AAMP_TUNE_UNSUPPORTED_STREAM_TYPE, 60, "AAMP: Unsupported Stream Type"}, //"Unable to determine stream type for DRM Init"
	{AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE, 60, "AAMP: No supported Audio Types in Manifest"},
	{AAMP_TUNE_FAILED_TO_GET_KEYID, 50, "AAMP: Failed to parse key id from PSSH"},
	{AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN, 50, "AAMP: Failed to get access token from Auth Service"},
	{AAMP_TUNE_CORRUPT_DRM_DATA, 51, "AAMP: DRM failure due to Corrupt DRM files"},
	{AAMP_TUNE_CORRUPT_DRM_METADATA, 50, "AAMP: DRM failure due to Bad DRMMetadata in stream"},
	{AAMP_TUNE_DRM_DECRYPT_FAILED, 50, "AAMP: DRM Decryption Failed for Fragments"},
	{AAMP_TUNE_DRM_UNSUPPORTED, 50, "AAMP: DRM format Unsupported"},
	{AAMP_TUNE_DRM_SELF_ABORT, 50, "AAMP: DRM license request aborted by player"},
	{AAMP_TUNE_GST_PIPELINE_ERROR, 80, "AAMP: Error from gstreamer pipeline"},
	{AAMP_TUNE_PLAYBACK_STALLED, 7600, "AAMP: Playback was stalled due to lack of new fragments"},
	{AAMP_TUNE_CONTENT_NOT_FOUND, 20, "AAMP: Resource was not found at the URL(HTTP 404)"},
	{AAMP_TUNE_DRM_KEY_UPDATE_FAILED, 50, "AAMP: Failed to process DRM key"},
	{AAMP_TUNE_DEVICE_NOT_PROVISIONED, 52, "AAMP: Device not provisioned"},
	{AAMP_TUNE_HDCP_COMPLIANCE_ERROR, 53, "AAMP: HDCP Compliance Check Failure"},
	{AAMP_TUNE_INVALID_MANIFEST_FAILURE, 10, "AAMP: Invalid Manifest, parse failed"},
	{AAMP_TUNE_FAILED_PTS_ERROR, 80, "AAMP: Playback failed due to PTS error"},
	{AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING, 10, "AAMP: init fragments missing in playlist"},
	{AAMP_TUNE_FAILURE_UNKNOWN, 100, "AAMP: Unknown Failure"}
};

static const std::pair<std::string , std::string> gCDAIErrorDetails[] = {
	{"1051-2", "A configuration issue prevents player from handling ads"},
	{"1051-6", "An ad was unplayable due to invalid manifest/playlist formatting."},
	{"1051-7", "An ad was unplayable due to invalid media."},
	{"1051-8", "An ad was unplayable due to the content being out of spec and unInsertable."},
	{"1051-11", "The ad decisioning service took too long to respond."},
	{"1051-12", "The ad delivery service took too long to respond."},
	{"1051-13", "The ad delivery service returned a HTTP error."},
	{"1051-14", "The ad delivery service returned a error."},
	{"1051-15", "An unknown error occurred when trying to insert an ad."},
	{"", ""}
};

static constexpr const char *BITRATECHANGE_STR[] =
{
	(const char *)"BitrateChanged - Network adaptation",				// eAAMP_BITRATE_CHANGE_BY_ABR
	(const char *)"BitrateChanged - Rampdown due to network failure",		// eAAMP_BITRATE_CHANGE_BY_RAMPDOWN
	(const char *)"BitrateChanged - Reset to default bitrate due to tune",		// eAAMP_BITRATE_CHANGE_BY_TUNE
	(const char *)"BitrateChanged - Reset to default bitrate due to seek",		// eAAMP_BITRATE_CHANGE_BY_SEEK
	(const char *)"BitrateChanged - Reset to default bitrate due to trickplay",	// eAAMP_BITRATE_CHANGE_BY_TRICKPLAY
	(const char *)"BitrateChanged - Rampup since buffers are full",			// eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL
	(const char *)"BitrateChanged - Rampdown since buffers are empty",		// eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY
	(const char *)"BitrateChanged - Network adaptation by FOG",			// eAAMP_BITRATE_CHANGE_BY_FOG_ABR
	(const char *)"BitrateChanged - Information from OTA",                          // eAAMP_BITRATE_CHANGE_BY_OTA
	(const char *)"BitrateChanged - Video stream information from HDMIIN",          // eAAMP_BITRATE_CHANGE_BY_HDMIIN
	(const char *)"BitrateChanged - Unknown reason"					// eAAMP_BITRATE_CHANGE_MAX
};

#define BITRATEREASON2STRING(id) BITRATECHANGE_STR[id]

static constexpr const char *ADEVENT_STR[] =
{
	(const char *)"AAMP_EVENT_AD_RESERVATION_START",
	(const char *)"AAMP_EVENT_AD_RESERVATION_END",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_START",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_END",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_ERROR",
	(const char *)"AAMP_EVENT_AD_PLACEMENT_PROGRESS"
};

#define ADEVENT2STRING(id) ADEVENT_STR[id - AAMP_EVENT_AD_RESERVATION_START]

static constexpr const char *mMediaFormatName[] =
{
	"HLS","DASH","PROGRESSIVE","HLS_MP4","OTA","HDMI_IN","COMPOSITE_IN","SMOOTH_STREAMING", "RMF", "UNKNOWN"
};

static_assert(sizeof(mMediaFormatName)/sizeof(mMediaFormatName[0]) == (eMEDIAFORMAT_UNKNOWN + 1), "Ensure 1:1 mapping between mMediaFormatName[] and enum MediaFormat");
/**
 * @brief Get the idle task's source ID
 * @retval source ID
 */
static guint aamp_GetSourceID()
{
	guint callbackId = 0;
	GSource *source = g_main_current_source();
	if (source != NULL)
	{
		callbackId = g_source_get_id(source);
	}
	return callbackId;
}

/**
 * @brief Idle task to resume aamp
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval True/False
 */
static gboolean PrivateInstanceAAMP_Resume(gpointer ptr)
{
	bool retValue = true;
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP* )ptr;
	TuneType tuneType = eTUNETYPE_SEEK;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);

	aamp->NotifyFirstBufferProcessed(sink ? sink->GetVideoRectangle() : std::string());

	if (!aamp->mSeekFromPausedState && (aamp->rate == AAMP_NORMAL_PLAY_RATE) && !aamp->IsLocalAAMPTsb())
	{
		if(sink)
		{
			retValue = sink->Pause(false, false);
		}
		aamp->pipeline_paused = false;
	}
	else
	{
		// Live immediate : seek to live position from paused state.
		if (aamp->mPausedBehavior == ePAUSED_BEHAVIOR_LIVE_IMMEDIATE)
		{
			tuneType = eTUNETYPE_SEEKTOLIVE;
		}
		aamp->rate = AAMP_NORMAL_PLAY_RATE;
		aamp->pipeline_paused = false;
		aamp->mSeekFromPausedState = false;
		aamp->AcquireStreamLock();
		aamp->TuneHelper(tuneType);
		aamp->ReleaseStreamLock();
	}

	aamp->ResumeDownloads();
	if(retValue)
	{
		aamp->NotifySpeedChanged(aamp->rate);
	}
	aamp->mAutoResumeTaskPending = false;
	return G_SOURCE_REMOVE;
}

/**
 * @brief Idle task to process discontinuity
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval G_SOURCE_REMOVE
 */
static gboolean PrivateInstanceAAMP_ProcessDiscontinuity(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;

	GSource *src = g_main_current_source();
	if (src == NULL || !g_source_is_destroyed(src))
	{
		bool ret = aamp->ProcessPendingDiscontinuity();
		// This is to avoid calling cond signal, in case Stop() interrupts the ProcessPendingDiscontinuity
		if (ret)
		{
			aamp->SyncBegin();
			aamp->mDiscontinuityTuneOperationId = 0;
			aamp->SyncEnd();
		}
		aamp->mCondDiscontinuity.notify_one();
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Tune again to currently viewing asset. Used for internal error handling
 * @param ptr pointer to PrivateInstanceAAMP object
 * @retval G_SOURCE_REMOVE
 */
static gboolean PrivateInstanceAAMP_Retune(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP*) ptr;
	bool activeAAMPFound = false;
	bool reTune = false;
	gActivePrivAAMP_t *gAAMPInstance = NULL;
	std::unique_lock<std::mutex> lock(gMutex);
	for (std::list<gActivePrivAAMP_t>::iterator iter = gActivePrivAAMPs.begin(); iter != gActivePrivAAMPs.end(); iter++)
	{
		if (aamp == iter->pAAMP)
		{
			gAAMPInstance = &(*iter);
			activeAAMPFound = true;
			reTune = gAAMPInstance->reTune;
			break;
		}
	}
	if (!activeAAMPFound)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: %p not in Active AAMP list", aamp);
	}
	else if (!reTune)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: %p reTune flag not set", aamp);
	}
	else
	{
		if (aamp->pipeline_paused)
		{
			aamp->pipeline_paused = false;
		}

		aamp->mIsRetuneInProgress = true;
		lock.unlock();

		aamp->AcquireStreamLock();
		aamp->TuneHelper(eTUNETYPE_RETUNE);
		aamp->ReleaseStreamLock();

		lock.lock();
		aamp->mIsRetuneInProgress = false;
		gAAMPInstance->reTune = false;
		gCond.notify_one();
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Get the telemetry type for a media type
 * @param type media type
 * @retval telemetry type
 */
static MediaTypeTelemetry aamp_GetMediaTypeForTelemetry(AampMediaType type)
{
	MediaTypeTelemetry ret;
	switch(type)
	{
			case eMEDIATYPE_VIDEO:
			case eMEDIATYPE_AUDIO:
			case eMEDIATYPE_SUBTITLE:
			case eMEDIATYPE_AUX_AUDIO:
			case eMEDIATYPE_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_AVS;
						break;
			case eMEDIATYPE_MANIFEST:
			case eMEDIATYPE_PLAYLIST_VIDEO:
			case eMEDIATYPE_PLAYLIST_AUDIO:
			case eMEDIATYPE_PLAYLIST_SUBTITLE:
			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			case eMEDIATYPE_PLAYLIST_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_MANIFEST;
						break;
			case eMEDIATYPE_INIT_VIDEO:
			case eMEDIATYPE_INIT_AUDIO:
			case eMEDIATYPE_INIT_SUBTITLE:
			case eMEDIATYPE_INIT_AUX_AUDIO:
			case eMEDIATYPE_INIT_IFRAME:
						ret = eMEDIATYPE_TELEMETRY_INIT;
						break;
			case eMEDIATYPE_LICENCE:
						ret = eMEDIATYPE_TELEMETRY_DRM;
						break;
			default:
						ret = eMEDIATYPE_TELEMETRY_UNKNOWN;
						break;
	}
	return ret;
}

double PrivateInstanceAAMP::RecalculatePTS(AampMediaType mediaType, const void *ptr, size_t len )
{
    double ret = 0;
    uint32_t timeScale = 0;
    switch( mediaType )
    {
    case eMEDIATYPE_VIDEO:
        timeScale = GetVidTimeScale();
        break;
    case eMEDIATYPE_AUDIO:
    case eMEDIATYPE_AUX_AUDIO:
        timeScale = GetAudTimeScale();
        break;
    case eMEDIATYPE_SUBTITLE:
        timeScale = GetSubTimeScale();
        break;
    default:
        AAMPLOG_WARN("Invalid media type %d", mediaType);
        break;
    }
    IsoBmffBuffer isobuf;
    isobuf.setBuffer((uint8_t *)ptr, len);
    bool bParse = false;
    try
    {
        bParse = isobuf.parseBuffer();
    }
    catch( std::bad_alloc& ba)
    {
        AAMPLOG_ERR("Bad allocation: %s", ba.what() );
    }
    catch( std::exception &e)
    {
        AAMPLOG_ERR("Unhandled exception: %s", e.what() );
    }
    catch( ... )
    {
        AAMPLOG_ERR("Unknown exception");
    }
    if(bParse && (0 != timeScale))
    {
        uint64_t fPts = 0;
        bool bParse = isobuf.getFirstPTS(fPts);
        if (bParse)
        {
            ret = fPts/(timeScale*1.0);
        }
    }
    return ret;
}

/**
 * @brief Updates a vector of CCTrackInfo objects with data from a vector of TextTrackInfo objects.
 *
 * This function clears the provided `updatedTextTracks` vector and populates it with
 * CCTrackInfo objects created from the data in the `textTracksCopy` vector.
 *
 * @param[in] textTracksCopy A vector of TextTrackInfo objects to be processed.
 * @param[out] updatedTextTracks A vector of CCTrackInfo objects to be updated with the processed data.
 */
void PrivateInstanceAAMP::UpdateCCTrackInfo(const std::vector<TextTrackInfo>& textTracksCopy, std::vector<CCTrackInfo>& updatedTextTracks)
{
	updatedTextTracks.clear(); // Clear the vector to ensure no stale data remains.

	for (const auto& track : textTracksCopy)
	{
		CCTrackInfo ccTrack;
		ccTrack.language = track.language;
		ccTrack.instreamId = track.instreamId;
		updatedTextTracks.push_back(ccTrack);
	}
}

/**
 * @brief de-fog playback URL to play directly from CDN instead of fog
 * @param[in][out] dst Buffer containing URL
 */
static void DeFog(std::string& url)
{
	std::string prefix("&recordedUrl=");
	size_t startPos = url.find(prefix);
	if( startPos != std::string::npos )
	{
		startPos += prefix.size();
		size_t len = url.find( '&',startPos );
		if( len != std::string::npos )
		{
			len -= startPos;
		}
		url = url.substr(startPos,len);
		aamp_DecodeUrlParameter(url);
	}
}

/**
 * @brief replace all occurrences of existingSubStringToReplace in str with replacementString
 * @param str string to be scanned/modified
 * @param existingSubStringToReplace substring to be replaced
 * @param replacementString string to be substituted
 * @retval true iff str was modified
 */
static bool replace(std::string &str, const char *existingSubStringToReplace, const char *replacementString)
{
	bool rc = false;
	std::size_t fromPos = 0;
	size_t existingSubStringToReplaceLen = 0;
	size_t replacementStringLen = 0;
	for(;;)
	{
		std::size_t pos = str.find(existingSubStringToReplace,fromPos);
		if( pos == std::string::npos )
		{ // done - pattern not found
			break;
		}
		if( !rc )
		{ // lazily measure input strings - no need to measure unless match found
			rc = true;
			existingSubStringToReplaceLen = strlen(existingSubStringToReplace);
			replacementStringLen = strlen(replacementString);
		}
		str.replace( pos, existingSubStringToReplaceLen, replacementString );
		fromPos  = pos + replacementStringLen;
	}
	return rc;
}


/**
 * @brief convert https to https in recordedUrl part of manifestUrl
 * @param[in][out] dst Buffer containing URL
 * @param[in]string - https
 * @param[in]string - http
 */
void ForceHttpConversionForFog(std::string& url,const std::string& from, const std::string& to)
{
	std::string prefix("&recordedUrl=");
	size_t startPos = url.find(prefix);
	if( startPos != std::string::npos )
	{
		startPos += prefix.size();
		url.replace(startPos, from.length(), to);
	}
}

/**
 * @brief Active streaming interface is wifi
 *
 * @return bool - true if wifi interface connected
 */
static bool IsActiveStreamingInterfaceWifi (void)
{
	bool wifiStatus = false;
	wifiStatus = PlayerExternalsInterface::IsActiveStreamingInterfaceWifi();
	activeInterfaceWifi =  pPlayerExternalsInterface->GetActiveInterface();
	return wifiStatus;
}

/**
* @brief helper function to extract numeric value from given buf after removing prefix
* @param buf String buffer to scan
* @param prefixPtr - prefix string to match in bufPtr
* @param value - receives numeric value after extraction
* @retval 0 if prefix not present or error
* @retval 1 if string converted to numeric value
*/
template<typename T>
static int ReadConfigNumericHelper(std::string buf, const char* prefixPtr, T& value)
{
	int ret = 0;

	try
	{
		std::size_t pos = buf.rfind(prefixPtr,0); // starts with check
		if (pos != std::string::npos)
		{
			pos += strlen(prefixPtr);
			std::string valStr = buf.substr(pos);
			if (std::is_same<T, int>::value)
				value = std::stoi(valStr);
			else if (std::is_same<T, long>::value)
				value = std::stol(valStr);
			else if (std::is_same<T, float>::value)
				value = std::stof(valStr);
			else
				value = std::stod(valStr);
			ret = 1;
		}
	}
	catch(exception& e)
	{
		// NOP
	}

	return ret;
}


// End of helper functions for loading configuration

/**
 * @brief HandleSSLWriteCallback - Handle write callback from CURL
 */
size_t PrivateInstanceAAMP::HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata )
{
	size_t ret = 0;
	CurlCallbackContext *context = (CurlCallbackContext *)userdata;
	if(!context) return ret;
	if( ISCONFIGSET_PRIV(eAAMPConfig_CurlThroughput) )
	{
		AAMPLOG_MIL( "curl-write type=%d size=%zu total=%zu",
					context->mediaType,
					size*nmemb,
					context->contentLength );
	}
	// There is scope for rework here, mDownloadsEnabled can be queried with a lock, rather than acquiring lock here
	std::unique_lock<std::recursive_mutex> lock(context->aamp->mLock);
	if (context->aamp->mDownloadsEnabled && context->aamp->mMediaDownloadsEnabled[context->mediaType])
	{
		if ((NULL == context->buffer->GetPtr() ) && (context->contentLength > 0))
		{
			size_t len = context->contentLength;
			if(context->downloadIsEncoded && (len < DEFAULT_ENCODED_CONTENT_BUFFER_SIZE))
			{
				// Allocate a fixed buffer for encoded contents. Content length is not trusted here
				len = DEFAULT_ENCODED_CONTENT_BUFFER_SIZE;
			}
			context->buffer->ReserveBytes(len);
		}
		size_t numBytesForBlock = size*nmemb;
		if(ptr && numBytesForBlock > 0)
		{
			context->buffer->AppendBytes( ptr, numBytesForBlock );
		}
		ret = numBytesForBlock;
		MediaStreamContext *mCtx = context->aamp->GetMediaStreamContext(context->mediaType);

		if(mCtx)
		{
			bool ischunkMode = context->aamp->GetLLDashServiceData()->lowLatencyMode &&
							   context->aamp->GetLLDashChunkMode() &&
							   !mCtx->IsLocalTSBInjection() &&
							   !(IsLocalAAMPTsb() && pipeline_paused);

			if (ischunkMode && ptr && (numBytesForBlock > 0) &&
				(context->mediaType == eMEDIATYPE_VIDEO ||
				context->mediaType ==  eMEDIATYPE_AUDIO ||
				context->mediaType ==  eMEDIATYPE_SUBTITLE))
			{
				// Release PrivateInstanceAAMP mutex to unblock async APIs
				lock.unlock();
				AAMPLOG_TRACE("[%d] Caching chunk with size %zu nmemb:%zu size:%zu", context->mediaType, numBytesForBlock, nmemb, size);
				long long startTime = aamp_GetCurrentTimeMS();
				mCtx->CacheFragmentChunk(context->mediaType, ptr, numBytesForBlock,context->remoteUrl,context->downloadStartTime);
				context->processDelay += aamp_GetCurrentTimeMS() - startTime;
				lock.lock();
			}
		}
	}
	else
	{
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost)
		{
			ret = (size*nmemb);
		}
		else
		{
			AAMPLOG_WARN("CurlTrace write_callback - interrupted, ret:%zu", ret);
		}
	}
	return ret;
}

/**
 * @brief function to print header response during download failure and latency.
 * @param mediaType current media type
 */
static void print_headerResponse(std::vector<std::string> &allResponseHeaders, AampMediaType mediaType)
{
	if (gpGlobalConfig->IsConfigSet(eAAMPConfig_CurlHeader) )
	{
		if( eMEDIATYPE_VIDEO == mediaType || eMEDIATYPE_PLAYLIST_VIDEO == mediaType )
		{
			size_t size = allResponseHeaders.size();
			while( size-- )
			{
				AAMPLOG_MIL("* %s", allResponseHeaders.at(size).c_str());
			}
		}
	}
	allResponseHeaders.clear();
}

/**
 * @brief HandleSSLHeaderCallback - Hanlde header callback from SSL
 */
size_t PrivateInstanceAAMP::HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* user_data )
{
	size_t len = nmemb * size;
	if( user_data )
	{
		CurlCallbackContext *context = static_cast<CurlCallbackContext *>(user_data);
		httpRespHeaderData *httpHeader = context->responseHeaderData;
		size_t startPos = 0;
		size_t endPos = len-2; // strip CRLF

		bool isBitrateHeader = false;
		bool isFogRecordingIdHeader = false;
		bool isProfileCapHeader = false;

		if( len<2 || ptr[endPos] != '\r' || ptr[endPos+1] != '\n' )
		{ // only proceed if this is a CRLF terminated curl header, as expected
			return len;
		}

		if (context->aamp->mConfig->IsConfigSet(eAAMPConfig_CurlHeader) && ptr[0] &&
			(eMEDIATYPE_VIDEO == context->mediaType || eMEDIATYPE_PLAYLIST_VIDEO == context->mediaType))
		{
			std::string temp = std::string(ptr,endPos);
			context->allResponseHeaders.push_back(temp);
		}

		// As per Hypertext Transfer Protocol ==> Field names are case-insensitive
		// HTTP/1.1 4.2 Message Headers : Each header field consists of a name followed by a colon (":") and the field value. Field names are case-insensitive
		if (STARTS_WITH_IGNORE_CASE(ptr, FOG_REASON_STRING))
		{
			httpHeader->type = eHTTPHEADERTYPE_FOG_REASON;
			startPos = STRLEN_LITERAL(FOG_REASON_STRING);
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, CURLHEADER_X_REASON))
		{
			httpHeader->type = eHTTPHEADERTYPE_XREASON;
			startPos = STRLEN_LITERAL(CURLHEADER_X_REASON);
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, BITRATE_HEADER_STRING))
		{
			startPos = STRLEN_LITERAL(BITRATE_HEADER_STRING);
			isBitrateHeader = true;
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, SET_COOKIE_HEADER_STRING))
		{
			httpHeader->type = eHTTPHEADERTYPE_COOKIE;
			startPos = STRLEN_LITERAL(SET_COOKIE_HEADER_STRING);
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, LOCATION_HEADER_STRING))
		{
			httpHeader->type = eHTTPHEADERTYPE_EFF_LOCATION;
			startPos = STRLEN_LITERAL(LOCATION_HEADER_STRING);
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, FOG_RECORDING_ID_STRING))
		{
			startPos = STRLEN_LITERAL(FOG_RECORDING_ID_STRING);
			isFogRecordingIdHeader = true;
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, CONTENT_ENCODING_STRING ))
		{
			// Enabled IsEncoded as Content-Encoding header is present
			// The Content-Encoding entity header indicates media is compressed
			context->downloadIsEncoded = true;
		}
		else if (context->aamp->mConfig->IsConfigSet(eAAMPConfig_LimitResolution) && context->aamp->IsFirstRequestToFog() && STARTS_WITH_IGNORE_CASE(ptr, CAPPED_PROFILE_STRING ))
		{
			startPos = STRLEN_LITERAL(CAPPED_PROFILE_STRING);
			isProfileCapHeader = true;
		}
		else if (STARTS_WITH_IGNORE_CASE(ptr, TRANSFER_ENCODING_STRING ))
		{
			context->chunkedDownload = true;
		}
		else if (0 == context->buffer->GetAvail() )
		{
			if (STARTS_WITH_IGNORE_CASE(ptr, CONTENTLENGTH_STRING))
			{
				int contentLengthStartPosition = STRLEN_LITERAL(CONTENTLENGTH_STRING);
				const char * contentLengthStr = ptr + contentLengthStartPosition;
				context->contentLength = atoi(contentLengthStr);
			}
		}

		// This implementation is needed for HLS which still uses GetFile
		// Check for http header tags, only if event listener for HTTPResponseHeaderEvent is available
		if (eMEDIATYPE_MANIFEST == context->mediaType && context->aamp->IsEventListenerAvailable(AAMP_EVENT_HTTP_RESPONSE_HEADER))
		{
			std::vector<std::string> responseHeaders = context->aamp->manifestHeadersNeeded;
			if (responseHeaders.size() > 0)
			{
				for (int header=0; header < responseHeaders.size(); header++) {
					std::string headerType = responseHeaders[header].c_str();
					// check if subscribed header is available
					if (0 == strncasecmp(ptr, headerType.c_str() , headerType.length()))
					{
						startPos = headerType.length();
						// strip only the header value from the response
						context->aamp->httpHeaderResponses[headerType] = std::string( ptr + startPos + 2, endPos - startPos - 2).c_str();
						AAMPLOG_INFO("httpHeaderResponses");
						for (auto const& pair: context->aamp->httpHeaderResponses) {
							AAMPLOG_INFO("{ %s, %s }", pair.first.c_str(), pair.second.c_str());
						}
					}
				}
			}
		}

		if(startPos > 0)
		{
			while( endPos>startPos && ptr[endPos-1] == ' ' )
			{ // strip trailing whitespace
				endPos--;
			}
			while( startPos < endPos && ptr[startPos] == ' ')
			{ // strip leading whitespace
				startPos++;
			}

			if(isBitrateHeader)
			{
				const char * strBitrate = ptr + startPos;
				context->bitrate = atol(strBitrate);
				AAMPLOG_TRACE("Parsed HTTP %s: %" BITSPERSECOND_FORMAT, isBitrateHeader? "Bitrate": "False", context->bitrate);
			}
			else if(isFogRecordingIdHeader)
			{
				context->aamp->mTsbRecordingId = string( ptr + startPos, endPos - startPos );
				AAMPLOG_TRACE("Parsed Fog-Id : %s", context->aamp->mTsbRecordingId.c_str());
			}
			else if(isProfileCapHeader)
			{
				const char * strProfileCap = ptr + startPos;
				context->aamp->mProfileCappedStatus = atol(strProfileCap)? true : false;
				AAMPLOG_TRACE("Parsed Profile-Capped Header : %d", context->aamp->mProfileCappedStatus);
			}
			else
			{
				httpHeader->data = string( ptr + startPos, endPos - startPos );
				if(httpHeader->type != eHTTPHEADERTYPE_EFF_LOCATION)
				{ //Append delimiter ";"
					httpHeader->data += ';';
				}
			}
			AAMPLOG_TRACE("Parsed HTTP %s header: %s", httpHeader->type==eHTTPHEADERTYPE_COOKIE? "Cookie": "X-Reason", httpHeader->data.c_str());
		}
	}
	return len;
}

/**
 * @brief Get Current Content Download Speed
 * @param aamp ptr aamp context
 * @param mediaType File Type
 * @param bDownloadStart Download start flag
 * @param start Download start time
 * @param dlnow current downloaded bytes
 * @retval bps bits per second
 */
long getCurrentContentDownloadSpeed(PrivateInstanceAAMP *aamp,
									AampMediaType mediaType, //File Type Download
									bool bDownloadStart,
									long start,
									double dlnow) // downloaded bytes so far)
{
	long bitsPerSecond = 0;
	long time_now = 0;
	long time_diff = 0;
	long dl_diff = 0;

	struct SpeedCache* speedcache = NULL;
	speedcache = aamp->GetLLDashSpeedCache();

	if(!aamp->mhAbrManager.GetLowLatencyStartABR())
	{
		speedcache->last_sample_time_val = start;
	}

	time_now = NOW_STEADY_TS_MS;
	time_diff = (time_now - speedcache->last_sample_time_val);

	if(bDownloadStart)
	{
		speedcache->prev_dlnow = 0;
	}

	dl_diff = (long)dlnow -  speedcache->prev_dlnow;

	speedcache->prev_dlnow = dlnow;

	long currentTotalDownloaded = 0;
	long total_dl_diff  = 0;
	currentTotalDownloaded = speedcache->totalDownloaded + dl_diff;
	total_dl_diff = currentTotalDownloaded - speedcache->prevSampleTotalDownloaded;
	if(total_dl_diff<=0) total_dl_diff = 0;
	if(aamp->mhAbrManager.IsABRDataGoodToEstimate(time_diff))
	{
		aamp->mhAbrManager.CheckLLDashABRSpeedStoreSize(speedcache,bitsPerSecond,time_now,total_dl_diff,time_diff,currentTotalDownloaded);
	}
	else
	{
		AAMPLOG_TRACE("[%d] Ignore Speed Calculation -> time_diff [%ld]",mediaType, time_diff);
	}

	speedcache->totalDownloaded += dl_diff;

	return bitsPerSecond;
}

/**
 * @brief HandleSSLProgressCallback - Process progress callback from CURL
 *
 * @param clientp opaque context passed by caller
 * @param dltotal total number of bytes libcurl expects to download
 * @param dlnow number of bytes downloaded so far
 * @param ultotal total number of bytes libcurl expects to upload
 * @param ulnow number of bytes uploaded so far
 *
 * @retval -1 to cancel in progress download
 */
int PrivateInstanceAAMP::HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	CurlProgressCbContext *context = (CurlProgressCbContext *)clientp;
	PrivateInstanceAAMP *aamp = context->aamp;
	AampConfig *mConfig = context->aamp->mConfig;

	if(context->aamp->GetLLDashServiceData()->lowLatencyMode &&
		context->mediaType == eMEDIATYPE_VIDEO &&
		context->aamp->CheckABREnabled() &&
		!(ISCONFIGSET_PRIV(eAAMPConfig_DisableLowLatencyABR)))
	{
		//AAMPLOG_WARN("[%d] dltotal: %.0f , dlnow: %.0f, ultotal: %.0f, ulnow: %.0f, time: %.0f\n", context->mediaType,
		//	dltotal, dlnow, ultotal, ulnow, difftime(time(NULL), 0));

		// int AbrChunkThresholdSize = GETCONFIGVALUE(eAAMPConfig_ABRChunkThresholdSize);

		if (/*(dlnow > AbrChunkThresholdSize) &&*/ (context->downloadNow != dlnow))
		{
			long downloadbps = 0;

			context->downloadNow = dlnow;
			context->downloadNowUpdatedTime = NOW_STEADY_TS_MS;

			if(!aamp->mhAbrManager.GetLowLatencyStartABR())
			{
				//Reset speedcache when Fragment download Starts
				struct SpeedCache* speedcache = NULL;
				speedcache = aamp->GetLLDashSpeedCache();
				memset(speedcache, 0x00, sizeof(struct SpeedCache));
			}

			downloadbps = getCurrentContentDownloadSpeed(aamp, context->mediaType, context->dlStarted, (long)context->downloadStartTime, dlnow);

			if(context->dlStarted)
			{
				context->dlStarted = false;
			}

			if(!aamp->mhAbrManager.GetLowLatencyStartABR())
			{
				aamp->mhAbrManager.SetLowLatencyStartABR(true);
			}

			if(downloadbps)
			{
				std::lock_guard<std::recursive_mutex> guard(context->aamp->mLock);
				aamp->mhAbrManager.UpdateABRBitrateDataBasedOnCacheLength(context->aamp->mAbrBitrateData,downloadbps,true);
			}
		}
	}

	int rc = 0;
	context->aamp->SyncBegin();
	if (!context->aamp->mDownloadsEnabled && context->aamp->mMediaDownloadsEnabled[context->mediaType])
	{
		rc = -1; // CURLE_ABORTED_BY_CALLBACK
	}

	context->aamp->SyncEnd();
	if( rc==0 )
	{ // only proceed if not an aborted download
		if (dlnow > 0 && context->stallTimeout > 0)
		{
			if (context->downloadSize == -1)
			{ // first byte(s) downloaded
				context->downloadSize = dlnow;
				context->downloadUpdatedTime = NOW_STEADY_TS_MS;
			}
			else
			{
				if (dlnow == context->downloadSize)
				{ // no change in downloaded bytes - check time since last update to infer stall
					double timeElapsedSinceLastUpdate = (NOW_STEADY_TS_MS - context->downloadUpdatedTime) / 1000.0; //in secs
					if (timeElapsedSinceLastUpdate >= context->stallTimeout)
					{ // no change for at least <stallTimeout> seconds - consider download stalled and abort
						AAMPLOG_WARN("Abort download as mid-download stall detected for %.2f seconds, download size:%.2f bytes", timeElapsedSinceLastUpdate, dlnow);
						context->abortReason = eCURL_ABORT_REASON_STALL_TIMEDOUT;
						rc = -1;
					}
				}
				else
				{ // received additional bytes - update state to track new size/time
					context->downloadSize = dlnow;
					context->downloadUpdatedTime = NOW_STEADY_TS_MS;
				}
			}
		}
		if (dlnow == 0 && context->startTimeout > 0)
		{ // check to handle scenario where <startTimeout> seconds delay occurs without any bytes having been downloaded (stall at start)
			double timeElapsedInSec = (double)(NOW_STEADY_TS_MS - context->downloadStartTime) / 1000; //in secs  //CID:85922 - UNINTENDED_INTEGER_DIVISION
			if (timeElapsedInSec >= context->startTimeout)
			{
				AAMPLOG_WARN("Abort download as no data received for %.2f seconds", timeElapsedInSec);
				context->abortReason = eCURL_ABORT_REASON_START_TIMEDOUT;
				rc = -1;
			}
		}
		if (dlnow > 0 && context->lowBWTimeout> 0 && eMEDIATYPE_VIDEO == context->mediaType)
		{
			double elapsedTimeMs = (double)(NOW_STEADY_TS_MS - context->downloadStartTime);
			if( elapsedTimeMs >= context->lowBWTimeout*1000 )
			{
				if(dltotal)
				{
					double predictedTotalDownloadTimeMs = elapsedTimeMs*dltotal/dlnow;
					if( predictedTotalDownloadTimeMs > aamp->mNetworkTimeoutMs )
					{
						AAMPLOG_WARN("lowBWTimeout=%ds; predictedTotalDownloadTime=%fs>%fs (network timeout)",
								context->lowBWTimeout,
								predictedTotalDownloadTimeMs/1000.0,
								aamp->mNetworkTimeoutMs/1000.0 );
						context->abortReason = eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT;
						rc = -1;
					}
				}
				else
				{
					if(context->aamp->GetLLDashServiceData()->lowLatencyMode && !IsLocalAAMPTsb())
					{
						long downloadbps = getCurrentContentDownloadSpeed(aamp, context->mediaType, context->dlStarted, (long)context->downloadStartTime, dlnow);
						long currentProfilebps  = context->aamp->mpStreamAbstractionAAMP->GetVideoBitrate();
						MediaStreamContext *mCtx = context->aamp->GetMediaStreamContext(context->mediaType);
						if(downloadbps > 0 && mCtx && !mCtx->IsLocalTSBInjection())
						{
							if((downloadbps + DEFAULT_BITRATE_OFFSET_FOR_DOWNLOAD) < currentProfilebps)
							{
								AAMPLOG_WARN("Abort download as content is estimated to be expired current BW : %ld bps, min required:%ld bps", downloadbps, currentProfilebps);
								context->abortReason = eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT;
								rc = -1;
							}
						}
					}
				}

			}
		}
	}

	if(rc)
	{
		if( !( eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT == context->abortReason || eCURL_ABORT_REASON_START_TIMEDOUT == context->abortReason ||\
			eCURL_ABORT_REASON_STALL_TIMEDOUT == context->abortReason ) && (ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost ) )
		{
			rc = 0;
		}
		else
		{
			AAMPLOG_WARN("CurlTrace Progress interrupted, ret:%d", rc);
		}
	}
	return rc;
}


// End of curl callback functions

/**
 * @brief PrivateInstanceAAMP Constructor
 */
PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) : mReportProgressPosn(0.0), mLastTelemetryTimeMS(0), mDiscontinuityFound(false), mTelemetryInterval(0), mAbrBitrateData(), mLock(),
	mpStreamAbstractionAAMP(NULL), mInitSuccess(false), mVideoFormat(FORMAT_INVALID), mAudioFormat(FORMAT_INVALID), mDownloadsDisabled(),
	mDownloadsEnabled(true), profiler(), licenceFromManifest(false), previousAudioType(eAUDIO_UNKNOWN),isPreferredDRMConfigured(false),
	mbDownloadsBlocked(false), streamerIsActive(false), mFogTSBEnabled(false), mIscDVR(false), mLiveOffset(AAMP_LIVE_OFFSET),
	seek_pos_seconds(-1), rate(0), pipeline_paused(false), mMaxLanguageCount(0), zoom_mode(VIDEO_ZOOM_NONE),
	video_muted(false), subtitles_muted(true), audio_volume(100), subscribedTags(), manifestHeadersNeeded(), httpHeaderResponses(), timedMetadata(), timedMetadataNew(), IsTuneTypeNew(false), trickStartUTCMS(-1), durationSeconds(0.0), culledSeconds(0.0), culledOffset(0.0), maxRefreshPlaylistIntervalSecs(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS/1000),
	mEventListener(NULL), mNewSeekInfo(), discardEnteringLiveEvt(false),
	mIsRetuneInProgress(false), mCondDiscontinuity(), mDiscontinuityTuneOperationId(0), mIsVSS(false),
	m_fd(-1), mIsLive(false), mIsAudioContextSkipped(false), mLogTune(false), mTuneCompleted(false), mFirstTune(true), mfirstTuneFmt(-1), mTuneAttempts(0), mPlayerLoadTime(0),
	mState(eSTATE_RELEASED), mMediaFormat(eMEDIAFORMAT_HLS), mPersistedProfileIndex(0), mAvailableBandwidth(0),
	mDiscontinuityTuneOperationInProgress(false), mContentType(ContentType_UNKNOWN), mTunedEventPending(false),
	mSeekOperationInProgress(false), mTrickplayInProgress(false), mPendingAsyncEvents(), mCustomHeaders(),
	mManifestUrl(""), mTunedManifestUrl(""), mOrigManifestUrl(), mServiceZone(), mVssVirtualStreamId(),
	mCurrentLanguageIndex(0),
	preferredLanguagesString(), preferredLanguagesList(), preferredLabelList(),mhAbrManager(),
	mVideoEnd(NULL),
	//mTimeToTopProfile(0),
	mTimeAtTopProfile(0),mPlaybackDuration(0),mTraceUUID(),
	mIsFirstRequestToFOG(false),
	mPausePositionMonitorMutex(), mPausePositionMonitorCV(), mPausePositionMonitoringThreadID(), mPausePositionMonitoringThreadStarted(false),
	mTuneType(eTUNETYPE_NEW_NORMAL)
	,mCdaiObject(NULL), mAdEventsQ(),mAdEventQMtx(), mAdPrevProgressTime(0), mAdCurOffset(0), mAdDuration(0), mAdProgressId(""), mAdAbsoluteStartTime(0)
	,mBufUnderFlowStatus(false), mVideoBasePTS(0)
	,mCustomLicenseHeaders(), mIsIframeTrackPresent(false), mManifestTimeoutMs(-1), mNetworkTimeoutMs(-1)
	,mbPlayEnabled(true), mPlayerPreBuffered(false), mPlayerId(PLAYERID_CNTR++),mAampCacheHandler(NULL)
	,mAsyncTuneEnabled(false)
	,waitforplaystart()
	,mCurlShared(NULL)
	,mDrmDecryptFailCount(MAX_SEG_DRM_DECRYPT_FAIL_COUNT)
	,mPlaylistTimeoutMs(-1)
	,mMutexPlaystart()
	,mNetworkBandwidth(0)
	,mTimeToTopProfile(0)
	, fragmentCdmEncrypted(false) ,drmParserMutex(), aesCtrAttrDataList()
	, drmSessionThreadStarted(false), createDRMSessionThreadID()
	, mDRMLicenseManager(NULL)
	,  mPreCachePlaylistThreadId(), mPreCacheDnldList()
	, mPreCacheDnldTimeWindow(0), mParallelPlaylistFetchLock(), mAppName()
	, mProgressReportFromProcessDiscontinuity(false)
	, mPlaylistFetchFailError(0L),mAudioDecoderStreamSync(true)
	, mPrevPositionMilliseconds()
	, mGetPositionMillisecondsMutexHard()
	, mGetPositionMillisecondsMutexSoft()
	, mPausePositionMilliseconds(AAMP_PAUSE_POSITION_INVALID_POSITION)
	, mCurrentDrm(), mDrmInitData(), mMinInitialCacheSeconds(DEFAULT_MINIMUM_INIT_CACHE_SECONDS)
	//, mLicenseServerUrls()
	, mFragmentCachingRequired(false), mFragmentCachingLock()
	, mPauseOnFirstVideoFrameDisp(false)
	, mPreferredTextTrack(), mFirstVideoFrameDisplayedEnabled(false)
	, mSessionToken()
	, vDynamicDrmData()
	, midFragmentSeekCache(false)
	, mLiveOffsetDrift(AAMP_DEFAULT_LIVE_OFFSET_DRIFT)
	, mDisableRateCorrection (false)
	, mRateCorrectionThread ()
	, mRateCorrectionWait()
	, mRateCorrectionTimeoutLock()
	, mAbortRateCorrection (false)
	, mCorrectionRate(AAMP_NORMAL_PLAY_RATE)
	, mPreviousAudioType (FORMAT_INVALID)
	, mTsbRecordingId()
	, mthumbIndexValue(-1)
	, mManifestRefreshCount (0)
	, mJumpToLiveFromPause(false), mPausedBehavior(ePAUSED_BEHAVIOR_AUTOPLAY_IMMEDIATE), mSeekFromPausedState(false)
	, mProgramDateTime (0), mMPDPeriodsInfo()
	, mProfileCappedStatus(false),mSchemeIdUriDai("")
	, mDisplayWidth(0)
	, mDisplayHeight(0)
	, preferredRenditionString("")
	, preferredRenditionList()
	, preferredTypeString("")
	, preferredCodecString("")
	, preferredCodecList()
	, mAudioTuple()
	, preferredLabelsString("")
	, preferredAudioAccessibilityNode()
	, preferredTextLanguagesString("")
	, preferredTextLanguagesList()
	, preferredTextRenditionString("")
	, preferredTextTypeString("")
	, preferredTextLabelString("")
	, preferredTextAccessibilityNode()
	, preferredInstreamIdString("")
	, preferredTextNameString("")
	, preferredNameString("")
	, mProgressReportOffset(-1)
	, mFirstFragmentTimeOffset(-1)
	, mProgressReportAvailabilityOffset(-1)
	, mAutoResumeTaskId(AAMP_TASK_ID_INVALID)
	, mAutoResumeTaskPending(false)
	, mScheduler(NULL)
	, mEventLock()
	, mEventPriority(G_PRIORITY_DEFAULT_IDLE)
	, mStreamLock()
	, mConfig (config)
	, mSubLanguage()
	, preferredSubtitleLanguageVctr()
	, mHarvestCountLimit(0)
	, mHarvestConfig(0)
	, mIsWVKIDWorkaround(false)
	, mAuxFormat(FORMAT_INVALID), mAuxAudioLanguage()
	, mAbsoluteEndPosition(0), mIsLiveStream(false)
	, mbUsingExternalPlayer (false)
	, mCCId(0)
	, seiTimecode()
	, contentGaps()
	, mAampLLDashServiceData{}
	, bLowLatencyServiceConfigured(false)
	, bLLDashAdjustPlayerSpeed(false)
	, mLLDashCurrentPlayRate(AAMP_NORMAL_PLAY_RATE)
	, vidTimeScale(0)
	, audTimeScale(0)
	, subTimeScale(0)
	, speedCache {}
	, mCurrentLatency(0)
	, mLiveOffsetAppRequest(false)
	, bLowLatencyStartABR(false)
	, mEventManager (NULL)
	, mCMCDCollector(NULL)
	, mbDetached(false)
	, mIsFakeTune(false)
	, mCurrentAudioTrackId(-1)
	, mCurrentVideoTrackId(-1)
	, mIsTrackIdMismatch(false)
	, mIsDefaultOffset(false)
	, mNextPeriodDuration(0)
	, mNextPeriodStartTime(0)
	, mNextPeriodScaledPtoStartTime(0)
	, mOffsetFromTunetimeForSAPWorkaround(0)
	, mLanguageChangeInProgress(false)
	, mAampTsbLanguageChangeInProgress(false)
	, mSupportedTLSVersion(0)
	, mbSeeked(false)
	, mFailureReason("")
	, mTimedMetadataStartTime(0)
	, mTimedMetadataDuration(0)
	, playerStartedWithTrickPlay(false)
	, mPlaybackMode("UNKNOWN")
	, mApplyVideoRect(false)
	, mApplyContentRestriction(false)
	, mVideoRect{}
	, mData()
	, mIsInbandCC(true)
	, bitrateList()
	, userProfileStatus(false)
	, mApplyCachedVideoMute(false)
	, mFirstProgress(false)
	, mTsbSessionRequestUrl()
	, mcurrent_keyIdArray()
	, mDynamicDrmDefaultconfig()
	, mWaitForDynamicDRMToUpdate()
	, mDynamicDrmUpdateLock()
	, mDynamicDrmCache()
	, mAudioComponentCount(-1)
	, mAudioDelta(0)
	, mSubtitleDelta(0)
	, mVideoComponentCount(-1)
	, mAudioOnlyPb(false)
	, mVideoOnlyPb(false)
	, mCurrentAudioTrackIndex(-1)
	, mCurrentTextTrackIndex(-1)
	, mMediaDownloadsEnabled()
	, playerrate(1.0)
	, mSetPlayerRateAfterFirstframe(false)
	, mEncryptedPeriodFound(false)
	, mPipelineIsClear(false)
	, mLLActualOffset(-1)
	, mIsStream4K(false)
	, mTextStyle()
	, mFogDownloadFailReason("")
	, mBlacklistedProfiles()
	, mBufferFor4kRampup(0)
	, mBufferFor4kRampdown(0)
	, mId3MetadataCache{}
	, mMPDDownloaderInstance(nullptr)
	, mMPDStichOption(OPT_1_FULL_MANIFEST_TUNE),mMPDStichRefreshUrl("")
	, mTsbType("none")
	, mTsbDepthMs(0)
	, mDiscStartTime(0)
	, mRateCorrectionDelay(false)
	, mDownloadDelay(0)
	, curlhost{}
	, mWaitForDiscoToComplete()
	, mDiscoCompleteLock()
	, mIsPeriodChangeMarked(false)
	, m_lastSubClockSyncTime()
	, mIsLoggingNeeded(false)
	, mLiveEdgeDeltaFromCurrentTime(0.0)
	, mTrickModePositionEOS(0.0)
	, mTSBSessionManager(NULL)
	, mLocalAAMPTsb(false), mLocalAAMPInjectionEnabled(false)
	, mLocalAAMPTsbFromConfig(false)
	, mbPauseOnStartPlayback(false)
	, mTSBStore(nullptr)
	, mIsFlushFdsInCurlStore(false)
	, mProvidedManifestFile("")
	, mIsChunkMode(false)
	, prevFirstPeriodStartTime(0)
	, mIsFlushOperationInProgress(false)
{
	AAMPLOG_MIL("Create Private Player %d", mPlayerId);
	mAampCacheHandler = new AampCacheHandler(mPlayerId);
	// Create the event manager for player instance
	mEventManager = new AampEventManager(mPlayerId);
	// Create the CMCD collector
	mCMCDCollector = new AampCMCDCollector();

	preferredLanguagesString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLanguage);
	preferredRenditionString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioRendition);
	preferredCodecString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioCodec);
	preferredLabelsString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLabel);
	preferredTypeString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioType);
	preferredTextRenditionString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextRendition);
	preferredTextLanguagesString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLanguage);
	preferredTextLabelString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLabel);
	preferredTextTypeString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextType);
	int maxDrmSession = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxDASHDRMSessions);
	mDRMLicenseManager = new AampDRMLicenseManager(maxDrmSession, this);
	mSubLanguage = GETCONFIGVALUE_PRIV(eAAMPConfig_SubTitleLanguage);
	for (int i = 0; i < eCURLINSTANCE_MAX; i++)
	{
		curl[i] = NULL;
		//cookieHeaders[i].clear();
		httpRespHeaders[i].type = eHTTPHEADERTYPE_UNKNOWN;
		httpRespHeaders[i].data.clear();
		curlDLTimeout[i] = 0;
	}

	if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) )
	{
		for (int i = 0; i < eCURLINSTANCE_MAX; i++)
		{
			curlhost[i] = new eCurlHostMap();
		}
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		mbTrackDownloadsBlocked[i] = false;
		mTrackInjectionBlocked[i] = false;
		lastUnderFlowTimeMs[i] = 0;
		mProcessingDiscontinuity[i] = false;
		mIsDiscontinuityIgnored[i] = false;
		mbNewSegmentEvtSent[i] = true;
	}
	{
		std::lock_guard<std::mutex> guard(gMutex);
		gActivePrivAAMP_t gAAMPInstance = { this, false, 0 };
		gActivePrivAAMPs.push_back(gAAMPInstance);
	}
	mPendingAsyncEvents.clear();

	pPlayerExternalsInterface = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();

	if (ISCONFIGSET_PRIV(eAAMPConfig_WifiCurlHeader)) {
		if (true == IsActiveStreamingInterfaceWifi()) {
			mCustomHeaders["Wifi:"] = std::vector<std::string> { "1" };
			activeInterfaceWifi = true;
		}
		else
		{
			mCustomHeaders["Wifi:"] = std::vector<std::string> { "0" };
			activeInterfaceWifi = false;
		}
	}
	// Add Connection: Keep-Alive custom header
	mCustomHeaders["Connection:"] = std::vector<std::string> { "Keep-Alive" };
	preferredLanguagesList.push_back("en");

	memset(&aesCtrAttrDataList, 0, sizeof(aesCtrAttrDataList));
	mHarvestCountLimit = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestCountLimit);
	mHarvestConfig = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig);
	mAsyncTuneEnabled = ISCONFIGSET_PRIV(eAAMPConfig_AsyncTune);
    AampGrowableBuffer::EnableLogging(ISCONFIGSET_PRIV(eAAMPConfig_TrackMemory));
	mLastTelemetryTimeMS = aamp_GetCurrentTimeMS();
}

/**
 * @brief PrivateInstanceAAMP Destructor
 */
PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
	StopPausePositionMonitoring("AAMP destroyed");
	PlayerCCManager::GetInstance()->Release(mCCId);
	mCCId = 0;
	{
		std::lock_guard<std::mutex> guard(gMutex);
		auto iter = std::find_if(std::begin(gActivePrivAAMPs), std::end(gActivePrivAAMPs), [this](const gActivePrivAAMP_t& el)
		{
			return el.pAAMP == this;
		});
		if(iter != gActivePrivAAMPs.end())
		{
			gActivePrivAAMPs.erase(iter);
		}
	}
	mMediaDownloadsEnabled.clear();
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		SAFE_DELETE(mVideoEnd);
	}
	aesCtrAttrDataList.clear();
	SAFE_DELETE(mAampCacheHandler);

	SAFE_DELETE(mDRMLicenseManager);
	if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) )
	{
		for (int i = 0; i < eCURLINSTANCE_MAX; i++)
		{
			SAFE_DELETE(curlhost[i]);
		}
	}

	if ( !(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore)) )
	{
		if(mCurlShared)
		{
			curl_share_cleanup(mCurlShared);
			mCurlShared = NULL;
		}
	}

	SAFE_DELETE(mEventManager);
	SAFE_DELETE(mCMCDCollector);

	AampStreamSinkManager::GetInstance().DeleteStreamSink(this);

	SAFE_DELETE(mTSBSessionManager);
	if (HasSidecarData())
	{ // has sidecar data
		if (mpStreamAbstractionAAMP)
			mpStreamAbstractionAAMP->ResetSubtitle();
	}

}

/**
 * @brief Get the singleton object of the TSB Store
 */
std::shared_ptr<TSB::Store> PrivateInstanceAAMP::GetTSBStore(const TSB::Store::Config& config, TSB::LogFunction logger, TSB::LogLevel level)
{
	if(mTSBStore == nullptr)
	{
		try
		{
			mTSBStore = std::make_shared<TSB::Store>(config, logger, mPlayerId, level);
		}
		catch (std::exception &e)
		{
			AAMPLOG_ERR("Failed to instantiate TSB Store object, reason: %s", e.what());
		}
	}
	return mTSBStore;
}

/**
 * @brief perform pause of the pipeline and notifications for PauseAt functionality
 */
static gboolean PrivateInstanceAAMP_PausePosition(gpointer ptr)
{
	PrivateInstanceAAMP* aamp = (PrivateInstanceAAMP* )ptr;
	long long pausePositionMilliseconds = aamp->mPausePositionMilliseconds;
	aamp->mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;

	if  (pausePositionMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION)
	{
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
		if (sink)
		{
			if (!sink->Pause(true, false))
			{
				AAMPLOG_ERR("Pause failed");
			}
		}

		aamp->pipeline_paused = true;

		aamp->StopDownloads();

		if (aamp->mpStreamAbstractionAAMP)
		{
			aamp->mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
		}

		bool updateSeekPosition = false;
		if ((aamp->rate > AAMP_NORMAL_PLAY_RATE) || (aamp->rate < AAMP_RATE_PAUSE) ||
			(!ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && !aamp->mbDetached))
		{
			aamp->seek_pos_seconds = aamp->GetPositionSeconds();
			aamp->trickStartUTCMS = -1;
			AAMPLOG_INFO("Updated seek pos %fs", aamp->seek_pos_seconds);
			updateSeekPosition = true;
		}
		else
		{
			// (See SetRateInternal)
			if (!ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && !aamp->mbDetached)
			{
				aamp->seek_pos_seconds = aamp->GetPositionSeconds();
				aamp->trickStartUTCMS = -1;
				AAMPLOG_INFO("Updated seek pos %fs", aamp->seek_pos_seconds);
				updateSeekPosition = true;
			}
		}


		long long positionMs = aamp->GetPositionMilliseconds();
		if (updateSeekPosition)
		{
			AAMPLOG_WARN("PLAYER[%d] Paused at position %lldms, requested position %lldms, rate %f, seek pos updated to %fs",
						aamp->mPlayerId, positionMs, pausePositionMilliseconds, aamp->rate, aamp->seek_pos_seconds);
		}
		else
		{
			AAMPLOG_WARN("PLAYER[%d] Paused at position %lldms, requested position %lldms, rate %f",
						aamp->mPlayerId, positionMs, pausePositionMilliseconds, aamp->rate);
		}

		// Notify the client that play has paused
		aamp->NotifySpeedChanged(0, true);
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief the PositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::RunPausePositionMonitoring(void)
{
	long long localPauseAtMilliseconds = mPausePositionMilliseconds;
	long long posMs = GetPositionMilliseconds();
	int previousPollPeriodMs = 0;
	int previousVodTrickplayFPS = 0;

	AAMPLOG_WARN("PLAYER[%d] Pause at position %lldms, current position %lldms, rate %f",
				mPlayerId, mPausePositionMilliseconds.load(), posMs, rate);

	while(localPauseAtMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION)
	{
		int pollPeriodMs = AAMP_PAUSE_POSITION_POLL_PERIOD_MS;
		long long trickplayTargetPosMs = localPauseAtMilliseconds;
		bool forcePause = false;

		if ((rate == AAMP_RATE_PAUSE) || pipeline_paused)
		{
			// Shouldn't get here if already paused
			AAMPLOG_WARN("Already paused, exiting loop");
			mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;
			break;
		}
		// If normal speed or slower, i.e. not iframe trick mode
		else if ((rate > AAMP_RATE_PAUSE) && (rate <= AAMP_NORMAL_PLAY_RATE))
		{
			// If current pos is within a poll period of the target position,
			// set the sleep time to be the difference, and then perform
			// the pause.
			if (posMs >= (localPauseAtMilliseconds - pollPeriodMs))
			{
				pollPeriodMs = (localPauseAtMilliseconds - posMs) / rate;
				forcePause = true;
				AAMPLOG_INFO("Requested pos %lldms current pos %lldms rate %f, pausing in %dms",
							localPauseAtMilliseconds, posMs, rate, pollPeriodMs);
			}
			else
			{
				// The first time print WARN diag
				if (previousPollPeriodMs != pollPeriodMs)
				{
					AAMPLOG_WARN("PLAYER[%d] Polling period %dms, rate %f",
								mPlayerId, pollPeriodMs, rate);
					previousPollPeriodMs = pollPeriodMs;
				}
				AAMPLOG_INFO("Requested pos %lldms current pos %lldms rate %f, polling period %dms",
							localPauseAtMilliseconds, posMs, rate, pollPeriodMs);
			}
		}
		else
		{
			int vodTrickplayFPS = GETCONFIGVALUE_PRIV(eAAMPConfig_VODTrickPlayFPS);

			assert (vodTrickplayFPS != 0);

			// Poll at half the frame period (twice the frame rate)
			pollPeriodMs = (1000 / vodTrickplayFPS) / 2;

			// If rate > 0, the target position should be earlier than requested pos
			// If rate < 0, the target position should be later than requested pos
			trickplayTargetPosMs -= ((rate * 1000) / vodTrickplayFPS);

			if ((previousPollPeriodMs != pollPeriodMs) ||
				(previousVodTrickplayFPS != vodTrickplayFPS))
			{
				AAMPLOG_WARN("PLAYER[%d] Polling period %dms, rate %f, fps %d",
						mPlayerId, pollPeriodMs, rate, vodTrickplayFPS);
				previousPollPeriodMs = pollPeriodMs;
				previousVodTrickplayFPS = vodTrickplayFPS;
			}
			AAMPLOG_INFO("Requested pos %lldms current pos %lldms target pos %lld rate %f, fps %d, polling period %dms",
						 localPauseAtMilliseconds, posMs, trickplayTargetPosMs, rate, vodTrickplayFPS, pollPeriodMs);
		}

		// The calculation of pollPeriodMs for playback speeds, could result in a negative value
		if (pollPeriodMs > 0)
		{
			std::unique_lock<std::mutex> lock(mPausePositionMonitorMutex);
			std::cv_status cvStatus = std::cv_status::no_timeout;
			std::chrono::time_point<std::chrono::system_clock> waitUntilMs = std::chrono::system_clock::now() +
																			 std::chrono::milliseconds(pollPeriodMs);

			// Wait until now + pollPeriodMs, unless pauseAt is being canceled
			while ((localPauseAtMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION) &&
					(cvStatus == std::cv_status::no_timeout))
			{
				cvStatus = mPausePositionMonitorCV.wait_until(lock, waitUntilMs);
				localPauseAtMilliseconds = mPausePositionMilliseconds;
			}
			if (localPauseAtMilliseconds == AAMP_PAUSE_POSITION_INVALID_POSITION)
			{
				break;
			}
		}

		// Only need to get an updated pos if not forcing pause
		if (!forcePause)
		{
			posMs = GetPositionMilliseconds();
		}

		// Check if forcing pause at playback, or exceeded target position for trickplay
		if (forcePause ||
			((rate > AAMP_NORMAL_PLAY_RATE) && (posMs >= trickplayTargetPosMs)) ||
			((rate < AAMP_RATE_PAUSE) && (posMs <= trickplayTargetPosMs)))
		{
			(void)ScheduleAsyncTask(PrivateInstanceAAMP_PausePosition, this, "PrivateInstanceAAMP_PausePosition");
			break;
		}
	}
}

/**
 * @brief start the PausePositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::StartPausePositionMonitoring(long long pausePositionMilliseconds)
{
	StopPausePositionMonitoring("Start new pos monitor");

	if (pausePositionMilliseconds < 0)
	{
		AAMPLOG_ERR("The position (%lld) must be >= 0", pausePositionMilliseconds);
	}
	else
	{
		mPausePositionMilliseconds = pausePositionMilliseconds;
		if( mMediaFormat == eMEDIAFORMAT_DASH && ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) )
		{
			long long availabilityStartTimeMs = mpStreamAbstractionAAMP->GetAvailabilityStartTime()*1000;
			mPausePositionMilliseconds += availabilityStartTimeMs;
		}

		AAMPLOG_INFO("Start PausePositionMonitoring at position %lld", pausePositionMilliseconds);

		try
		{
			mPausePositionMonitoringThreadID = std::thread(&PrivateInstanceAAMP ::RunPausePositionMonitoring, this);
			mPausePositionMonitoringThreadStarted = true;
			AAMPLOG_INFO("Thread created for RunPausePositionMonitoring [%zx]", GetPrintableThreadID(mPausePositionMonitoringThreadID));
		}
		catch(const std::exception& e)
		{
			AAMPLOG_ERR("Failed to create PausePositionMonitor thread: %s", e.what());
		}
	}
}

/**
 * @brief stop the PausePositionMonitoring thread used for PauseAt functionality
 */
void PrivateInstanceAAMP::StopPausePositionMonitoring(std::string reason)
{
	if (mPausePositionMonitoringThreadStarted)
	{
		std::unique_lock<std::mutex> lock(mPausePositionMonitorMutex);

		if (mPausePositionMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION)
		{
			long long positionMs = GetPositionMilliseconds();
			AAMPLOG_WARN("PLAYER[%d] Stop position monitoring, reason: '%s', current position %lldms, requested position %lldms, rate %f",
						mPlayerId, reason.c_str(), positionMs, mPausePositionMilliseconds.load(), rate);
			mPausePositionMilliseconds = AAMP_PAUSE_POSITION_INVALID_POSITION;
			mPausePositionMonitorCV.notify_one();
		}
		lock.unlock();
		mPausePositionMonitoringThreadID.join();
		AAMPLOG_TRACE("joined PositionMonitor");
		mPausePositionMonitoringThreadStarted = false;
	}
}

/**
 * @brief wait for Discontinuity handling complete
 */
void PrivateInstanceAAMP::WaitForDiscontinuityProcessToComplete(void)
{
	// CID:306170 - Data race condition
	AAMPLOG_WARN("Discontinuity process is yet to complete, going to wait until it is done");
	std::unique_lock<std::mutex>lock(mDiscoCompleteLock);
	mWaitForDiscoToComplete.wait(lock, [this]{ return (false == mIsPeriodChangeMarked); });
	AAMPLOG_WARN("Discontinuity process wait is done");
}

/**
 * @brief unblock wait for Discontinuity handling complete
 */
void PrivateInstanceAAMP::UnblockWaitForDiscontinuityProcessToComplete(void)
{
	// CID:306170 - Data race condition
	SetIsPeriodChangeMarked(false);
}

/**
 * @brief Set to pause on next playback start
 */
void PrivateInstanceAAMP::SetPauseOnStartPlayback(bool enable)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);

	if (sink)
	{
		if (enable)
		{
			AAMPLOG_MIL("Initiated pause on start playback");
		}
		else if (mbPauseOnStartPlayback)
		{
			AAMPLOG_WARN("Interrupted pause on start playback");
		}
		else
		{
			// Intentionally left blank
		}

		sink->SetPauseOnStartPlayback(enable);
		mbPauseOnStartPlayback = enable;
	}
	else
	{
		AAMPLOG_WARN("No StreamSink");
		mbPauseOnStartPlayback = false;
	}
}

/**
 * @brief Notify reached paused when starting playback into paused state
 */
void PrivateInstanceAAMP::NotifyPauseOnStartPlayback(void)
{
	if (mbPauseOnStartPlayback)
	{
		AAMPLOG_INFO("Completed pause on start playback");
		mbPauseOnStartPlayback = false;

		StopDownloads();

		NotifySpeedChanged(0, true);

		if (mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
		}
		if(GetLLDashServiceData()->lowLatencyMode)
		{
			AAMPLOG_INFO("LL-Dash speed correction disabled after Pause");
			SetLLDashAdjustSpeed(false);
		}

		AAMPLOG_INFO("Live latency correction is disabled after Pause");
		mDisableRateCorrection = true;

		pipeline_paused = true;
	}
}

/**
 * @brief Set Discontinuity handling period change marked flag
 * @param[in] value Period change marked flag
 */
void PrivateInstanceAAMP::SetIsPeriodChangeMarked(bool value)
{
	// CID:306170 - Data race condition
	std::lock_guard<std::mutex>lock(mDiscoCompleteLock);
	mIsPeriodChangeMarked = value;
	AAMPLOG_TRACE("isPeriodChangeMarked %d", mIsPeriodChangeMarked);

	if (false == mIsPeriodChangeMarked)
	{
		/* Unblock the wait for discontinuity process to complete. */
		mWaitForDiscoToComplete.notify_one();
	}
}

/**
 * @brief Get Discontinuity handling period change marked flag
 * @return Period change marked flag
 */
bool PrivateInstanceAAMP::GetIsPeriodChangeMarked()
{
	// CID:306170 - Data race condition
	std::lock_guard<std::mutex>lock(mDiscoCompleteLock);
	return mIsPeriodChangeMarked;
}

/**
 * @brief complete sending discontinuity data when PTS restamp enabled
 */
void PrivateInstanceAAMP::CompleteDiscontinuityDataDeliverForPTSRestamp(AampMediaType type)
{
	UnblockWaitForDiscontinuityProcessToComplete();
}

/**
 *   @brief GStreamer operation start
 */
void PrivateInstanceAAMP::SyncBegin(void)
{
	mLock.lock();
}

/**
 * @brief GStreamer operation end
 *
 */
void PrivateInstanceAAMP::SyncEnd(void)
{
	mLock.unlock();
}

/**
 * @brief Abort wait for playlist download
 */
void PrivateInstanceAAMP::WakeupLatencyCheck()
{
	mRateCorrectionWait.notify_one();
}

/**
 * @brief Wait until timeout is reached or interrupted
 */
void PrivateInstanceAAMP::TimedWaitForLatencyCheck(int timeInMs)
{
	if(timeInMs > 0 && DownloadsAreEnabled())
	{
		std::unique_lock<std::mutex> lock(mRateCorrectionTimeoutLock);
		if(mRateCorrectionWait.wait_for(lock, std::chrono::milliseconds(timeInMs)) == std::cv_status::timeout)
		{
			AAMPLOG_TRACE(" Timeout exceeded Rate Correction Thread : %d", timeInMs); // make it trace
		}
		else
		{
			mAbortRateCorrection = true;
			AAMPLOG_INFO(" Aborted Rate Correction Thread"); // TRACE
		}
	}
}

/**
 * @brief The helper function which perform tuning
 * Common tune operations used on Tune, Seek, SetRate etc
 */
void PrivateInstanceAAMP::StopRateCorrectionWorkerThread(void)
{
	if (mRateCorrectionThread.joinable())
	{
		try
		{
			mAbortRateCorrection = true;
			WakeupLatencyCheck();
			mRateCorrectionThread.join();
			mCorrectionRate = AAMP_NORMAL_PLAY_RATE;
		}
		catch (exception& exp)
		{
			AAMPLOG_ERR("Rate Correction Thread Failed to Stop : %s ", exp.what());
		}
	}
}
/**
 * @brief The helper function which perform tuning
 * Common tune operations used on Tune, Seek, SetRate etc
 */
void PrivateInstanceAAMP::StartRateCorrectionWorkerThread(void)
{
	try
	{
		bool newTune = IsNewTune();
		bool enabled = ISCONFIGSET_PRIV(eAAMPConfig_EnableLiveLatencyCorrection);
		/** Spawn the rate Correction thread if it is live, new tune, thread not started yet, and rate correction enabled **/
		if(IsLive() && newTune && !mRateCorrectionThread.joinable() && enabled )
		{
			mAbortRateCorrection = false;
			mRateCorrectionThread = std::thread(&PrivateInstanceAAMP::RateCorrectionWorkerThread, this);
			AAMPLOG_INFO("Rate Correction Thread started [%zx]", GetPrintableThreadID(mRateCorrectionThread)); //Print Id - KC
		}
	}
	catch (exception& exp)
	{
		AAMPLOG_ERR("Rate Correction Thread Failed to start : %s ", exp.what());
	}
}

/**
 * @brief Rate correction API call in thread to avoid the time delay for setting rate
 * This is single sleeping thread ; only wake up whenever it needed
 */
void PrivateInstanceAAMP::RateCorrectionWorkerThread(void)
{
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableLiveLatencyCorrection))
	{
		int latencyMonitorInterval = GETCONFIGVALUE_PRIV(eAAMPConfig_LatencyMonitorInterval);
		double normalPlaybackRate = GETCONFIGVALUE_PRIV(eAAMPConfig_NormalLatencyCorrectionPlaybackRate);
		double maxPlaybackRate = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxLatencyCorrectionPlaybackRate);
		double minPlaybackRate = GETCONFIGVALUE_PRIV(eAAMPConfig_MinLatencyCorrectionPlaybackRate);
		int disableRateCorrectionTimeInSeconds = GETCONFIGVALUE_PRIV(eAAMPConfig_RateCorrectionDelay);
		int latencyMonitorDelay = GETCONFIGVALUE_PRIV(eAAMPConfig_LatencyMonitorDelay);
		AAMPLOG_TRACE("latencyMonitorDelay %d latencyMonitorInterval=%d", latencyMonitorDelay,latencyMonitorInterval );
		double latencyMonitorScheduleTime = latencyMonitorDelay - latencyMonitorInterval;
		//To handle latencyMonitorDelay < latencyMonitorInterval case
		if( latencyMonitorScheduleTime < 0)
		{ // clamp!
			AAMPLOG_INFO("unexpected latencyMonitorScheduleTime(%lf) sec", latencyMonitorScheduleTime );
			latencyMonitorScheduleTime = 0.5; //TimedWaitForLatencyCheck here is 500 ms
		}
		while(!mAbortRateCorrection)
		{
			mCorrectionRate = rate; /**< To align with main playback rate start with rate*/
			double rateRequired = normalPlaybackRate; /**< Can be vary for debug*/
			TimedWaitForLatencyCheck(latencyMonitorScheduleTime*1000);
			while(DownloadsAreEnabled())
			{
				interruptibleMsSleep(latencyMonitorInterval * 1000);
				AAMPPlayerState state = GetState();
				if (!mAbortRateCorrection &&!mDisableRateCorrection && DownloadsAreEnabled() && state == eSTATE_PLAYING)
				{
					if(mFirstFragmentTimeOffset < 0)
					{
						AAMPLOG_WARN("First Fragment offset time is invalid, rate correction stopped!");
						mAbortRateCorrection = true;
						break;
					}
					double bufferedDuration = 0.0;
					{
						std::lock_guard<std::recursive_mutex> guard(mStreamLock);
						if (mpStreamAbstractionAAMP)
						{
							bufferedDuration = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec();
						}
					}
					//If the player detect an empty period switch, it will set the  buffer duration as -1 even though  the player has content from previous period.
					//In this case, player should not increase the playback speed to catchup the latency.
					//So Setting isEnoughBuffer  true by default and the value will update accordingly if only buffer duration is greater than -1.
					bool isEnoughBuffer = true;
					if(bufferedDuration > -1.0)
					{
						isEnoughBuffer = bufferedDuration > (mLiveOffset / 2);
					}
					double liveTime = (double)mNewSeekInfo.GetInfo().getUpdateTime()/1000.0;
					double finalProgressTime = (mFirstFragmentTimeOffset) + ((double)mNewSeekInfo.GetInfo().getPosition()/1000.0);
					double latency = liveTime - finalProgressTime;
					if(mProgressReportOffset >= 0)
					{
						// Correction with progress offset
						latency += mProgressReportOffset;
					}
					AAMPLOG_INFO("Current latency is %.02lf current playback rate is %.02lf maxLiveOffset is %.02lf sec, target LiveOffset is %.02lf sec, minLiveOffset is %.02lf sec AvailableBuffer = %.02lf",
					latency, mCorrectionRate, (mLiveOffset + mLiveOffsetDrift ), mLiveOffset, (mLiveOffset - mLiveOffsetDrift), bufferedDuration );
					if ((latency > (mLiveOffset + mLiveOffsetDrift)) && isEnoughBuffer)
					{
						rateRequired = maxPlaybackRate;
					}
					else if (latency < (mLiveOffset - mLiveOffsetDrift))
					{
						rateRequired = minPlaybackRate;
					}
					else if (((latency <= mLiveOffset) && mCorrectionRate ==  maxPlaybackRate) ||
							((latency >= mLiveOffset) && mCorrectionRate == minPlaybackRate) ||
							((mCorrectionRate ==  maxPlaybackRate) && !isEnoughBuffer))
					{
						rateRequired = normalPlaybackRate;
					}

					if (disableRateCorrectionTimeInSeconds > 0 && mDiscStartTime > 0 && true == mRateCorrectionDelay)
					{
						int deltaTime = (int)(NOW_STEADY_TS_SECS- mDiscStartTime);

						AAMPLOG_INFO("mDiscStartTime %ld currenttime=%" PRId64 " delta=%d disableRateCorrectionTimeInSeconds=%d",
							mDiscStartTime, NOW_STEADY_TS_SECS, deltaTime, disableRateCorrectionTimeInSeconds);

						if ( deltaTime > disableRateCorrectionTimeInSeconds )
						{
							mRateCorrectionDelay = false;
							mDiscStartTime = 0;
							AAMPLOG_WARN("Rate correction is enabled after discontinuity processing");
						}
						else
						{
							AAMPLOG_INFO("Rate correction is still disabled because still in discontinuity processing %ld", mDiscStartTime);
						}
					}

					StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
					if (sink)
 					{
 						if( !mRateCorrectionDelay && (mCorrectionRate != rateRequired) && !mDiscontinuityTuneOperationInProgress)
						{
 							if (sink->SetPlayBackRate(rateRequired))
							{
								mCorrectionRate = rateRequired;
								UpdateVideoEndMetrics(rateRequired);
								SendAnomalyEvent(ANOMALY_WARNING, "Rate changed to:%f", rateRequired);
								AAMPLOG_WARN("Rate Changed to : %f Live latency : %lf", rateRequired, latency);
								profiler.IncrementChangeCount(Count_RateCorrection);
							}
 						}
					}
				}
				else
				{
					if (mDisableRateCorrection && DownloadsAreEnabled() && (rate == AAMP_NORMAL_PLAY_RATE && mCorrectionRate != normalPlaybackRate))
					{
						//Rate correction stopping from correction rate so reset to normal
 						StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
						if (sink)
						{
							if (sink->SetPlayBackRate(normalPlaybackRate))
							{
								mCorrectionRate = normalPlaybackRate;
								AAMPLOG_WARN("Rate Changed to : %f", mCorrectionRate);
							}
							else
							{
								AAMPLOG_WARN("Failed to reset the playback rate!!");
							}
 						}
					}
				}
			}
		}
	}
	else
	{
		AAMPLOG_WARN("Rate Correction Ignored Due to Rate Correction disabled from config;  EnableLiveLatencyCorrection [%d]",
		ISCONFIGSET_PRIV(eAAMPConfig_EnableLiveLatencyCorrection));
	}
}

/**
 * @brief API returns true is live stream and playing at the live point
 */
bool PrivateInstanceAAMP::IsAtLivePoint()
{
	if (mpStreamAbstractionAAMP)
	{
		if (IsLiveStream())
		{
			return mpStreamAbstractionAAMP->mIsAtLivePoint;
		}
	}
	return false;
}
/**
 * @brief API to correct the latency by adjusting rate of playback
 */
void PrivateInstanceAAMP::ReportProgress(bool sync, bool beginningOfStream)
{
	AAMPPlayerState state = GetState();
	if (state == eSTATE_SEEKING)
	{
		AAMPLOG_WARN("Progress reporting skipped whilst seeking.");
	}

	//Once GST_MESSAGE_EOS is received, AAMP does not want any stray progress to be sent to player. so added the condition state != eSTATE_COMPLETE
	if (mDownloadsEnabled &&
		(state != eSTATE_IDLE) &&
		(state != eSTATE_RELEASED) &&
		(state != eSTATE_COMPLETE) &&
		(state != eSTATE_SEEKING))
	{
		// set position to 0 if the rewind operation has reached Beginning Of Stream
		double position = beginningOfStream? 0: GetPositionMilliseconds();
		double duration = durationSeconds * 1000.0;
		float speed = pipeline_paused ? 0 : rate;
		double start = -1;
		double end = -1;
		long long videoPTS = -1;
		double videoBufferedDuration = 0.0;
		double audioBufferedDuration = 0.0;
		bool bProcessEvent = true;
		double latency = 0;


		//Report Progress report position based on Availability Start Time
		start = (culledSeconds*1000.0);
		AAMPLOG_TRACE("position = %fms, start = %fms, ProgressReportOffset = %fms, ReportProgressPosn = %fms",
						position, start , (mProgressReportOffset * 1000), mReportProgressPosn);
		if((mProgressReportOffset >= 0) && !IsUninterruptedTSB())
		{
			end = (mAbsoluteEndPosition * 1000);
		}
		else
		{
			end = start + duration;
		}

		if (position > end)
		{ // clamp end
			//AAMPLOG_WARN("aamp clamp end");
			position = end;
		}
		else if (position < start)
		{ // clamp start
			AAMPLOG_WARN( "clamp position %fms < start %fms", position, start );
			position = start;
		}
		DeliverAdEvents(false, position); // use progress reporting as trigger to belatedly deliver ad events
		ReportAdProgress(position);

		if(ISCONFIGSET_PRIV(eAAMPConfig_ReportVideoPTS))
		{
			/*For HLS, tsprocessor.cpp removes the base PTS value and sends to gstreamer.
			**In order to report PTS of video currently being played out, we add the base PTS
			**to video PTS received from gstreamer
			*/
			/*For DASH,mVideoBasePTS value will be zero */
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				videoPTS = sink->GetVideoPTS() + mVideoBasePTS;
			}
		}
		{
			std::lock_guard<std::recursive_mutex> guard(mStreamLock);
			if (mpStreamAbstractionAAMP)
			{
				videoBufferedDuration = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec() * 1000.0;
				audioBufferedDuration = mpStreamAbstractionAAMP->GetBufferedAudioDurationSec() * 1000.0;
			}

		}
		if ((mReportProgressPosn == position) && !pipeline_paused && beginningOfStream != true)
		{
			// Avoid sending the progress event, if the previous position and the current position is same when pipeline is in playing state.
			// Added exception if it's beginning of stream to prevent JSPP not loading previous AD while rewind
			bProcessEvent = false;
		}

		/**mNewSeekInfo is:
		**  -Used by PlayerInstanceAAMP::SetRateInternal() to calculate seek position.
		**  -Included for consistency with previous code but isn't directly related to reporting.
		**  -A good candidate for future refactoring*/
		mNewSeekInfo.Update(position, seek_pos_seconds);
		int CurrentPositionDeltaToManifestEnd = end - position;

		double offset = GetFormatPositionOffsetInMSecs();
		/* Need to get the formatted position, start and end value */
		double reportFormattedCurrPos = position - offset;
		if (start != -1 && end != -1)
		{
			start -= offset;
			end -= offset;
		}

		// If tsb is not available for linear send -1  for start and end
		// so that xre detect this as tsbless playback
		// Override above logic if mEnableSeekableRange is set, used by third-party apps
		if (!ISCONFIGSET_PRIV(eAAMPConfig_EnableSeekRange) && (mContentType == ContentType_LINEAR && !mFogTSBEnabled && !IsLocalAAMPTsb()))
		{
			start = -1;
			end = -1;
		}

		if(IsLiveStream())
		{
			if(mFirstFragmentTimeOffset > 0)
			{
				latency = (mNewSeekInfo.GetInfo().getUpdateTime() - ((mFirstFragmentTimeOffset*1000) + mNewSeekInfo.GetInfo().getPosition()));
				if(mProgressReportOffset >= 0)
				{
					// Correction with progress offset
					latency += (mProgressReportOffset * 1000);
				}
			}
			else
			{
				latency = end - position;
			}
			SetCurrentLatency(latency);
			// update available buffer to Manifest refresh cycle .
			if(mMPDDownloaderInstance != nullptr)
			{
				mMPDDownloaderInstance->SetBufferAvailability((int)videoBufferedDuration);
				mMPDDownloaderInstance->SetCurrentPositionDeltaToManifestEnd(CurrentPositionDeltaToManifestEnd);
			}
		}

		if(GetCurrentlyAvailableBandwidth() != -1)
		{
			mNetworkBandwidth = GetCurrentlyAvailableBandwidth();
		}

		double currentRate;
		if(pipeline_paused)
		{
			currentRate = 0;
		}
		else if( (rate < 0) || (rate > GETCONFIGVALUE_PRIV(eAAMPConfig_MaxLatencyCorrectionPlaybackRate)) || (AAMP_SLOWMOTION_RATE == rate))
		{
			// This is trickplay or slow motion
			currentRate = rate;
		}
		else if(mAampLLDashServiceData.lowLatencyMode)
		{
			currentRate = mLLDashCurrentPlayRate;
		}
		else if (!mAampLLDashServiceData.lowLatencyMode && ISCONFIGSET_PRIV(eAAMPConfig_EnableLiveLatencyCorrection) )
		{
			currentRate = mCorrectionRate;
		}
		else
		{
	   		currentRate  = rate;
		}
		// This is a short-term solution. We are not acquiring StreamLock here, so we could still access mpStreamAbstractionAAMP
		// as its getting deleted. StreamLock is acquired for a lot stuff, so getting it here would lead to unexpected delays
		// Another approach would be to save the bitrate in a local variable as bitrateChangedEvents are fired
		// Planning a tech-debt to stop deleting mpStreamAbstractionAAMP in-between seek/trickplay
		BitsPerSecond bps = 0;
		if (mpStreamAbstractionAAMP)
		{
			bps = mpStreamAbstractionAAMP->GetVideoBitrate();
		}

		ProgressEventPtr evt = std::make_shared<ProgressEvent>(duration, reportFormattedCurrPos, start, end, speed, videoPTS, videoBufferedDuration, audioBufferedDuration, seiTimecode.c_str(), latency, bps, mNetworkBandwidth, currentRate, GetSessionId());

		if (trickStartUTCMS >= 0 && (bProcessEvent || mFirstProgress))
		{
			if (mFirstProgress)
			{
				mFirstProgress = false;
				AAMPLOG_WARN("Send first progress event with position %ld", (long)(reportFormattedCurrPos / 1000));
			}


			if(mAampLLDashServiceData.lowLatencyMode && mConfig->GetConfigOwner(eAAMPConfig_InfoLogging) == AAMP_DEFAULT_SETTING)
			{
				int abrMinBuffer = AAMP_BUFFER_MONITOR_GREEN_THRESHOLD_LLD;
				bool bufferBelowMin = videoBufferedDuration < (abrMinBuffer * 1000);

				if (bufferBelowMin && !mIsLoggingNeeded)
				{
					mIsLoggingNeeded = true;
					AampLogManager::setLogLevel(eLOGLEVEL_INFO);
					SETCONFIGVALUE_PRIV(AAMP_STREAM_SETTING, eAAMPConfig_ProgressLogging, true);
				}
				else if (!bufferBelowMin && mIsLoggingNeeded)
				{
					mIsLoggingNeeded = false;
					AampLogManager::setLogLevel(eLOGLEVEL_WARN);
					SETCONFIGVALUE_PRIV(AAMP_STREAM_SETTING, eAAMPConfig_ProgressLogging, false);
				}
			}
			if (ISCONFIGSET_PRIV(eAAMPConfig_ProgressLogging))
			{
				static int tick;
				int divisor = GETCONFIGVALUE_PRIV(eAAMPConfig_ProgressLoggingDivisor);
				if( divisor==0 || (tick++ % divisor) == 0 )
				{
					AAMPLOG_MIL("aamp pos: [%ld..%ld..%ld..%lld..%.2f..%.2f..%.2f..%s..%ld..%ld..%.2f]",
						(long)(start / 1000),
						(long)(reportFormattedCurrPos / 1000),
						(long)(end / 1000),
						(long long) videoPTS,
						(double)(videoBufferedDuration / 1000.0),
						(double)(audioBufferedDuration /1000.0),
						(latency / 1000),
						seiTimecode.c_str(),
						bps,
						mNetworkBandwidth,
						currentRate);
				}
			}

			long long currTimeMS = aamp_GetCurrentTimeMS();
			long long diff = currTimeMS - mLastTelemetryTimeMS;
			if(mTelemetryInterval > 0 && (diff > mTelemetryInterval))
			{
				mLastTelemetryTimeMS = currTimeMS;
				profiler.SetLatencyParam(latency, (double)(videoBufferedDuration/1000.0), currentRate, mNetworkBandwidth);
				profiler.GetTelemetryParam();
			}

			if (sync)
			{
				mEventManager->SendEvent(evt,AAMP_EVENT_SYNC_MODE);
			}
			else
			{
				mEventManager->SendEvent(evt);
			}

			mReportProgressPosn = position;
		}
	}
}

/**
 *   @brief Report Ad progress event to listeners
 *          Sending Ad progress percentage to JSPP
 */
void PrivateInstanceAAMP::ReportAdProgress(double positionMs)
{
	// This API reports progress of Ad playback in percentage
	double pct = -1;

	if (mDownloadsEnabled && !mAdProgressId.empty())
	{
		// Report Ad progress percentage to JSPP
		double curPosition = 0;
		if(positionMs >= 0)
		{
			curPosition = positionMs;
		}
		else
		{
			curPosition = static_cast<double>(NOW_STEADY_TS_MS);
		}
		if (!pipeline_paused)
		{
			//Update the percentage only if the pipeline is in playing.
			pct = ((curPosition - static_cast<double>(mAdAbsoluteStartTime)) / static_cast<double>(mAdDuration)) * 100;
		}

		if (pct < 0)
		{
			pct = 0;
		}
		if(pct > 100)
		{
			pct = 100;
		}

		if (ISCONFIGSET_PRIV(eAAMPConfig_ProgressLogging))
		{
			static int tick;
			uint64_t adEnd = mAdAbsoluteStartTime + mAdDuration;
			int divisor = GETCONFIGVALUE_PRIV(eAAMPConfig_ProgressLoggingDivisor);
			if( divisor==0 || (tick++ % divisor) == 0 )
			{
				AAMPLOG_WARN("AdId:%s pos:  %" PRIu64 "..%.2lf..%" PRIu64 "..%.2f%%)", mAdProgressId.c_str(), mAdAbsoluteStartTime/1000, curPosition/1000, adEnd/1000, pct);
			}
		}

		AdPlacementEventPtr evt = std::make_shared<AdPlacementEvent>(AAMP_EVENT_AD_PLACEMENT_PROGRESS, mAdProgressId, static_cast<uint32_t>(pct), 0, GetSessionId());
		// AAMP_EVENT_AD_PLACEMENT_START is async so we need AAMP_EVENT_AD_PLACEMENT_PROGRESS to be async as well to keep them in order
		mEventManager->SendEvent(evt, AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief Update playlist duration
 */
void PrivateInstanceAAMP::UpdateDuration(double seconds)
{
	AAMPLOG_INFO("aamp_UpdateDuration(%f)", seconds);
	durationSeconds = seconds;
}

/**
 * @brief Update playlist culling
 */
void PrivateInstanceAAMP::UpdateCullingState(double culledSecs)
{
	if (culledSecs == 0)
	{
		return;
	}

	if((!this->culledSeconds) && culledSecs)
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: culling started, first value %f", culledSecs);
	}

	this->culledSeconds += culledSecs;
	long long limitMs = (long long) std::round(this->culledSeconds * 1000.0);

	for (auto iter = timedMetadata.begin(); iter != timedMetadata.end(); )
	{
		// If the timed metadata has expired due to playlist refresh, remove it from local cache
		// For X-CONTENT-IDENTIFIER, -X-IDENTITY-ADS, X-MESSAGE_REF in DASH which has _timeMS as 0
		if (iter->_timeMS != 0 && iter->_timeMS < limitMs)
		{
			//AAMPLOG_WARN("ERASE(limit:%lld) aamp_ReportTimedMetadata(%lld, '%s', '%s', nb)", limitMs,iter->_timeMS, iter->_name.c_str(), iter->_content.c_str());
			//AAMPLOG_WARN("ERASE(limit:%lld) aamp_ReportTimedMetadata(%lld)", limitMs,iter->_timeMS);
			iter = timedMetadata.erase(iter);
		}
		else
		{
			iter++;
		}
	}

	// Remove contentGaps vector based on culling.
	if(ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling))
	{
		for (auto iter = contentGaps.begin(); iter != contentGaps.end();)
		{
			if (iter->_timeMS != 0 && iter->_timeMS < limitMs)
			{
				iter = contentGaps.erase(iter);
			}
			else
			{
				iter++;
			}
		}
	}

	// Check if we are paused and culled past paused playback position
	// AAMP internally caches fragments in sw and gst buffer, so we should be good here
	// Pipeline will be in Paused state when Lightning trickplay is done. During this state XRE will send the resume position to exit pause state .
	// Issue observed when culled position reaches the paused position during lightning trickplay and player resumes the playback with paused position as playback position ignoring XRE shown position.
	// Fix checks if the player is put into paused state with lighting mode(by checking last stored rate).
  	// In this state player will not come out of Paused state, even if the culled position reaches paused position.
	// The rate check is a special case for a specific player, if this is contradicting to other players, we will have to add a config to enable/disable
	if( pipeline_paused && mpStreamAbstractionAAMP )
	{
		double position = GetPositionSeconds();
		double minPlaylistPositionToResume = (position < maxRefreshPlaylistIntervalSecs) ? position : (position - maxRefreshPlaylistIntervalSecs);
		if (this->culledSeconds >= position)
		{
			if (mPausedBehavior <= ePAUSED_BEHAVIOR_LIVE_IMMEDIATE
					&& this->culledSeconds != culledSecs /* Don't auto resume for first culled on PAUSED */)
			{
				// Immediate play from paused state, Execute player resume.
				// Live immediate - Play from live position
				// Autoplay immediate - Play from start of live window
				if(ePAUSED_BEHAVIOR_LIVE_IMMEDIATE == mPausedBehavior)
				{
					// Enable this flag to perform seek to live.
					mSeekFromPausedState = true;
				}
				AAMPLOG_WARN("Resume playback since playlist start position(%f) has moved past paused position(%f) ", this->culledSeconds, position);
				if (!mAutoResumeTaskPending)
				{
					mAutoResumeTaskPending = true;
					mAutoResumeTaskId = ScheduleAsyncTask(PrivateInstanceAAMP_Resume, (void *)this, "PrivateInstanceAAMP_Resume");
				}
				else
				{
					AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
				}
			}
			else if(mPausedBehavior >= ePAUSED_BEHAVIOR_AUTOPLAY_DEFER)
			{
				// Wait for play() call to resume, enable mSeekFromPausedState for reconfigure.
				// Live differ - Play from live position
				// Autoplay differ -Play from eldest part (start of live window)
				mSeekFromPausedState = true;
				if(ePAUSED_BEHAVIOR_LIVE_DEFER == mPausedBehavior)
				{
					mJumpToLiveFromPause = true;
				}
			}
		}
		else if (this->culledSeconds >= minPlaylistPositionToResume)
		{
			// Here there is a chance that paused position will be culled after next refresh playlist
			// AAMP internally caches fragments in sw buffer after paused position, so we are at less risk
			// Make sure that culledSecs is within the limits of maxRefreshPlaylistIntervalSecs
			// This check helps us to avoid initial culling done by FOG after channel tune

			if (culledSecs <= maxRefreshPlaylistIntervalSecs)
			{
				if (mPausedBehavior <= ePAUSED_BEHAVIOR_LIVE_IMMEDIATE)
				{
					if(ePAUSED_BEHAVIOR_LIVE_IMMEDIATE == mPausedBehavior)
					{
						mSeekFromPausedState = true;
					}
					AAMPLOG_WARN("Resume playback since start position(%f) moved very close to minimum resume position(%f) ", this->culledSeconds, minPlaylistPositionToResume);
					if (!mAutoResumeTaskPending)
					{
						mAutoResumeTaskPending = true;
						mAutoResumeTaskId = ScheduleAsyncTask(PrivateInstanceAAMP_Resume, (void *)this, "PrivateInstanceAAMP_Resume");
					}
					else
					{
						AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
					}
				}
				else if(mPausedBehavior >= ePAUSED_BEHAVIOR_AUTOPLAY_DEFER)
				{
					mSeekFromPausedState = true;
					if(ePAUSED_BEHAVIOR_LIVE_DEFER == mPausedBehavior)
					{
						mJumpToLiveFromPause = true;
					}
				}
			}
			else
			{
				AAMPLOG_WARN("Auto resume playback task already exists, avoid creating duplicates for now!");
			}
		}
	}
}

/**
 * @brief Add listener to aamp events
 */
void PrivateInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	mEventManager->AddEventListener(eventType,eventListener);
}


/**
 * @brief Deregister event lister, Remove listener to aamp events
 */
void PrivateInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	mEventManager->RemoveEventListener(eventType,eventListener);
}

/**
 * @brief IsEventListenerAvailable Check if Event is registered
 */
bool PrivateInstanceAAMP::IsEventListenerAvailable(AAMPEventType eventType)
{
	return mEventManager->IsEventListenerAvailable(eventType);
}

/**
 * @brief Handles DRM errors and sends events to application if required.
 */
void PrivateInstanceAAMP::SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled)
{
	if (event)
	{
		AAMPTuneFailure tuneFailure = event->getFailure();
		int error_code = event->getResponseCode();
		bool isSecClientError = event->getSecclientError();
		int secManagerReasonCode = event->getSecManagerReasonCode();

		if(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure || AAMP_TUNE_LICENCE_REQUEST_FAILED == tuneFailure)
		{
			char description[128] = {};
			//When using secmanager the erro_code would not be less than 100
			if(AAMP_TUNE_LICENCE_REQUEST_FAILED == tuneFailure && (error_code < 100 || (ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager) || ISCONFIGSET_PRIV(eAAMPConfig_UseFireboltSDK))))
			{

				if (isSecClientError)
				{
					if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager) || ISCONFIGSET_PRIV(eAAMPConfig_UseFireboltSDK))
					{
						snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : SecManager Error Code %d:%d", tuneFailureMap[tuneFailure].description,error_code, secManagerReasonCode);
					}
					else
					{
						snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Secclient Error Code %d", tuneFailureMap[tuneFailure].description, error_code);
					}
				}
				else
				{
					snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Curl Error Code %d", tuneFailureMap[tuneFailure].description, error_code);
				}
			}
			else if (AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure && eAUTHTOKEN_TOKEN_PARSE_ERROR == (AuthTokenErrors)error_code)
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Access Token Parse Error", tuneFailureMap[tuneFailure].description);
			}
			else if(AAMP_TUNE_FAILED_TO_GET_ACCESS_TOKEN == tuneFailure && eAUTHTOKEN_INVALID_STATUS_CODE == (AuthTokenErrors)error_code)
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Invalid status code", tuneFailureMap[tuneFailure].description);
			}
			else
			{
				snprintf(description, MAX_ERROR_DESCRIPTION_LENGTH - 1, "%s : Http Error Code %d", tuneFailureMap[tuneFailure].description, error_code);
			}
			SendErrorEvent(tuneFailure, description, isRetryEnabled, event->getSecManagerClassCode(),event->getSecManagerReasonCode(), event->getBusinessStatus(), event->getResponseData());
		}
		else if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
		{
			SendErrorEvent(tuneFailure, NULL, isRetryEnabled, event->getSecManagerClassCode(),event->getSecManagerReasonCode(), event->getBusinessStatus(), event->getResponseData());
		}
		else
		{
			AAMPLOG_WARN("Received unknown error event %d", tuneFailure);
			SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN);
		}
	}
}

/**
 * @brief Handles download errors and sends events to application if required.
 */
void PrivateInstanceAAMP::SendDownloadErrorEvent(AAMPTuneFailure tuneFailure, int error_code)
{
	AAMPTuneFailure actualFailure = tuneFailure;
	bool retryStatus = true;

	if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
	{
		char description[MAX_DESCRIPTION_SIZE] = {};
		if (((error_code >= PARTIAL_FILE_CONNECTIVITY_AAMP) && (error_code <= PARTIAL_FILE_START_STALL_TIMEOUT_AAMP)) || error_code == CURLE_OPERATION_TIMEDOUT)
		{
			switch(error_code)
			{
				case PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP:
						error_code = CURLE_PARTIAL_FILE;
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Download time expired", tuneFailureMap[tuneFailure].description, error_code);
						break;
				case CURLE_OPERATION_TIMEDOUT:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Download time expired", tuneFailureMap[tuneFailure].description, error_code);
						break;
				case PARTIAL_FILE_START_STALL_TIMEOUT_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Start/Stall timeout", tuneFailureMap[tuneFailure].description, CURLE_PARTIAL_FILE);
						break;
				case OPERATION_TIMEOUT_CONNECTIVITY_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Connectivity failure", tuneFailureMap[tuneFailure].description, CURLE_OPERATION_TIMEDOUT);
						break;
				case PARTIAL_FILE_CONNECTIVITY_AAMP:
						snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d, Connectivity failure", tuneFailureMap[tuneFailure].description, CURLE_PARTIAL_FILE);
						break;
			}
		}
		else if(error_code < 100)
		{
			snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Curl Error Code %d", tuneFailureMap[tuneFailure].description, error_code);  //CID:86441 - DC>STRING_BUFFER
		}
		else
		{
			snprintf(description,MAX_DESCRIPTION_SIZE, "%s : Http Error Code %d", tuneFailureMap[tuneFailure].description, error_code);
			if (error_code == 404)
			{
				actualFailure = AAMP_TUNE_CONTENT_NOT_FOUND;
			}
			else if (error_code == 421)	// http 421 - Fog power saving mode failure
			{
				 retryStatus = false;
			}
		}
		if( IsFogTSBSupported() )
		{
			strcat(description, "(FOG)");
		}

		SendErrorEvent(actualFailure, description, retryStatus);
	}
	else
	{
		AAMPLOG_WARN("Received unknown error event %d", tuneFailure);
		SendErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN);
	}
}

/**
 * @brief Sends Anomaly Error/warning messages
 */
void PrivateInstanceAAMP::SendAnomalyEvent(AAMPAnomalyMessageType type, const char* format, ...)
{
	if(NULL != format && mEventManager->IsEventListenerAvailable(AAMP_EVENT_REPORT_ANOMALY))
	{
		va_list args;
		va_start(args, format);

		char msgData[MAX_ANOMALY_BUFF_SIZE];

		msgData[(MAX_ANOMALY_BUFF_SIZE-1)] = 0;
		vsnprintf(msgData, (MAX_ANOMALY_BUFF_SIZE-1), format, args);

		AnomalyReportEventPtr e = std::make_shared<AnomalyReportEvent>(type, msgData, GetSessionId());

		AAMPLOG_INFO("Anomaly evt:%d msg:%s", e->getSeverity(), msgData);
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		va_end(args);  //CID:82734 - VARAGAS
	}
}


/**
 *   @brief  Update playlist refresh interval
 */
void PrivateInstanceAAMP::UpdateRefreshPlaylistInterval(float maxIntervalSecs)
{
	AAMPLOG_INFO("maxRefreshPlaylistIntervalSecs (%f)", maxIntervalSecs);
	maxRefreshPlaylistIntervalSecs = maxIntervalSecs;
}

/**
 * @brief Sends UnderFlow Event messages
 */
void PrivateInstanceAAMP::SendBufferChangeEvent(bool bufferingStopped)
{
	// Buffer Change event indicate buffer availability
	// Buffering stop notification need to be inverted to indicate if buffer available or not
	// BufferChangeEvent with False = Underflow / non-availability of buffer to play
	// BufferChangeEvent with True  = Availability of buffer to play
	BufferingChangedEventPtr e = std::make_shared<BufferingChangedEvent>(!bufferingStopped, GetSessionId());

	SetBufUnderFlowStatus(bufferingStopped);
	AAMPLOG_INFO("PrivateInstanceAAMP: Sending Buffer Change event status (Buffering): %s", (e->buffering() ? "End": "Start"));
#ifdef AAMP_TELEMETRY_SUPPORT
	AAMPTelemetry2 at2(mAppName);
	std::string telemetryName = bufferingStopped?"VideoBufferingStart":"VideoBufferingEnd";
	at2.send(telemetryName,{/*int data*/},{/*string data*/},{/*float data*/});
#endif //AAMP_TELEMETRY_SUPPORT
	SendEvent(e,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief To change the the gstreamer pipeline to pause/play
 */
bool PrivateInstanceAAMP::PausePipeline(bool pause, bool forceStopGstreamerPreBuffering)
{
	bool ret_val = true;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		if (true != sink->Pause(pause, forceStopGstreamerPreBuffering))
		{
			ret_val = false;
		}
		else
		{
			pipeline_paused = pause;
		}
	}
	return ret_val;
}


/**
 * @brief providing the Tune Timemetric info
 */
void PrivateInstanceAAMP::SendTuneMetricsEvent(std::string &timeMetricData)
{
	// Providing the Tune Timemetric info as an event
	TuneTimeMetricsEventPtr e = std::make_shared<TuneTimeMetricsEvent>(timeMetricData, GetSessionId());

	AAMPLOG_INFO("PrivateInstanceAAMP: Sending TuneTimeMetric event: %s", e->getTuneMetricsData().c_str());
	SendEvent(e,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Handles errors and sends events to application if required.
 * For download failures, use SendDownloadErrorEvent instead.
 */
void PrivateInstanceAAMP::SendErrorEvent(AAMPTuneFailure tuneFailure, const char * description, bool isRetryEnabled, int32_t secManagerClassCode, int32_t secManagerReasonCode, int32_t secClientBusinessStatus, const std::string &responseData)
{
	bool sendErrorEvent = false;
	std::unique_lock<std::recursive_mutex> lock(mLock);
	if(mState != eSTATE_ERROR)
	{
		if(IsFogTSBSupported() && mState <= eSTATE_PREPARED)
		{
			// Send a TSB delete request when player is not tuned successfully.
			// If player is once tuned, retune happens with same content and player can reuse same TSB.
			std::string remoteUrl = "127.0.0.1:9080/tsb";
			AampCurlDownloader T1;
			DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
			DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
			inpData->bIgnoreResponseHeader	= true;
			inpData->eRequestType = eCURL_DELETE;
			T1.Initialize(inpData);
			T1.Download(remoteUrl, respData);
		}
		sendErrorEvent = true;
		mState = eSTATE_ERROR;
	}
	lock.unlock();
	if (sendErrorEvent)
	{
		int code;
		const char *errorDescription = NULL;
		DisableDownloads();
		if(tuneFailure >= 0 && tuneFailure < AAMP_TUNE_FAILURE_UNKNOWN)
		{
			if (tuneFailure == AAMP_TUNE_PLAYBACK_STALLED)
			{ // allow config override for stall detection error code
				code = GETCONFIGVALUE_PRIV(eAAMPConfig_StallErrorCode);
			}
			else
			{
				code = tuneFailureMap[tuneFailure].code;
			}
			if(description)
			{
				errorDescription = description;
			}
			else
			{
				errorDescription = tuneFailureMap[tuneFailure].description;
			}
		}
		else
		{
			code = tuneFailureMap[AAMP_TUNE_FAILURE_UNKNOWN].code;
			errorDescription = tuneFailureMap[AAMP_TUNE_FAILURE_UNKNOWN].description;
		}
		MediaErrorEventPtr e = std::make_shared<MediaErrorEvent>(tuneFailure, code, errorDescription, isRetryEnabled, secManagerClassCode, secManagerReasonCode, secClientBusinessStatus, responseData, GetSessionId());
		SendAnomalyEvent(ANOMALY_ERROR, "Error[%d]:%s", tuneFailure, e->getDescription().c_str());
		if (!mAppName.empty())
		{
			AAMPLOG_ERR("%s PLAYER[%d] APP: %s Sending error %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mAppName.c_str(), e->getDescription().c_str());
		}
		else
		{
			AAMPLOG_ERR("%s PLAYER[%d] Sending error %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, e->getDescription().c_str());
		}

		if (rate != AAMP_NORMAL_PLAY_RATE)
		{
			NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, false); // During trick play if the playback failed, send speed change event to XRE to reset its current speed rate.
		}

		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		mFailureReason=tuneFailureMap[tuneFailure].description;

#ifdef AAMP_TELEMETRY_SUPPORT
		AAMPTelemetry2 at2(mAppName);

		std::string telemetryName;

		if(this->mTuneCompleted)
		{
			telemetryName = "VideoPlaybackFailure";
		}
		else
		{
			telemetryName = "VideoStartFailure";
		}

		std::map<std::string, int> intData;
		intData["err"] = tuneFailure; 	// Error code from AAMPTuneFailure enum
		intData["cat"] = code; 			// Error Category from tuneFailureMap.code;

		// Sec Manager Codes used when sec manager is used.
		if(secManagerClassCode >0)
		{
			intData["cls"] = secManagerClassCode; // sec manager class
		}
		if(secManagerReasonCode >0)
		{
			intData["smc"] = secManagerReasonCode; // sec manager reason code
		}
		if(secClientBusinessStatus >0)
		{
			intData["sbc"] = secClientBusinessStatus; // sec manager Business Status
		}

		at2.send(telemetryName,intData,{/* string data */},{/* float data */});
#endif // AAMP_TELEMETRY_SUPPORT
	}
	else
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: Ignore error %d[%s]", (int)tuneFailure, description);
	}
}

void PrivateInstanceAAMP::LicenseRenewal(DrmHelperPtr drmHelper, void* userData)
{
	if (mDRMLicenseManager == nullptr)
	{
		SendAnomalyEvent(ANOMALY_WARNING, "Failed to renew license as mDrmSessionManager not available");
		AAMPLOG_ERR("Failed to renew License as no mDrmSessionManager available");
			AAMPLOG_ERR("DRM is not supported");
		return;
	}
	mDRMLicenseManager->renewLicense(drmHelper, userData, this);
}

/**
 * @brief Send event to listeners
 */
void PrivateInstanceAAMP::SendEvent(AAMPEventPtr eventData, AAMPEventMode eventMode)
{
	mEventManager->SendEvent(eventData, eventMode);
}

/**
 * @brief Notify bit rate change event to listeners
 */
void PrivateInstanceAAMP::NotifyBitRateChangeEvent(BitsPerSecond bitrate, BitrateChangeReason reason, int width, int height, double frameRate, double position, bool GetBWIndex, VideoScanType scantype, int aspectRatioWidth, int aspectRatioHeight)
{
	if(mEventManager->IsEventListenerAvailable(AAMP_EVENT_BITRATE_CHANGED))
	{
		AAMPEventPtr event = std::make_shared<BitrateChangeEvent>((int)aamp_GetCurrentTimeMS(), bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight,
			GetSessionId());
#ifdef AAMP_TELEMETRY_SUPPORT
	AAMPTelemetry2 at2(mAppName);
	std::string telemetryName;
	telemetryName = "VideoBitrateChange";
	std::map<std::string, int> bitrateData;
	bitrateData["bit"] = (int)bitrate;
	bitrateData["wdh"] = width;
	bitrateData["hth"] = height;
	bitrateData["pcap"] = mProfileCappedStatus;
	bitrateData["tw"] = mDisplayWidth;
	bitrateData["th"] = mDisplayHeight;
	bitrateData["sct"] = scantype;
	bitrateData["asw"] = aspectRatioWidth;
	bitrateData["ash"] = aspectRatioHeight;
	std::map<std::string, std::string> bitrateDesc;
	bitrateDesc["desc"] = BITRATEREASON2STRING(reason);
	std::map<std::string, float> bitrateFloat;
	bitrateFloat["frt"] = frameRate;
	bitrateFloat["pos"] = position;
	at2.send(telemetryName,bitrateData,bitrateDesc,bitrateFloat);
#endif //AAMP_TELEMETRY_SUPPORT

		if(GetBWIndex)
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent :: bitrate:%" BITSPERSECOND_FORMAT " desc:%s width:%d height:%d fps:%f position:%f IndexFromTopProfile: %d%s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mpStreamAbstractionAAMP->GetBWIndex(bitrate), (IsFogTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		else
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent :: bitrate:%" BITSPERSECOND_FORMAT " desc:%s width:%d height:%d fps:%f position:%f %s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, (IsFogTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}

		SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
	else
	{
		if(GetBWIndex)
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent ::NO LISTENERS bitrate:%" BITSPERSECOND_FORMAT " desc:%s width:%d height:%d, fps:%f position:%f IndexFromTopProfile: %d%s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, mpStreamAbstractionAAMP->GetBWIndex(bitrate), (IsFogTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
		else
		{
			AAMPLOG_WARN("NotifyBitRateChangeEvent ::NO LISTENERS bitrate:%" BITSPERSECOND_FORMAT " desc:%s width:%d height:%d fps:%f position:%f %s profileCap:%d tvWidth:%d tvHeight:%d, scantype:%d, aspectRatioW:%d, aspectRatioH:%d",
				bitrate, BITRATEREASON2STRING(reason), width, height, frameRate, position, (IsFogTSBSupported()? ", fog": " "), mProfileCappedStatus, mDisplayWidth, mDisplayHeight, scantype, aspectRatioWidth, aspectRatioHeight);
		}
	}

	AAMPLOG_WARN("BitrateChanged:%d", reason);
}


/**
 * @brief Notify speed change event to listeners
 */
void PrivateInstanceAAMP::NotifySpeedChanged(float rate, bool changeState)
{
	if (changeState)
	{
		if (rate == 0)
		{
			SetState(eSTATE_PAUSED);
			if (HasSidecarData())
			{ // has sidecar data
				if (mpStreamAbstractionAAMP)
					mpStreamAbstractionAAMP->MuteSubtitleOnPause();
			}
		}
		else if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			if (mTrickplayInProgress)
			{
				mTrickplayInProgress = false;
			}
			else
			{
				if (HasSidecarData())
				{ // has sidecar data
					if (mpStreamAbstractionAAMP)
						mpStreamAbstractionAAMP->ResumeSubtitleOnPlay(subtitles_muted, mData.get());
				}
			}
			SetState(eSTATE_PLAYING);
		}
		else
		{
			mTrickplayInProgress = true;
			if (HasSidecarData())
			{ // has sidecar data
				if (mpStreamAbstractionAAMP)
					mpStreamAbstractionAAMP->MuteSidecarSubtitles(true);
			}
		}
	}

	if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
	{
		if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			PlayerCCManager::GetInstance()->SetTrickplayStatus(false);
		}
		else
		{
			PlayerCCManager::GetInstance()->SetTrickplayStatus(true);
		}
	}
	if(ISCONFIGSET_PRIV(eAAMPConfig_RepairIframes))
	{
		AAMPLOG_WARN("mRepairIframes is set, sending pseudo rate %f for the actual rate %f", getPseudoTrickplayRate(rate), rate);
		SendEvent(std::make_shared<SpeedChangedEvent>(getPseudoTrickplayRate(rate), GetSessionId()),AAMP_EVENT_ASYNC_MODE);
	}
	else
	{
		SendEvent(std::make_shared<SpeedChangedEvent>(rate, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
	}
	if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager) || ISCONFIGSET_PRIV(eAAMPConfig_UseFireboltSDK))
	{
		mDRMLicenseManager->setPlaybackSpeedState(IsLive(), GetCurrentLatency(), IsAtLivePoint(), GetLiveOffsetMs(),rate, GetStreamPositionMs());
	}
}

/**
 * @brief Send DRM metadata event
 */
void PrivateInstanceAAMP::SendDRMMetaData(DrmMetaDataEventPtr e)
{
	SendEvent(e,AAMP_EVENT_ASYNC_MODE);
	AAMPLOG_WARN("SendDRMMetaData name = %s value = %x", e->getAccessStatus().c_str(), e->getAccessStatusValue());
}

/**
 *   @brief Check if discontinuity processing is pending
 */
bool PrivateInstanceAAMP::IsDiscontinuityProcessPending()
{
	bool vidDiscontinuity = (mVideoFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	return (vidDiscontinuity || audDiscontinuity);
}

/**
 *   @brief get last injected position from video track
 */
double PrivateInstanceAAMP::getLastInjectedPosition()
{
	return (seek_pos_seconds + mpStreamAbstractionAAMP->GetLastInjectedFragmentPosition());
}

/**
 *   @brief Process pending discontinuity and continue playback of stream after discontinuity
 *
 *   @return true if pending discontinuity was processed successful, false if interrupted
 */
bool PrivateInstanceAAMP::ProcessPendingDiscontinuity()
{
	bool ret = true;
	SyncBegin();
	if (mDiscontinuityTuneOperationInProgress)
	{
		SyncEnd();
		AAMPLOG_WARN("PrivateInstanceAAMP: Discontinuity Tune Operation already in progress");
		UnblockWaitForDiscontinuityProcessToComplete();
		return ret; // true so that PrivateInstanceAAMP_ProcessDiscontinuity can cleanup properly
	}
	SyncEnd();

	if (!(DiscontinuitySeenInAllTracks()))
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Discontinuity status of video - (%d), audio - (%d) and aux - (%d)", mProcessingDiscontinuity[eMEDIATYPE_VIDEO], mProcessingDiscontinuity[eMEDIATYPE_AUDIO], mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);
		UnblockWaitForDiscontinuityProcessToComplete();
		return ret; // true so that PrivateInstanceAAMP_ProcessDiscontinuity can cleanup properly
	}

	SyncBegin();
	mDiscontinuityTuneOperationInProgress = true;
	SyncEnd();

	if (DiscontinuitySeenInAllTracks())
	{
		bool continueDiscontProcessing = true;
		AAMPLOG_WARN("PrivateInstanceAAMP: mProcessingDiscontinuity set");
		// there is a chance that synchronous progress event sent will take some time to return back to AAMP
		// This can lead to discontinuity stall detection kicking in. So once we start discontinuity processing, reset the flags
		ResetDiscontinuityInTracks();
		ResetTrackDiscontinuityIgnoredStatus();
		lastUnderFlowTimeMs[eMEDIATYPE_VIDEO] = 0;
		lastUnderFlowTimeMs[eMEDIATYPE_AUDIO] = 0;
		lastUnderFlowTimeMs[eMEDIATYPE_AUX_AUDIO] = 0;

		{
			double newPosition = GetPositionSeconds();
			double injectedPosition = seek_pos_seconds + mpStreamAbstractionAAMP->GetLastInjectedFragmentPosition();
			double startTimeofFirstSample = 0;
			AAMPLOG_WARN("PrivateInstanceAAMP: last injected position:%f position calculated: %f", injectedPosition, newPosition);

			// Reset with injected position from StreamAbstractionAAMP. This ensures that any drift in
			// GStreamer position reporting is taken care of.
			// Set seek_pos_seconds to injected position only in case of westerossink. In cases with
			// brcmvideodecoder, we have noticed a drift of 500ms for HLS-TS assets (due to PTS restamping
			if (injectedPosition != 0 && (fabs(injectedPosition - newPosition) < 5.0) && ISCONFIGSET_PRIV(eAAMPConfig_UseWesterosSink))
			{
				seek_pos_seconds = injectedPosition;
			}
			else
			{
				seek_pos_seconds = newPosition;
			}

			if(!IsUninterruptedTSB() && (mMediaFormat == eMEDIAFORMAT_DASH))
			{
				startTimeofFirstSample = mpStreamAbstractionAAMP->GetStartTimeOfFirstPTS() / 1000;
				if(startTimeofFirstSample > 0)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Position is updated to start time of discontinuity : %lf", startTimeofFirstSample);
					seek_pos_seconds = startTimeofFirstSample;
				}
			}
			AAMPLOG_WARN("PrivateInstanceAAMP: Updated seek_pos_seconds:%f", seek_pos_seconds);
		}
		trickStartUTCMS = -1;

		SyncBegin();
		mProgressReportFromProcessDiscontinuity = true;
		SyncEnd();

		// To notify app of discontinuity processing complete
		ReportProgress();

		// There is a chance some other operation maybe invoked from JS/App because of the above ReportProgress
		// Make sure we have still mDiscontinuityTuneOperationInProgress set
		SyncBegin();
		AAMPLOG_WARN("Progress event sent as part of ProcessPendingDiscontinuity, mDiscontinuityTuneOperationInProgress:%d", mDiscontinuityTuneOperationInProgress);
		mProgressReportFromProcessDiscontinuity = false;
		continueDiscontProcessing = mDiscontinuityTuneOperationInProgress;
		SyncEnd();

		if (continueDiscontProcessing)
		{
			// mStreamLock is not exactly required here, this will be called from Scheduler/GMainLoop based on AAMP config
			// The same thread will be executing operations involving TeardownStream.
			mpStreamAbstractionAAMP->StopInjection();

			GetStreamFormat(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat);

			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				sink->Configure(
					mVideoFormat,
					mAudioFormat,
					mAuxFormat,
					mSubtitleFormat,
					mpStreamAbstractionAAMP->GetESChangeStatus(),
					mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus(),
					mIsTrackIdMismatch /*setReadyAfterPipelineCreation*/);

				/*
				*  Truth table for Flush call as per previous impl for reference
				*  ContentType   | PTS ReStamp | Video Format | DoStreamSinkFlushOnDiscontinuity | Flush
				*  --------------|-------------|--------------|----------------------------------|--------
				*  HLS MP4       | NO          | ISO BMFF     | NO                               | NO
				*  HLS MP4       | YES         | ISO BMFF     | NO                               | NO
				*  HLS TS        | NO          | MPEGTS       | NO                               | YES
				*  HLS TS        | YES         | MPEGTS       | NO                               | YES
				*  DASH MP4      | NO          | ISO BMFF     | NO                               | YES
				*  DASH MP4      | YES         | ISO BMFF     | NO                               | NO
				*  DASH MP4      | YES         | ISO BMFF     | YES                              | YES
				*  In HLS MP4, DASH, mediaProcessor will be doing a delayed flush
				*  Only in DASH, PTS values will be known from manifest. If that is the case, flush from here.
				*  Content types other than HLS and DASH are not expected to have discontinuity.
				*/
				if (mpStreamAbstractionAAMP->DoStreamSinkFlushOnDiscontinuity())
				{
					if(mDiscontinuityFound)
					{
						profiler.ProfileBegin(PROFILE_BUCKET_DISCO_FLUSH);
					}
					sink->Flush(mpStreamAbstractionAAMP->GetFirstPTS(), rate, false);
					if(mDiscontinuityFound)
					{
						profiler.ProfileEnd(PROFILE_BUCKET_DISCO_FLUSH);
					}
				}
			}
			mpStreamAbstractionAAMP->ResetESChangeStatus();

			bool isRateCorrectionEnabled = ISCONFIGSET_PRIV(eAAMPConfig_EnableLiveLatencyCorrection);
			int  disableRateCorrectionTimeInSeconds = GETCONFIGVALUE_PRIV(eAAMPConfig_RateCorrectionDelay);
			if( disableRateCorrectionTimeInSeconds > 0  && isRateCorrectionEnabled )
			{
				mRateCorrectionDelay = true;
				mDiscStartTime = NOW_STEADY_TS_SECS;
				AAMPLOG_WARN("Rate correction is disabled on discontinuity processing : %ld", mDiscStartTime);
			}
			else
			{
				AAMPLOG_WARN("DisableRateCorrectionTimeInSeconds : %d isRateCorrectionEnabled : %d", disableRateCorrectionTimeInSeconds, isRateCorrectionEnabled);
			}

			if(mDiscontinuityFound)
			{
				profiler.ProfileBegin(PROFILE_BUCKET_DISCO_FIRST_FRAME);
			}

			// Reset clock sync on discontinuity processing. Segment event as part of flush will send a new timestamp packet to subtec.
			m_lastSubClockSyncTime = std::chrono::system_clock::now();
			mpStreamAbstractionAAMP->StartInjection();
			if (sink)
			{
				sink->Stream();
			}
			mIsTrackIdMismatch = false;
		}
		else
		{
			ret = false;
			AAMPLOG_WARN("PrivateInstanceAAMP: mDiscontinuityTuneOperationInProgress was reset during operation, since other command received from app!");
		}
	}

	if (ret)
	{
		SyncBegin();
		mDiscontinuityTuneOperationInProgress = false;
		SyncEnd();
	}

	UnblockWaitForDiscontinuityProcessToComplete();
	return ret;
}

/**
 * @brief Get the Current Audio Track Id
 * Currently it is implemented for AC4 track selection only
 * @return int return the index number of current audio track selected
 */
int PrivateInstanceAAMP::GetCurrentAudioTrackId()
{
	int trackId = -1;
	AudioTrackInfo currentAudioTrack;

	/** Only select track Id for setting gstplayer in case of muxed ac4 stream */
	if (mpStreamAbstractionAAMP->GetCurrentAudioTrack(currentAudioTrack) && (currentAudioTrack.codec.find("ac4") == std::string::npos) && currentAudioTrack.isMuxed )
	{
		AAMPLOG_INFO("Found AC4 track as current Audio track  index = %s language - %s role - %s codec %s type %s bandwidth = %ld",
		currentAudioTrack.index.c_str(), currentAudioTrack.language.c_str(), currentAudioTrack.rendition.c_str(),
		currentAudioTrack.codec.c_str(), currentAudioTrack.contentType.c_str(), currentAudioTrack.bandwidth);
		trackId = std::stoi( currentAudioTrack.index );

	}

	return trackId;
}

/**
 * @brief Process EOS from Sink and notify listeners if required
 */
void PrivateInstanceAAMP::NotifyEOSReached()
{
	bool isDiscontinuity = IsDiscontinuityProcessPending();
	bool isLive = IsLive();

	AAMPLOG_MIL("Enter . processingDiscontinuity %d isLive %d", isDiscontinuity, isLive);
	mDiscontinuityFound = isDiscontinuity;
	if(mDiscontinuityFound)
	{
		profiler.ProfileBegin(PROFILE_BUCKET_DISCO_TOTAL);
	}
	if (!isDiscontinuity)
	{
		/*
		This appears to be caused by late calls to previously stopped/destroyed objects due to a scheduling issue.
		In this case it makes sense to exit this function ASAP.
		A more complete (larger, higher risk, more time consuming, threadsafe) change to scheduling is required in the future.
		*/
		// Used TryStreamLock() to avoid crash when mpStreamAbstractionAAMP gets deleted by SetRate b/w checking for
		// mpStreamAbstractionAAMP not null & IsEOSReached()
		if( TryStreamLock() )
		{
			int ret = false;
			if(!mpStreamAbstractionAAMP)
			{
				AAMPLOG_ERR("null Stream Abstraction AAMP");
				ret = true;
			}
			else if (!mpStreamAbstractionAAMP->IsEOSReached())
			{
				AAMPLOG_ERR("Bogus EOS event received from GStreamer, discarding it!");
				ret = true;
			}
			ReleaseStreamLock();
			if (ret)
			{
				return;
			}
		}
		else
		{
			AAMPLOG_WARN("StreamLock not available");
		}

		if (!isLive && rate > AAMP_RATE_PAUSE)
		{
			SetState(eSTATE_COMPLETE);
			SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
			if (ContentType_EAS == mContentType)
			{
				StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
				if (sink)
				{
					sink->Stop(false);
				}
			}
			SendAnomalyEvent(ANOMALY_TRACE, "Generating EOS event");
			trickStartUTCMS = -1;
			return;
		}

		/* If rate is normal play, no need to seek to live etc. This can be due to the EPG changing rate from RWD to play near begging of the TSB. */
		if (rate < AAMP_RATE_PAUSE)
		{
			seek_pos_seconds = culledSeconds;
			AAMPLOG_WARN("Updated seek_pos_seconds %f on BOS", seek_pos_seconds);
			if (trickStartUTCMS == -1)
			{
				// Resetting trickStartUTCMS if it's default due to no first frame on high speed rewind. This enables ReportProgress to
				// send BOS event to JSPP
				ResetTrickStartUTCTime();
				AAMPLOG_INFO("Resetting trickStartUTCMS to %lld since no first frame on trick play rate %f", trickStartUTCMS, rate);
			}
			// A new report progress event to be emitted with position 0 when rewind reaches BOS
			ReportProgress(true, true);
			rate = AAMP_NORMAL_PLAY_RATE;
			AcquireStreamLock();
			TuneHelper(eTUNETYPE_SEEK);
			ReleaseStreamLock();
			NotifySpeedChanged(rate);
		}
		else if (rate > AAMP_NORMAL_PLAY_RATE)
		{
			rate = AAMP_NORMAL_PLAY_RATE;
			AcquireStreamLock();
			TuneHelper(eTUNETYPE_SEEKTOLIVE);
			ReleaseStreamLock();
			NotifySpeedChanged(rate);
		}
	}
	else
	{
		ProcessPendingDiscontinuity();
		mCondDiscontinuity.notify_one();
		// EOS reached with discontinuity handling, send events without position check
		DeliverAdEvents();
		AAMPLOG_WARN("PrivateInstanceAAMP:  EOS due to discontinuity handled");
	}
}

/**
 * @brief Notify when entering live point to listeners
 */
void PrivateInstanceAAMP::NotifyOnEnteringLive()
{
	if (discardEnteringLiveEvt)
	{
		return;
	}
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_ENTERING_LIVE, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief track description string from TrackType enum
 */
static std::string TrackTypeString(TrackType track)
{
	return GetMediaTypeName((AampMediaType)track);
}

/**
* @brief Additional Tune Fail Diagnostics
 */
void PrivateInstanceAAMP::AdditionalTuneFailLogEntries()
{
	{
		std::string downloadsBlockedMessage = "Downloads";
		if (!mbDownloadsBlocked)
		{
			downloadsBlockedMessage += " not";
		}
		downloadsBlockedMessage += " blocked, track download status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			downloadsBlockedMessage+=TrackTypeString((TrackType)i);
			if (!mbTrackDownloadsBlocked[i])
			{
				downloadsBlockedMessage += " not";
			}
			downloadsBlockedMessage += " blocked, ";
		}
		AAMPLOG_WARN("%s", downloadsBlockedMessage.c_str());
	}

	{
		std::string injectionBlockedMessage = "Track injection status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			injectionBlockedMessage+=TrackTypeString((TrackType)i);
			if (!mTrackInjectionBlocked[i])
			{
				injectionBlockedMessage += " not";
			}
			injectionBlockedMessage += " blocked, ";
		}
		AAMPLOG_WARN("%s", injectionBlockedMessage.c_str());
	}

	if(mpStreamAbstractionAAMP)
	{
		std::string trackBufferStatusMessage = "Track buffer status: ";
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			trackBufferStatusMessage+=TrackTypeString((TrackType)i);
			auto track = mpStreamAbstractionAAMP->GetMediaTrack(static_cast<TrackType>(i));
			if(nullptr != track)
			{
				const auto status = track->GetBufferStatus();
				trackBufferStatusMessage += " ";
				switch (status)
				{
					case BUFFER_STATUS_GREEN:
						trackBufferStatusMessage += "green";
						break;
					case BUFFER_STATUS_YELLOW:
						trackBufferStatusMessage += "yellow";
						break;
					case BUFFER_STATUS_RED:
						trackBufferStatusMessage += "red";
						break;
					default:
						trackBufferStatusMessage += "unknown";
						break;
				}
			}
			else
			{
				trackBufferStatusMessage += "invalid track";
			}
			trackBufferStatusMessage += ", ";
		}
		AAMPLOG_WARN( "%s", trackBufferStatusMessage.c_str());
	}
}

/**
 * @brief Profiler for failure tune
 */
void PrivateInstanceAAMP::TuneFail(bool fail)
{
	AAMPPlayerState state = GetState();
	TuneEndMetrics mTuneMetrics = {0, 0, 0,0,0,0,0,0,0,(ContentType)0};
	mTuneMetrics.mTotalTime                 = (int)NOW_STEADY_TS_MS ;
	mTuneMetrics.success         	 	= ((state != eSTATE_ERROR) ? -1 : !fail);
	int streamType 				= getStreamType();
	mTuneMetrics.mFirstTune			= mFirstTune;
	mTuneMetrics.mTimedMetadata 	 	= (int)timedMetadata.size();
	mTuneMetrics.mTimedMetadataStartTime 	= mTimedMetadataStartTime;
	mTuneMetrics.mTimedMetadataDuration  	= (int)mTimedMetadataDuration;
	mTuneMetrics.mTuneAttempts 		= mTuneAttempts;
	mTuneMetrics.contentType 		= mContentType;
	mTuneMetrics.streamType 		= streamType;
	mTuneMetrics.mFogTSBEnabled             	= mFogTSBEnabled;
	if(mTuneMetrics.success  == -1 && mPlayerPreBuffered)
	{
		LogPlayerPreBuffered();        //Need to calculate prebuffered time when tune interruption happens with player prebuffer
	}
	bool eventAvailStatus = IsEventListenerAvailable(AAMP_EVENT_TUNE_TIME_METRICS);
	std::string tuneData("");
	activeInterfaceWifi =  pPlayerExternalsInterface->GetActiveInterface();
	profiler.TuneEnd(mTuneMetrics, mAppName,(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mPlayerPreBuffered, durationSeconds, activeInterfaceWifi, mFailureReason, eventAvailStatus ? &tuneData : NULL);
	if(eventAvailStatus)
	{
		SendTuneMetricsEvent(tuneData);
	}
	AdditionalTuneFailLogEntries();
}

/**
 *  @brief Notify tune end for profiling/logging
 */
void PrivateInstanceAAMP::LogTuneComplete(void)
{
	TuneEndMetrics mTuneMetrics = {0, 0, 0,0,0,0,0,0,0,(ContentType)0};

	mTuneMetrics.success 		 	 = true;
	int streamType 				 = getStreamType();
	mTuneMetrics.contentType 		 = mContentType;
	mTuneMetrics.mTimedMetadata 	 	 = (int)timedMetadata.size();
	mTuneMetrics.mTimedMetadataStartTime 	 = mTimedMetadataStartTime;
	mTuneMetrics.mTimedMetadataDuration      = (int)mTimedMetadataDuration;
	mTuneMetrics.mTuneAttempts 		 = mTuneAttempts;
	mTuneMetrics.streamType 		 = streamType;
	mTuneMetrics.mFogTSBEnabled              = mFogTSBEnabled;
	mTuneMetrics.mFirstTune                  = mFirstTune;
	bool eventAvailStatus = IsEventListenerAvailable(AAMP_EVENT_TUNE_TIME_METRICS);
	std::string tuneData("");
	activeInterfaceWifi =  pPlayerExternalsInterface->GetActiveInterface();
	profiler.TuneEnd(mTuneMetrics,mAppName,(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mPlayerPreBuffered, durationSeconds, activeInterfaceWifi, mFailureReason, eventAvailStatus ? &tuneData : NULL);
	if(eventAvailStatus)
	{
		SendTuneMetricsEvent(tuneData);
	}
	//update tunedManifestUrl if FOG was NOT used as manifestUrl might be updated with redirected url.
	if(!IsFogTSBSupported())
	{
		SetTunedManifestUrl(); /* Redirect URL in case on VOD */
	}
	if (!mTuneCompleted)
	{
		if(mLogTune)
		{
			char classicTuneStr[AAMP_MAX_PIPE_DATA_SIZE];
			mLogTune = false;
			if (ISCONFIGSET_PRIV(eAAMPConfig_XRESupportedTune)) {
				profiler.GetClassicTuneTimeInfo(mTuneMetrics.success, mTuneAttempts, mfirstTuneFmt, mPlayerLoadTime, streamType, IsLive(), durationSeconds, classicTuneStr);
				SendMessage2Receiver(E_AAMP2Receiver_TUNETIME,classicTuneStr);
			}
			mFirstTune = false;
		}
		mTuneCompleted = true;
		AAMPAnomalyMessageType eMsgType = AAMPAnomalyMessageType::ANOMALY_TRACE;
		if(mTuneAttempts > 1 )
		{
			eMsgType = AAMPAnomalyMessageType::ANOMALY_WARNING;
		}
		std::string playbackType = GetContentTypString();

		if(mContentType == ContentType_LINEAR)
		{
			if(mFogTSBEnabled)
			{
				playbackType.append(":TSB=true");
			}
			else
			{
				playbackType.append(":TSB=false");
			}
		}

		SendAnomalyEvent(eMsgType, "Tune attempt#%d. %s:%s URL:%s", mTuneAttempts,playbackType.c_str(),getStreamTypeString().c_str(),GetTunedManifestUrl());
	}
	AampLogManager::setLogLevel(eLOGLEVEL_WARN);
}

/**
 *  @brief Notifies profiler that first frame is presented
 */
void PrivateInstanceAAMP::LogFirstFrame(void)
{
	profiler.ProfilePerformed(PROFILE_BUCKET_FIRST_FRAME);
}

/**
 *  @brief Profile Player changed from background to foreground i.e prebuffered
 */
void PrivateInstanceAAMP::ResetProfileCache(void)
{
	profiler.ProfileReset(PROFILE_BUCKET_INIT_VIDEO);
	profiler.ProfileReset(PROFILE_BUCKET_INIT_AUDIO);
	profiler.ProfileReset(PROFILE_BUCKET_INIT_SUBTITLE);
	profiler.ProfileReset(PROFILE_BUCKET_INIT_AUXILIARY);
	profiler.ProfileReset(PROFILE_BUCKET_FRAGMENT_VIDEO);
	profiler.ProfileReset(PROFILE_BUCKET_FRAGMENT_AUDIO);
	profiler.ProfileReset(PROFILE_BUCKET_FRAGMENT_SUBTITLE);
	profiler.ProfileReset(PROFILE_BUCKET_FRAGMENT_AUXILIARY);
}
void PrivateInstanceAAMP::ActivatePlayer(void)
{
	AampStreamSinkManager::GetInstance().ActivatePlayer(this);
}
/**
 *  @brief Profile Player changed from background to foreground i.e prebuffered
 */
void PrivateInstanceAAMP::LogPlayerPreBuffered(void)
{
	profiler.ProfilePerformed(PROFILE_BUCKET_PLAYER_PRE_BUFFERED);
}

/**
 *   @brief Notifies profiler that drm initialization is complete
 */
void PrivateInstanceAAMP::LogDrmInitComplete(void)
{
	profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
}

/**
 *   @brief Notifies profiler that decryption has started
 */
void PrivateInstanceAAMP::LogDrmDecryptBegin(ProfilerBucketType bucketType)
{
	profiler.ProfileBegin(bucketType);
}

/**
 *   @brief Notifies profiler that decryption has ended
 */
void PrivateInstanceAAMP::LogDrmDecryptEnd(int bucketTypeIn)
{
	ProfilerBucketType bucketType = (ProfilerBucketType)bucketTypeIn;
	profiler.ProfileEnd(bucketType);
}

/**
 * @brief Stop downloads of all tracks.
 * Used by aamp internally to manage states
 */
void PrivateInstanceAAMP::StopDownloads()
{
	AAMPLOG_INFO("Stop downloads");
	if (!mbDownloadsBlocked)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mbDownloadsBlocked = true;
	}
}

/**
 * @brief Resume downloads of all tracks.
 * Used by aamp internally to manage states
 */
void PrivateInstanceAAMP::ResumeDownloads()
{
	AAMPLOG_INFO("Resume downloads");
	if (mbDownloadsBlocked)
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mbDownloadsBlocked = false;
	}
}

/**
 * @brief Stop downloads for a track.
 * Called from StreamSink to control flow
 */
void PrivateInstanceAAMP::StopTrackDownloads(AampMediaType type)
{ // called from gstreamer main event loop
	if (!mbTrackDownloadsBlocked[type])
	{
		AAMPLOG_INFO("gstreamer-enough-data from source[%d]", type);
		{
			std::lock_guard<std::recursive_mutex> guard(mLock);
			mbTrackDownloadsBlocked[type] = true;
		}
		NotifySinkBufferFull(type);
	}
	AAMPLOG_TRACE("PrivateInstanceAAMP:: Exit. type = %d", (int)type);
}

/**
 * @brief Resume downloads for a track.
 * Called from StreamSink to control flow
 */
void PrivateInstanceAAMP::ResumeTrackDownloads(AampMediaType type)
{ // called from gstreamer main event loop
	if (mbTrackDownloadsBlocked[type])
	{
		AAMPLOG_INFO("gstreamer-needs-data from source[%d]", type);
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mbTrackDownloadsBlocked[type] = false;
	}
	AAMPLOG_TRACE("PrivateInstanceAAMP::Exit. type = %d", (int)type);
}

/**
 *  @brief Block the injector thread until gstreamer needs buffer/more data.
 */
void PrivateInstanceAAMP::BlockUntilGstreamerWantsData(void(*cb)(void), int periodMs, int track)
{ // called from FragmentCollector thread; blocks until gstreamer wants data
	AAMPLOG_DEBUG("PrivateInstanceAAMP::Enter. type = %d and downloads:%d",  track, mbTrackDownloadsBlocked[track]);
	int elapsedMs = 0;
	while (mbDownloadsBlocked || mbTrackDownloadsBlocked[track])
	{
		if (!mDownloadsEnabled || mTrackInjectionBlocked[track])
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: track:%d interrupted. mDownloadsEnabled:%d mTrackInjectionBlocked:%d", track, mDownloadsEnabled, mTrackInjectionBlocked[track]);
			break;
		}
		if (cb && periodMs)
		{ // support for background tasks, i.e. refreshing manifest while gstreamer doesn't need additional data
			if (elapsedMs >= periodMs)
			{
				cb();
				elapsedMs -= periodMs;
			}
			elapsedMs += 10;
		}
		interruptibleMsSleep(10);
	}
	AAMPLOG_DEBUG("PrivateInstanceAAMP::Exit. type = %d",  track);
}

/**
 * @brief Curl initialization function
 */
void PrivateInstanceAAMP::CurlInit(AampCurlInstance startIdx, unsigned int instanceCount, std::string proxyName)
{
	int instanceEnd = startIdx + instanceCount;
	std::string UserAgentString;
	UserAgentString=mConfig->GetUserAgentString();
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	CurlStore::GetCurlStoreInstance(this).CurlInit(this, startIdx, instanceCount, proxyName);
}

/**
 * @brief Storing audio language list
 */
void PrivateInstanceAAMP::StoreLanguageList(const std::set<std::string> &langlist)
{
	// store the language list
	int langCount = (int)langlist.size();
	if (langCount > MAX_LANGUAGE_COUNT)
	{
		langCount = MAX_LANGUAGE_COUNT; //boundary check
	}
	mMaxLanguageCount = langCount;
	std::set<std::string>::const_iterator iter = langlist.begin();
	for (int cnt = 0; cnt < langCount; cnt++, iter++)
	{
		strncpy(mLanguageList[cnt], iter->c_str(), MAX_LANGUAGE_TAG_LENGTH);
		mLanguageList[cnt][MAX_LANGUAGE_TAG_LENGTH-1] = 0;
		if( this->mVideoEnd )
		{
			mVideoEnd->Setlanguage(VideoStatTrackType::STAT_AUDIO, (*iter), cnt+1);
		}
	}
}

/**
 * @brief Checking whether audio language supported
 */
bool PrivateInstanceAAMP::IsAudioLanguageSupported (const char *checkLanguage)
{
	bool retVal =false;
	for (int cnt=0; cnt < mMaxLanguageCount; cnt ++)
	{
		if(strncmp(mLanguageList[cnt], checkLanguage, MAX_LANGUAGE_TAG_LENGTH) == 0)
		{
			retVal = true;
			break;
		}
	}

	if(mMaxLanguageCount == 0)
	{
		AAMPLOG_WARN("IsAudioLanguageSupported No Audio language stored !!!");
	}
	else if(!retVal)
	{
		AAMPLOG_WARN("IsAudioLanguageSupported lang[%s] not available in list",checkLanguage);
	}
	return retVal;
}

/**
 * @brief Set curl timeout(CURLOPT_TIMEOUT)
 */
void PrivateInstanceAAMP::SetCurlTimeout(long timeoutMS, AampCurlInstance instance)
{
	if(ContentType_EAS == mContentType)
		return;
	if(instance < eCURLINSTANCE_MAX && curl[instance])
	{
		CURL_EASY_SETOPT_LONG(curl[instance], CURLOPT_TIMEOUT_MS, timeoutMS);
		curlDLTimeout[instance] = timeoutMS;
	}
	else
	{
		AAMPLOG_ERR("Failed to update timeout for curl instance %d",instance);
	}
}

/**
 * @brief Terminate curl contexts
 */
void PrivateInstanceAAMP::CurlTerm(AampCurlInstance startIdx, unsigned int instanceCount)
{
	int instanceEnd = startIdx + instanceCount;
	assert (instanceEnd <= eCURLINSTANCE_MAX);

	if (ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && \
		( startIdx == eCURLINSTANCE_VIDEO ) && (eCURLINSTANCE_AUX_AUDIO < instanceEnd) )
	{
		for(int i=0; i<eCURLINSTANCE_MAX;++i)
		{
			if(curlhost[i]->curl)
			{
				CurlStore::GetCurlStoreInstance(this).CurlTerm(this, (AampCurlInstance)i, 1, mIsFlushFdsInCurlStore, curlhost[i]->hostname);
			}
			curlhost[i]->isRemotehost=true;
			curlhost[i]->redirect=true;
		}
	}

	CurlStore::GetCurlStoreInstance(this).CurlTerm(this, startIdx, instanceCount,  mIsFlushFdsInCurlStore);
}

/**
 * @brief GetPlaylistCurlInstance -  Function to return the curl instance for playlist download
 * Considers parallel download to decide the curl instance
 * @return AampCurlInstance - curl instance for download
 */
AampCurlInstance PrivateInstanceAAMP::GetPlaylistCurlInstance(AampMediaType type, bool isInitialDownload)
{
	AampCurlInstance retType = eCURLINSTANCE_MANIFEST_MAIN;
	bool indivCurlInstanceFlag = false;

	// Removed condition check to get config value of parallel playlist download, Now by default select parallel playlist for non init downloads
	indivCurlInstanceFlag = isInitialDownload ? false : true;
	if(indivCurlInstanceFlag)
	{
		switch(type)
		{
			case eMEDIATYPE_PLAYLIST_VIDEO:
			case eMEDIATYPE_PLAYLIST_IFRAME:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO;
				break;
			case eMEDIATYPE_PLAYLIST_AUDIO:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_AUDIO;
				break;
			case eMEDIATYPE_PLAYLIST_SUBTITLE:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_SUBTITLE;
				break;
			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
				retType = eCURLINSTANCE_MANIFEST_PLAYLIST_AUX_AUDIO;
				break;
			default:
				break;
		}
	}
	return retType;
}

/**
 * @brief Reset bandwidth value
 * Artificially resetting the bandwidth. Low for quicker tune times
 */
void PrivateInstanceAAMP::ResetCurrentlyAvailableBandwidth(long bitsPerSecond , bool trickPlay,int profile)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	if (mAbrBitrateData.size())
	{
		mAbrBitrateData.erase(mAbrBitrateData.begin(),mAbrBitrateData.end());
	}
}

/**
 * @brief Get the current network bandwidth
 * using most recently recorded 3 samples
 * @return Available bandwidth in bps
 */
BitsPerSecond PrivateInstanceAAMP::GetCurrentlyAvailableBandwidth(void)
{
	// 1. Check for any old bitrate beyond threshold time . remove those before calculation
	// 2. Sort and get median
	// 3. if any outliers  , remove those entries based on a threshold value.
	// 4. Get the average of remaining data.
	// 5. if no item in the list , return -1 . Caller to ignore bandwidth based processing

	std::vector<BitsPerSecond> tmpData;
	long ret = -1;
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mhAbrManager.UpdateABRBitrateDataBasedOnCacheLife(mAbrBitrateData,tmpData);
	}
	if (tmpData.size())
	{
		//AAMPLOG_WARN("NwBW with newlogic size[%d] avg[%ld] ",tmpData.size(), avg/tmpData.size());
		ret =mhAbrManager.UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
		mAvailableBandwidth = ret;
		//Store the PersistBandwidth and UpdatedTime on ABRManager
		//Bitrate Update only for foreground player
		if(ISCONFIGSET_PRIV(eAAMPConfig_PersistLowNetworkBandwidth)||ISCONFIGSET_PRIV(eAAMPConfig_PersistHighNetworkBandwidth))
		{
			if(mAvailableBandwidth  > 0 && mbPlayEnabled)
			{
				ABRManager::setPersistBandwidth(mAvailableBandwidth );
				ABRManager::mPersistBandwidthUpdatedTime = aamp_GetCurrentTimeMS();
			}
		}
	}
	else
	{
		//AAMPLOG_WARN("No prior data available for abr , return -1 ");
		ret = -1;
	}
	return ret;
}

/**
 * @brief Set track data for CMCD data collection
 *
 * @param mediaType Type of media track (video or audio)
 */
void PrivateInstanceAAMP::SetCMCDTrackData(AampMediaType mediaType)
{
	MediaTrack *mediaTrack = NULL;
	BitsPerSecond currentBitrate;
	switch( mediaType )
	{
		case eMEDIATYPE_VIDEO:
			currentBitrate = mpStreamAbstractionAAMP->GetVideoBitrate();
			mediaTrack = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO);
			break;
		case eMEDIATYPE_AUDIO:
			currentBitrate = mpStreamAbstractionAAMP->GetAudioBitrate();
			mediaTrack = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_AUDIO);
			break;
		default:
			break;
	}
	if( mediaTrack )
	{
		int bufferedDurationMs = (int)(mediaTrack->GetBufferedDuration()*1000);
		bool bufferRedStatus = (mediaTrack->GetBufferStatus() == BUFFER_STATUS_RED);
		int kBitsPerSecond = (int)(currentBitrate/1000);
		mCMCDCollector->SetTrackData( mediaType, bufferRedStatus, bufferedDurationMs, kBitsPerSecond, IsMuxedStream() );
	}
}

/**
 * @brief Download a file from the CDN
 */
bool PrivateInstanceAAMP::GetFile( std::string remoteUrl, AampMediaType mediaType, AampGrowableBuffer *buffer, std::string& effectiveUrl, int * http_error, double *downloadTimeS, const char *range, unsigned int curlInstance, bool resetBuffer, BitsPerSecond *bitrate, int * fogError, double fragmentDurationS, ProfilerBucketType bucketType, int maxInitDownloadTimeMS)
{
	if( ISCONFIGSET_PRIV(eAAMPConfig_CurlThroughput) )
	{
		AAMPLOG_MIL( "curl-begin type=%d", mediaType);
	}
	if( bucketType!=PROFILE_BUCKET_TYPE_COUNT)
	{
		profiler.ProfileBegin(bucketType);
	}
	MediaTypeTelemetry mediaTypeTelemetry = aamp_GetMediaTypeForTelemetry(mediaType);
	replace( remoteUrl, " ", "%20" ); // CURL gives error if passed URL containing whitespace

	int http_code = -1;
	double total = 0;
	bool ret = false;
	int downloadAttempt = 0;
	struct curl_slist* httpHeaders = NULL;
	CURLcode res = CURLE_OK;
	int fragmentDurationMs = (int)(fragmentDurationS*1000);

	int maxDownloadAttempt = 1;
	switch( mediaType )
	{
		case eMEDIATYPE_INIT_VIDEO:
		case eMEDIATYPE_INIT_AUDIO:
		case eMEDIATYPE_INIT_SUBTITLE:
		case eMEDIATYPE_INIT_AUX_AUDIO:
		case eMEDIATYPE_INIT_IFRAME:
			maxDownloadAttempt += GETCONFIGVALUE_PRIV(eAAMPConfig_InitFragmentRetryCount);
			break;
		default:
			maxDownloadAttempt += DEFAULT_DOWNLOAD_RETRY_COUNT;
			break;
	}

	if( mediaType == eMEDIATYPE_MANIFEST && ISCONFIGSET_PRIV(eAAMPConfig_CurlHeader) )
	{ // append custom uri parameter with remoteUrl at the end before curl request if curlHeader logging enabled.
		std::string uriParameter = GETCONFIGVALUE_PRIV(eAAMPConfig_URIParameter);
		if( !uriParameter.empty() )
		{
			if( remoteUrl.find("?") == std::string::npos )
			{
				uriParameter[0] = '?';
			}
			remoteUrl.append(uriParameter.c_str());
		}
	}

	if (resetBuffer)
	{
		buffer->Clear();
	}

	if (mDownloadsEnabled)
	{
		int downloadTimeMS = 0;
		bool isDownloadStalled = false;
		CurlAbortReason abortReason = eCURL_ABORT_REASON_NONE;
		double connectTime = 0;

		CURL* curl = GetCurlInstanceForURL(remoteUrl,curlInstance);

		AAMPLOG_INFO("aamp url:%d,%d,%d,%f,%s", mediaTypeTelemetry, mediaType, curlInstance, fragmentDurationS, remoteUrl.c_str());
		CurlCallbackContext context;
		if (curl)
		{
			CURL_EASY_SETOPT_STRING(curl, CURLOPT_URL, remoteUrl.c_str());
			if(this->mAampLLDashServiceData.lowLatencyMode)
			{
				CURL_EASY_SETOPT_LONG(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
				context.remoteUrl = remoteUrl;
			}
			context.aamp = this;
			context.buffer = buffer;
			context.responseHeaderData = &httpRespHeaders[curlInstance];
			context.mediaType = mediaType;

			CURL_EASY_SETOPT_POINTER(curl, CURLOPT_WRITEDATA, &context);
			CURL_EASY_SETOPT_POINTER(curl, CURLOPT_HEADERDATA, &context);

			if(!ISCONFIGSET_PRIV(eAAMPConfig_SslVerifyPeer))
			{
				CURL_EASY_SETOPT_LONG(curl, CURLOPT_SSL_VERIFYHOST, 0);
				CURL_EASY_SETOPT_LONG(curl, CURLOPT_SSL_VERIFYPEER, 0);
			}
			else
			{
				CURL_EASY_SETOPT_LONG(curl, CURLOPT_SSLVERSION, mSupportedTLSVersion);
				CURL_EASY_SETOPT_LONG(curl, CURLOPT_SSL_VERIFYPEER, 1);
			}

			CurlProgressCbContext progressCtx;
			progressCtx.aamp = this;
			progressCtx.mediaType = mediaType;
			progressCtx.dlStarted = true;
			progressCtx.fragmentDurationMs = fragmentDurationMs;

			if ((mediaType == eMEDIATYPE_VIDEO) && (mAampLLDashServiceData.lowLatencyMode))
			{
				progressCtx.remoteUrl = remoteUrl;
			}

			SetCMCDTrackData(mediaType);

			//Disable download stall detection checks for FOG playback done by JS PP
			if(mediaType == eMEDIATYPE_MANIFEST || mediaType == eMEDIATYPE_PLAYLIST_VIDEO ||
			   mediaType == eMEDIATYPE_PLAYLIST_AUDIO || mediaType == eMEDIATYPE_PLAYLIST_SUBTITLE ||
			   mediaType == eMEDIATYPE_PLAYLIST_IFRAME || mediaType == eMEDIATYPE_PLAYLIST_AUX_AUDIO)
			{
				// For Manifest file : Set starttimeout to 0 ( no wait for first byte). Playlist/Manifest with DAI
				// contents take more time , hence to avoid frequent timeout, its set as 0
				progressCtx.startTimeout = 0;
			}
			else
			{
				// for Video/Audio segments , set the start timeout as configured by Application
				progressCtx.startTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadStartTimeout);
				// to enable lowBWTimeout based network timeout factor, if lowBWTimeout is not configured
				int lowBWTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout);
				if ((0 == lowBWTimeout) && (AAMP_DEFAULT_SETTING == GETCONFIGOWNER_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout)))
				{
					lowBWTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout) * LOW_BW_TIMEOUT_FACTOR;
					lowBWTimeout = std::max(DEFAULT_LOW_BW_TIMEOUT, lowBWTimeout);
				}
				if (mFogTSBEnabled)
				{
					AAMPLOG_DEBUG("Disable low bandwidth timeout in aamp, it will be done in fog");
					progressCtx.lowBWTimeout = 0;
				}
				else
				{
					progressCtx.lowBWTimeout = lowBWTimeout;
				}
			}
			progressCtx.stallTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_CurlStallTimeout);

			AAMPLOG_INFO("lowBWTimeout:%d, stallTimeout:%d", progressCtx.lowBWTimeout, progressCtx.stallTimeout);
			// caller must pass either NULL or a string encoding range
			// here we add sanity check to use null instead of empty string; this avoids undefined behavior
			if( range && *range=='\0' ) range = NULL;
			CURL_EASY_SETOPT_STRING(curl, CURLOPT_RANGE, range);

			if ((httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_COOKIE) && (httpRespHeaders[curlInstance].data.length() > 0))
			{
				AAMPLOG_TRACE("Appending cookie headers to HTTP request");
				CURL_EASY_SETOPT_STRING(curl, CURLOPT_COOKIE, httpRespHeaders[curlInstance].data.c_str());
			}

			std::vector<std::string> cmcdCustomHeader;
			AampMediaType mmediaT;
			mmediaT = (mediaType == eMEDIATYPE_INIT_VIDEO) ? eMEDIATYPE_VIDEO : (mediaType == eMEDIATYPE_INIT_AUDIO) ? eMEDIATYPE_AUDIO :mediaType;
			mCMCDCollector->CMCDGetHeaders(mmediaT,cmcdCustomHeader);

			if (cmcdCustomHeader.size() > 0)
			{
				for (std::vector<string>::iterator it=cmcdCustomHeader.begin(); it!=cmcdCustomHeader.end(); ++it)
				{
					// Confirm if all headers are coming right before adding it to curl
					AAMPLOG_TRACE("CMCD Header:[%s]",(*it).c_str());
					httpHeaders = curl_slist_append(httpHeaders, (*it).c_str());
				}
			}

			struct curl_slist* customHeaders = GetCustomHeaders(mediaType);
			curl_slist* Header = customHeaders;
			while (Header != NULL) {
				httpHeaders = curl_slist_append(httpHeaders, Header->data);
				Header = Header->next;
			}
			curl_slist_free_all(customHeaders);
			if (httpHeaders != NULL)
			{
				CURL_EASY_SETOPT_LIST(curl, CURLOPT_HTTPHEADER, httpHeaders);
			}
			long curlDownloadTimeoutMS = curlDLTimeout[curlInstance]; // curlDLTimeout is in msec
			long long maxInitDownloadRetryUntil = maxInitDownloadTimeMS + NOW_STEADY_TS_MS;
			AAMPLOG_INFO("[%s] steady ms %lld, maxInitDownloadRetryUntil %lld, maxInitDownloadTimeMS %d maxDownloadAttempt %d",
				GetMediaTypeName(mediaType), (long long int)NOW_STEADY_TS_MS, maxInitDownloadRetryUntil, maxInitDownloadTimeMS, maxDownloadAttempt);

			while(downloadAttempt < maxDownloadAttempt)
			{
				progressCtx.downloadStartTime = NOW_STEADY_TS_MS;

				if(this->mAampLLDashServiceData.lowLatencyMode)
				{
					context.downloadStartTime = progressCtx.downloadStartTime;
				}
				progressCtx.downloadUpdatedTime = -1;
				progressCtx.downloadSize = -1;
				progressCtx.abortReason = eCURL_ABORT_REASON_NONE;
				CURL_EASY_SETOPT_POINTER(curl, CURLOPT_PROGRESSDATA, &progressCtx);
				if(buffer->GetPtr() != NULL)
				{
					buffer->Clear();
				}

				isDownloadStalled = false;
				abortReason = eCURL_ABORT_REASON_NONE;

				long long tStartTime = NOW_STEADY_TS_MS;
				CURLcode res = curl_easy_perform(curl); // synchronous; callbacks allow interruption
				if(!mAampLLDashServiceData.lowLatencyMode)
				{
					int insertDownloadDelay = GETCONFIGVALUE_PRIV(eAAMPConfig_DownloadDelay);
					/* optionally locally induce extra per-download latency */
					if( insertDownloadDelay > 0 )
					{
						interruptibleMsSleep( insertDownloadDelay );
					}
				}

				long long tEndTime = NOW_STEADY_TS_MS;
				downloadAttempt++;

				downloadTimeMS = (int)(tEndTime - tStartTime);
				bool loopAgain = false;
				if (res == CURLE_OK)
				{ // all data collected
					if( memcmp(remoteUrl.c_str(), "file:", 5) == 0 )
					{ // file uri scheme
						// libCurl does not provide CURLINFO_RESPONSE_CODE for 'file:' protocol.
						// Handle CURL_OK to http_code mapping here, other values handled below (see http_code = res).
						http_code = 200;
					}
					else
					{
						http_code = GetCurlResponseCode(curl);
					}
					char *effectiveUrlPtr = NULL;
					if(http_code == 204)
					{
						if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_EFF_LOCATION) && (httpRespHeaders[curlInstance].data.length() > 0) )
						{
							AAMPLOG_WARN("Received Location header: '%s'", httpRespHeaders[curlInstance].data.c_str());
							effectiveUrlPtr =  const_cast<char *>(httpRespHeaders[curlInstance].data.c_str());
						}
					}
					else
					{
						//When Fog is having tsb write error , then it will respond back with 302 with direct CDN url,In this case alone TSB should be disabled
						if(mFogTSBEnabled && http_code == 302)
						{
							mFogTSBEnabled = false;
						}
						effectiveUrlPtr = aamp_CurlEasyGetinfoString(curl, CURLINFO_EFFECTIVE_URL);
					}

					if(effectiveUrlPtr)
					{
						effectiveUrl.assign(effectiveUrlPtr);    //CID:81493 - Resolve Forward null

						if( ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && (remoteUrl!=effectiveUrl) )
						{
							curlhost[curlInstance]->redirect = true;
						}
					}
					if (http_code != 200 && http_code != 204 && http_code != 206)
					{
						AampLogManager::LogNetworkError (effectiveUrl.empty() ? remoteUrl.c_str() : effectiveUrl.c_str(), // Effective URL could be different than remoteURL
						AAMPNetworkErrorHttp, http_code, mediaType);
						print_headerResponse(context.allResponseHeaders, mediaType);
						//Http error 502 to be reattempted once per fragment download and remaining http error to be reattempted as per config
						if(((http_code >= 500 && http_code !=502) && downloadAttempt < maxDownloadAttempt) || (http_code == 502 && downloadAttempt <= DEFAULT_FRAGMENT_DOWNLOAD_502_RETRY_COUNT))
						{
							int waitTimeBeforeRetryHttp5xxMSValue = GETCONFIGVALUE_PRIV(eAAMPConfig_Http5XXRetryWaitInterval);
							interruptibleMsSleep(waitTimeBeforeRetryHttp5xxMSValue);
							AAMPLOG_WARN("Download failed due to Server error. Retrying Attempt:%d!", downloadAttempt);
							loopAgain = true;
						}
					}

					// check if redirected url is pointing to fog / local ip
					if(mIsFirstRequestToFOG)
					{
						if(mTsbRecordingId.empty())
						{
							AAMPLOG_INFO("TSB not available from fog, playing from:%s ", effectiveUrl.c_str());
						}
						this->UpdateVideoEndTsbStatus(mFogTSBEnabled);
					}

					/*
					 * Latency should be printed in the case of successful download which exceeds the download threshold value,
					 * other than this case is assumed as network error and those will be logged with AampLogManager::LogNetworkError.
					 */
					if (fragmentDurationS != 0.0)
					{
						/*in case of fetch fragment this will be non zero value */
						if (downloadTimeMS > fragmentDurationMs )
						{
							AampLogManager::LogNetworkLatency (effectiveUrl.c_str(), downloadTimeMS, fragmentDurationMs, mediaType);
						}
					}
					else if (downloadTimeMS > FRAGMENT_DOWNLOAD_WARNING_THRESHOLD )
					{
						AampLogManager::LogNetworkLatency (effectiveUrl.c_str(), downloadTimeMS, FRAGMENT_DOWNLOAD_WARNING_THRESHOLD, mediaType);
						print_headerResponse(context.allResponseHeaders, mediaType);
					}

					// Do the empty buffer check only for successful downloads
					if ((http_code == 200 || http_code == 204 || http_code == 206) && (buffer->GetPtr() == NULL || buffer->GetLen() == 0))
					{
#if LIBCURL_VERSION_NUM >= 0x073700 // CURL version >= 7.55.0
						double dlSize = aamp_CurlEasyGetinfoOffset(curl, CURLINFO_SIZE_DOWNLOAD_T);
#else
#warning LIBCURL_VERSION<7.55.0
						double dlSize = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_SIZE_DOWNLOAD);
#endif
						long reqSize  = aamp_CurlEasyGetinfoLong(curl, CURLINFO_REQUEST_SIZE);
						AAMPLOG_WARN("Invalid buffer - BufferPtr: %p, BufferLen: %zu, Dlsize : %lf ,Reqsize : %ld, Url: %s",
									buffer->GetPtr(), buffer->GetLen(), dlSize,reqSize,
									(res == CURLE_OK) ? effectiveUrl.c_str() : remoteUrl.c_str());
						// Treat empty buffer as a network error, to trigger rampdown
						// Use CURLE_PARTIAL_FILE to avoid bandwidth recalculation
						res = CURLE_PARTIAL_FILE;
						http_code = res;
					}
				}
				else
				{
					//abortReason for progress_callback exit scenarios
					// curl sometimes exceeds the wait time by few milliseconds.Added buffer of 10msec
					isDownloadStalled = ((res == CURLE_PARTIAL_FILE) || (progressCtx.abortReason != eCURL_ABORT_REASON_NONE));
					// set flag if download aborted with start/stall timeout.
					abortReason = progressCtx.abortReason;

					/* Curl 23 and 42 is not a real network error, so no need to log it here */
					//Log errors due to curl stall/start detection abort
					if (AampLogManager::isLogworthyErrorCode(res) || progressCtx.abortReason != eCURL_ABORT_REASON_NONE)
					{
						std::string effectiveUrl;
						char *effectiveUrlPtr = aamp_CurlEasyGetinfoString(curl, CURLINFO_EFFECTIVE_URL);
						if(effectiveUrlPtr)
						{
							effectiveUrl.assign(effectiveUrlPtr);
						}
						else
						{
							effectiveUrl.assign(remoteUrl);
						}
						AampLogManager::LogNetworkError (effectiveUrl.c_str(), // Effective URL could be different than remoteURL
						AAMPNetworkErrorCurl, (int)(progressCtx.abortReason == eCURL_ABORT_REASON_NONE ? res : CURLE_PARTIAL_FILE), mediaType);
						print_headerResponse(context.allResponseHeaders, mediaType);
					}
					if (res == CURLE_COULDNT_CONNECT || res == CURLE_OPERATION_TIMEDOUT || (isDownloadStalled && (eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT != abortReason)))
					{
						if(mpStreamAbstractionAAMP)
						{
							switch (mediaType)
							{
							case eMEDIATYPE_MANIFEST:
							case eMEDIATYPE_AUDIO:
							case eMEDIATYPE_PLAYLIST_VIDEO:
							case eMEDIATYPE_PLAYLIST_AUDIO:
							case eMEDIATYPE_AUX_AUDIO:
								// always retry small, critical fragments on timeout
								loopAgain = true;
								break;

							case eMEDIATYPE_INIT_VIDEO:
							case eMEDIATYPE_INIT_AUDIO:
							case eMEDIATYPE_INIT_SUBTITLE:
							case eMEDIATYPE_INIT_AUX_AUDIO:
							case eMEDIATYPE_INIT_IFRAME:
								loopAgain = true;
								if (downloadAttempt == maxDownloadAttempt)
								{
									double bufferDurationS = mpStreamAbstractionAAMP->GetBufferedDuration();
									// Keep retrying init segments whilst there is enough buffer depth to last until curl times out
									if (bufferDurationS * 1000 > curlDownloadTimeoutMS)
									{
										// Only retry again if its likely the segment is still available
										if (((NOW_STEADY_TS_MS + curlDownloadTimeoutMS)  < maxInitDownloadRetryUntil) || (maxInitDownloadTimeMS == 0))
										{
											maxDownloadAttempt++;
										}
									}
									AAMPLOG_INFO("Keep trying init request while enough buffer buffer %fs, curlDownloadTimeoutMS %ldms, maxInitDownloadTimeMS %d, steady ms %lld, maxInitDownloadRetryUntil %lld, maxDownloadAttempt %d",
										bufferDurationS, curlDownloadTimeoutMS, maxInitDownloadTimeMS,
										(long long int)NOW_STEADY_TS_MS, maxInitDownloadRetryUntil, maxDownloadAttempt);
								}
								break;

							default:
								double bufferDurationS = mpStreamAbstractionAAMP->GetBufferedDuration();
								// buffer is -1 when sesssion not created. buffer is 0 when session created but playlist not downloaded
								if (bufferDurationS == -1.0 || bufferDurationS == 0 || bufferDurationS * 1000 > (curlDownloadTimeoutMS + fragmentDurationMs))
								{
									// Check if buffer is available and more than timeout interval then only reattempt
									// Not to retry download if there is no buffer left
									loopAgain = true;
									if (mediaType == eMEDIATYPE_VIDEO)
									{
										if (buffer->GetLen())
										{
											long downloadbps = ((long)(buffer->GetLen() / downloadTimeMS) * 8000);
											long currentProfilebps = mpStreamAbstractionAAMP->GetVideoBitrate();
											if (currentProfilebps - downloadbps > BITRATE_ALLOWED_VARIATION_BAND)
											{
												loopAgain = false;
												AAMPLOG_WARN("Video retry disabled on timeout bps:%ld var:%d", (currentProfilebps - downloadbps), BITRATE_ALLOWED_VARIATION_BAND);
											}
										}
										curlDownloadTimeoutMS = mNetworkTimeoutMs;
									}
								}
								break;
							}
						}
						AAMPLOG_WARN("Download failed due to curl timeout or isDownloadStalled:%d Retrying:%d Attempt:%d abortReason:%d", isDownloadStalled, loopAgain && (downloadAttempt < maxDownloadAttempt), downloadAttempt, abortReason);
					}

					/*
					 * Assigning curl error to http_code, for sending the error code as
					 * part of error event if required
					 * We can distinguish curl error and http error based on value
					 * curl errors are below 100 and http error starts from 100
					 */
					if( res == CURLE_FILE_COULDNT_READ_FILE )
					{
						http_code = 404; // translate file not found to URL not found
					}
					else if(abortReason > eCURL_ABORT_REASON_NONE)
					{
						http_code = CURLE_OPERATION_TIMEDOUT; // Timed out wrt configured timeouts(start/lowBW/stall)
					}
					else
					{
						http_code = res;
					}
				}
				double connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize;
				long reqSize, downloadbps = 0;
				AAMP_LogLevel reqEndLogLevel = eLOGLEVEL_INFO;
				if(downloadTimeMS != 0 && buffer->GetLen() != 0)
				{
					downloadbps = ((long)(buffer->GetLen() / downloadTimeMS)*8000);
				}
				total = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_TOTAL_TIME);
				connect = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_CONNECT_TIME);
				resolve = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_NAMELOOKUP_TIME);
				startTransfer = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_STARTTRANSFER_TIME);
				connectTime = connect;
				if(res != CURLE_OK || http_code == 0 || http_code >= 400 || total > 2.0 /*seconds*/)
				{
					reqEndLogLevel = eLOGLEVEL_WARN;
				}
				// Store the CMCD data irrespective of logging level
				mCMCDCollector->CMCDSetNetworkMetrics(mediaType , (int)(startTransfer*1000),(int)(total*1000),(int)(resolve*1000));
				// IsTuneTypeNew set to false in streamabstraction.cpp once top profile has been reached
				if(IsTuneTypeNew)
				{
					reqEndLogLevel = eLOGLEVEL_MIL;
				}
				appConnect = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_APPCONNECT_TIME);
				preTransfer = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_PRETRANSFER_TIME);
				redirect = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_REDIRECT_TIME);
				if( ISCONFIGSET_PRIV(eAAMPConfig_CurlThroughput) )
				{
					AAMPLOG_MIL( "curl-end type=%d appConnect=%f redirect=%f error=%d",
						   mediaType,
						   appConnect,
						   redirect,
						   http_code );
				}
				if (AampLogManager::isLogLevelAllowed(reqEndLogLevel))
				{
					double totalPerformRequest = (double)(downloadTimeMS)/1000;
#if LIBCURL_VERSION_NUM >= 0x073700 // CURL version >= 7.55.0
					dlSize = aamp_CurlEasyGetinfoOffset(curl, CURLINFO_SIZE_DOWNLOAD_T);
#else
#warning LIBCURL_VERSION<7.55.0
					dlSize = aamp_CurlEasyGetinfoDouble(curl, CURLINFO_SIZE_DOWNLOAD);
#endif
					reqSize = aamp_CurlEasyGetinfoLong(curl, CURLINFO_REQUEST_SIZE);

					std::string appName, timeoutClass;
					if (!mAppName.empty())
					{
						// append app name with class data
						appName = mAppName + ",";
					}
					if (CURLE_OPERATION_TIMEDOUT == res || CURLE_PARTIAL_FILE == res || CURLE_COULDNT_CONNECT == res)
					{
						// introduce  extra marker for connection status curl 7/18/28,
						// example 18(0) if connection failure with PARTIAL_FILE code
						timeoutClass = "(" + to_string(reqSize > 0) + ")";
					}

					AAMPLOG(reqEndLogLevel, "HttpRequestEnd: %s%d,%d,%d%s,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%2.4f,%g,%ld,%ld,%ld,%.500s%s%s",
							appName.c_str(), mediaTypeTelemetry, mediaType, http_code, timeoutClass.c_str(), totalPerformRequest, total, connect, startTransfer, resolve, appConnect, preTransfer, redirect, dlSize, reqSize,downloadbps,
					((mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_INIT_VIDEO || mediaType == eMEDIATYPE_PLAYLIST_VIDEO) ? (context.bitrate > 0 ? context.bitrate : mpStreamAbstractionAAMP->GetVideoBitrate()): 0),((res == CURLE_OK) ? effectiveUrl.c_str() : remoteUrl.c_str()), // Effective URL could be different than remoteURL and it is updated only for CURLE_OK case
									range?";":"", range?range:"");
					if (context.processDelay > 0)
					{
						AAMPLOG_INFO("External Processing Delay : %lld", context.processDelay);
					}
					if(ui32CurlTrace < 10 )
					{
						AAMPLOG_INFO("%d.CurlTrace:Dns:%2.4f, Conn:%2.4f, Ssl:%2.4f, Redir:%2.4f, Pre:Start[%2.4f:%2.4f], Hdl:%p, Url:%s",
								ui32CurlTrace, resolve, connect, appConnect, redirect, preTransfer, startTransfer, curl,((res==CURLE_OK)?effectiveUrl.c_str():remoteUrl.c_str()));
						++ui32CurlTrace;
					}
				}
			 	//To handle initial fragment download delays before ABR starts
				if(GetLLDashServiceData()->lowLatencyMode && mediaType == eMEDIATYPE_VIDEO)
				{
					double downloadTime = (double)(downloadTimeMS)/1000;
					//DownloadTime greater than 60% of fragmentDuration are categorized as Delay in download
					//DownloadTime greater than 105% means there is a huge chance of buffer underflow
					if(downloadTime > (fragmentDurationS/100) * 105) /** If download time is greater */
					{
						mDownloadDelay += 3; /** Increment faster way to avoid buffer drain*/
					}
					else if(downloadTime > (fragmentDurationS/100) * 60)
					{
						mDownloadDelay++;
					}
					else
					{
						mDownloadDelay = 0;
					}

				}
				if(!loopAgain)
					break;
			}
		}

		if (http_code == 200 || http_code == 206 || http_code == CURLE_OPERATION_TIMEDOUT)
		{
			if (http_code == CURLE_OPERATION_TIMEDOUT && buffer->GetLen() > 0)
			{
				AAMPLOG_WARN("Download timedout and obtained a partial buffer of size %zu for a downloadTime=%d and isDownloadStalled:%d", buffer->GetLen(), downloadTimeMS, isDownloadStalled);
			}

			if (downloadTimeMS > 0 && mediaType == eMEDIATYPE_VIDEO && CheckABREnabled())
			{
				int  AbrThresholdSize = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRThresholdSize);
				//HybridABRManager mhABRManager;
				HybridABRManager::CurlAbortReason hybridabortReason = (HybridABRManager::CurlAbortReason) abortReason;
				if((buffer->GetLen() > AbrThresholdSize) && (!GetLLDashServiceData()->lowLatencyMode ||
							( GetLLDashServiceData()->lowLatencyMode  && ISCONFIGSET_PRIV(eAAMPConfig_DisableLowLatencyABR))))
				{
					long currentProfilebps  = mpStreamAbstractionAAMP->GetVideoBitrate();
					long downloadbps = (long)mhAbrManager.CheckAbrThresholdSize((int)buffer->GetLen(),downloadTimeMS,currentProfilebps,fragmentDurationMs,hybridabortReason);
					{
						std::lock_guard<std::recursive_mutex> guard(mLock);
						mhAbrManager.UpdateABRBitrateDataBasedOnCacheLength(mAbrBitrateData,downloadbps,false);
					}
				}
			}
		}
		if (http_code == 200 || http_code == 206)
		{
			if((mHarvestCountLimit > 0) && (mHarvestConfig & getHarvestConfigForMedia(mediaType)))
			{
				/* Avoid chance of overwriting , in case of manifest and playlist, name will be always same */
				if(mediaType == eMEDIATYPE_MANIFEST || mediaType == eMEDIATYPE_PLAYLIST_AUDIO
				|| mediaType == eMEDIATYPE_PLAYLIST_IFRAME || mediaType == eMEDIATYPE_PLAYLIST_SUBTITLE || mediaType == eMEDIATYPE_PLAYLIST_VIDEO )
				{
					mManifestRefreshCount++;
				}

				AAMPLOG_WARN("aamp harvestCountLimit: %d mManifestRefreshCount %d", mHarvestCountLimit,mManifestRefreshCount);
				std::string harvestPath = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestPath);
				if(harvestPath.empty() )
				{
					getDefaultHarvestPath(harvestPath);
					AAMPLOG_WARN("Harvest path has not configured, taking default path %s", harvestPath.c_str());
				}
				if(buffer->GetPtr() )
				{
					if(aamp_WriteFile(remoteUrl, buffer->GetPtr(), buffer->GetLen(), mediaType, mManifestRefreshCount,harvestPath.c_str()))
						mHarvestCountLimit--;
				}  //CID:168113 - forward null
			}
			ret = true; // default
			if( !context.downloadIsEncoded )
			{
				double expectedContentLength;
#if LIBCURL_VERSION_NUM >= 0x073700 // CURL version >= 7.55.0
				expectedContentLength = aamp_CurlEasyGetinfoOffset(curl,CURLINFO_CONTENT_LENGTH_DOWNLOAD_T);
#else
#warning LIBCURL_VERSION<7.55.0
				expectedContentLength = aamp_CurlEasyGetInfoDouble(CURLINFO_CONTENT_LENGTH_DOWNLOAD);
#endif
				if( (static_cast<int>(lround(expectedContentLength)) > 0) &&
				   (static_cast<int>(lround(expectedContentLength)) != (int)buffer->GetLen()) )
				{
					//Note: For non-compressed data, Content-Length header and buffer size should be same. For gzipped data, 'Content-Length' will be <= deflated data.
					AAMPLOG_WARN("AAMP Content-Length=%d actual=%d", (int)expectedContentLength, (int)buffer->GetLen() );
					http_code       =       416; // Range Not Satisfiable
					ret             =       false; // redundant, but harmless
					buffer->Free();
				}
			}
		}
		else
		{
			if (AampLogManager::isLogworthyErrorCode(res))
			{
				AAMPLOG_WARN("BAD URL:%s", remoteUrl.c_str());
			}
			buffer->Free();
			if (rate != 1.0)
			{
				mediaType = eMEDIATYPE_IFRAME;
			}

			// dont generate anomaly reports for write and aborted errors
			// these are generated after trick play options,
			if( !(http_code == CURLE_ABORTED_BY_CALLBACK || http_code == CURLE_WRITE_ERROR || http_code == 204))
			{
				SendAnomalyEvent(ANOMALY_WARNING, "%s:%s,%s-%d url:%s", (mFogTSBEnabled ? "FOG" : "CDN"),
								 GetMediaTypeName(mediaType), (http_code < 100) ? "Curl" : "HTTP", http_code, remoteUrl.c_str());
			}

			if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_XREASON) && (httpRespHeaders[curlInstance].data.length() > 0) )
			{
				AAMPLOG_WARN("Received X-Reason header from %s: '%s'", mFogTSBEnabled?"Fog":"CDN Server", httpRespHeaders[curlInstance].data.c_str());
				SendAnomalyEvent(ANOMALY_WARNING, "%s X-Reason:%s", mFogTSBEnabled ? "Fog" : "CDN", httpRespHeaders[curlInstance].data.c_str());
			}
			else if ( (httpRespHeaders[curlInstance].type == eHTTPHEADERTYPE_FOG_REASON) && (httpRespHeaders[curlInstance].data.length() > 0) )
			{
				//extract error and url used by fog to download content from cdn
				// it is part of fog-reason
				if(fogError)
				{
					std::regex errRegx("-(.*),");
					std::smatch match;
					if (std::regex_search(httpRespHeaders[curlInstance].data, match, errRegx) && match.size() > 1) {
						if (!match.str(1).empty())
						{
							*fogError = std::stoi(match.str(1));
							AAMPLOG_INFO("Received FOG-Reason fogError: '%d'", *fogError);
						}
					}
				}

				//	get failed url from fog reason and update effectiveUrl
				if(!effectiveUrl.empty())
				{
					std::regex fromRegx("from:(.*),");
					std::smatch match;

					if (std::regex_search(httpRespHeaders[curlInstance].data, match, fromRegx) && match.size() > 1) {
						if (!match.str(1).empty())
						{
							effectiveUrl.assign(match.str(1).c_str());
							AAMPLOG_INFO("Received FOG-Reason effectiveUrl: '%s'", effectiveUrl.c_str());
						}
					}
				}

				if(http_code == 512 && mediaType == eMEDIATYPE_MANIFEST && httpRespHeaders[curlInstance].data.length() > 0){
					mFogDownloadFailReason.clear();
					mFogDownloadFailReason = httpRespHeaders[curlInstance].data.c_str();
				}


				AAMPLOG_WARN("Received FOG-Reason header: '%s'", httpRespHeaders[curlInstance].data.c_str());
				SendAnomalyEvent(ANOMALY_WARNING, "FOG-Reason:%s", httpRespHeaders[curlInstance].data.c_str());
			}
		}

		if (bitrate && (context.bitrate > 0))
		{
			AAMPLOG_INFO("Received getfile Bitrate : %" BITSPERSECOND_FORMAT, context.bitrate);
			*bitrate = context.bitrate;
		}

		if(abortReason != eCURL_ABORT_REASON_NONE && abortReason != eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT)
		{
			http_code = PARTIAL_FILE_START_STALL_TIMEOUT_AAMP;
		}
		else if (connectTime == 0.0)
		{
			//curl connection is failure
			if(CURLE_PARTIAL_FILE == http_code)
			{
				http_code = PARTIAL_FILE_CONNECTIVITY_AAMP;
			}
			else if(CURLE_OPERATION_TIMEDOUT == http_code)
			{
				http_code = OPERATION_TIMEOUT_CONNECTIVITY_AAMP;
			}
		}
		else if (CURLE_PARTIAL_FILE == http_code)
		{
			// download time expired with partial file for playlists/init fragments
			http_code = PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP;
		}
	}
	else
	{
		AAMPLOG_WARN("downloads disabled");
	}

	if (http_error)
	{
		*http_error = http_code;
		if(downloadTimeS)
		{
			*downloadTimeS = total;
		}
	}
	if (httpHeaders != NULL)
	{
		curl_slist_free_all(httpHeaders);
	}
	if (mIsFirstRequestToFOG)
	{
		mIsFirstRequestToFOG = false;
	}

	// Strip downloaded chunked Iframes when ranged requests receives 200 as HTTP response for HLS MP4
	if( mConfig->IsConfigSet(eAAMPConfig_RepairIframes) && NULL != range && '\0' != range[0] && 200 == http_code && NULL != buffer->GetPtr() && FORMAT_ISO_BMFF == this->mVideoFormat)
	{
		AAMPLOG_INFO( "Received HTTP 200 for ranged request (chunked iframe: %s: %s), starting to strip the fragment", range, remoteUrl.c_str() );
		size_t start;
		size_t end;
		try {
			if(2 == sscanf(range, "%zu-%zu", &start, &end))
			{
				// #EXT-X-BYTERANGE:19301@88 from manifest is equivalent to 88-19388 in HTTP range request
				size_t len = (end - start) + 1;
				if( buffer->GetLen() >= len)
				{
					buffer->Clear();
					buffer->AppendBytes(buffer->GetPtr() + start, len);
				}

				// hack - repair wrong size in box
				IsoBmffBuffer repair;
				repair.setBuffer((uint8_t *)buffer->GetPtr(), buffer->GetLen() );
				repair.parseBuffer(true);  //correctBoxSize=true
				AAMPLOG_INFO("Stripping the fragment for range request completed");
			}
			else
			{
				AAMPLOG_ERR("Stripping the fragment for range request failed, failed to parse range string");
			}
		}
		catch (std::exception &e)
		{
				AAMPLOG_ERR("Stripping the fragment for ranged request failed (%s)", e.what());
		}
	}

	if( bucketType!=PROFILE_BUCKET_TYPE_COUNT)
	{
		if( !ret )
		{
			profiler.ProfileError(bucketType, *http_error);
		}
		profiler.ProfileEnd(bucketType);
	}
	return ret;
}

/**
 * @brief Download VideoEnd Session statistics from fog
 *
 * @return string tsbSessionEnd data from fog
 */
void PrivateInstanceAAMP::GetOnVideoEndSessionStatData(std::string &data)
{
	std::string remoteUrl = "127.0.0.1:9080/sessionstat";
	if(!mTsbRecordingId.empty())
	{
		/* Request session statistics for current recording ID
		 *
		 * example request: 127.0.0.1:9080/sessionstat/<recordingID>
		 *
		 */
		remoteUrl.append("/");
		remoteUrl.append(mTsbRecordingId);
		AampCurlDownloader T1;
		DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
		DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
		inpData->bIgnoreResponseHeader	= true;
		inpData->eRequestType = eCURL_GET;
		inpData->proxyName        = GetNetworkProxy();
		T1.Initialize(inpData);
		T1.Download(remoteUrl, respData);

		if(respData->iHttpRetValue == 200)
		{
			std::string dataStr =  std::string( respData->mDownloadData.begin(), respData->mDownloadData.end());
			if(dataStr.size())
			{
				cJSON *root = cJSON_Parse(dataStr.c_str());
				if (root == NULL)
				{
					const char *error_ptr = cJSON_GetErrorPtr();
					if (error_ptr != NULL)
					{
						AAMPLOG_ERR("Invalid Json format: %s", error_ptr);
					}
				}
				else
				{
					char *jsonStr = cJSON_PrintUnformatted(root);
					data = jsonStr;
					cJSON_free( jsonStr );
					cJSON_Delete(root);
				}
			}
		}
		else
		{
			// Failure in request
			AAMPLOG_ERR("curl request %s failed[%d]", remoteUrl.c_str(), respData->iHttpRetValue);
		}
	}
	return ;
}


/**
 * @brief Terminate the stream
 */
void PrivateInstanceAAMP::TeardownStream(bool newTune, bool disableDownloads)
{
	std::unique_lock<std::recursive_mutex> lock(mLock);
	//Have to perform this for trick and stop operations but avoid ad insertion related ones
	AAMPLOG_WARN(" mProgressReportFromProcessDiscontinuity:%d mDiscontinuityTuneOperationId:%d newTune:%d", mProgressReportFromProcessDiscontinuity, mDiscontinuityTuneOperationId, newTune);
	if ((mDiscontinuityTuneOperationId != 0) && (!newTune || mState == eSTATE_IDLE))
	{
		bool waitForDiscontinuityProcessing = true;
		if (mProgressReportFromProcessDiscontinuity)
		{
			AAMPLOG_WARN("TeardownStream invoked while mProgressReportFromProcessDiscontinuity and mDiscontinuityTuneOperationId[%d] set!", mDiscontinuityTuneOperationId);
			guint callbackID = aamp_GetSourceID();
			if ((callbackID != 0 && mDiscontinuityTuneOperationId == callbackID) || mAsyncTuneEnabled)
			{
				AAMPLOG_WARN("TeardownStream idle callback id[%d] and mDiscontinuityTuneOperationId[%d] match. Ignore further discontinuity processing!", callbackID, mDiscontinuityTuneOperationId);
				waitForDiscontinuityProcessing = false; // to avoid deadlock
				mDiscontinuityTuneOperationInProgress = false;
				mDiscontinuityTuneOperationId = 0;
			}
		}
		if (waitForDiscontinuityProcessing)
		{
			//wait for discontinuity tune operation to finish before proceeding with stop
			if (mDiscontinuityTuneOperationInProgress)
			{
				AAMPLOG_WARN("TeardownStream invoked while mDiscontinuityTuneOperationInProgress set. Wait until the Discontinuity Tune operation to complete!!");
				mCondDiscontinuity.wait(lock);
			}
			else
			{
				RemoveAsyncTask(mDiscontinuityTuneOperationId);
				mDiscontinuityTuneOperationId = 0;
			}
		}
	}
	// Maybe mDiscontinuityTuneOperationId is 0, ProcessPendingDiscontinuity can be invoked from NotifyEOSReached too
	else if (mProgressReportFromProcessDiscontinuity || mDiscontinuityTuneOperationInProgress)
	{
		if(mDiscontinuityTuneOperationInProgress)
		{
			AAMPLOG_WARN("TeardownStream invoked while mDiscontinuityTuneOperationInProgress set. Wait until the pending discontinuity tune operation to complete !!");
			mCondDiscontinuity.wait(lock);
		}
		else
		{
			AAMPLOG_WARN("TeardownStream invoked while mProgressReportFromProcessDiscontinuity set!");
			mDiscontinuityTuneOperationInProgress = false;
		}
	}

	//reset discontinuity related flags
	ResetDiscontinuityInTracks();
	UnblockWaitForDiscontinuityProcessToComplete();
	ResetTrackDiscontinuityIgnoredStatus();
	lock.unlock();
	if (mpStreamAbstractionAAMP)
	{
		// Using StreamLock to make sure this is not interfering with GetFile() from PreCachePlaylistDownloadTask
		AcquireStreamLock();
		mpStreamAbstractionAAMP->Stop(disableDownloads);

		if(mContentType == ContentType_HDMIIN)
		{
			StreamAbstractionAAMP_HDMIIN::ResetInstance();
			mpStreamAbstractionAAMP = NULL;
		}
		else if(mContentType == ContentType_COMPOSITEIN)
		{
			StreamAbstractionAAMP_COMPOSITEIN::ResetInstance();
			mpStreamAbstractionAAMP = NULL;
		}
		else
		{
			if(!IsLocalAAMPTsb())
			{
				SAFE_DELETE(mpStreamAbstractionAAMP);
			}
		}
		ReleaseStreamLock();
	}
	m_lastSubClockSyncTime = std::chrono::system_clock::time_point();

	lock.lock();
	mVideoFormat = FORMAT_INVALID;
	lock.unlock();
	if (streamerIsActive)
	{
		const bool forceStop = false;
		if (!forceStop && !newTune)
		{
			if ((eMEDIAFORMAT_PROGRESSIVE == mMediaFormat) && (true == mSeekOperationInProgress))
			{
				AAMPLOG_TRACE("Skip mid-seek flushing of progressive pipeline to position 0");
				// If format is progressive and we're doing a teardown to facilitate a seek operation, avoid a flushing seek to position 0.
				// With progressive content, playbin will immediately start playback from position 0 and that may not be the desired position.
				// TuneHelper() will perform a flushing seek to the correct position afterwards.
			}
			else
			{
				StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
				if (sink)
				{
					sink->Flush(0, rate);
				}
			}
		}
		else
		{
			AAMPLOG_INFO("before CC Release - mTuneType:%d mbPlayEnabled:%d ", mTuneType, mbPlayEnabled);
			if (mbPlayEnabled && mTuneType != eTUNETYPE_RETUNE)
			{
				PlayerCCManager::GetInstance()->Release(mCCId);
				mCCId = 0;
			}
			else
			{
				AAMPLOG_WARN("CC Release - skipped ");
			}
			if(!mbUsingExternalPlayer)
			{
				StreamSink *sink = AampStreamSinkManager::GetInstance().GetStoppingStreamSink(this);
				if (sink)
				{
					sink->Stop(!newTune);
				}
			}
		}
	}
	else
	{
		for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
		{
			mbTrackDownloadsBlocked[iTrack] = true;
		}
		streamerIsActive = true;
	}
	mAdProgressId = "";
	std::queue<AAMPEventPtr> emptyEvQ;
	{
		std::lock_guard<std::mutex> lock(mAdEventQMtx);
		std::swap( mAdEventsQ, emptyEvQ );
	}
}

/**
 * @brief Establish PIPE session with Receiver
 */
bool PrivateInstanceAAMP::SetupPipeSession()
{
	bool retVal = false;
	if(m_fd != -1)
	{
		retVal = true; //Pipe exists
		return retVal;
	}
	if(mkfifo(strAAMPPipeName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1)
	{
		if(errno == EEXIST)
		{
			// Pipe exists
			//AAMPLOG_WARN("CreatePipe: Pipe already exists");
			retVal = true;
		}
		else
		{
			// Error
			AAMPLOG_ERR("CreatePipe: Failed to create named pipe %s for reading errno = %d (%s)",
				strAAMPPipeName, errno, strerror(errno));
		}
	}
	else
	{
		// Success
		//AAMPLOG_WARN("CreatePipe: mkfifo succeeded");
		retVal = true;
	}

	if(retVal)
	{
		// Open the named pipe for writing
		m_fd = open(strAAMPPipeName, O_WRONLY | O_NONBLOCK  );
		if (m_fd == -1)
		{
			// error
			AAMPLOG_ERR("OpenPipe: Failed to open named pipe %s for writing errno = %d (%s)",
				strAAMPPipeName, errno, strerror(errno));
		}
		else
		{
			// Success
			retVal = true;
		}
	}
	return retVal;
}


/**
 * @brief Close PIPE session with Receiver
 */
void PrivateInstanceAAMP::ClosePipeSession()
{
	if(m_fd != -1)
	{
		close(m_fd);
		m_fd = -1;
	}
}

/**
 * @brief Send messages to Receiver over PIPE
 */
void PrivateInstanceAAMP::SendMessageOverPipe(const char *str,int nToWrite)
{
	if(m_fd != -1)
	{
		// Write the packet data to the pipe
		int nWritten =  (int)write(m_fd, str, nToWrite);
		if(nWritten != nToWrite)
		{
			// Error
			AAMPLOG_ERR("Error writing data written = %d, size = %d errno = %d (%s)",
				nWritten, nToWrite, errno, strerror(errno));
			if(errno == EPIPE)
			{
				// broken pipe, lets reset and open again when the pipe is avail
				ClosePipeSession();
			}
		}
	}
}

/**
 * @brief Send message to receiver over PIPE
 */
void PrivateInstanceAAMP::SendMessage2Receiver(AAMP2ReceiverMsgType type, const char *data)
{
#ifdef CREATE_PIPE_SESSION_TO_XRE
	if(SetupPipeSession())
	{
		int dataLen = strlen(data);
		int sizeToSend = AAMP2ReceiverMsgHdrSz + dataLen;
		std::vector<uint8_t> tmp(sizeToSend,0);
		AAMP2ReceiverMsg *msg = (AAMP2ReceiverMsg *)(tmp.data());
		msg->type = (unsigned int)type;
		msg->length = dataLen;
		memcpy(msg->data, data, dataLen);
		SendMessageOverPipe((char *)tmp.data(), sizeToSend);
	}
#else
	AAMPLOG_INFO("AAMP=>XRE: %s",data);
#endif
}

CURL * PrivateInstanceAAMP::GetCurlInstanceForURL(std::string &remoteUrl,unsigned int curlInstance)
{
	CURL* lcurl = curl[curlInstance];

	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableCurlStore) && mOrigManifestUrl.isRemotehost )
	{
		if( curlhost[curlInstance]->isRemotehost && curlhost[curlInstance]->redirect &&
			( NULL == curlhost[curlInstance]->curl || std::string::npos == remoteUrl.find(curlhost[curlInstance]->hostname)) )
		{
			if(NULL != curlhost[curlInstance]->curl)
			{
				CurlStore::GetCurlStoreInstance(this).CurlTerm(this, (AampCurlInstance)curlInstance, 1, false, curlhost[curlInstance]->hostname);
			}

			curlhost[curlInstance]->hostname = aamp_getHostFromURL(remoteUrl);
			curlhost[curlInstance]->isRemotehost =!(aamp_IsLocalHost(curlhost[curlInstance]->hostname));
			curlhost[curlInstance]->redirect = false;

			if( curlhost[curlInstance]->isRemotehost && (std::string::npos == mOrigManifestUrl.hostname.find(curlhost[curlInstance]->hostname)) )
			{
				CurlStore::GetCurlStoreInstance(this).CurlInit(this, (AampCurlInstance)curlInstance, 1, GetNetworkProxy(), curlhost[curlInstance]->hostname);
				CURL_EASY_SETOPT_LONG(curlhost[curlInstance]->curl, CURLOPT_TIMEOUT_MS, curlDLTimeout[curlInstance]);
			}
		}

		if ( curlhost[curlInstance]->curl )
		{
			lcurl=curlhost[curlInstance]->curl;
		}
	}

	return lcurl;
}

static int aampApplyThreadPrioFromEnv(const char *env, int defaultPolicy, int defaultPriority)
{
	int ret = -1;
	int priority = defaultPriority;
	int policy = defaultPolicy;
	/* get env settings from file for envName */
	const char *envVal = getenv(env);
	if (envVal)
	{
		size_t len= strlen(envVal);
		if ( (len >= 3) && (envVal[1]==',') )
		{
			char c = envVal[0];
			/* parse thread policy value */
			switch(c)
			{
				case 'o':
				case 'O':
					policy = SCHED_OTHER;
					break;
				case 'f':
				case 'F':
					policy = SCHED_FIFO;
					break;
				case 'r':
				case 'R':
					policy = SCHED_RR;
					break;
			}
			/* get thread priority value */
			priority = atoi(envVal+2);
		}
	}
	if((policy >= 0) && (policy <= 6))
	{
		ret = aamp_SetThreadSchedulingParameters(policy, priority);
	}
	else
	{
		/* fallback thread priority setting in case of corruption */
		priority = defaultPriority;
		policy = defaultPolicy;
		ret = aamp_SetThreadSchedulingParameters(policy, priority);
	}
	return ret;
}

/**
 * @brief The helper function which perform tuning
 * Common tune operations used on Tune, Seek, SetRate etc
 */
void PrivateInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
	bool newTune;

	aampApplyThreadPrioFromEnv("AAMP_AV_PIPELINE_PRIORITY", SCHED_OTHER, 0);
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		lastUnderFlowTimeMs[i] = 0;
	}
	{
		std::lock_guard<std::recursive_mutex> guard(mFragmentCachingLock);
		EnableAllMediaDownloads();
		//LazilyLoadConfigIfNeeded();
		mFragmentCachingRequired = false;
		mPauseOnFirstVideoFrameDisp = false;
		mFirstVideoFrameDisplayedEnabled = false;
		prevFirstPeriodStartTime = 0;
	}
	if( seekWhilePaused )
	{ // Player state not updated correctly after seek
		// Prevent gstreamer callbacks from placing us back into playing state by setting these gate flags before CBs are triggered
		// in this routine. See NotifyFirstFrameReceived(), NotifyFirstBufferProcessed(), NotifyFirstVideoFrameDisplayed()
		mPauseOnFirstVideoFrameDisp = true;
		mFirstVideoFrameDisplayedEnabled = true;
	}

	if((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_NEW_SEEK == tuneType))
	{
		/** Enabled rate Correction by default, seek case and live added later point  **/
		AAMPLOG_INFO("Live latency correction is disabled for seek by default!!");
		mDisableRateCorrection = true;
		//Logging should be deactivated if the buffer exceeds the minimum buffer size or if seeking occurs
		if(mIsLoggingNeeded && mConfig->GetConfigOwner(eAAMPConfig_InfoLogging) == AAMP_DEFAULT_SETTING)
		{
			AampLogManager::setLogLevel((eLOGLEVEL_WARN));
			SETCONFIGVALUE_PRIV(AAMP_STREAM_SETTING, eAAMPConfig_ProgressLogging, false);
			mIsLoggingNeeded = false;
		}
	}
	else
	{
		mDisableRateCorrection = false;
	}

	if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
	{
		mSeekOperationInProgress = true;
		if ((mMediaFormat == eMEDIAFORMAT_HLS) || (mMediaFormat == eMEDIAFORMAT_HLS_MP4))
		{
			mFirstFragmentTimeOffset = -1 ; //reset the firstFragmentOffsetTime when seek operation is done
		}
	}

	if (eTUNETYPE_LAST == tuneType)
	{
		tuneType = mTuneType;
		AAMPLOG_INFO("Set tune type to last value %d", tuneType);
	}
	else
	{
		mTuneType = tuneType;
	}

	newTune = IsNewTune();
	AAMPLOG_INFO("tuneType %d newTune %d", tuneType, newTune);

	// Get position before pipeline is teared down
	if (eTUNETYPE_RETUNE == tuneType)
	{
		seek_pos_seconds = GetPositionSeconds();
	}
	else
	{
		//Only trigger the clear to encrypted pipeline switch while on retune
		mEncryptedPeriodFound = false;
		mPipelineIsClear = false;
		AAMPLOG_INFO ("Resetting mClearPipeline & mEncryptedPeriodFound");
	}

	TeardownStream(newTune|| (eTUNETYPE_RETUNE == tuneType));
	if(SocUtils::ResetNewSegmentEvent())
	{
		// Send new SEGMENT event only on all trickplay and trickplay -> play, not on pause -> play / seek while paused
		// this shouldn't impact seekplay or ADs
		if (tuneType == eTUNETYPE_SEEK && !(mbSeeked == true || rate == 0 || (rate == 1 && pipeline_paused == true)))
			for (int i = 0; i < AAMP_TRACK_COUNT; i++) mbNewSegmentEvtSent[i] = false;
	}
	ui32CurlTrace=0;

	if((mTelemetryInterval == 0) && mbPlayEnabled)
	{
		mTelemetryInterval = GETCONFIGVALUE_PRIV(eAAMPConfig_TelemetryInterval) * 1000;
		mLastTelemetryTimeMS = aamp_GetCurrentTimeMS();
	}

	if (newTune)
	{

		// send previous tune VideoEnd Metrics data
		// this is done here because events are cleared on stop and there is chance that event may not get sent
		// check for mEnableVideoEndEvent and call SendVideoEndEvent ,object mVideoEnd is created inside SendVideoEndEvent
		if(mTuneAttempts == 1) // only for first attempt, dont send event when JSPP retunes.
		{
			SendVideoEndEvent();
		}

		mTsbRecordingId.clear();
		// initialize defaults
		SetState(eSTATE_INITIALIZING);
		culledSeconds = 0;
		durationSeconds = 60 * 60; // 1 hour
		rate = AAMP_NORMAL_PLAY_RATE;
		StoreLanguageList(std::set<std::string>());
		mTunedEventPending = true;
		mProfileCappedStatus = false;
		pPlayerExternalsInterface->GetDisplayResolution(mDisplayWidth, mDisplayHeight);
		AAMPLOG_INFO ("Display Resolution width:%d height:%d", mDisplayWidth, mDisplayHeight);

		mOrigManifestUrl.hostname = aamp_getHostFromURL(mManifestUrl);
		mOrigManifestUrl.isRemotehost = !(aamp_IsLocalHost(mOrigManifestUrl.hostname));
		AAMPLOG_TRACE("CurlTrace OrigManifest url:%s", mOrigManifestUrl.hostname.c_str());
	}
	if(mMediaFormat == eMEDIAFORMAT_DASH)
	{
		if(NULL == mMPDDownloaderInstance)
		{
			// Create MPD Downloader instance , which will be used by StreamAbstraction module ( for DASH)
			mMPDDownloaderInstance = new AampMPDDownloader();
		}
		// If downloader is inactive then initialize it
		if(mMPDDownloaderInstance->IsDownloaderDisabled())
		{
			// Prepare the manifest download configuration
			std::shared_ptr<ManifestDownloadConfig> inpData = prepareManifestDownloadConfig();
			if(!inpData->mPreProcessedManifest.empty())
			{
				mMPDDownloaderInstance->Initialize(inpData, mAppName, std::bind(&PrivateInstanceAAMP::SendManifestPreProcessEvent, this));
			}
			else
			{
				mMPDDownloaderInstance->Initialize(inpData, mAppName, nullptr);
			}
			mMPDDownloaderInstance->Start();
		}
	}

	trickStartUTCMS = -1;

	double playlistSeekPos = seek_pos_seconds - culledSeconds;
	AAMPLOG_INFO("playlistSeek : %f seek_pos_seconds:%f culledSeconds : %f ",playlistSeekPos,seek_pos_seconds,culledSeconds);
	if (playlistSeekPos < 0)
	{
		playlistSeekPos = 0;
		seek_pos_seconds = culledSeconds;
		AAMPLOG_MIL("Updated seek_pos_seconds %f ", seek_pos_seconds);
	}

	if (mMediaFormat == eMEDIAFORMAT_DASH)
	{
		/* This relies on the fact that when tuning to a new channel, the flag mLocalAAMPTsb is set after this point,
		so the StreamAbstraction object is created even if Local AAMP TSB is used. When TuneHelper is called for
		another reason, like changing the rate, the flag is already set and the object is not re-created. */
		if(!mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(this, playlistSeekPos, rate,
					std::bind(&PrivateInstanceAAMP::ID3MetadataHandler, this,
						std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)
					);
			AAMPLOG_MIL("New stream abstraction object created");
			if (NULL == mCdaiObject)
			{
				mCdaiObject = new CDAIObjectMPD(this); // special version for DASH
			}
		}
		else
		{
			mpStreamAbstractionAAMP->ReinitializeInjection(rate);
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_HLS || mMediaFormat == eMEDIAFORMAT_HLS_MP4)
	{ // m3u8
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_HLS(this, playlistSeekPos, rate,
			std::bind(&PrivateInstanceAAMP::ID3MetadataHandler, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
			std::bind(&PrivateInstanceAAMP::UpdatePTSOffsetFromTune, this,
				std::placeholders::_1, std::placeholders::_2)
		);
		if(NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_PROGRESSIVE(this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
		// Set to false so that EOS events can be sent. Flag value was whatever previous asset had set it to.
		SetIsLive(false);
	}
	else if (mMediaFormat == eMEDIAFORMAT_HDMI)
	{
		mpStreamAbstractionAAMP = StreamAbstractionAAMP_HDMIIN::GetInstance(this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_OTA)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_OTA(this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_RMF)
	{
		mpStreamAbstractionAAMP = new StreamAbstractionAAMP_RMF(this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_COMPOSITE)
	{
		mpStreamAbstractionAAMP = StreamAbstractionAAMP_COMPOSITEIN::GetInstance(this, playlistSeekPos, rate);
		if (NULL == mCdaiObject)
		{
			mCdaiObject = new CDAIObject(this);    //Placeholder to reject the SetAlternateContents()
		}
	}
	else if (mMediaFormat == eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA)
	{
		AAMPLOG_ERR("Error: SmoothStreamingMedia playback not supported");
		mInitSuccess = false;
		SendErrorEvent(AAMP_TUNE_UNSUPPORTED_STREAM_TYPE);
		return;
	}

	mInitSuccess = true;
	AAMPStatusType retVal = eAAMPSTATUS_GENERIC_ERROR;
	if(newTune && !IsLocalAAMPTsb() && GetTSBSessionManager())
	{
		// Set Local TSB flag after starting the streamabstraction
		AAMPLOG_MIL("Enabling local TSB handling for the new tune");
		SetLocalAAMPTsb(true);
	}
	// Local AAMP TSB injection is true if Local AAMP TSB is enabled and TuneHelper() is called for
	// any reason other than a new tune or seek to live (set rate, seek...).
	// Also, set LocalAAMPTsbInjection to true when tuneType is SEEKTOLIVE and AAMP TSB is not empty
	// to avoid video freeze in live-pause-live scenario.
	if (!newTune && IsLocalAAMPTsb() )
	{
		AampTSBSessionManager *tsbSessionManager = GetTSBSessionManager();

		if( (tuneType != eTUNETYPE_SEEKTOLIVE) || ( (NULL != tsbSessionManager)
			&& (tsbSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO) > 0)))
		{
			SetLocalAAMPTsbInjection(true);
		}
	}

	if (mpStreamAbstractionAAMP)
	{
		if (IsLocalAAMPTsbInjection())
		{
			// Update StreamAbstraction object seek position to the absolute position (seconds since 1970)
			mpStreamAbstractionAAMP->SeekPosUpdate(seek_pos_seconds);
			retVal = mpStreamAbstractionAAMP->InitTsbReader(tuneType);
		}
		else
		{
			mpStreamAbstractionAAMP->SetCDAIObject(mCdaiObject);
			retVal = mpStreamAbstractionAAMP->Init(tuneType);
		}
	}
	else
	{
		AAMPLOG_WARN("Stream abstraction object is NULL");
		retVal = eAAMPSTATUS_GENERIC_ERROR;
	}

	// Validate tune type
	// (need to find a better way to do this)
	if (tuneType == eTUNETYPE_NEW_NORMAL) // either no offset (mIsDefaultOffset = true) or -1 was specified
	{
		if(!IsLive() && !mIsDefaultOffset)
		{
			if (mMediaFormat == eMEDIAFORMAT_DASH) //currently only supported for dash
			{
				tuneType = eTUNETYPE_NEW_END;
			}
		}
	}
	mIsDefaultOffset = false;

	if ((tuneType == eTUNETYPE_NEW_END) ||
		(tuneType == eTUNETYPE_SEEKTOEND))
	{
		if (mMediaFormat != eMEDIAFORMAT_DASH)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: tune to end not supported for format");
			retVal = eAAMPSTATUS_GENERIC_ERROR;
		}
	}

	if (retVal != eAAMPSTATUS_OK)
	{
		// Check if the seek position is beyond the duration
		if(retVal == eAAMPSTATUS_SEEK_RANGE_ERROR)
		{
			AAMPLOG_ERR("mpStreamAbstractionAAMP Init Failed.Seek Position(%f) out of range(%lld)",mpStreamAbstractionAAMP->GetStreamPosition(),(GetDurationMs()/1000));
			NotifyEOSReached();
		}
		else if(mIsFakeTune)
		{
			if(retVal == eAAMPSTATUS_FAKE_TUNE_COMPLETE)
			{
				AAMPLOG_MIL( "Fake tune completed");
			}
			else
			{
				SetState(eSTATE_COMPLETE);
				mEventManager->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS, GetSessionId()));
				AAMPLOG_MIL( "Stopping fake tune playback");
			}
		}
		else if (DownloadsAreEnabled())
		{
			AAMPLOG_ERR("mpStreamAbstractionAAMP Init Failed.Error(%d)",retVal);
			AAMPTuneFailure failReason = AAMP_TUNE_INIT_FAILED;
			switch(retVal)
			{
			case eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_DNLD_ERROR; break;
			case eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR; break;
			case eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR; break;
			case eAAMPSTATUS_MANIFEST_CONTENT_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR; break;
			case eAAMPSTATUS_MANIFEST_PARSE_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_MANIFEST_PARSE_ERROR; break;
			case eAAMPSTATUS_TRACKS_SYNCHRONIZATION_ERROR:
			case eAAMPSTATUS_INVALID_PLAYLIST_ERROR:
				failReason = AAMP_TUNE_INIT_FAILED_TRACK_SYNC_ERROR; break;
			case eAAMPSTATUS_UNSUPPORTED_DRM_ERROR:
				failReason = AAMP_TUNE_DRM_UNSUPPORTED; break;
			default :
				break;
			}

			if (failReason == AAMP_TUNE_INIT_FAILED_PLAYLIST_VIDEO_DNLD_ERROR || failReason == AAMP_TUNE_INIT_FAILED_PLAYLIST_AUDIO_DNLD_ERROR)
			{
				int http_error = mPlaylistFetchFailError;
				SendDownloadErrorEvent(failReason, http_error);
			}
			else
			{
				SendErrorEvent(failReason);
			}
		}
		mInitSuccess = false;
		return;
	}
	else
	{
		//explicitly invalidate previous position for consistency with previous code
		mPrevPositionMilliseconds.Invalidate();

		int volume = audio_volume;
		double updatedSeekPosition = mpStreamAbstractionAAMP->GetStreamPosition();
		if(mMediaFormat != eMEDIAFORMAT_DASH)
		{
			/* For non-DASH formats, the stream position returned by the StreamAbstraction object is relative to the
			time of tuning. Add culledSeconds to get the absolute position. */
			seek_pos_seconds = updatedSeekPosition + culledSeconds;
		}
		else
		{
			// For absolute timeline reporting, culledSecond is considered as manifest start time.
			seek_pos_seconds = updatedSeekPosition;
		}
		culledOffset = culledSeconds;
		UpdateProfileCappedStatus();

		/*
		Do not modify below log line since it is used in checking L2 test case results.
		If need to be modified then make sure below test cases are modified to
		reflect the same.
		AAMP-CONFIG-2033_live
		AAMP-CONFIG-2029_seekMidFragment
		*/
		AAMPLOG_MIL("Updated seek_pos_seconds %f culledSeconds/start %f culledOffset %f", seek_pos_seconds, culledSeconds, culledOffset);

		GetStreamFormat(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat);
		AAMPLOG_INFO("TuneHelper : mVideoFormat %d, mAudioFormat %d mAuxFormat %d", mVideoFormat, mAudioFormat, mAuxFormat);

		//Identify if HLS with mp4 fragments, to change media format
		if (mVideoFormat == FORMAT_ISO_BMFF && mMediaFormat == eMEDIAFORMAT_HLS)
		{
			mMediaFormat = eMEDIAFORMAT_HLS_MP4;
		}

		if(mFirstFragmentTimeOffset < 0)
		{
			long long  duration = 0;
			// Update first fragment time, ie time of the tune for new tune, and time of retune for seektolive
			// For LL-DASH, we update mFirstFragmentTimeOffset as the Absolute start time of fragment.
			if(mSeekOperationInProgress && mProgressReportOffset < 0 )
			{
					duration = DurationFromStartOfPlaybackMs();
			}
			else
			{
					duration = GetDurationMs();
			}
			mFirstFragmentTimeOffset = (double)(aamp_GetCurrentTimeMS() - duration)/1000.0;
			AAMPLOG_INFO("Updated FirstFragmentTimeOffset:%lf %lld %lld", mFirstFragmentTimeOffset,aamp_GetCurrentTimeMS(),duration);
			StartRateCorrectionWorkerThread();
		}

		// Enable fragment initial caching. Retune not supported
		if(tuneType != eTUNETYPE_RETUNE
			&& GetInitialBufferDuration() > 0
			&& rate == AAMP_NORMAL_PLAY_RATE
			&& mpStreamAbstractionAAMP->IsInitialCachingSupported())
		{
			std::lock_guard<std::recursive_mutex> guard(mFragmentCachingLock);
			mFirstVideoFrameDisplayedEnabled = true;
			mFragmentCachingRequired = true;
		}

		AAMPLOG_INFO("TuneHelper - seek_pos: %f", seek_pos_seconds);
		UpdatePTSOffsetFromTune(seek_pos_seconds, true);

		// Set Pause on First Video frame if seeking and requested
		if( mSeekOperationInProgress && seekWhilePaused )
		{
			mFirstVideoFrameDisplayedEnabled = true;
			mPauseOnFirstVideoFrameDisp = true;
		}

		if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
		{
			if (rate > AAMP_NORMAL_PLAY_RATE)
			{
				volume = 0; // Mute audio to avoid glitches
			}
			seek_pos_seconds = 0; // Reset seek position
		}

		// Increase Buffer value dynamically according to Max Profile Bandwidth to accommodate HiFi Content Buffers
		if (newTune && GETCONFIGOWNER_PRIV(eAAMPConfig_GstVideoBufBytes) == AAMP_DEFAULT_SETTING && mpStreamAbstractionAAMP && mpStreamAbstractionAAMP->GetProfileCount())
		{
			IncreaseGSTBufferSize();
		}

		if (!mbUsingExternalPlayer)
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				sink->SetVideoZoom(zoom_mode);
				AAMPLOG_INFO("SetVideoMute video_muted %d mApplyCachedVideoMute %d", video_muted, mApplyCachedVideoMute);
				sink->SetVideoMute(video_muted);
				if (mApplyCachedVideoMute)
				{
					mApplyCachedVideoMute = false;
					CacheAndApplySubtitleMute(video_muted);
				}
				sink->SetAudioVolume(volume);
				if (mbPlayEnabled)
				{
					sink->Configure(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat, mpStreamAbstractionAAMP->GetESChangeStatus(), mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
				}
			}
			else
			{
				AAMPLOG_ERR("GetStreamSink() returned NULL");
			}
		}

		/* executing the flush earlier in order to avoid the tune delay while waiting for the first video and audio fragment to download
		 * and retrieve the pts value, as in the segmenttimeline streams we get the pts value from manifest itself
		 */
		if (mpStreamAbstractionAAMP->DoEarlyStreamSinkFlush(newTune, rate))
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				double flushPosition = (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE) ? updatedSeekPosition : mpStreamAbstractionAAMP->GetFirstPTS();
				// shouldTearDown is set to false, because in case of a new tune pipeline
				// might not be in a playing/paused state which causes Flush() to destroy
				// pipeline. This has to be avoided.
				sink->Flush(flushPosition, rate, false);
			}
		}

		if (newTune && IsLocalAAMPTsb() && !GetTSBSessionManager())
		{
			SetLocalAAMPTsb(false);
			AAMPLOG_MIL("Disabling local TSB handling for this tune");
		}

		// TODO - X1-TSB : ES Change status needs to be checked
		mpStreamAbstractionAAMP->ResetESChangeStatus();
		mpStreamAbstractionAAMP->Start();
		if (!mbUsingExternalPlayer)
		{
			if (mbPlayEnabled)
			{
				StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
				if (sink)
				{
					sink->Stream();
				}
			}
		}

		if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
		{
			if (HasSidecarData())
			{
				// has sidecar data
				mpStreamAbstractionAAMP->ResumeSubtitleAfterSeek(subtitles_muted, mData.get());
			}

			if (!mTextStyle.empty())
			{
				// Restore the subtitle text style after a seek.
				(void)mpStreamAbstractionAAMP->SetTextStyle(mTextStyle);
			}
		}
	}

	if (IsLocalAAMPTsb() && !IsLocalAAMPTsbInjection())
	{
		// Update culled seconds and duration based on TSB when watching live with AAMP Local TSB enabled
		culledSeconds = seek_pos_seconds;
		durationSeconds = mAbsoluteEndPosition - culledSeconds;
	}

	if (tuneType == eTUNETYPE_SEEK || tuneType == eTUNETYPE_SEEKTOLIVE || tuneType == eTUNETYPE_SEEKTOEND)
	{
		mSeekOperationInProgress = false;
		// Pipeline is not configured if mbPlayEnabled is false, so not required
		if (mbPlayEnabled && seekWhilePaused == false && pipeline_paused == true)
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				if(!sink->Pause(true, false))
				{
					AAMPLOG_INFO("GetStreamSink() Pause failed");
				}
			}
		}
	}

	if(!mIsFakeTune)
	{
		AAMPLOG_INFO("mCCId: %d",mCCId);
		// if mCCId has non zero value means it is same instance and cc release was not callee then dont get id. if zero then call getid.
		if(mCCId == 0 )
		{
			mCCId = PlayerCCManager::GetInstance()->GetId();
		}
		//restore CC if it was enabled for previous content.
		if(mIsInbandCC)
			PlayerCCManager::GetInstance()->RestoreCC();
	}

	if (newTune && !mIsFakeTune)
	{
		AAMPPlayerState state = GetState();
		if((state != eSTATE_ERROR) && (mMediaFormat != eMEDIAFORMAT_OTA) && (mMediaFormat != eMEDIAFORMAT_RMF))
		{
			/*For OTA/RMF this event will be generated from StreamAbstractionAAMP_OTA*/
			SetState(eSTATE_PREPARED);
			SendMediaMetadataEvent();
		}
	}
}

/**
 * @brief Reload TSB for same URL .
 */
void PrivateInstanceAAMP::ReloadTSB()
{
	mEventManager->SetPlayerState(eSTATE_IDLE);
	mManifestUrl = mTsbSessionRequestUrl + "&reloadTSB=true";
	// To post player configurations to fog on 1st time tune
	long configPassCode = -1;
	if(mFogTSBEnabled && ISCONFIGSET_PRIV(eAAMPConfig_EnableAampConfigToFog))
	{
		configPassCode = LoadFogConfig();
	}
	if(mMediaFormat == eMEDIAFORMAT_DASH)
	{
		// Restart MPD downloader thread with new session
		std::shared_ptr<ManifestDownloadConfig> inpData = prepareManifestDownloadConfig();
		mMPDDownloaderInstance->Initialize(inpData,mAppName);
		mMPDDownloaderInstance->Start();
	}
	if(configPassCode == 200 || configPassCode == 204 || configPassCode == 206)
	{
		mMediaFormat = GetMediaFormatType(mManifestUrl.c_str());
		ResumeDownloads();

		mIsFirstRequestToFOG = (mFogTSBEnabled == true);

		{
			AAMPLOG_WARN("Reloading TSB, URL: %s", mManifestUrl.c_str());
		}
	}
}

/**
 * @brief Tune API
 */
void PrivateInstanceAAMP::Tune(const char *mainManifestUrl,
								bool autoPlay,
								const char *contentType,
								bool bFirstAttempt,
								bool bFinalAttempt,
								const char *pTraceID,
								bool audioDecoderStreamSync,
								const char *refreshManifestUrl,
								int mpdStitchingMode,
								std::string sid,
								const char *manifestData )
{
	int iCacheMaxSize = 0;
	double tmpVar=0;
	int intTmpVar=0;
	/** Disable iframe extraction by default*/
	SetIsIframeExtractionEnabled(false);
	TuneType tuneType =  eTUNETYPE_NEW_NORMAL;
	const char *remapUrl = mConfig->GetChannelOverride(mainManifestUrl);
	if (remapUrl )
	{
		const char *remapLicenseUrl = NULL;
		mainManifestUrl = remapUrl;
		remapLicenseUrl = mConfig->GetChannelLicenseOverride(mainManifestUrl);
		if (remapLicenseUrl )
		{
			AAMPLOG_INFO("Channel License Url Override: [%s]", remapLicenseUrl);
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_LicenseServerUrl,std::string(remapLicenseUrl));
		}
	}
	mEventManager->SetPlayerState(eSTATE_IDLE);
	mConfig->CustomSearch(mainManifestUrl,mPlayerId,mAppName);
	AampLogManager::setLogLevel(eLOGLEVEL_INFO);
	SetSessionId(std::move(sid));
	mProvidedManifestFile.clear();
	if(manifestData != NULL)
	{
		mProvidedManifestFile = manifestData;
	}
	seek_pos_seconds = GETCONFIGVALUE_PRIV(eAAMPConfig_PlaybackOffset);
	preferredRenditionString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioRendition);
	preferredCodecString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioCodec);
	preferredLanguagesString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLanguage);
	preferredLabelsString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLabel);
	preferredTypeString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioType);
	preferredTextRenditionString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextRendition);
	preferredTextLanguagesString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLanguage);
	preferredTextLabelString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextLabel);
	preferredTextTypeString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredTextType);
	UpdatePreferredAudioList();
	mDrmDecryptFailCount = GETCONFIGVALUE_PRIV(eAAMPConfig_DRMDecryptThreshold);
	mPreCacheDnldTimeWindow = GETCONFIGVALUE_PRIV(eAAMPConfig_PreCachePlaylistTime);
	mHarvestCountLimit = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestCountLimit);
	mHarvestConfig = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig);
	mSessionToken = GETCONFIGVALUE_PRIV(eAAMPConfig_AuthToken);
	mSubLanguage = GETCONFIGVALUE_PRIV(eAAMPConfig_SubTitleLanguage);
	preferredSubtitleLanguageVctr.clear();
	std::istringstream ss(mSubLanguage);
	std::string lng;
	while(std::getline(ss, lng, ','))
	{
		preferredSubtitleLanguageVctr.push_back(lng);
		AAMPLOG_INFO("Parsed preferred subtitle lang: %s", lng.c_str());
	}

	mSupportedTLSVersion = GETCONFIGVALUE_PRIV(eAAMPConfig_TLSVersion);
	mLiveOffsetDrift = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffsetDriftCorrectionInterval);
	mAsyncTuneEnabled = ISCONFIGSET_PRIV(eAAMPConfig_AsyncTune);
	intTmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_LivePauseBehavior);
	mPausedBehavior = (PausedBehavior)intTmpVar;
	tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout);
	mNetworkTimeoutMs = CONVERT_SEC_TO_MS(tmpVar);
	tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout);
	mManifestTimeoutMs = CONVERT_SEC_TO_MS(tmpVar);
	if(AAMP_DEFAULT_SETTING == GETCONFIGOWNER_PRIV(eAAMPConfig_ManifestTimeout))
	{
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_ManifestTimeout,mNetworkTimeoutMs/1000);
		tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout);
		mManifestTimeoutMs = CONVERT_SEC_TO_MS(tmpVar);
	}
	tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_PlaylistTimeout);
	mPlaylistTimeoutMs = CONVERT_SEC_TO_MS(tmpVar);
	mFogTSBEnabled = IsFogUrl(mainManifestUrl);
	mTsbType = GETCONFIGVALUE_PRIV(eAAMPConfig_TsbType);
	mLocalAAMPTsbFromConfig = ISCONFIGSET_PRIV(eAAMPConfig_LocalTSBEnabled) || (mTsbType == "local" && !mFogTSBEnabled);
	if (mLocalAAMPTsbFromConfig && mFogTSBEnabled)
	{
		AAMPLOG_WARN("AAMP TSB and FOG both enabled, using AAMP TSB");
		mFogTSBEnabled = false;
	}
	if(mPlaylistTimeoutMs <= 0) mPlaylistTimeoutMs = mManifestTimeoutMs;
	if(AAMP_DEFAULT_SETTING == GETCONFIGOWNER_PRIV(eAAMPConfig_PlaylistTimeout))
	{
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PlaylistTimeout,mNetworkTimeoutMs/1000);
		tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_PlaylistTimeout);
		mPlaylistTimeoutMs = CONVERT_SEC_TO_MS(tmpVar);
	}
	// Reset mProgramDateTime to 0 , to avoid spill over to next tune if same session is
	// reused
	mProgramDateTime = 0;
	mMPDPeriodsInfo.clear();
	// Reset current audio/text track index
	mCurrentAudioTrackIndex = -1;
	mCurrentTextTrackIndex = -1;
	SetPauseOnStartPlayback(false);

	mSchemeIdUriDai = GETCONFIGVALUE_PRIV(eAAMPConfig_SchemeIdUriDaiStream);

	UpdateBufferBasedOnLiveOffset();
	// Set the EventManager config
	// TODO When faketune code is added later , push the faketune status here
	mEventManager->SetAsyncTuneState(mAsyncTuneEnabled);
	mIsFakeTune = strcasestr(mainManifestUrl, "fakeTune=true");
	if(mIsFakeTune)
	{
		AampLogManager::setLogLevel(eLOGLEVEL_ERROR);
	}
	mEventManager->SetFakeTuneFlag(mIsFakeTune);

	mManifestUrl = mainManifestUrl; // TBR

	// store the url 2 from the application for mpd stitching
	mMPDStichRefreshUrl		=	refreshManifestUrl ? refreshManifestUrl : "";
	mMPDStichOption			=	(MPDStichOptions) (mpdStitchingMode % 2);
	mMediaFormat = GetMediaFormatType(mainManifestUrl);

	// Calling SetContentType without checking contentType != NULL, so that
	// mContentType will be reset to ContentType_UNKNOWN at the start of tune by default
	SetContentType(contentType);
	AAMPLOG_INFO("Content type (%d): %s", mContentType, contentType == nullptr ? "null" : contentType);

	if (ContentType_CDVR == mContentType)
	{
		mIscDVR = true;
	}

#ifdef ENABLE_PTS_RESTAMP
	if (ContentType_LINEAR == mContentType)
	{
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING, eAAMPConfig_EnablePTSReStamp, true);
	}

	AAMPLOG_MIL("ContentType(%d) EnablePTSReStamp(%d)", mContentType, GETCONFIGVALUE_PRIV(eAAMPConfig_EnablePTSReStamp));
#endif

	CreateTsbSessionManager();

	std::string sTraceId = (pTraceID?pTraceID:"unknown");
	//CMCD to be enabled for player direct downloads, not for Fog . All downloads in Fog , CMCD response to be done in Fog.
	mCMCDCollector->Initialize((ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD) && !mFogTSBEnabled),sTraceId);
// This feature is causing trickplay issues for client dai
// hence removing code which reads this config from tune url , Ideally it should be fixed by app and not to enable this feature
//	SETCONFIGVALUE_PRIV(AAMP_STREAM_SETTING, eAAMPConfig_InterruptHandling, (mFogTSBEnabled && strcasestr(mainManifestUrl, "networkInterruption=true")));
	if(!ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) && ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling))
	{
		AAMPLOG_INFO("Absolute timeline reporting enabled for interrupt enabled TSB stream");
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING, eAAMPConfig_UseAbsoluteTimeline, true);
	}
	if (bFirstAttempt)
	{
		// To post player configurations to fog on 1st time tune
		if(mFogTSBEnabled && ISCONFIGSET_PRIV(eAAMPConfig_EnableAampConfigToFog))
		{
			LoadFogConfig();
		}
		else
		{
			LoadAampAbrConfig();
		}

	}
	//temporary hack
	if (strcasestr(mAppName.c_str(), "peacock"))
	{
		// Enable PTS Restamping
		if(SocUtils::EnableLiveLatencyCorrection())
		{
			SETCONFIGVALUE_PRIV(AAMP_DEFAULT_SETTING, eAAMPConfig_EnableLiveLatencyCorrection, true);
		}
		SETCONFIGVALUE_PRIV(AAMP_DEFAULT_SETTING, eAAMPConfig_EnablePTSReStamp, SocUtils::EnablePTSRestamp());
	}

	/* Reset counter in new tune */
	mManifestRefreshCount = 0;

	// For PreCaching of playlist , no max limit set as size will vary for each playlist length
	iCacheMaxSize = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxPlaylistCacheSize);
	if(iCacheMaxSize != MAX_PLAYLIST_CACHE_SIZE)
	{
		getAampCacheHandler()->SetMaxPlaylistCacheSize(iCacheMaxSize*1024); // convert KB inputs to bytes
	}
	else if(mPreCacheDnldTimeWindow > 0)
	{
		// if precaching enabled, then set cache to infinite
		// support download of all the playlist files
		getAampCacheHandler()->SetMaxPlaylistCacheSize(PLAYLIST_CACHE_SIZE_UNLIMITED);
	}

	// Set max no of init fragment to be maintained in cache table, ByDefault 5.
	iCacheMaxSize = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxInitFragCachePerTrack);
	if(iCacheMaxSize != MAX_INIT_FRAGMENT_CACHE_PER_TRACK)
	{
		getAampCacheHandler()->SetMaxInitFragCacheSize(iCacheMaxSize);
	}

	mAudioDecoderStreamSync = audioDecoderStreamSync;

	mbUsingExternalPlayer = (mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat== eMEDIAFORMAT_HDMI) || (mMediaFormat==eMEDIAFORMAT_COMPOSITE) || \
		(mMediaFormat == eMEDIAFORMAT_RMF);

	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink == nullptr)
	{
		AampStreamSinkManager::GetInstance().CreateStreamSink( this,
											   std::bind(&PrivateInstanceAAMP::ID3MetadataHandler, this,
											   			 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
	}

	if (autoPlay)
	{
		ActivatePlayer();
	}
	else
	{
		AampStreamSinkManager::GetInstance().UpdateTuningPlayer(this);
	}

	/* Initialize gstreamer plugins with correct priority to co-exist with webkit plugins.
	 * Initial priority of aamp plugins is PRIMARY which is less than webkit plugins.
	 * if aamp->Tune is called, aamp plugins should be used, so set priority to a greater value
	 * than that of that of webkit plugins*/
	static bool gstPluginsInitialized = false;
	if ((!gstPluginsInitialized) && (!mbUsingExternalPlayer))
	{
		gstPluginsInitialized = true;
		AAMPGstPlayer::InitializeAAMPGstreamerPlugins();
	}

	mbPlayEnabled = autoPlay;
	mPlayerPreBuffered = !autoPlay ;

	mVideoBasePTS = 0;
	ResumeDownloads();

	if (!autoPlay)
	{
		pipeline_paused = true;
		AAMPLOG_WARN("AutoPlay disabled; Just caching the stream now.");
	}

	mIsDefaultOffset = (AAMP_DEFAULT_PLAYBACK_OFFSET == seek_pos_seconds);
	if (mIsDefaultOffset)
	{
		// eTUNETYPE_NEW_NORMAL
		// default behavior is play live streams from 'live' point and VOD streams from the start
		seek_pos_seconds = 0;
	}
	else if (-1 == seek_pos_seconds)
	{
		// eTUNETYPE_NEW_NORMAL
		// behavior is play live streams from 'live' point and VOD streams skip to the end (this will
		// be corrected later for vod)
		seek_pos_seconds = 0;
	}
	else
	{
		AAMPLOG_WARN("PrivateInstanceAAMP: seek position already set, so eTUNETYPE_NEW_SEEK");
		tuneType = eTUNETYPE_NEW_SEEK;
	}

	AAMPLOG_INFO("Paused behavior : %d", mPausedBehavior);

	for(int i = 0; i < eCURLINSTANCE_MAX; i++)
	{
		//cookieHeaders[i].clear();
		httpRespHeaders[i].type = eHTTPHEADERTYPE_UNKNOWN;
		httpRespHeaders[i].data.clear();
	}

	//Add Custom Header via config
	{
		std::string customLicenseHeaderStr = GETCONFIGVALUE_PRIV(eAAMPConfig_CustomHeaderLicense);
		if(!customLicenseHeaderStr.empty())
		{
			if (ISCONFIGSET_PRIV(eAAMPConfig_CurlLicenseLogging))
			{
				AAMPLOG_WARN("CustomHeader :%s",customLicenseHeaderStr.c_str());
			}
			char* token = NULL;
			char* tokenHeader = NULL;
			char* str = (char*) customLicenseHeaderStr.c_str();

			while ((token = strtok_r(str, ";", &str)))
			{
				int headerTokenIndex = 0;
				std::string headerName;
				std::vector<std::string> headerValue;

				while ((tokenHeader = strtok_r(token, ":", &token)))
				{
					if(headerTokenIndex == 0)
						headerName = tokenHeader;
					else if(headerTokenIndex == 1)
						headerValue.push_back(std::string(tokenHeader));
					else
						break;

					headerTokenIndex++;
				}
				if(!headerName.empty() && !headerValue.empty())
				{
					AddCustomHTTPHeader(headerName, headerValue, true);
				}
			}
		}
	}
	/** Least priority operator setting will override the value only if it is not set from dev config **/
	SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_WideVineKIDWorkaround,IsWideVineKIDWorkaround(mainManifestUrl));
	mIsWVKIDWorkaround = ISCONFIGSET_PRIV(eAAMPConfig_WideVineKIDWorkaround);
	if (mIsWVKIDWorkaround)
	{
		/** Set preferred DRM as Widevine with highest configuration **/
		AAMPLOG_INFO("WideVine KeyID workaround present: Setting preferred DRM as Widevine");
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_PreferredDRM,(int)eDRM_WideVine);
	}

	std::tie(mManifestUrl, mDrmInitData) = ExtractDrmInitData(mainManifestUrl);

	mIsVSS = (strstr(mainManifestUrl, VSS_MARKER) || strstr(mainManifestUrl, VSS_MARKER_FOG));
	mTuneCompleted 	=	false;
	mPersistedProfileIndex	=	-1;
	mServiceZone.clear(); //clear the value if present
	mIsIframeTrackPresent = false;
	mIsTrackIdMismatch = false;
	mCurrentAudioTrackId = -1;
	mCurrentVideoTrackId = -1;
	mCurrentDrm = nullptr;


	// Enable the eAAMPConfig_EnableMediaProcessor if the PTS Restamp set for DASH.
	if (ISCONFIGSET_PRIV(eAAMPConfig_EnablePTSReStamp) && (eMEDIAFORMAT_DASH == mMediaFormat))
	{
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING, eAAMPConfig_EnableMediaProcessor, true);
		AAMPLOG_WARN ("PTS Restamp and MediaProcessor enabled for DASH");
	}

	SetLowLatencyServiceConfigured(false);

	UpdateLiveOffset();
	if (eMEDIAFORMAT_OTA == mMediaFormat)
	{
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			PlayerCCManager::GetInstance()->SetParentalControlStatus(false);
		}
	}

	if(bFirstAttempt)
	{
		mTuneAttempts = 1;	//Only the first attempt is xreInitiated.
		mPlayerLoadTime = NOW_STEADY_TS_MS;
		mLogTune = true;
		mFirstProgress = true;
	}
	else
	{
		mTuneAttempts++;
	}
	profiler.TuneBegin();
	ResetBufUnderFlowStatus();

	if( !remapUrl )
	{
		//Fog can be disable by  having option fog=0 option in aamp.cfg,based on  that gpGlobalConfig->noFog is updated
		//Removed variable gpGlobalConfig->fogSupportsDash as it has similar usage
		if(!mFogTSBEnabled)
		{
			AAMPLOG_INFO("Defog URL '%s'", mManifestUrl.c_str());
			DeFog(mManifestUrl);
			AAMPLOG_INFO("Defogged URL '%s'", mManifestUrl.c_str());
		}

		if(ISCONFIGSET_PRIV(eAAMPConfig_ForceHttp))
		{
			replace(mManifestUrl, "https://", "http://");
			if(mFogTSBEnabled)
			{
				ForceHttpConversionForFog(mManifestUrl,"https","http");
			}
		}

		if (mManifestUrl.find("mpd")!= std::string::npos) // new - limit this option to linear content
		{
			replace(mManifestUrl, "-eac3.mpd", ".mpd");
		} // mpd
	} // !remap_url

	mIsFirstRequestToFOG = (mFogTSBEnabled == true);

	{
		char tuneStrPrefix[64];
		mTsbSessionRequestUrl.clear();
		memset(tuneStrPrefix, '\0', sizeof(tuneStrPrefix));
		if (!mAppName.empty())
		{
			snprintf(tuneStrPrefix, sizeof(tuneStrPrefix), "%s PLAYER[%d] APP: %s",(mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId, mAppName.c_str());
		}
		else
		{
			snprintf(tuneStrPrefix, sizeof(tuneStrPrefix), "%s PLAYER[%d]", (mbPlayEnabled?STRFGPLAYER:STRBGPLAYER), mPlayerId);
		}
		AAMPLOG_MIL("%s aamp_tune: attempt: %d format: %s URL: %s", tuneStrPrefix, mTuneAttempts, mMediaFormatName[mMediaFormat], mManifestUrl.c_str());
		if(!mMPDStichRefreshUrl.empty())
		{
			AAMPLOG_WARN("%s aamp_stich: Option[%d] URL: %s", tuneStrPrefix, mMPDStichOption, mMPDStichRefreshUrl.c_str());
		}
		if(IsFogTSBSupported())
		{
			mTsbSessionRequestUrl = mManifestUrl;
		}
	}

	// this function uses mIsVSS and mFogTSBEnabled, hence it should be called after these variables are updated.
	ExtractServiceZone(mManifestUrl);
	SetTunedManifestUrl(mFogTSBEnabled);

	if(bFirstAttempt)
	{ // TODO: make mFirstTuneFormat of type MediaFormat
		mfirstTuneFmt = (int)mMediaFormat;
	}

	SAFE_DELETE(mCdaiObject);
	
	AcquireStreamLock();
	TuneHelper(tuneType);

	//Apply the cached video mute call as it got invoked when stream lock was not available
	if(mApplyCachedVideoMute)
	{
		mApplyCachedVideoMute = false;
		AAMPLOG_INFO("Cached videoMute is being executed, mute value: %d", video_muted);
		if (mpStreamAbstractionAAMP)
		{
			//There two fns are being called in PlayerInstanceAAMP::SetVideoMute
			SetVideoMute(video_muted);
			CacheAndApplySubtitleMute(video_muted);
		}
		else
		{
			AAMPLOG_ERR("mpStreamAbstractionAAMP is NULL, cannot apply cached video mute");
		}
	}
	ReleaseStreamLock();

	// To check and apply stored video rectangle properties
	if (mApplyVideoRect)
	{
		if ((mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE) || \
			(mMediaFormat == eMEDIAFORMAT_RMF))
		{
			mpStreamAbstractionAAMP->SetVideoRectangle(mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
		}
		else
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				sink->SetVideoRectangle(mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
			}
		}
		AAMPLOG_INFO("Update SetVideoRectangle x:%d y:%d w:%d h:%d", mVideoRect.horizontalPos, mVideoRect.verticalPos, mVideoRect.width, mVideoRect.height);
		mApplyVideoRect = false;
	}
	// To check and apply content restriction
	if(mApplyContentRestriction)
	{
		if (mMediaFormat == eMEDIAFORMAT_OTA)
		{
			mpStreamAbstractionAAMP->EnableContentRestrictions();
		}
		mApplyContentRestriction = false;
	}
	// do not change location of this set, it should be done after sending previous VideoEnd data which
	// is done in TuneHelper->SendVideoEndEvent function.
	this->mTraceUUID = sTraceId;
}

/**
 *  @brief Sets the session ID
 */
void PrivateInstanceAAMP::SetSessionId(std::string sid)
{
	// AAMPLOG_INFO(" DBG :: Session ID set to `%s`", sid.c_str());
	mSessionId = std::move(sid);
}

/**
 *  @brief Get Language preference from aamp.cfg.
 */
LangCodePreference PrivateInstanceAAMP::GetLangCodePreference() const
{
	int langCodePreference = GETCONFIGVALUE_PRIV(eAAMPConfig_LanguageCodePreference);
	return (LangCodePreference)langCodePreference;
}

/**
 *  @brief Get Mediaformat types by parsing the url
 */
MediaFormat PrivateInstanceAAMP::GetMediaFormatType(const char *url)
{
	MediaFormat rc = eMEDIAFORMAT_UNKNOWN;
	std::string urlStr(url); // for convenience, convert to std::string

#ifdef TRUST_LOCATOR_EXTENSION_IF_PRESENT // disable to exercise alternate path
	if( urlStr.rfind("hdmiin:",0)==0 )
	{
		rc = eMEDIAFORMAT_HDMI;
	}
	else if( urlStr.rfind("cvbsin:",0)==0 )
	{
		rc = eMEDIAFORMAT_COMPOSITE;
	}
	else if((urlStr.rfind("live:",0)==0) || (urlStr.rfind("tune:",0)==0)  || (urlStr.rfind("mr:",0)==0))
	{
		rc = eMEDIAFORMAT_OTA;
	}
	else if(urlStr.rfind("ocap://") != std::string::npos)
	{
		rc = eMEDIAFORMAT_RMF;
	}
	else if(urlStr.rfind("http://127.0.0.1", 0) == 0) // starts with localhost
	{ // where local host is used; inspect further to determine if this locator involves FOG

		size_t fogUrlStart = urlStr.find("recordedUrl=", 16); // search forward, skipping 16-char http://127.0.0.1

		if(fogUrlStart != std::string::npos)
		{ // definitely FOG - extension is inside recordedUrl URI parameter

			size_t fogUrlEnd = urlStr.find("&", fogUrlStart); // end of recordedUrl

			if(urlStr.rfind("m3u8", fogUrlEnd) != std::string::npos)
			{
				rc = eMEDIAFORMAT_HLS;
			}
			else if(urlStr.rfind("mpd", fogUrlEnd)!=std::string::npos)
			{
				rc = eMEDIAFORMAT_DASH;
			}
			else
			{
				// should never get here with UNKNOWN format, but if we do, just fall through to normallocator scanning
			}

		}
	}

	if(rc == eMEDIAFORMAT_UNKNOWN)
	{ // do 'normal' (non-FOG) locator parsing

		size_t extensionEnd = urlStr.find("?"); // delimited for URI parameters, or end-of-string
		std::size_t extensionStart = urlStr.rfind(".", extensionEnd); // scan backwards to find final "."
		int extensionLength;

		if(extensionStart != std::string::npos)
		{ // found an extension
			if(extensionEnd == std::string::npos)
			{
				extensionEnd = urlStr.length();
			}

			extensionStart++; // skip past the "." - no reason to re-compare it

			extensionLength = (int)(extensionEnd - extensionStart); // bytes between "." and end of query delimiter/end of string

			if(extensionLength == 4 && urlStr.compare(extensionStart, extensionLength, "m3u8") == 0)
			{
				rc = eMEDIAFORMAT_HLS;
			}
			else if(extensionLength == 3)
			{
				if(urlStr.compare(extensionStart,extensionLength,"mpd") == 0)
				{
					rc = eMEDIAFORMAT_DASH;
				}
				else if(urlStr.compare(extensionStart,extensionLength,"mp3") == 0 || urlStr.compare(extensionStart,extensionLength,"mp4") == 0 ||
					urlStr.compare(extensionStart,extensionLength,"mkv") == 0)
				{
					rc = eMEDIAFORMAT_PROGRESSIVE;
				}
			}
			else if((extensionLength == 2) || (urlStr.rfind("srt:",0)==0))
			{
				if((urlStr.compare(extensionStart,extensionLength,"ts") == 0) || (urlStr.rfind("srt:",0)==0))
				{
					rc = eMEDIAFORMAT_PROGRESSIVE;
				}
			}
		}
	}
#endif // TRUST_LOCATOR_EXTENSION_IF_PRESENT

	if(rc == eMEDIAFORMAT_UNKNOWN)
	{
		// no extension - sniff first few bytes of file to disambiguate
		AampGrowableBuffer sniffedBytes("sniffedBytes");
		std::string effectiveUrl;
		int http_error;
		double downloadTime;
		BitsPerSecond bitrate;
		int fogError;

		mOrigManifestUrl.hostname=aamp_getHostFromURL(url);
		mOrigManifestUrl.isRemotehost = !(aamp_IsLocalHost(mOrigManifestUrl.hostname));
		CurlInit(eCURLINSTANCE_MANIFEST_MAIN, 1, GetNetworkProxy());
		EnableMediaDownloads(eMEDIATYPE_MANIFEST);
		bool gotManifest = GetFile(url,
							eMEDIATYPE_MANIFEST,
							&sniffedBytes,
							effectiveUrl,
							&http_error,
							&downloadTime,
							"0-150", // download first few bytes only
							// TODO: ideally could use "0-6" for range but write_callback sometimes not called before curl returns http 206
							eCURLINSTANCE_MANIFEST_MAIN,
							false,
							&bitrate,
							&fogError,
							0.0 );

		if(gotManifest)
		{
			if(sniffedBytes.GetLen() >= 7 && memcmp(sniffedBytes.GetPtr(), "#EXTM3U8", 7) == 0)
			{
				rc = eMEDIAFORMAT_HLS;
			}
			else
			{
				rc = eMEDIAFORMAT_PROGRESSIVE; // default
				const char *ptr = sniffedBytes.GetPtr();
				const char *fin = ptr + sniffedBytes.GetLen();
				while( ptr<fin )
				{
					char c = *ptr++;
					if( c == '<' )
					{
						if( memcmp(ptr,"SmoothStreamingMedia ",21)==0 )
						{
							rc = eMEDIAFORMAT_SMOOTHSTREAMINGMEDIA;
							break;
						}
						else if( memcmp(ptr,"MPD ",4)==0 )
						{
							rc = eMEDIAFORMAT_DASH;
							break;
						}
					}
				}
			}
		}
		sniffedBytes.Free();
		CurlTerm(eCURLINSTANCE_MANIFEST_MAIN);
	}
	return rc;
}

/**
 *   @brief Check if AAMP is in stalled state after it pushed EOS to
 *   notify discontinuity
 */
void PrivateInstanceAAMP::CheckForDiscontinuityStall(AampMediaType mediaType)
{
	AAMPLOG_DEBUG("Enter mediaType %d", mediaType);
	int discontinuityTimeoutValue = GETCONFIGVALUE_PRIV(eAAMPConfig_DiscontinuityTimeout);
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		if(!(sink->CheckForPTSChangeWithTimeout(discontinuityTimeoutValue)))
		{
			{
				std::lock_guard<std::recursive_mutex> guard(mLock);
				if (mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Ignored retune!! Discontinuity handler already spawned(%d) or in progress(%d)",
						mDiscontinuityTuneOperationId, mDiscontinuityTuneOperationInProgress);
					return;
				}
			}
			AAMPLOG_INFO("No change in PTS for more than %d ms, schedule retune!", discontinuityTimeoutValue);
			ResetDiscontinuityInTracks();

			ResetTrackDiscontinuityIgnoredStatus();
			ScheduleRetune(eSTALL_AFTER_DISCONTINUITY, mediaType);
		}
	}
	AAMPLOG_DEBUG("Exit mediaType %d", mediaType);
}

/**
 * @brief updates mServiceZone (service zone) member with string extracted from locator &sz URI parameter
 */
void PrivateInstanceAAMP::ExtractServiceZone(std::string url)
{
	if(mIsVSS && !url.empty())
	{
		if(mFogTSBEnabled)
		{ // extract original locator from FOG recordedUrl URI parameter
			DeFog(url);
		}
		size_t vssStart = url.find(VSS_MARKER);
		if( vssStart != std::string::npos )
		{
			vssStart += VSS_MARKER_LEN; // skip "?sz="
			size_t vssLen = url.find('&',vssStart);
			if( vssLen != std::string::npos )
			{
				vssLen -= vssStart;
			}
			mServiceZone = url.substr(vssStart, vssLen );
			aamp_DecodeUrlParameter(mServiceZone);
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: ERROR: url does not have vss marker :%s ",url.c_str());
		}
	}
}

/**
 *  @brief Set Content Type
 */
std::string PrivateInstanceAAMP::GetContentTypString()
{
	std::string strRet;
	switch(mContentType)
	{
		case ContentType_CDVR :
			{
				strRet = "CDVR"; //cdvr
				break;
			}
		case ContentType_VOD :
			{
				strRet = "VOD"; //vod
				break;
			}
		case ContentType_LINEAR :
			{
				strRet = "LINEAR"; //linear
				break;
			}
		case ContentType_IVOD :
			{
				strRet = "IVOD"; //ivod
				break;
			}
		case ContentType_EAS :
			{
				strRet ="EAS"; //eas
				break;
			}
		case ContentType_CAMERA :
			{
				strRet = "XfinityHome"; //camera
				break;
			}
		case ContentType_DVR :
			{
				strRet = "DVR"; //dvr
				break;
			}
		case ContentType_MDVR :
			{
				strRet =  "MDVR" ; //mdvr
				break;
			}
		case ContentType_IPDVR :
			{
				strRet ="IPDVR" ; //ipdvr
				break;
			}
		case ContentType_PPV :
			{
				strRet =  "PPV"; //ppv
				break;
			}
		case ContentType_OTT :
			{
				strRet =  "OTT"; //ott
				break;
			}
		case ContentType_OTA :
			{
				strRet =  "OTA"; //ota
				break;
			}
		case ContentType_SLE :
			{
				strRet = "SLE"; // single live event
				break;
			}
		default:
			{
				strRet =  "Unknown";
				break;
			}
	}

	return strRet;
}

/**
 * @brief Notify about sink buffer full
 */
void PrivateInstanceAAMP::NotifySinkBufferFull(AampMediaType type)
{
	if(type != eMEDIATYPE_VIDEO)
		return;

	if(mpStreamAbstractionAAMP)
	{
		MediaTrack* video = mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO);
		if(video && video->enabled)
			video->OnSinkBufferFull();
	}
}

/**
 * @brief Set Content Type
 */
void PrivateInstanceAAMP::SetContentType(const char *cType)
{
	mContentType = ContentType_UNKNOWN; //default unknown
	if(NULL != cType)
	{
		mPlaybackMode = std::string(cType);
		if(mPlaybackMode == "CDVR")
		{
			mContentType = ContentType_CDVR; //cdvr
		}
		else if(mPlaybackMode == "VOD")
		{
			mContentType = ContentType_VOD; //vod
		}
		else if(mPlaybackMode == "LINEAR_TV")
		{
			mContentType = ContentType_LINEAR; //linear
		}
		else if(mPlaybackMode == "IVOD")
		{
			mContentType = ContentType_IVOD; //ivod
		}
		else if(mPlaybackMode == "EAS")
		{
			mContentType = ContentType_EAS; //eas
		}
		else if(mPlaybackMode == "xfinityhome")
		{
			mContentType = ContentType_CAMERA; //camera
		}
		else if(mPlaybackMode == "DVR")
		{
			mContentType = ContentType_DVR; //dvr
		}
		else if(mPlaybackMode == "MDVR")
		{
			mContentType = ContentType_MDVR; //mdvr
		}
		else if(mPlaybackMode == "IPDVR")
		{
			mContentType = ContentType_IPDVR; //ipdvr
		}
		else if(mPlaybackMode == "PPV")
		{
			mContentType = ContentType_PPV; //ppv
		}
		else if(mPlaybackMode == "OTT")
		{
			mContentType = ContentType_OTT; //ott
		}
		else if(mPlaybackMode == "OTA")
		{
			mContentType = ContentType_OTA; //ota
		}
		else if(mPlaybackMode == "HDMI_IN")
		{
			mContentType = ContentType_HDMIIN; //ota
		}
		else if(mPlaybackMode == "COMPOSITE_IN")
		{
			mContentType = ContentType_COMPOSITEIN; //ota
		}
		else if(mPlaybackMode == "SLE")
		{
			mContentType = ContentType_SLE; //single live event
		}
	}
}

/**
 * @brief Get Content Type
 */
ContentType PrivateInstanceAAMP::GetContentType() const
{
	return mContentType;
}

/**
 *   @brief Extract DRM init data from the provided URL
 *          If present, the init data will be removed from the returned URL
 *          and provided as a separate string
 *   @return tuple containing the modified URL and DRM init data
 */
const std::tuple<std::string, std::string> PrivateInstanceAAMP::ExtractDrmInitData(const char *url)
{
	std::string urlStr(url);
	std::string drmInitDataStr;

	const size_t queryPos = urlStr.find("?");
	std::string modUrl;
	if (queryPos != std::string::npos)
	{
		// URL contains a query string. Strip off & decode the drmInitData (if present)
		modUrl = urlStr.substr(0, queryPos);
		const std::string parameterDefinition("drmInitData=");
		std::string parameter;
		std::stringstream querySs(urlStr.substr(queryPos + 1, std::string::npos));
		while (std::getline(querySs, parameter, '&'))
		{ // with each URI parameter
			if (parameter.rfind(parameterDefinition, 0) == 0)
			{ // found drmInitData URI parameter
				drmInitDataStr = parameter.substr(parameterDefinition.length());
				aamp_DecodeUrlParameter( drmInitDataStr );
			}
			else
			{ // filter out drmInitData; reintroduce all other URI parameters
				modUrl.append((queryPos == modUrl.length()) ? "?" : "&");
				modUrl.append(parameter);
			}
		}
		urlStr = modUrl;
	}
	return std::tuple<std::string, std::string>(urlStr, drmInitDataStr);
}

/**
 *   @brief Check if autoplay enabled for current stream
 */
bool PrivateInstanceAAMP::IsPlayEnabled()
{
	return mbPlayEnabled;
}

/**
 * @brief Soft stop the player instance.
 *
 */
void PrivateInstanceAAMP::detach()
{
	// Protect against StreamAbstraction being modified from a different thread
	AcquireStreamLock();
	if(mpStreamAbstractionAAMP && mbPlayEnabled) //Player is running
	{
		pipeline_paused = true;
		seek_pos_seconds  = GetPositionSeconds();
		AAMPLOG_WARN("Player %s=>%s and soft release.Detach at position %f", STRFGPLAYER, STRBGPLAYER,seek_pos_seconds );
		DisableDownloads(); //disable download
		mpStreamAbstractionAAMP->SeekPosUpdate(seek_pos_seconds );
		mpStreamAbstractionAAMP->StopInjection();
		if(mMPDDownloaderInstance != nullptr)
		{
			mMPDDownloaderInstance->Release();
		}
		// Stop CC when pipeline is stopped
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			PlayerCCManager::GetInstance()->Release(mCCId);
			mCCId = 0;
		}
		mDRMLicenseManager->hideWatermarkOnDetach();
		AampStreamSinkManager::GetInstance().DeactivatePlayer(this, false);

		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			sink->Stop(true);
		}
		mbPlayEnabled = false;
		mbDetached=true;
		mPlayerPreBuffered  = false;
		mTelemetryInterval = 0;
		//EnableDownloads();// enable downloads
	}
	else
	{
		AampStreamSinkManager::GetInstance().DeactivatePlayer(this, false);
	}
	ReleaseStreamLock();
}

/**
 * @brief Get AampCacheHandler instance
 */
AampCacheHandler * PrivateInstanceAAMP::getAampCacheHandler()
{
	return mAampCacheHandler;
}

/**
 * @brief Get maximum bitrate value.
 */
long PrivateInstanceAAMP::GetMaximumBitrate()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_MaxBitrate);
}

/**
 * @brief Get minimum bitrate value.
 */
BitsPerSecond PrivateInstanceAAMP::GetMinimumBitrate()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_MinBitrate);
}

/**
 * @brief Get default bitrate value.
 */
BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate);
}

/**
 * @brief Get Default bitrate for 4K
 */
BitsPerSecond PrivateInstanceAAMP::GetDefaultBitrate4K()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_DefaultBitrate4K);
}

/**
 * @brief Get Default Iframe bitrate value.
 */
BitsPerSecond PrivateInstanceAAMP::GetIframeBitrate()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_IFrameDefaultBitrate);
}

/**
 * @brief Get Default Iframe bitrate 4K value.
 */
BitsPerSecond PrivateInstanceAAMP::GetIframeBitrate4K()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_IFrameDefaultBitrate4K);
}

/**
 * @brief Fetch a file from CDN and update profiler
 */
void PrivateInstanceAAMP::LoadIDX(ProfilerBucketType bucketType, std::string fragmentUrl, std::string& effectiveUrl, AampGrowableBuffer *fragment, unsigned int curlInstance, const char *range, int * http_code, double *downloadTime, AampMediaType mediaType,int * fogError)
{
	profiler.ProfileBegin(bucketType);
	if (!GetFile(fragmentUrl, mediaType, fragment, effectiveUrl, http_code, downloadTime, range, curlInstance, true, NULL,fogError))
	{
		profiler.ProfileError(bucketType, *http_code);
		profiler.ProfileEnd(bucketType);
	}
	else
	{
		profiler.ProfileEnd(bucketType);
	}
}

/**
 * @brief End of stream reached
 */
void PrivateInstanceAAMP::EndOfStreamReached(AampMediaType mediaType)
{
	if (mediaType != eMEDIATYPE_SUBTITLE)
	{
		SyncBegin();
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			sink->EndOfStreamReached(mediaType);
		}
		SyncEnd();

		// If EOS during Buffering, set Playing and let buffer to dry out
		// Sink is already unpaused by EndOfStreamReached()
		std::lock_guard<std::recursive_mutex> guard(mFragmentCachingLock);
		mFragmentCachingRequired = false;
		AAMPPlayerState state = GetState();
		if(state == eSTATE_BUFFERING)
		{
			if(mpStreamAbstractionAAMP)
			{
				mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
			}
			SetState(eSTATE_PLAYING);
		}
	}
}

/**
 * @brief Get seek base position
 */
double PrivateInstanceAAMP::GetSeekBase(void)
{
	return seek_pos_seconds;
}

/**
 * @brief Get current drm
 */
DrmHelperPtr PrivateInstanceAAMP::GetCurrentDRM(void)
{
	return mCurrentDrm;
}

/**
 *    @brief Get available thumbnail tracks.
 */
std::string PrivateInstanceAAMP::GetThumbnailTracks()
{
	std::string op;
	AcquireStreamLock();
	if(mpStreamAbstractionAAMP)
	{
		AAMPLOG_TRACE("Entering PrivateInstanceAAMP");
		std::vector<StreamInfo*> data = mpStreamAbstractionAAMP->GetAvailableThumbnailTracks();
		cJSON *root;
		cJSON *item;
		if(!data.empty())
		{
			root = cJSON_CreateArray();
			if(root)
			{
				for( int i = 0; i < data.size(); i++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if(data[i]->bandwidthBitsPerSecond >= 0)
					{
						char buf[32];
						snprintf(buf,sizeof(buf), "%dx%d",data[i]->resolution.width,data[i]->resolution.height);
						cJSON_AddStringToObject(item,"RESOLUTION",buf);
						cJSON_AddNumberToObject(item,"BANDWIDTH",data[i]->bandwidthBitsPerSecond);
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					op.assign(jsonStr);
					free(jsonStr);
				}

				{
					char * tracks_str = cJSON_PrintUnformatted(root);
					if (tracks_str)
					{
						AAMPLOG_TRACE(" Current ThumbnailTracks: %s .", tracks_str);
					}
					cJSON_free(tracks_str);
				}

				cJSON_Delete(root);
			}
		}
		AAMPLOG_TRACE("In PrivateInstanceAAMP::Json string:%s",op.c_str());
	}
	ReleaseStreamLock();
	return op;
}

/**
 *  @brief Get the Thumbnail Tile data.
 */
std::string PrivateInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	std::string rc;
	AcquireStreamLock();
	if(mpStreamAbstractionAAMP && mthumbIndexValue != -1)
	{
		std::string baseurl;
		int raw_w = 0, raw_h = 0, width = 0, height = 0;
		std::vector<ThumbnailData> datavec = mpStreamAbstractionAAMP->GetThumbnailRangeData(tStart, tEnd, &baseurl, &raw_w, &raw_h, &width, &height);
		cJSON *root = cJSON_CreateObject();
		if(!baseurl.empty())
		{
			cJSON_AddStringToObject(root,"baseUrl",baseurl.c_str());
		}
		if(raw_w > 0)
		{
			cJSON_AddNumberToObject(root,"raw_w",raw_w);
		}
		if(raw_h > 0)
		{
			cJSON_AddNumberToObject(root,"raw_h",raw_h);
		}
		cJSON_AddNumberToObject(root,"width",width);
		cJSON_AddNumberToObject(root,"height",height);

		cJSON *tile = cJSON_AddArrayToObject(root,"tile");
		for( const ThumbnailData &iter : datavec )
		{
			cJSON *item;
			cJSON_AddItemToArray(tile, item = cJSON_CreateObject() );
			if(!iter.url.empty())
			{
				cJSON_AddStringToObject(item,"url",iter.url.c_str());
			}
			cJSON_AddNumberToObject(item,"t",iter.t);
			cJSON_AddNumberToObject(item,"d",iter.d);
			cJSON_AddNumberToObject(item,"x",iter.x);
			cJSON_AddNumberToObject(item,"y",iter.y);
		}
		char *jsonStr = cJSON_Print(root);
		if( jsonStr )
		{
			rc.assign( jsonStr );
		}
		cJSON_Delete(root);
	}
	else if (mthumbIndexValue == -1)
	{
		AAMPLOG_WARN("No thumbnail track is currently selected: no information is available.");
	}

	ReleaseStreamLock();
	return rc;
}


TunedEventConfig PrivateInstanceAAMP::GetTuneEventConfig(bool isLive)
{
	int tunedEventConfig = GETCONFIGVALUE_PRIV(eAAMPConfig_TuneEventConfig);
	return (TunedEventConfig)tunedEventConfig;
}

/**
 * @brief to update the preferredaudio codec, rendition and languages  list
 */
void PrivateInstanceAAMP::UpdatePreferredAudioList()
{
	if(!preferredRenditionString.empty())
	{
		preferredRenditionList.clear();
		std::istringstream ss(preferredRenditionString);
		std::string rendition;
		while(std::getline(ss, rendition, ','))
		{
			preferredRenditionList.push_back(rendition);
			AAMPLOG_INFO("Parsed preferred rendition: %s",rendition.c_str());
		}
		AAMPLOG_INFO("Number of preferred Renditions: %zu",
				preferredRenditionList.size());
	}

	if(!preferredCodecString.empty())
	{
		preferredCodecList.clear();
		std::istringstream ss(preferredCodecString);
		std::string codec;
		while(std::getline(ss, codec, ','))
		{
			preferredCodecList.push_back(codec);
			AAMPLOG_INFO("Parsed preferred codec: %s",codec.c_str());
		}
		AAMPLOG_INFO("Number of preferred codec: %zu",
				preferredCodecList.size());
	}

	if(!preferredLanguagesString.empty())
	{
		preferredLanguagesList.clear();
		std::istringstream ss(preferredLanguagesString);
		std::string lng;
		while(std::getline(ss, lng, ','))
		{
			preferredLanguagesList.push_back(lng);
			AAMPLOG_INFO("Parsed preferred lang: %s",lng.c_str());
		}
		AAMPLOG_INFO("Number of preferred languages: %zu",
				preferredLanguagesList.size());
	}
	if(!preferredLabelsString.empty())
	{
		preferredLabelList.clear();
		std::istringstream ss(preferredLabelsString);
		std::string lng;
		while(std::getline(ss, lng, ','))
		{
			preferredLabelList.push_back(lng);
			AAMPLOG_INFO("Parsed preferred Label: %s",lng.c_str());
		}
		AAMPLOG_INFO("Number of preferred Labels: %zu", preferredLabelList.size());
	}
}

/**
 *  @brief Set async tune configuration for EventPriority
 */
void PrivateInstanceAAMP::SetEventPriorityAsyncTune(bool bValue)
{
	if(bValue)
	{
		mEventPriority = AAMP_MAX_EVENT_PRIORITY;
	}
	else
	{
		mEventPriority = G_PRIORITY_DEFAULT_IDLE;
	}
}

/**
 * @brief Get async tune configuration
 */
bool PrivateInstanceAAMP::GetAsyncTuneConfig()
{
	return mAsyncTuneEnabled;
}

/**
 * @brief Set video rectangle
 */
void PrivateInstanceAAMP::UpdateVideoRectangle (int x, int y, int w, int h)
{
	mVideoRect.horizontalPos = x;
	mVideoRect.verticalPos   = y;
	mVideoRect.width = w;
	mVideoRect.height = h;
	mApplyVideoRect = true;
	AAMPLOG_INFO("Backup VideoRectangle x:%d y:%d w:%d h:%d", x, y, w, h);
}

/**
 * @brief Set video rectangle
 */
void PrivateInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
	AAMPPlayerState state = GetState();
	if( TryStreamLock() )
	{
		switch( mMediaFormat )
		{
			case eMEDIAFORMAT_OTA:
			case eMEDIAFORMAT_HDMI:
			case eMEDIAFORMAT_COMPOSITE:
			case eMEDIAFORMAT_RMF:
				if( mpStreamAbstractionAAMP )
				{
					mpStreamAbstractionAAMP->SetVideoRectangle(x, y, w, h);
				}
				break;
			default: // IP Playback
				if( state > eSTATE_PREPARING)
				{
					StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
					if (sink)
					{
						sink->SetVideoRectangle(x, y, w, h);
					}
				}
				else
				{
					AAMPLOG_INFO("state: %d", state );
					UpdateVideoRectangle (x, y, w, h);
				}
				break;
		}
		ReleaseStreamLock();
	}
	else
	{
		AAMPLOG_INFO("StreamLock not available; state: %d", state );
		UpdateVideoRectangle (x, y, w, h);
	}
}
/**
 *   @brief Set video zoom.
 */
void PrivateInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->SetVideoZoom(zoom);
	}
}

/**
 *   @brief Enable/ Disable Video.
 */
void PrivateInstanceAAMP::SetVideoMute(bool muted)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->SetVideoMute(muted);
	}
	else
	{
		AAMPLOG_WARN("StreamSink not available, muted %d", muted);
	}
	if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager) || ISCONFIGSET_PRIV(eAAMPConfig_UseFireboltSDK))
	{
		mDRMLicenseManager->setVideoMute(IsLive(), GetCurrentLatency(), IsAtLivePoint(), GetLiveOffsetMs(),muted, GetStreamPositionMs());
	}
}

/**
 *   @brief Enable/ Disable Subtitles.
 *
 *   @param  muted - true to disable subtitles, false to enable subtitles.
 */
void PrivateInstanceAAMP::SetSubtitleMute(bool muted)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->SetSubtitleMute(video_muted || muted);
	}
}

/**
 *   @brief Set Audio Volume.
 *
 *   @param  volume - Minimum 0, maximum 100.
 */
void PrivateInstanceAAMP::SetAudioVolume(int volume)
{
	if(volume >=0)
	{
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			sink->SetAudioVolume(volume);
		}
	}
}

/**
 * @brief abort ongoing downloads and returns error on future downloads
 * called while stopping fragment collector thread
 */
void PrivateInstanceAAMP::DisableDownloads(void)
{
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		AAMPLOG_MIL("Disable downloads");
		mDownloadsEnabled = false;
		mDownloadsDisabled.notify_all();
	}
	// Notify playlist downloader threads
	if(mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->DisablePlaylistDownloads();
	}
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->NotifyInjectorToPause();
	}
}

/**
 * @brief Check if track can inject data into GStreamer.
 */
bool PrivateInstanceAAMP::DownloadsAreEnabled(void)
{
	return mDownloadsEnabled; // needs mutex protection?
}

/**
 * @brief Enable downloads after aamp_DisableDownloads.
 * Called after stopping fragment collector thread
 */
void PrivateInstanceAAMP::EnableDownloads()
{
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		AAMPLOG_MIL("Enable downloads");
		mDownloadsEnabled = true;
	}
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->NotifyInjectorToResume();
	}
}

/**
 * @brief Sleep until timeout is reached or interrupted
 */
void PrivateInstanceAAMP::interruptibleMsSleep(int timeInMs)
{
	if (timeInMs > 0)
	{
		std::unique_lock<std::recursive_mutex> lock(mLock);
		if (mDownloadsEnabled)
		{
			(void)mDownloadsDisabled.wait_for(lock,std::chrono::milliseconds(timeInMs));
		}
	}
}

/**
 * @brief Get asset duration in milliseconds
 */
long long PrivateInstanceAAMP::GetDurationMs()
{
	long long ms = durationSeconds*1000.0;

	if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
	 	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			ms = sink->GetDurationMilliseconds();
			durationSeconds = ms/1000.0;
		}
	}
	return ms;
}

/**
 *   @brief Get asset duration in milliseconds
 *   For VIDEO TAG Based playback, mainly when
 *   aamp is used as plugin
 */
long long PrivateInstanceAAMP::DurationFromStartOfPlaybackMs()
{
	long long ms = durationSeconds*1000.0;

	if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
	{
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			ms = sink->GetDurationMilliseconds();
			durationSeconds = ms/1000.0;
		}
	}
	else
	{
		if( mIsLive )
		{
			ms = (culledSeconds * 1000.0) + (durationSeconds * 1000.0);
		}
	}
	return ms;
}

/**
 *   @brief Get current stream position
 */
long long PrivateInstanceAAMP::GetPositionMs()
{
	const auto prevPositionInfo = mPrevPositionMilliseconds.GetInfo();
	double seek_pos_seconds_copy = seek_pos_seconds;
	if(prevPositionInfo.isPositionValid(seek_pos_seconds_copy))
	{
		return (prevFirstPeriodStartTime + prevPositionInfo.getPosition());
	}
	else
	{
		if(prevPositionInfo.isPopulated())
		{
			//previous position values calculated using different values of seek_pos_seconds are considered invalid.
			AAMPLOG_WARN("prev-pos-ms (%lld) is invalid. seek_pos_seconds = %f, seek_pos_seconds when prev-pos-ms was stored = %f.",prevPositionInfo.getPosition(), seek_pos_seconds_copy, prevPositionInfo.getSeekPositionSec());
		}
		return GetPositionMilliseconds();
	}
}

bool PrivateInstanceAAMP::LockGetPositionMilliseconds()
{
	if(!mGetPositionMillisecondsMutexSoft.try_lock())
	{
		//In situations that could have deadlocked, continue & make a log entry instead.
		AAMPLOG_ERR("Failed to acquire lock.");
		return false;
	}
	return true;
}

void PrivateInstanceAAMP::UnlockGetPositionMilliseconds()
{

	//Avoid the possibility of unlocking an unlocked mutex (undefined behavior).
	if(mGetPositionMillisecondsMutexSoft.try_lock())
	{
		AAMPLOG_WARN("Acquire lock (unexpected condition unless a previous lock has failed or there is a missing call to LockGetPositionMilliseconds()).");
	}
	mGetPositionMillisecondsMutexSoft.unlock();
}

long long PrivateInstanceAAMP::GetPositionRelativeToSeekMilliseconds(long long rate, long long trickStartUTCMS)
{
	long long position = -1;
	//Audio only playback is un-tested. Hence disabled for now
	if (ISCONFIGSET_PRIV(eAAMPConfig_EnableGstPositionQuery) && !ISCONFIGSET_PRIV(eAAMPConfig_AudioOnlyPlayback) && !mAudioOnlyPb)
	{
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			auto gstPosition = sink->GetPositionMilliseconds();

			/* Prevent spurious values being returned by this function during seek.
			* PrivateInstanceAAMP::GetPositionMilliseconds() is called elsewhere e.g. setting seek_pos_seconds
			* note for this to work correctly mState and seek_pos_seconds must updated atomically otherwise
			* spuriously low (mState = eSTATE_SEEKING before seek_pos_seconds updated) or
			* spuriously high (seek_pos_seconds updated before mState = eSTATE_SEEKING) values could result.
			*/
			if(mState == eSTATE_SEEKING)
			{
				if(gstPosition!=0)
				{
					AAMPLOG_WARN("Ignoring gst position of %lld ms and using seek_pos_seconds only until seek completes.", gstPosition);
				}
				position = 0;
			}
			else
			{
				position = gstPosition;
			}
		}
	}
	else
	{
		long long elapsedTime = aamp_GetCurrentTimeMS() - trickStartUTCMS;
		position = (((elapsedTime > 1000) ? elapsedTime : 0) * rate);
	}

	return position;
}

/**
 * @brief Get current stream playback position in milliseconds
 */
long long PrivateInstanceAAMP::GetPositionMilliseconds()
{
	 /* Ideally between LockGetPositionMilliseconds() & UnlockGetPositionMilliseconds() this function would be blocked
	 * (i.e. all mGetPositionMillisecondsMutexSoft.try_lock() replaced with lock()) this would
	 * ensure mState & seek_pos_seconds are synchronized during this function.
	 * however it is difficult to be certain that this would not result in a deadlock.
	 * Instead raise an error and potentially return a spurious position in cases that could have deadlocked.
	*/
	std::lock_guard<std::mutex> functionLock{mGetPositionMillisecondsMutexHard};
	bool locked = mGetPositionMillisecondsMutexSoft.try_lock();
	if(!locked)
	{
		AAMPLOG_ERR("Failed to acquire lock. A spurious position value may be calculated.");
	}

	//Local copy to avoid race. consider further improvements to the thread safety of this variable.
	double seek_pos_seconds_copy = seek_pos_seconds;
	long long positionMilliseconds = seek_pos_seconds_copy != -1 ? seek_pos_seconds_copy * 1000.0 : 0.0;

	//Local copy to avoid race. Consider further improvements to the thread safety of this variable.
	auto trickStartUTCMS_copy = trickStartUTCMS;
	AAMPLOG_TRACE("trickStartUTCMS=%lld", trickStartUTCMS_copy);
	if (trickStartUTCMS_copy >= 0)
	{
		//Local copy to avoid race. Consider further improvements to the thread safety of this variable.
		auto rate_copy = rate;
		AAMPLOG_TRACE("rate=%f", rate_copy);

		positionMilliseconds+=GetPositionRelativeToSeekMilliseconds(rate_copy, trickStartUTCMS_copy);

		if(AAMP_NORMAL_PLAY_RATE == rate_copy)
		{
			/*standardized & tightened validity checking of previous position to
			  avoid spurious 'restore prev-pos as current-pos!!' around seeks*/
			const auto prevPositionInfo = mPrevPositionMilliseconds.GetInfo();
			if(prevPositionInfo.isPositionValid(seek_pos_seconds_copy))
			{
				long long diff = positionMilliseconds - prevPositionInfo.getPosition();

				if ((diff > MAX_DIFF_BETWEEN_PTS_POS_MS) || (diff < 0))
				{
					AAMPLOG_WARN("diff %lld prev-pos-ms %lld current-pos-ms %lld, restore prev-pos as current-pos!!", diff, prevPositionInfo.getPosition(), positionMilliseconds);
					positionMilliseconds = prevPositionInfo.getPosition();
				}
			}
			else if(prevPositionInfo.isPopulated())
			{
				//Previous position values calculated using different values of seek_pos_seconds are considered invalid.
				AAMPLOG_WARN("prev-pos-ms (%lld) is invalid. seek_pos_seconds = %f, seek_pos_seconds when prev-pos-ms was stored = %f.",prevPositionInfo.getPosition(), seek_pos_seconds_copy, prevPositionInfo.getSeekPositionSec());
			}
		}

		if (positionMilliseconds < 0)
		{
			AAMPLOG_WARN("Correcting positionMilliseconds %lld to zero", positionMilliseconds);
			positionMilliseconds = 0;
		}
		else
		{
			// Optimization, culledSeconds will be 0 for VOD
			long long contentEndMs = 0;
			if(IsLocalAAMPTsb())
			{
				contentEndMs = (mAbsoluteEndPosition * 1000);
			}
			else
			{
				contentEndMs = (GetDurationMs() + (culledSeconds * 1000));
			}
			if(positionMilliseconds > contentEndMs && GetDurationMs() > 0)
			{
				AAMPLOG_WARN("Correcting positionMilliseconds %lld to contentEndMs %lld", positionMilliseconds, contentEndMs);
				positionMilliseconds = contentEndMs;
			}
		}
	}

	AAMPLOG_DEBUG("Returning Position as %lld (seek_pos_seconds = %f) and updating previous position.", positionMilliseconds, seek_pos_seconds_copy);
	mPrevPositionMilliseconds.Update(positionMilliseconds ,seek_pos_seconds_copy);

	if(locked)
	{
		mGetPositionMillisecondsMutexSoft.unlock();
	}

	return positionMilliseconds;
}

/**
 * @brief  API to send audio/video stream into the sink.
 */
bool PrivateInstanceAAMP::SendStreamCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
	bool rc = false;
 	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
 	if (sink)
 	{
		rc = sink->SendCopy(mediaType, ptr, len, fpts, fdts, fDuration);
 	}
	return rc;
}

/**
 * @brief  API to send audio/video stream into the sink.
 */
void PrivateInstanceAAMP::SendStreamTransfer(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		if( sink->SendTransfer(mediaType, buffer->GetPtr(), buffer->GetLen(), fpts, fdts, fDuration, fragmentPTSoffset, initFragment, discontinuity) )
		{
			buffer->Transfer();
		}
		else
		{ // unable to transfer - free up the buffer we were passed.
			buffer->Free();
		}
		//memset(buffer, 0x00, sizeof(AampGrowableBuffer));
	}
	else
	{
		buffer->Free();
	}
}

/**
 * @brief Checking if the stream is live or not
 */
bool PrivateInstanceAAMP::IsLive()
{
	return mIsLive;
}

/**
 * @brief Check if audio playcontext creation skipped for Demuxed HLS file.
 * @retval true if playcontext creation skipped, false if not.
 */
bool PrivateInstanceAAMP::IsAudioPlayContextCreationSkipped()
{
	return mIsAudioContextSkipped;
}

/**
 * @brief Check if stream is live
 */
bool PrivateInstanceAAMP::IsLiveStream()
{
	return mIsLiveStream;
}

/**
 * @brief Stop playback and release resources.
 *
 */
void PrivateInstanceAAMP::Stop( bool isDestructing )
{
	// Clear all the player events in the queue and sets its state to RELEASED as everything is done
	mEventManager->FlushPendingEvents();
	if( !isDestructing )
	{
		SetState(eSTATE_STOPPING);
	}
	
	{
		std::unique_lock<std::mutex> lock(gMutex);
		auto iter = std::find_if(std::begin(gActivePrivAAMPs), std::end(gActivePrivAAMPs), [this](const gActivePrivAAMP_t& el)
		{
			return el.pAAMP == this;
		});
		if(iter != gActivePrivAAMPs.end())
		{
			if (iter->reTune && mIsRetuneInProgress)
			{
				// Wait for any ongoing re-tune operation to complete
				gCond.wait(lock);
			}
			iter->reTune = false;
		}
	}
	if (mAutoResumeTaskPending)
	{
		RemoveAsyncTask(mAutoResumeTaskId);
		mAutoResumeTaskId = AAMP_TASK_ID_INVALID;
		mAutoResumeTaskPending = false;
	}

	DisableDownloads();
	//Moved the tsb delete request from XRE to AAMP to avoid the HTTP-404 erros
	if(IsFogTSBSupported())
	{
		std::string remoteUrl = "127.0.0.1:9080/tsb";
		AampCurlDownloader T1;
		DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
		DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
		inpData->bIgnoreResponseHeader	= true;
		inpData->eRequestType = eCURL_DELETE;
		inpData->proxyName        = GetNetworkProxy();
		T1.Initialize(std::move(inpData));
		T1.Download(remoteUrl, std::move(respData) );
	}

	UnblockWaitForDiscontinuityProcessToComplete();
	StopRateCorrectionWorkerThread();
	if(mTelemetryInterval > 0)
	{
		double bufferedDuration = 0.0;
		if (mpStreamAbstractionAAMP)
		{
			bufferedDuration = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec();
		}
		double latency = GetCurrentLatency();
		profiler.SetLatencyParam(latency, bufferedDuration, rate, mNetworkBandwidth);
		profiler.GetTelemetryParam();
		mTelemetryInterval = 0;
	}

	// AAMP TSB flags have to be cleared before the stream abstraction object is deleted
	// so downloads are disabled among other things
	SetLocalAAMPTsb(false);
	SetLocalAAMPTsbInjection(false);
	// Stopping the playback, release all DRM context
	if (mpStreamAbstractionAAMP)
	{
		AcquireStreamLock();
		if (mDRMLicenseManager)
		{
			ReleaseDynamicDRMToUpdateWait();
			mDRMLicenseManager->setLicenseRequestAbort(true);
		}
		if (HasSidecarData())
		{ // has sidecar data
			mpStreamAbstractionAAMP->ResetSubtitle();
		}
		ReleaseStreamLock();
	}
	TeardownStream(true,true); //disable download as well

	// stop the mpd update immediately after Stream abstraction delete
	if(mMPDDownloaderInstance != nullptr)
	{
		mMPDDownloaderInstance->Release();
	}

	if(mTSBSessionManager)
	{
		// Clear all the local TSB data
		mTSBSessionManager->Flush();
	}

	mId3MetadataCache.Reset();
	{
		std::lock_guard<std::recursive_mutex> guard(mEventLock);
		if (mPendingAsyncEvents.size() > 0)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: mPendingAsyncEvents.size - %zu", mPendingAsyncEvents.size());
			for (std::map<guint, bool>::iterator it = mPendingAsyncEvents.begin(); it != mPendingAsyncEvents.end(); it++)
			{
				if (it->first != 0)
				{
					if (it->second)
					{
						AAMPLOG_WARN("PrivateInstanceAAMP: remove id - %d", (int) it->first);
						g_source_remove(it->first);
					}
					else
					{
						AAMPLOG_WARN("PrivateInstanceAAMP: Not removing id - %d as not pending", (int) it->first);
					}
				}
			}
			mPendingAsyncEvents.clear();
		}
	}
	// Streamer threads are stopped when we reach here, thread synchronization not required
	if (timedMetadata.size() > 0)
	{
		timedMetadata.clear();
	}
	mFailureReason="";
	mBlacklistedProfiles.clear();

	// explicitly invalidate previous position for consistency with previous code
	mPrevPositionMilliseconds.Invalidate();
	seek_pos_seconds = -1;
	culledSeconds = 0;
	mIsLiveStream = false;
	durationSeconds = 0;
	mProgressReportOffset = -1;
	mFirstFragmentTimeOffset = -1;
	mProgressReportAvailabilityOffset = -1;
	rate = 1;
	
	if( !isDestructing )
	{
		SetState(eSTATE_IDLE);
	}
	
	SetPauseOnStartPlayback(false);
	mSeekOperationInProgress = false;
	mTrickplayInProgress = false;
	mDisableRateCorrection = false;
	mMaxLanguageCount = 0; // reset language count
	//mPreferredAudioTrack = AudioTrackInfo(); // reset
	mPreferredTextTrack = TextTrackInfo(); // reset
	// send signal to any thread waiting for play
	mDiscontinuityFound = false;
	SetLLDashChunkMode(false); //Reset ChunkMode
	{
		std::lock_guard<std::mutex> guard(mMutexPlaystart);
		waitforplaystart.notify_all();
	}
	if(mPreCachePlaylistThreadId.joinable())
	{
		mPreCachePlaylistThreadId.join();
	}

	if (mAampCacheHandler)
	{
		mAampCacheHandler->StopPlaylistCache();
	}

	if (pipeline_paused)
	{
		pipeline_paused = false;
	}
	if (mDRMLicenseManager)
	{
		/** Reset the license fetcher only DRM handle is deleting **/
		mDRMLicenseManager->Stop();
	}

	SAFE_DELETE(mCdaiObject);

#if 0
	/* Clear the session data*/
	if(!mSessionToken.empty()){
		mSessionToken.clear();
	}
#endif
	if(mMPDDownloaderInstance != nullptr)
	{
		// delete the MPD Downloader Instance
		AAMPLOG_INFO("Calling delete of Downloader instance ");
		SAFE_DELETE(mMPDDownloaderInstance);
	}

	if(mTSBSessionManager != nullptr)
	{
		//delete the TSBSession Manager instance
		AAMPLOG_INFO("Calling delete of TSBSessionManager instance");
		SAFE_DELETE(mTSBSessionManager);
	}
	SetFlushFdsNeededInCurlStore(false);
	EnableDownloads();

	AampStreamSinkManager::GetInstance().DeactivatePlayer(this, true);
}

const std::vector<TimedMetadata> & PrivateInstanceAAMP::GetTimedMetadata( void ) const
{
	return timedMetadata;
}
/**
 * @brief SaveTimedMetadata Function to store Metadata for bulk reporting during Initialization
 */
void PrivateInstanceAAMP::SaveTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
	std::string content(szContent, nb);
	timedMetadata.push_back(TimedMetadata(timeMilliseconds, std::string((szName == NULL) ? "" : szName), content, std::string((id == NULL) ? "" : id), durationMS));
}

/**
 * @brief SaveNewTimedMetadata Function to store Metadata and reporting event one by one after DRM Initialization
 */
void PrivateInstanceAAMP::SaveNewTimedMetadata(long long timeMilliseconds, const char* szName, const char* szContent, int nb, const char* id, double durationMS)
{
	std::string content(szContent, nb);
	timedMetadataNew.push_back(TimedMetadata(timeMilliseconds, std::string((szName == NULL) ? "" : szName), content, std::string((id == NULL) ? "" : id), durationMS));
}

/**
 * @brief Report timed metadata Function to send timedMetadata
 */
void PrivateInstanceAAMP::ReportTimedMetadata(bool init)
{
	bool bMetadata = ISCONFIGSET_PRIV(eAAMPConfig_BulkTimedMetaReport) || ISCONFIGSET_PRIV(eAAMPConfig_BulkTimedMetaReportLive);
	if(bMetadata && init && IsNewTune())
	{
		ReportBulkTimedMetadata();
	}
	else
	{
		std::vector<TimedMetadata>::iterator iter;
		mTimedMetadataStartTime = NOW_STEADY_TS_MS ;
		for (iter = timedMetadataNew.begin(); iter != timedMetadataNew.end(); iter++)
		{
			ReportTimedMetadata(iter->_timeMS, iter->_name.c_str(), iter->_content.c_str(), (int)iter->_content.size(), init, iter->_id.c_str(), iter->_durationMS);
		}
		timedMetadataNew.clear();
		mTimedMetadataDuration = (NOW_STEADY_TS_MS - mTimedMetadataStartTime);
	}
}

/**
 * @brief Report bulk timedMetadata Function to send bulk timedMetadata in json format
 */
void PrivateInstanceAAMP::ReportBulkTimedMetadata()
{
	mTimedMetadataStartTime = NOW_STEADY_TS_MS;
	std::vector<TimedMetadata>::iterator iter;
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableSubscribedTags) && timedMetadata.size())
	{
		AAMPLOG_INFO("Sending bulk Timed Metadata");

		cJSON *root;
		cJSON *item;
		root = cJSON_CreateArray();
		if(root)
		{
			for (iter = timedMetadata.begin(); iter != timedMetadata.end(); iter++)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				cJSON_AddStringToObject(item, "name", iter->_name.c_str());
				cJSON_AddStringToObject(item, "id", iter->_id.c_str());
				cJSON_AddNumberToObject(item, "timeMs", iter->_timeMS);
				cJSON_AddNumberToObject (item, "durationMs",iter->_durationMS);
				cJSON_AddStringToObject(item, "data", iter->_content.c_str());
			}

			char* bulkData = cJSON_PrintUnformatted(root);
			if(bulkData)
			{
				BulkTimedMetadataEventPtr eventData = std::make_shared<BulkTimedMetadataEvent>(std::string(bulkData), GetSessionId());
				AAMPLOG_INFO("Sending bulkTimedData");
				if (ISCONFIGSET_PRIV(eAAMPConfig_MetadataLogging))
				{
					AAMPLOG_INFO("bulkTimedData : %s", bulkData);
				}
				// Sending BulkTimedMetaData event as synchronous event.
				// SCTE35 events are async events in TimedMetadata, and this event is sending only from HLS
				mEventManager->SendEvent(eventData);
				cJSON_free(bulkData);
			}
			timedMetadata.clear();
			cJSON_Delete(root);

		}
		mTimedMetadataDuration = (NOW_STEADY_TS_MS - mTimedMetadataStartTime);
	}
}

/**
 * @brief Report timed metadata Function to send timedMetadata events
 */
void PrivateInstanceAAMP::ReportTimedMetadata(long long timeMilliseconds, const char *szName, const char *szContent, int nb, bool bSyncCall, const char *id, double durationMS)
{
	std::string content(szContent, nb);
	bool bFireEvent = false;

	// Check if timedMetadata was already reported
	std::vector<TimedMetadata>::iterator i;
	bool ignoreMetaAdd = false;

	for (i = timedMetadata.begin(); i != timedMetadata.end(); i++)
	{
		if ((timeMilliseconds >= i->_timeMS-1000 && timeMilliseconds <= i->_timeMS+1000 ))
		{
			if((i->_name.compare(szName) == 0) && (i->_content.compare(content) == 0))
			{
				// Already same exists , ignore
				ignoreMetaAdd = true;
				break;
			}
			else
			{
				continue;
			}
		}
		else if (i->_timeMS < timeMilliseconds)
		{
			// move to next entry
			continue;
		}
		else if (i->_timeMS > timeMilliseconds)
		{
			break;
		}
	}

	if(!ignoreMetaAdd)
	{
		bFireEvent = true;
		if(i == timedMetadata.end())
		{
			// Comes here for
			// 1.No entry in the table
			// 2.Entries available which is only having time < NewMetatime
			timedMetadata.push_back(TimedMetadata(timeMilliseconds, szName, content, id, durationMS));
		}
		else
		{
			// New entry in between saved entries.
			// i->_timeMS >= timeMilliseconds && no similar entry in table
			timedMetadata.insert(i, TimedMetadata(timeMilliseconds, szName, content, id, durationMS));
		}
	}


	if (bFireEvent)
	{
		//szContent should not contain any tag name and ":" delimiter. This is not checked in JS event listeners
		TimedMetadataEventPtr eventData = std::make_shared<TimedMetadataEvent>(((szName == NULL) ? "" : szName), ((id == NULL) ? "" : id), 	timeMilliseconds, durationMS, content, GetSessionId());

		if (ISCONFIGSET_PRIV(eAAMPConfig_MetadataLogging))
		{
			AAMPLOG_WARN("aamp timedMetadata: [%ld] '%s'", (long)(timeMilliseconds), content.c_str());
		}

		if (!bSyncCall)
		{
			mEventManager->SendEvent(eventData,AAMP_EVENT_ASYNC_MODE);
		}
		else
		{
			mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
		}
	}
}


/**
 * @brief Report content gap events
 */
void PrivateInstanceAAMP::ReportContentGap(long long timeMilliseconds, std::string id, double durationMS)
{
	bool bFireEvent = false;
	bool ignoreMetaAdd = false;
	// Check if contentGap was already reported
	std::vector<ContentGapInfo>::iterator iter;

	for (iter = contentGaps.begin(); iter != contentGaps.end(); iter++)
	{
		if ((timeMilliseconds >= iter->_timeMS-1000 && timeMilliseconds <= iter->_timeMS+1000 ))
		{
			if(iter->_id == id)
			{
				// Already same exists , ignore if periodGap information is complete.
				if(iter->_complete)
				{
					ignoreMetaAdd = true;
					break;
				}
				else
				{
					if(durationMS >= 0)
					{
						// New request with duration, mark complete and report it.
						iter->_durationMS = durationMS;
						iter->_complete = true;
					}
					else
					{
						// Duplicate report request, already processed
						ignoreMetaAdd = true;
						break;
					}
				}
			}
			else
			{
				continue;
			}
		}
		else if (iter->_timeMS < timeMilliseconds)
		{
			// move to next entry
			continue;
		}
		else if (iter->_timeMS > timeMilliseconds)
		{
			break;
		}
	}

	if(!ignoreMetaAdd)
	{
		bFireEvent = true;
		if(iter == contentGaps.end())
		{
			contentGaps.push_back(ContentGapInfo(timeMilliseconds, id, durationMS));
		}
		else
		{
			contentGaps.insert(iter, ContentGapInfo(timeMilliseconds, id, durationMS));
		}
	}


	if (bFireEvent)
	{
		ContentGapEventPtr eventData = std::make_shared<ContentGapEvent>(timeMilliseconds, durationMS, GetSessionId());
		AAMPLOG_INFO("aamp contentGap: start: %lld duration: %ld", timeMilliseconds, (long) durationMS);
		mEventManager->SendEvent(eventData);
	}
}

/**
 *   @brief Initialize CC after first frame received
 *          Sends CC handle event to listeners when first frame receives or video_dec handle rests
 */
void PrivateInstanceAAMP::InitializeCC(unsigned long decoderHandle)
{
	PlayerCCManager::GetInstance()->Init((void *)decoderHandle);
	if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
	{
		int overrideCfg = GETCONFIGVALUE_PRIV(eAAMPConfig_CEAPreferred);
		if (overrideCfg == 0)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: CC format override to 608 present, selecting 608CC");
			PlayerCCManager::GetInstance()->SetTrack("CC1");
		}
	}
}

/**
 *  @brief Notify first frame is displayed. Sends CC handle event to listeners.
 */
void PrivateInstanceAAMP::NotifyFirstFrameReceived(unsigned long ccDecoderHandle)
{
	AAMPLOG_TRACE("NotifyFirstFrameReceived()");

	// In the middle of stop processing we can receive state changing callback
	AAMPPlayerState state = GetState();
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}

	// If mFirstVideoFrameDisplayedEnabled, state will be changed in NotifyFirstVideoDisplayed()
	if(!mFirstVideoFrameDisplayedEnabled)
	{
		SetState(eSTATE_PLAYING);
	}
	{
		std::lock_guard<std::mutex> guard(mMutexPlaystart);
		waitforplaystart.notify_all();
	}
	if (eTUNED_EVENT_ON_GST_PLAYING == GetTuneEventConfig(IsLive()))
	{
		// This is an idle callback, so we can sent event synchronously
		if (SendTunedEvent())
		{
			AAMPLOG_WARN("aamp: - sent tune event on Tune Completion.");
		}
	}
	InitializeCC(ccDecoderHandle);

	NotifyPauseOnStartPlayback();
}

/**
 *   @brief Signal discontinuity of track.
 *   Called from StreamAbstractionAAMP to signal discontinuity
 */
bool PrivateInstanceAAMP::Discontinuity(AampMediaType track, bool setDiscontinuityFlag)
{
	bool ret = false;

	if (setDiscontinuityFlag)
	{
		ret = true;
	}
	else
	{
		SyncBegin();
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			ret = sink->Discontinuity(track);
		}
		SyncEnd();
	}

	if (ret)
	{
		mProcessingDiscontinuity[track] = true;
	}
	return ret;
}

/**
 * @brief Schedules retune or discontinuity processing based on state.
 */
void PrivateInstanceAAMP::ScheduleRetune(PlaybackErrorType errorType, AampMediaType trackType, bool bufferFull)
{
	std::unique_lock<std::mutex> gLock(gMutex, std::defer_lock);
	if (AAMP_NORMAL_PLAY_RATE == rate && ContentType_EAS != mContentType)
	{
		AAMPPlayerState state = GetState();
		if (((state != eSTATE_PLAYING) && (eGST_ERROR_VIDEO_BUFFERING != errorType)) || mSeekOperationInProgress)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Not processing reTune since state = %d, mSeekOperationInProgress = %d",
						state, mSeekOperationInProgress);
			return;
		}

		//  retune which does not restart as current position.
		//  eMEDIAFORMAT_PROGRESSIVE is playback which is done completely by GStreamer and less involvement of AAMP.
		// skipping retune for eMEDIAFORMAT_PROGRESSIVE content
		//Adding log line useful for triage purposes
		if (eMEDIAFORMAT_PROGRESSIVE == mMediaFormat)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Not processing reTune for eMEDIAFORMAT_PROGRESSIVE content ");
			return;
		}

		gLock.lock();
		if (this->mIsRetuneInProgress)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP:: Already Retune in progress");
			return;
		}
		gLock.unlock();

		/*If underflow is caused by a discontinuity processing, continue playback from discontinuity*/
		// If discontinuity process in progress, skip further processing
		// discontinuity flags are reset a bit earlier, additional checks added below to check if discontinuity processing in progress
		std::unique_lock<std::recursive_mutex> lock(mLock);
		if ((errorType != eGST_ERROR_PTS) &&
			(IsDiscontinuityProcessPending() || mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress))
		{
			if (mDiscontinuityTuneOperationId != 0 || mDiscontinuityTuneOperationInProgress)
			{
				AAMPLOG_WARN("PrivateInstanceAAMP: Discontinuity Tune handler already spawned(%d) or in progress(%d)",
					mDiscontinuityTuneOperationId, mDiscontinuityTuneOperationInProgress);
				return;
			}
			mDiscontinuityTuneOperationId = ScheduleAsyncTask(PrivateInstanceAAMP_ProcessDiscontinuity, (void *)this, "PrivateInstanceAAMP_ProcessDiscontinuity");

			AAMPLOG_WARN("PrivateInstanceAAMP: Underflow due to discontinuity handled");
			return;
		}

		lock.unlock();

		if (mpStreamAbstractionAAMP && mpStreamAbstractionAAMP->IsStreamerStalled())
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Ignore reTune due to playback stall");
			return;
		}
		else if (!ISCONFIGSET_PRIV(eAAMPConfig_InternalReTune))
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: Ignore reTune as disabled in configuration");
			return;
		}

		MediaTrack* mediaTrack = (mpStreamAbstractionAAMP != NULL) ? (mpStreamAbstractionAAMP->GetMediaTrack((TrackType)trackType)) : NULL;

		if((ISCONFIGSET_PRIV(eAAMPConfig_ReportBufferEvent)) &&
		(errorType == eGST_ERROR_UNDERFLOW) &&
		(trackType == eMEDIATYPE_VIDEO) &&
		(mediaTrack) &&
		(mediaTrack->GetBufferStatus() == BUFFER_STATUS_RED)
		&& (!bufferFull)
		)
		{
			SendBufferChangeEvent(true);  // Buffer state changed, buffer Under flow started
			if (!pipeline_paused &&  !PausePipeline(true, true))
			{
					AAMPLOG_ERR("Failed to pause the Pipeline");
			}
		}


		SendAnomalyEvent(ANOMALY_WARNING, "%s %s", GetMediaTypeName(trackType), getStringForPlaybackError(errorType));
		bool activeAAMPFound = false;
		gLock.lock();
		for (std::list<gActivePrivAAMP_t>::iterator iter = gActivePrivAAMPs.begin(); iter != gActivePrivAAMPs.end(); iter++)
		{
			if (this == iter->pAAMP)
			{
				gActivePrivAAMP_t *gAAMPInstance = &(*iter);
				if (gAAMPInstance->reTune)
				{
					AAMPLOG_WARN("PrivateInstanceAAMP: Already scheduled");
				}
				else
				{
					if(eGST_ERROR_PTS == errorType || eGST_ERROR_UNDERFLOW == errorType)
					{
						long long now = aamp_GetCurrentTimeMS();
						long long lastErrorReportedTimeMs = lastUnderFlowTimeMs[trackType];
						int ptsErrorThresholdValue = GETCONFIGVALUE_PRIV(eAAMPConfig_PTSErrorThreshold);
						if (lastErrorReportedTimeMs)
						{
							bool isRetuneRequired = false;
							long long diffMs = (now - lastErrorReportedTimeMs);
							if(GetLLDashServiceData()->lowLatencyMode )
							{
								if (diffMs < AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS)
								{
									isRetuneRequired = true;
								}
							}
							else
							{
								if (diffMs < AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS)
								{
									isRetuneRequired = true;
								}
							}
							if(isRetuneRequired)
							{
								gAAMPInstance->numPtsErrors++;
								if (gAAMPInstance->numPtsErrors >= ptsErrorThresholdValue)
								{
									AAMPLOG_WARN("PrivateInstanceAAMP: numPtsErrors %d, ptsErrorThreshold %d",
									gAAMPInstance->numPtsErrors, ptsErrorThresholdValue);
									gAAMPInstance->numPtsErrors = 0;
									gAAMPInstance->reTune = true;
									AAMPLOG_WARN("PrivateInstanceAAMP: Schedule Retune. diffMs %lld < threshold %lld",
										diffMs, GetLLDashServiceData()->lowLatencyMode?
										AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS:AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS);
									AdditionalTuneFailLogEntries();
									ScheduleAsyncTask(PrivateInstanceAAMP_Retune, (void *)this, "PrivateInstanceAAMP_Retune");
								}
							}
							else
							{
								gAAMPInstance->numPtsErrors = 0;
								AAMPLOG_ERR("PrivateInstanceAAMP: Not scheduling reTune since (diff %lld > threshold %lld) numPtsErrors %d, ptsErrorThreshold %d.",
									diffMs, GetLLDashServiceData()->lowLatencyMode?
									AAMP_MAX_TIME_LL_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS:AAMP_MAX_TIME_BW_UNDERFLOWS_TO_TRIGGER_RETUNE_MS,
									gAAMPInstance->numPtsErrors, ptsErrorThresholdValue);
							}
						}
						else
						{
							gAAMPInstance->numPtsErrors = 0;
							AAMPLOG_WARN("PrivateInstanceAAMP: Not scheduling reTune since first %s.", getStringForPlaybackError(errorType));
						}
						lastUnderFlowTimeMs[trackType] = now;
					}
					else
					{
						AAMPLOG_ERR("PrivateInstanceAAMP: Schedule Retune errorType %d error %s", errorType, getStringForPlaybackError(errorType));
						gAAMPInstance->reTune = true;
						AdditionalTuneFailLogEntries();
						ScheduleAsyncTask(PrivateInstanceAAMP_Retune, (void *)this, "PrivateInstanceAAMP_Retune");
					}
				}
				activeAAMPFound = true;
				break;
			}
		}
		gLock.unlock();
		if (!activeAAMPFound)
		{
			AAMPLOG_WARN("PrivateInstanceAAMP: %p not in Active AAMP list", this);
		}
	}
	else if (AAMP_RATE_PAUSE != rate && ContentType_EAS != mContentType)
	{
		//pipeline error during trickplay
		if(errorType == eGST_ERROR_GST_PIPELINE_INTERNAL)
		{
			AAMPLOG_WARN("Processing retune for GstPipeline Internal Error and rate %f", rate);
			SendAnomalyEvent(ANOMALY_WARNING, "%s GstPipeline Internal Error", GetMediaTypeName(trackType));
			gLock.lock();
			for (std::list<gActivePrivAAMP_t>::iterator iter = gActivePrivAAMPs.begin(); iter != gActivePrivAAMPs.end(); iter++)
			{
				if (this == iter->pAAMP)
				{
					gActivePrivAAMP_t *gAAMPInstance = &(*iter);
					gAAMPInstance->reTune = true;
					AdditionalTuneFailLogEntries();
					ScheduleAsyncTask(PrivateInstanceAAMP_Retune, (void *)this, "PrivateInstanceAAMP_Retune");
				}
			}
			gLock.unlock();
		}
		else
		{
			AAMPLOG_INFO("Not processing reTune for rate = %f, errorType %d , error %s", rate, errorType, getStringForPlaybackError(errorType));
		}
	}
}

/**
 * @brief Set player state
 */
void PrivateInstanceAAMP::SetState(AAMPPlayerState state)
{
	//bool sentSync = true;

	if (mState == state)
	{ // noop
		return;
	}

	if ( (state == eSTATE_PLAYING || state == eSTATE_BUFFERING || state == eSTATE_PAUSED)
		&& mState == eSTATE_SEEKING && (mEventManager->IsEventListenerAvailable(AAMP_EVENT_SEEKED)))
	{
		SeekedEventPtr event = std::make_shared<SeekedEvent>(GetPositionMilliseconds(), GetSessionId());
		mEventManager->SendEvent(event,AAMP_EVENT_SYNC_MODE);
	}
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mState = state;
	}

	mScheduler->SetState(mState);
	if (mEventManager->IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED))
	{
		if (mState == eSTATE_PREPARING)
		{
			StateChangedEventPtr eventData = std::make_shared<StateChangedEvent>(eSTATE_INITIALIZED, GetSessionId());
			mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
		}

		StateChangedEventPtr eventData = std::make_shared<StateChangedEvent>(mState, GetSessionId());
		mEventManager->SendEvent(eventData,AAMP_EVENT_SYNC_MODE);
	}
}

/**
 * @brief Get player state
 */
AAMPPlayerState PrivateInstanceAAMP::GetState(void)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	return mState;
}

/**
 *  @brief Add high priority idle task to the gstreamer
 *  @note task shall return 0 to be removed, 1 to be repeated
 */
gint PrivateInstanceAAMP::AddHighIdleTask(IdleTask task, void* arg,DestroyTask dtask)
{
	gint callbackID = g_idle_add_full(G_PRIORITY_HIGH_IDLE, task, (gpointer)arg, dtask);
	return callbackID;
}

/**
 *   @brief Check sink cache empty
 */
bool PrivateInstanceAAMP::IsSinkCacheEmpty(AampMediaType mediaType)
{
	bool ret_val = false;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		ret_val = sink->IsCacheEmpty(mediaType);
	}
	return ret_val;
}

/**
 * @brief Reset EOS SignalledFlag
 */
void PrivateInstanceAAMP::ResetEOSSignalledFlag()
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->ResetEOSSignalledFlag();
	}
}

/**
 * @brief Notify fragment caching complete
 */
void PrivateInstanceAAMP::NotifyFragmentCachingComplete()
{
	std::lock_guard<std::recursive_mutex> guard(mFragmentCachingLock);
	mFragmentCachingRequired = false;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->NotifyFragmentCachingComplete();
	}
	AAMPPlayerState state = GetState();
	if (state == eSTATE_BUFFERING)
	{
		if(mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP->NotifyPlaybackPaused(false);
		}
		SetState(eSTATE_PLAYING);
	}
}

/**
 * @brief Send tuned event to listeners if required
 */
bool PrivateInstanceAAMP::SendTunedEvent(bool isSynchronous)
{
	bool ret = false;
	{
		// Required for synchronising btw audio and video tracks in case of cdmidecryptor
		std::lock_guard<std::recursive_mutex> guard(mLock);
		ret = mTunedEventPending;
		mTunedEventPending = false;
	}
	if(ret)
	{
		AAMPEventPtr ev = std::make_shared<AAMPEventObject>(AAMP_EVENT_TUNED, GetSessionId());
		mEventManager->SendEvent(ev , AAMP_EVENT_SYNC_MODE);
	}
	return ret;
}

/**
 *   @brief Send VideoEndEvent
 */
bool PrivateInstanceAAMP::SendVideoEndEvent()
{
	bool ret = false;
	char * strVideoEndJson = NULL;
	// Required for protecting mVideoEnd object
	std::unique_lock<std::recursive_mutex> lock(mLock);
	if(mVideoEnd)
	{
		//Update VideoEnd Data
		if(mTimeAtTopProfile > 0)
		{
			// Losing millisecond of data in conversion from double to long
			mVideoEnd->SetTimeAtTopProfile(mTimeAtTopProfile);
			mVideoEnd->SetTimeToTopProfile(mTimeToTopProfile);
		}
		mVideoEnd->SetTotalDuration(mPlaybackDuration);

		// re initialize for next tune collection
		mTimeToTopProfile = 0;
		mTimeAtTopProfile = 0;
		mPlaybackDuration = 0;
		mCurrentLanguageIndex = 0;

		//Memory of this string will be deleted after sending event by destructor of AsyncMetricsEventDescriptor
		if(ISCONFIGSET_PRIV(eAAMPConfig_EnableVideoEndEvent) && mEventManager->IsEventListenerAvailable(AAMP_EVENT_REPORT_METRICS_DATA))
		{
			if(mFogTSBEnabled)
			{
				std::string videoEndData;
				GetOnVideoEndSessionStatData(videoEndData);
				if(videoEndData.size())
				{
					AAMPLOG_INFO("TsbSessionEnd:%s", videoEndData.c_str());
					strVideoEndJson = mVideoEnd->ToJsonString(videoEndData.c_str());
					//cJSON_free( data );
					//data = NULL;
				}
			}
			else
			{
				strVideoEndJson = mVideoEnd->ToJsonString();
			}
		}
		SAFE_DELETE(mVideoEnd);
	}
	mVideoEnd = new CVideoStat(mMediaFormatName[mMediaFormat]);
	mVideoEnd->SetDisplayResolution(mDisplayWidth,mDisplayHeight);
	lock.unlock();

	if(strVideoEndJson)
	{
		AAMPLOG_INFO("VideoEnd:%s", strVideoEndJson);
		MetricsDataEventPtr e = std::make_shared<MetricsDataEvent>(MetricsDataType::AAMP_DATA_VIDEO_END, this->mTraceUUID, strVideoEndJson, GetSessionId());
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		free(strVideoEndJson);
		ret = true;
	}
	return ret;
}

/**
 * @brief updates profile Resolution to VideoStat object
 */
void PrivateInstanceAAMP::UpdateVideoEndProfileResolution(AampMediaType mediaType, BitsPerSecond bitrate, int width, int height)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	if(mVideoEnd)
	{
		VideoStatTrackType trackType = VideoStatTrackType::STAT_VIDEO;
		if(mediaType == eMEDIATYPE_IFRAME)
		{
			trackType = VideoStatTrackType::STAT_IFRAME;
		}
		mVideoEnd->SetProfileResolution(trackType,bitrate,width,height);
	}
}

/**
 *  @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calculation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime)
{
	UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl,duration,curlDownloadTime, false,false);
}

/**
 *   @brief updates time shift buffer status
 *
 */
void PrivateInstanceAAMP::UpdateVideoEndTsbStatus(bool btsbAvailable)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	if(mVideoEnd)
	{
		mVideoEnd->SetTsbStatus(btsbAvailable);
	}
}

/**
 * @brief updates profile capped status
 */
void PrivateInstanceAAMP::UpdateProfileCappedStatus(void)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	if(mVideoEnd)
	{
		mVideoEnd->SetProfileCappedStatus(mProfileCappedStatus);
	}
}

/**
 * @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calculation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double duration, double curlDownloadTime, bool keyChanged, bool isEncrypted, ManifestData * manifestData)
{
	int audioIndex = 1;
	// ignore for write and aborted errors
	// these are generated after trick play options,
	if( curlOrHTTPCode > 0 &&  !(curlOrHTTPCode == CURLE_ABORTED_BY_CALLBACK || curlOrHTTPCode == CURLE_WRITE_ERROR) )
	{
		VideoStatDataType dataType = VideoStatDataType::VE_DATA_UNKNOWN;

		VideoStatTrackType trackType = VideoStatTrackType::STAT_UNKNOWN;
		switch(mediaType)
		{
			case eMEDIATYPE_MANIFEST:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_MAIN;
			}
				break;

			case eMEDIATYPE_PLAYLIST_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_VIDEO;
			}
				break;

			case eMEDIATYPE_PLAYLIST_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;

			case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;

			case eMEDIATYPE_PLAYLIST_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_MANIFEST;
				trackType = STAT_IFRAME;
			}
				break;

			case eMEDIATYPE_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_VIDEO;
				// always Video fragment will be from same thread so mutex required

				// !!!!!!!!!! To Do : Support this stats for Audio Only streams !!!!!!!!!!!!!!!!!!!!!
				//Is success
				if (((curlOrHTTPCode == 200) || (curlOrHTTPCode == 206))  && duration > 0)
				{
					if(mpStreamAbstractionAAMP->GetProfileCount())
					{
						BitsPerSecond maxBitrateSupported = mpStreamAbstractionAAMP->GetMaxBitrate();
						if(maxBitrateSupported == bitrate)
						{
							mTimeAtTopProfile += duration;

						}

					}
					if(mTimeAtTopProfile == 0) // we havent achieved top profile yet
					{
						mTimeToTopProfile += duration; // started at top profile
					}

					mPlaybackDuration += duration;
				}

			}
				break;
			case eMEDIATYPE_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
			case eMEDIATYPE_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;
			case eMEDIATYPE_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_IFRAME;
			}
				break;

			case eMEDIATYPE_INIT_IFRAME:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_IFRAME;
			}
				break;

			case eMEDIATYPE_INIT_VIDEO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_VIDEO;
			}
				break;

			case eMEDIATYPE_INIT_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;

			case eMEDIATYPE_INIT_AUX_AUDIO:
			{
				dataType = VideoStatDataType::VE_DATA_INIT_FRAGMENT;
				trackType = VideoStatTrackType::STAT_AUDIO;
				audioIndex += mCurrentLanguageIndex;
			}
				break;

			case eMEDIATYPE_SUBTITLE:
			{
				dataType = VideoStatDataType::VE_DATA_FRAGMENT;
				trackType = VideoStatTrackType::STAT_SUBTITLE;
			}
				break;

			default:
				break;
		}


		// Required for protecting mVideoStat object
		if( dataType != VideoStatDataType::VE_DATA_UNKNOWN
			&& trackType != VideoStatTrackType::STAT_UNKNOWN)
		{
			std::lock_guard<std::recursive_mutex> guard(mLock);
			if(mVideoEnd)
			{
				//curl download time is in seconds, convert it into milliseconds for video end metrics
				mVideoEnd->Increment_Data(dataType,trackType,bitrate,curlDownloadTime * 1000,curlOrHTTPCode,false,audioIndex, manifestData);
				if((curlOrHTTPCode != 200) && (curlOrHTTPCode != 206) && strUrl.c_str())
				{
					//set failure url
					mVideoEnd->SetFailedFragmentUrl(trackType,bitrate,strUrl);
				}
				if(dataType == VideoStatDataType::VE_DATA_FRAGMENT)
				{
					mVideoEnd->Record_License_EncryptionStat(trackType,isEncrypted,keyChanged);
				}
			}
		}
		else
		{
			AAMPLOG_INFO("PrivateInstanceAAMP: Could Not update VideoEnd Event dataType:%d trackType:%d response:%d",
						 dataType,trackType,curlOrHTTPCode);
		}
	}
}

/**
 * @brief updates abr metrics to VideoStat object,
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(AAMPAbrInfo & info)
{
	//only for Ramp down case
	if(info.desiredProfileIndex < info.currentProfileIndex)
	{
		AAMPLOG_INFO("UpdateVideoEnd:abrinfo currIdx:%d desiredIdx:%d for:%d",  info.currentProfileIndex,info.desiredProfileIndex,info.abrCalledFor);
		std::lock_guard<std::recursive_mutex> guard(mLock);
		if(info.abrCalledFor == AAMPAbrType::AAMPAbrBandwidthUpdate)
		{
			if(mVideoEnd)
			{
				mVideoEnd->Increment_NetworkDropCount();
			}
		}
		else if (info.abrCalledFor == AAMPAbrType::AAMPAbrFragmentDownloadFailed)
		{
			if(mVideoEnd)
			{
				mVideoEnd->Increment_ErrorDropCount();
			}
		}
	}
}

/**
 *   @fn UpdateVideoEndMetrics
 *
 *   @param[in] adjustedRate - new rate after correction
 *   @return void
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(double adjustedRate)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	if(adjustedRate != (double)AAMP_NORMAL_PLAY_RATE)
	{
		if(mVideoEnd)
		{
			mVideoEnd->Increment_RateCorrectionCount();
		}
	}
}


/**
 * @brief updates download metrics to VideoStat object, this is used for VideoFragment as it takes duration for calculation purpose.
 */
void PrivateInstanceAAMP::UpdateVideoEndMetrics(AampMediaType mediaType, BitsPerSecond bitrate, int curlOrHTTPCode, std::string& strUrl, double curlDownloadTime, ManifestData * manifestData)
{
	UpdateVideoEndMetrics(mediaType, bitrate, curlOrHTTPCode, strUrl,0,curlDownloadTime, false, false, manifestData);
}

/**
 *   @brief Check if fragment caching is required
 */
bool PrivateInstanceAAMP::IsFragmentCachingRequired()
{
	//Prevent enabling Fragment Caching during Seek While Pause
	return (!mPauseOnFirstVideoFrameDisp && mFragmentCachingRequired);
}

/**
 * @brief Get player video size
 */
void PrivateInstanceAAMP::GetPlayerVideoSize(int &width, int &height)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->GetVideoSize(width, height);
	}
}

/**
 * @brief Set an idle callback as event dispatched state
 */
void PrivateInstanceAAMP::SetCallbackAsDispatched(guint id)
{
	std::lock_guard<std::recursive_mutex> guard(mEventLock);
	std::map<guint, bool>::iterator  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		assert (itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		AAMPLOG_WARN("id %d not in mPendingAsyncEvents, insert and mark as not pending", id);
		mPendingAsyncEvents[id] = false;
	}
}

/**
 *   @brief Set an idle callback as event pending state
 */
void PrivateInstanceAAMP::SetCallbackAsPending(guint id)
{
	std::lock_guard<std::recursive_mutex> guard(mEventLock);
	std::map<guint, bool>::iterator  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		assert (!itr->second);
		AAMPLOG_WARN("id %d already in mPendingAsyncEvents and completed, erase it", id);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		mPendingAsyncEvents[id] = true;
	}
}

/**
 * @brief Add/Remove a custom HTTP header and value.
 */
void PrivateInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
	const char *headerValueAsString = NULL;
	switch( headerValue.size() )
	{
		case 0:
			headerValueAsString = "<clear>";
			break;
		case 1:
			headerValueAsString = headerValue[0].c_str();
			break;
		default:
			headerValueAsString = "<array>";
			break;
	}
	AAMPLOG_MIL( "header=%s value=%s requestType=%s",
				headerName.c_str(),
				headerValueAsString,
				isLicenseHeader?"License":"CDN" );

	bool emptyHeader = (headerName.empty() || (0 == headerName.compare(":")) );
	bool emptyValue  = (headerValue.size() == 0);

	if(headerName.back() != ':')
	{ // must end with ":"
		headerName += ':';
	}
	if (isLicenseHeader)
	{ // requestType = License
		if ( !emptyValue )
		{
			mCustomLicenseHeaders[headerName] = headerValue;
		}
		else if (emptyHeader)
		{
			mCustomLicenseHeaders.clear();
		}
		else
		{
			mCustomLicenseHeaders.erase(headerName);
		}
	}
	else
	{ // requestType = CDN
		if ( !emptyValue )
		{
			mCustomHeaders[headerName] = headerValue;
		}
		else if (emptyHeader)
		{
			mCustomHeaders.clear();
		}
		else
		{
			mCustomHeaders.erase(headerName);
		}
	}
}

/**
 *  @brief UpdateLiveOffset live offset [Sec]
 */
void PrivateInstanceAAMP::UpdateLiveOffset()
{
	if(!IsLiveAdjustRequired()) /* Ideally checking the content is either "ivod/cdvr" to adjust the liveoffset on trickplay. */
	{
		mLiveOffset = GETCONFIGVALUE_PRIV(eAAMPConfig_CDVRLiveOffset);
	}
	else
	{
		mLiveOffset = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset);
	}
	AAMPLOG_INFO("Live offset value updated to %lf", mLiveOffset);
}

/**
 * @brief Send stalled event to listeners
 */
void PrivateInstanceAAMP::SendStalledErrorEvent()
{
	char description[MAX_ERROR_DESCRIPTION_LENGTH];
	memset(description, '\0', MAX_ERROR_DESCRIPTION_LENGTH);
	int stalltimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_StallTimeoutMS);
	snprintf(description, (MAX_ERROR_DESCRIPTION_LENGTH - 1), "Playback has been stalled for more than %d ms due to lack of new fragments", stalltimeout);
	SendErrorEvent(AAMP_TUNE_PLAYBACK_STALLED, description);
}

/**
 * @brief Sets up the timestamp sync for subtitle renderer
 */
void PrivateInstanceAAMP::UpdateSubtitleTimestamp()
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->StartSubtitleParser();
	}
}

/**
 * @brief pause/un-pause subtitles
 *
 */
void PrivateInstanceAAMP::PauseSubtitleParser(bool pause)
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->PauseSubtitleParser(pause);
	}
}


/**
 * @brief Notify if first buffer processed by gstreamer
 */
void PrivateInstanceAAMP::NotifyFirstBufferProcessed(const std::string& videoRectangle)
{
	// If mFirstVideoFrameDisplayedEnabled, state will be changed in NotifyFirstVideoDisplayed()
	AAMPPlayerState state = GetState();

	// In the middle of stop processing we can receive state changing callback
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}

	if (!mFirstVideoFrameDisplayedEnabled
			&& state == eSTATE_SEEKING)
	{
		SetState(eSTATE_PLAYING);
	}
	trickStartUTCMS = aamp_GetCurrentTimeMS();
	//Do not edit or remove this log - it is used in L2 test
	AAMPLOG_WARN("seek pos %.3f", seek_pos_seconds);


	if(ISCONFIGSET_PRIV(eAAMPConfig_UseSecManager) || ISCONFIGSET_PRIV(eAAMPConfig_UseFireboltSDK))
	{
		double streamPositionMs = GetStreamPositionMs();
		mDRMLicenseManager->setVideoMute(IsLive(), GetCurrentLatency(), IsAtLivePoint(), GetLiveOffsetMs(), video_muted, streamPositionMs);
		mDRMLicenseManager->setPlaybackSpeedState(IsLive(), GetCurrentLatency(), IsAtLivePoint(), GetLiveOffsetMs(),rate, streamPositionMs, true);
		int x = 0,y = 0,w = 0,h = 0;
		if (!videoRectangle.empty())
		{
			sscanf(videoRectangle.c_str(),"%d,%d,%d,%d",&x,&y,&w,&h);
		}
		AAMPLOG_WARN("calling setVideoWindowSize  w:%d x h:%d ",w,h);
		mDRMLicenseManager->setVideoWindowSize(w,h);
	}

}

/**
 * @brief Reset trick start position
 */
void PrivateInstanceAAMP::ResetTrickStartUTCTime()
{
	trickStartUTCMS = aamp_GetCurrentTimeMS();
}

/**
 * @brief Get stream type
 */
int PrivateInstanceAAMP::getStreamType()
{

	int type = 0;

	if(mMediaFormat == eMEDIAFORMAT_DASH)
	{
		type = 20;
	}
	else if( mMediaFormat == eMEDIAFORMAT_HLS)
	{
		type = 10;
	}
	else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)// eMEDIAFORMAT_PROGRESSIVE
	{
		type = 30;
	}
	else if (mMediaFormat == eMEDIAFORMAT_HLS_MP4)
	{
		type = 40;
	}
	else
	{
		type = 0;
	}

	if (mCurrentDrm != nullptr) {
		type += mCurrentDrm->getDrmCodecType();
	}
	return type;
}

/**
 * @brief Get Mediaformat type
 *
 * @returns eMEDIAFORMAT
 */
MediaFormat PrivateInstanceAAMP::GetMediaFormatTypeEnum() const
{
	return mMediaFormat;
}

/**
 * @brief Extracts / Generates MoneyTrace string
 */
void PrivateInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
	char moneytracebuf[512];
	memset(moneytracebuf, 0, sizeof(moneytracebuf));

	if (mCustomHeaders.size() > 0)
	{
		for (std::unordered_map<std::string, std::vector<std::string>>::const_iterator it = mCustomHeaders.begin();
			it != mCustomHeaders.end(); it++)
		{
			if (it->first.compare("X-MoneyTrace:") == 0)
			{
				if (it->second.size() >= 2)
				{
					snprintf(moneytracebuf, sizeof(moneytracebuf), "trace-id=%s;parent-id=%s;span-id=%lld",
					(const char*)it->second.at(0).c_str(),
					(const char*)it->second.at(1).c_str(),
					aamp_GetCurrentTimeMS());
				}
				else if (it->second.size() == 1)
				{
					snprintf(moneytracebuf, sizeof(moneytracebuf), "trace-id=%s;parent-id=%lld;span-id=%lld",
						(const char*)it->second.at(0).c_str(),
						aamp_GetCurrentTimeMS(),
						aamp_GetCurrentTimeMS());
				}
				customHeader.append(moneytracebuf);
				break;
			}
		}
	}
	// No money trace is available in customheader from JS , create a new moneytrace locally
	if(customHeader.size() == 0)
	{
		// No Moneytrace info available in tune data
		AAMPLOG_WARN("No Moneytrace info available in tune request,need to generate one");
		uuid_t uuid;
		uuid_generate(uuid);
		char uuidstr[128];
		uuid_unparse(uuid, uuidstr);
		for (char *ptr = uuidstr; *ptr; ++ptr) {
			*ptr = tolower(*ptr);
		}
		snprintf(moneytracebuf,sizeof(moneytracebuf),"trace-id=%s;parent-id=%lld;span-id=%lld",uuidstr,aamp_GetCurrentTimeMS(),aamp_GetCurrentTimeMS());
		customHeader.append(moneytracebuf);
	}
	AAMPLOG_TRACE("[GetMoneyTraceString] MoneyTrace[%s]",customHeader.c_str());
}

/**
 * @brief Notify the decryption completion of the fist fragment.
 */
void PrivateInstanceAAMP::NotifyFirstFragmentDecrypted()
{
	if(mTunedEventPending)
	{
		if (eTUNED_EVENT_ON_FIRST_FRAGMENT_DECRYPTED == GetTuneEventConfig(IsLive()))
		{
			// For HLS - This is invoked by fetcher thread, so we have to sent asynchronously
			if (SendTunedEvent(false))
			{
				AAMPLOG_WARN("aamp: %s - sent tune event after first fragment fetch and decrypt", mMediaFormatName[mMediaFormat]);
			}
		}
	}
}

/**
 * @brief  Get PTS of first sample.
 */
double PrivateInstanceAAMP::GetFirstPTS()
{
	assert(NULL != mpStreamAbstractionAAMP);
	return mpStreamAbstractionAAMP->GetFirstPTS();
}

/**
 * @brief  Get PTS offset for MidFragment
 */
double PrivateInstanceAAMP::GetMidSeekPosOffset()
{
	assert(NULL != mpStreamAbstractionAAMP);
	return mpStreamAbstractionAAMP->GetMidSeekPosOffset();
}

/**
 * @brief Check if Live Adjust is required for current content. ( For "vod/ivod/ip-dvr/cdvr/eas", Live Adjust is not required ).
 */
bool PrivateInstanceAAMP::IsLiveAdjustRequired()
{
	bool retValue;

	switch (mContentType)
	{
		case ContentType_IVOD:
		case ContentType_VOD:
		case ContentType_CDVR:
		case ContentType_IPDVR:
		case ContentType_EAS:
			retValue = false;
			break;

		case ContentType_SLE:
			retValue = true;
			break;

		default:
			retValue = true;
			break;
	}
	return retValue;
}

/**
 * @brief Generate http header response event
 */
void PrivateInstanceAAMP::SendHTTPHeaderResponse()
{
	for (auto const& pair: httpHeaderResponses) {
		HTTPResponseHeaderEventPtr event = std::make_shared<HTTPResponseHeaderEvent>(pair.first.c_str(), pair.second, GetSessionId());
		AAMPLOG_INFO("HTTPResponseHeader evt Header:%s Response:%s", event->getHeader().c_str(), event->getResponse().c_str());
		SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
}

std::vector<float> PrivateInstanceAAMP::getSupportedPlaybackSpeeds(void)
{
	std::vector<float> supportedPlaybackSpeeds = { 0,1 };
	if (mIsIframeTrackPresent)
	{ //Iframe track present and hence playbackRate change is supported
		supportedPlaybackSpeeds.push_back(-64);
		supportedPlaybackSpeeds.push_back(-32);
		supportedPlaybackSpeeds.push_back(-16);
		supportedPlaybackSpeeds.push_back(-4);
		supportedPlaybackSpeeds.push_back(4);
		supportedPlaybackSpeeds.push_back(16);
		supportedPlaybackSpeeds.push_back(32);
		supportedPlaybackSpeeds.push_back(64);
	}
	if( ISCONFIGSET_PRIV(eAAMPConfig_EnableSlowMotion) )
	{
		supportedPlaybackSpeeds.push_back( 0.5 );
	}
	return supportedPlaybackSpeeds;
}

bool PrivateInstanceAAMP::IsFogUrl(const char *mainManifestUrl)
{
	return strcasestr(mainManifestUrl, AAMP_FOG_TSB_URL_KEYWORD) && ISCONFIGSET_PRIV(eAAMPConfig_Fog);
}

/**
 * @brief  Generate media metadata event based on parsed attribute values.
 *
 */
void PrivateInstanceAAMP::SendMediaMetadataEvent(void)
{
	std::vector<BitsPerSecond> bitrateList;
	std::set<std::string> langList;
	int width  = 1280;
	int height = 720;

	bitrateList = mpStreamAbstractionAAMP->GetVideoBitrates();
	for (int i = 0; i <mMaxLanguageCount; i++)
	{
		langList.insert(mLanguageList[i]);
	}

	GetPlayerVideoSize(width, height);

	std::string drmType = "NONE";
	DrmHelperPtr helper = GetCurrentDRM();
	if (helper)
	{
		drmType = helper->friendlyName();
	}
	// Introduced to send the effective URL to app
	std::string url = mManifestUrl;
	if(mFogTSBEnabled)
	{
		url =  mTunedManifestUrl;
		// For Fog playback mTunedManifestUrl contains a defogged URL using the "_fogs" scheme
		// To send an event to app we convert the URL scheme to "https" by replacing the prefix which is the CDN url sent from app
		url.replace(0,4,"http");
	}
	MediaMetadataEventPtr event = std::make_shared<MediaMetadataEvent>(CONVERT_SEC_TO_MS(durationSeconds), width, height, mpStreamAbstractionAAMP->hasDrm, IsLive(), drmType, mpStreamAbstractionAAMP->mProgramStartTime, mTsbDepthMs, GetSessionId(), url);

	for (auto iter = langList.begin(); iter != langList.end(); iter++)
	{
		if (!iter->empty())
		{
			// assert if size >= < MAX_LANGUAGE_TAG_LENGTH
			assert(iter->size() < MAX_LANGUAGE_TAG_LENGTH);
			event->addLanguage((*iter));
		}
	}

	for (int i = 0; i < bitrateList.size(); i++)
	{
		event->addBitrate(bitrateList[i]);
	}

	auto supportedSpeeds = getSupportedPlaybackSpeeds();
	for( auto speed : supportedSpeeds )
	{
		event->addSupportedSpeed(speed);
	}

	event->setMediaFormat(mMediaFormatName[mMediaFormat]);

	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief  Generate supported speeds changed event based on arg passed.
 */
void PrivateInstanceAAMP::SendSupportedSpeedsChangedEvent(bool isIframeTrackPresent)
{
	SupportedSpeedsChangedEventPtr event = std::make_shared<SupportedSpeedsChangedEvent>(GetSessionId());
	std::vector<float> supportedPlaybackSpeeds { -64, -32, -16, -4, -1, 0, 0.5, 1, 4, 16, 32, 64 };
	auto supportedSpeeds = getSupportedPlaybackSpeeds();
	for( auto speed : supportedSpeeds )
	{
		event->addSupportedSpeed(speed);
	}
	AAMPLOG_WARN("aamp: sending supported speeds changed event with count %d", event->getSupportedSpeedCount());
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Generate Blocked event based on args passed.
 */
void PrivateInstanceAAMP::SendBlockedEvent(const std::string & reason, const std::string currentLocator)
{
	BlockedEventPtr event = std::make_shared<BlockedEvent>(reason, currentLocator, GetSessionId());
	SendEvent(event,AAMP_EVENT_SYNC_MODE);
	if (0 == reason.compare("SERVICE_PIN_LOCKED"))
	{
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			PlayerCCManager::GetInstance()->SetParentalControlStatus(true);
		}
	}
}

/**
 * @brief  Generate WatermarkSessionUpdate event based on args passed.
 */
void PrivateInstanceAAMP::SendWatermarkSessionUpdateEvent(uint32_t sessionHandle, uint32_t status, const std::string &system)
{
	WatermarkSessionUpdateEventPtr event = std::make_shared<WatermarkSessionUpdateEvent>(sessionHandle, status, system, GetSessionId());
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief  Check if tune completed or not.
 */
bool PrivateInstanceAAMP::IsTuneCompleted()
{
	return mTuneCompleted;
}

/**
 * @brief Get Preferred DRM.
 */
DRMSystems PrivateInstanceAAMP::GetPreferredDRM()
{
	int drmType = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredDRM);
	return (DRMSystems)drmType;
}

/**
 * @brief Notification from the stream abstraction that a new SCTE35 event is found.
 */
void PrivateInstanceAAMP::FoundEventBreak(const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo)
{
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableClientDai) && !adBreakId.empty())
	{
		AAMPLOG_WARN("[CDAI] Found Adbreak on period[%s] Duration[%d] isDAIEvent[%d]", adBreakId.c_str(), brInfo.duration, brInfo.isDAIEvent);
		if (brInfo.isDAIEvent)
		{
			std::string adId("");
			std::string url("");
			mCdaiObject->SetAlternateContents(adBreakId, adId, url, startMS, brInfo.duration);	//A placeholder to avoid multiple scte35 event firing for the same adbreak
		}
		//Ignoring past SCTE events.
		//mFogTSBEnabled check is added to ensure the change won't effect IPVOD
		AAMPLOG_INFO("[CDAI] mTuneCompleted:%d mFogTSBEnabled:%d", mTuneCompleted, mFogTSBEnabled);
		if (mTuneCompleted || !mFogTSBEnabled)
		{
			SaveNewTimedMetadata((long long) startMS, brInfo.name.c_str(), brInfo.payload.c_str(), (int)brInfo.payload.size(), adBreakId.c_str(), brInfo.duration);
		}
		else
		{
			AAMPLOG_WARN("[CDAI] Discarding SCTE event for period:%s  since tune is not completed",adBreakId.c_str());
		}
	}
}

/**
 *  @brief Setting the alternate contents' (Ads/blackouts) URL
 */
void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
	if(ISCONFIGSET_PRIV(eAAMPConfig_EnableClientDai) && mCdaiObject)
	{
		mCdaiObject->SetAlternateContents(adBreakId, adId, url);
	}
	else
	{
		AAMPLOG_WARN("is called! CDAI not enabled!! Rejecting the promise.");
		SendAdResolvedEvent(adId, false, 0, 0, eCDAI_ERROR_ADS_MISCONFIGURED);
	}
}

/**
 * @brief Send status of Ad manifest downloading & parsing
 */
void PrivateInstanceAAMP::SendAdResolvedEvent(const std::string &adId, bool status, uint64_t startMS, uint64_t durationMs, AAMPCDAIError errorCode)
{
	if (errorCode < eCDAI_ERROR_ADS_MISCONFIGURED || errorCode > eCDAI_ERROR_NONE)
	{
		errorCode = eCDAI_ERROR_UNKNOWN;
	}
	if (mDownloadsEnabled)	//Send it, only if Stop not called
	{
		AdResolvedEventPtr e = std::make_shared<AdResolvedEvent>(status, adId, startMS, durationMs, gCDAIErrorDetails[errorCode].first, gCDAIErrorDetails[errorCode].second, GetSessionId());
		AAMPLOG_WARN("PrivateInstanceAAMP: [CDAI] Sent resolved status=%d for adId[%s] with errorCode[%s] and errorDescription[%s]", status, adId.c_str(),
			gCDAIErrorDetails[errorCode].first.c_str(), gCDAIErrorDetails[errorCode].second.c_str());
		SendEvent(e,AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief Deliver all pending Ad events to JSPP
 *
 * @param[in] immediate - deliver immediately or not
 * @param[in] position - current playback position
 */
void PrivateInstanceAAMP::DeliverAdEvents(bool immediate, double position)
{
	std::lock_guard<std::mutex> lock(mAdEventQMtx);
	while (!mAdEventsQ.empty())
	{
		AAMPEventPtr e = mAdEventsQ.front();
		AdPlacementEventPtr placementEvt  = nullptr;
		AdReservationEventPtr reservationEvt = nullptr;
		AAMPEventType evtType = e->getType();

		//If immediate is true, it is a failed case and deliver all events immediately
		if(immediate)
		{
			AAMPLOG_MIL("PrivateInstanceAAMP:, [CDAI] Delivered AdEvent[%s] to JSPP. pos=%lfms", ADEVENT2STRING(evtType), position);
			mEventManager->SendEvent(e,AAMP_EVENT_SYNC_MODE);
		}
		else
		{
			double target = -1;
			if (AAMP_EVENT_AD_PLACEMENT_START <= evtType && AAMP_EVENT_AD_PLACEMENT_ERROR >= evtType)
			{
				placementEvt = std::dynamic_pointer_cast<AdPlacementEvent>(e);
				target = static_cast<double>(placementEvt->getAbsolutePositionMs());
			}
			else if (AAMP_EVENT_AD_RESERVATION_START <= evtType && AAMP_EVENT_AD_RESERVATION_END >= evtType)
			{
				reservationEvt = std::dynamic_pointer_cast<AdReservationEvent>(e);
				target = static_cast<double>(reservationEvt->getAbsolutePositionMs());
			}
			else
			{
				AAMPLOG_WARN("PrivateInstanceAAMP: [CDAI] Unknown Ad event type %d", evtType);
				mAdEventsQ.pop();
				continue;
			}
			// Check if the event is ready to be delivered
			if((position != -1) && (position < target))
			{
				AAMPLOG_TRACE( "Deferring transmission AdEvent[%s] pos=%lfms target=%lfms", ADEVENT2STRING(evtType), position, target);
				break;
			}

			AAMPLOG_MIL("PrivateInstanceAAMP:, [CDAI] Delivered AdEvent[%s] to JSPP. pos=%lfms target=%lfms", ADEVENT2STRING(evtType), position, target);
			mEventManager->SendEvent(e,AAMP_EVENT_ASYNC_MODE);
		}
		if(placementEvt && AAMP_EVENT_AD_PLACEMENT_START == evtType)
		{
			mAdProgressId       = placementEvt->getAdId();
			mAdPrevProgressTime = NOW_STEADY_TS_MS;
			mAdCurOffset        = placementEvt->getOffset();
			mAdDuration         = placementEvt->getDuration();
			mAdAbsoluteStartTime = placementEvt->getAbsolutePositionMs();
			AAMPLOG_INFO("PrivateInstanceAAMP: [CDAI] AdProgressId[%s] AdOffset[%d] AdDuration[%d] AdAbsoluteStartTime[%" PRIu64 "]", mAdProgressId.c_str(), mAdCurOffset, mAdDuration, mAdAbsoluteStartTime);
		}
		else if(AAMP_EVENT_AD_PLACEMENT_END == evtType || AAMP_EVENT_AD_PLACEMENT_ERROR == evtType)
		{
			mAdProgressId = "";
		}
		mAdEventsQ.pop();
	}
}

/**
 * @brief Send Ad reservation event
 */
void PrivateInstanceAAMP::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, uint64_t absolutePositionMs, bool immediate)
{
	if(AAMP_EVENT_AD_RESERVATION_START == type || AAMP_EVENT_AD_RESERVATION_END == type)
	{
		AAMPLOG_INFO("PrivateInstanceAAMP: [CDAI] Pushed [%s] of adBreakId[%s] to Queue.", ADEVENT2STRING(type), adBreakId.c_str());

		AdReservationEventPtr e = std::make_shared<AdReservationEvent>(type, adBreakId, position, absolutePositionMs, GetSessionId());

		{
			{
				std::lock_guard<std::mutex> lock(mAdEventQMtx);
				mAdEventsQ.push(e);
			}
			if(immediate)
			{
				//dispatch all ad events now
				DeliverAdEvents(true);
			}
		}
	}
}

/**
 * @brief Send Ad placement event
 */
void PrivateInstanceAAMP::SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, uint32_t adOffset, uint32_t adDuration, bool immediate, long error_code)
{
	if(AAMP_EVENT_AD_PLACEMENT_START <= type && AAMP_EVENT_AD_PLACEMENT_ERROR >= type)
	{
		AAMPLOG_INFO("PrivateInstanceAAMP: [CDAI] Pushed [%s] of adId[%s] to Queue.", ADEVENT2STRING(type), adId.c_str());

		AdPlacementEventPtr e = std::make_shared<AdPlacementEvent>(type, adId, position, absolutePositionMs, GetSessionId(), adOffset * 1000 /*MS*/, adDuration, error_code);

		{
			{
				std::lock_guard<std::mutex> lock(mAdEventQMtx);
				mAdEventsQ.push(e);
			}
			if(immediate)
			{
				//dispatch all ad events now
				DeliverAdEvents(true);
			}
		}
	}
}

/**
 *  @brief Get stream type as printable format
 */
std::string PrivateInstanceAAMP::getStreamTypeString()
{
	std::string type = mMediaFormatName[mMediaFormat];

	if(mCurrentDrm != nullptr) //Incomplete Init won't be set the DRM
	{
		type += "/";
		type += mCurrentDrm->friendlyName();
	}
	else
	{
		type += "/Clear";
	}
	return type;
}

/**
 * @brief Convert media file type to profiler bucket type
 */
ProfilerBucketType PrivateInstanceAAMP::mediaType2Bucket(AampMediaType mediaType)
{
	ProfilerBucketType pbt;
	switch(mediaType)
	{
		case eMEDIATYPE_VIDEO:
			pbt = PROFILE_BUCKET_FRAGMENT_VIDEO;
			break;
		case eMEDIATYPE_AUDIO:
			pbt = PROFILE_BUCKET_FRAGMENT_AUDIO;
			break;
		case eMEDIATYPE_SUBTITLE:
			pbt = PROFILE_BUCKET_FRAGMENT_SUBTITLE;
			break;
		case eMEDIATYPE_AUX_AUDIO:
			pbt = PROFILE_BUCKET_FRAGMENT_AUXILIARY;
			break;
		case eMEDIATYPE_MANIFEST:
			pbt = PROFILE_BUCKET_MANIFEST;
			break;
		case eMEDIATYPE_INIT_VIDEO:
			pbt = PROFILE_BUCKET_INIT_VIDEO;
			break;
		case eMEDIATYPE_INIT_AUDIO:
			pbt = PROFILE_BUCKET_INIT_AUDIO;
			break;
		case eMEDIATYPE_INIT_SUBTITLE:
			pbt = PROFILE_BUCKET_INIT_SUBTITLE;
			break;
		case eMEDIATYPE_INIT_AUX_AUDIO:
			pbt = PROFILE_BUCKET_INIT_AUXILIARY;
			break;
		case eMEDIATYPE_PLAYLIST_VIDEO:
			pbt = PROFILE_BUCKET_PLAYLIST_VIDEO;
			break;
		case eMEDIATYPE_PLAYLIST_AUDIO:
			pbt = PROFILE_BUCKET_PLAYLIST_AUDIO;
			break;
		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			pbt = PROFILE_BUCKET_PLAYLIST_SUBTITLE;
			break;
		case eMEDIATYPE_PLAYLIST_AUX_AUDIO:
			pbt = PROFILE_BUCKET_PLAYLIST_AUXILIARY;
			break;
		default:
			pbt = (ProfilerBucketType)mediaType;
			break;
	}
	return pbt;
}

/**
 * @brief Sets Recorded URL from Manifest received form XRE
 */
void PrivateInstanceAAMP::SetTunedManifestUrl(bool isrecordedUrl)
{
	mTunedManifestUrl.assign(mManifestUrl);
	//Do not edit or remove this log line - it is used log_pts_restamp tool
	AAMPLOG_INFO("mManifestUrl: %s",mManifestUrl.c_str());
	if(isrecordedUrl)
	{
		DeFog(mTunedManifestUrl);
		mTunedManifestUrl.replace(0,4,"_fog");
	}
	AAMPLOG_TRACE("PrivateInstanceAAMP::tunedManifestUrl:%s ", mTunedManifestUrl.c_str());
}

/**
 * @brief Gets Recorded URL from Manifest received form XRE.
 */
const char* PrivateInstanceAAMP::GetTunedManifestUrl()
{
	AAMPLOG_DEBUG("PrivateInstanceAAMP::tunedManifestUrl:%s ", mTunedManifestUrl.c_str());
	return mTunedManifestUrl.c_str();
}

/**
 *  @brief To get the network proxy
 */
std::string PrivateInstanceAAMP::GetNetworkProxy()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkProxy);
}

/**
 * @brief To get the proxy for license request
 */
std::string PrivateInstanceAAMP::GetLicenseReqProxy()
{
	return GETCONFIGVALUE_PRIV(eAAMPConfig_LicenseProxy);
}


/**
 * @brief Signal trick mode discontinuity to stream sink
 */
void PrivateInstanceAAMP::SignalTrickModeDiscontinuity()
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->SignalTrickModeDiscontinuity();
	}
}

/**
 *   @brief Check if current stream is muxed
 */
bool PrivateInstanceAAMP::IsMuxedStream()
{
	bool ret = false;
	if (mpStreamAbstractionAAMP)
	{
		ret = mpStreamAbstractionAAMP->IsMuxedStream();
	}
	return ret;
}

/**
 * @brief Stop injection for a track.
 * Called from StopInjection
 */
void PrivateInstanceAAMP::StopTrackInjection(AampMediaType type)
{
	if (!mTrackInjectionBlocked[type])
	{
		AAMPLOG_TRACE("PrivateInstanceAAMP: for type %s", GetMediaTypeName(type) );
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mTrackInjectionBlocked[type] = true;
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP::Exit. type = %d", (int) type);
}

/**
 * @brief Resume injection for a track.
 * Called from StartInjection
 */
void PrivateInstanceAAMP::ResumeTrackInjection(AampMediaType type)
{
	if (mTrackInjectionBlocked[type])
	{
		AAMPLOG_TRACE("PrivateInstanceAAMP: for type %s", GetMediaTypeName(type) );
		std::lock_guard<std::recursive_mutex> guard(mLock);
		mTrackInjectionBlocked[type] = false;
	}
	AAMPLOG_TRACE ("PrivateInstanceAAMP::Exit. type = %d", (int) type);
}

/**
 * @brief Receives first video PTS of the current playback
 */
void PrivateInstanceAAMP::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->NotifyFirstVideoPTS(pts, timeScale);
	}
}

/**
 * @brief Notifies base PTS of the HLS video playback
 */
void PrivateInstanceAAMP::NotifyVideoBasePTS(unsigned long long basepts, unsigned long timeScale)
{
	// mVideoBasePTS should be in 90KHz clock because GST gives it in the same range
	// Convert to 90KHz clock if not already
	if (timeScale != 90000)
	{
		mVideoBasePTS = (unsigned long long) (((double)basepts / (double)timeScale) * 90000);
	}
	else
	{
		mVideoBasePTS = basepts;
	}
	AAMPLOG_INFO("mVideoBasePTS::%llu", mVideoBasePTS);
}

/**
 * @brief To send webvtt cue as an event
 */
void PrivateInstanceAAMP::SendVTTCueDataAsEvent(VTTCue* cue)
{
	//This function is called from an idle handler and hence we call SendEventSync
	if (mEventManager->IsEventListenerAvailable(AAMP_EVENT_WEBVTT_CUE_DATA))
	{
		WebVttCueEventPtr ev = std::make_shared<WebVttCueEvent>(cue, GetSessionId());
		mEventManager->SendEvent(ev,AAMP_EVENT_SYNC_MODE);
	}
}

/**
 * @brief To check if subtitles are enabled
 */
bool PrivateInstanceAAMP::IsSubtitleEnabled(void)
{
	// Assumption being that enableSubtec and event listener will not be registered at the same time
	// in which case subtec gets priority over event listener
	return (ISCONFIGSET_PRIV(eAAMPConfig_Subtec_subtitle)  || 	WebVTTCueListenersRegistered());
}

/**
 * @brief To check if JavaScript cue listeners are registered
 */
bool PrivateInstanceAAMP::WebVTTCueListenersRegistered(void)
{
	return mEventManager->IsSpecificEventListenerAvailable(AAMP_EVENT_WEBVTT_CUE_DATA);
}

/**
 * @brief To get any custom license HTTP headers that was set by application
 */
void PrivateInstanceAAMP::GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders)
{
	customHeaders.insert(mCustomLicenseHeaders.begin(), mCustomLicenseHeaders.end());
}

/**
 * @brief to mark the discontinuity switch and save the Parameters
 */
void PrivateInstanceAAMP::SetDiscontinuityParam()
{
	profiler.SetDiscontinuityParam();
	mDiscontinuityFound = false;
}

/**
 * @brief to mark the latency Parameters
 */
void PrivateInstanceAAMP::SetLatencyParam(double latency, double buffer, double playbackRate, double bw)
{
	profiler.SetLatencyParam(latency, buffer, playbackRate, bw);
}

/**
 * @brief to mark the lld low buffer info
 */
void  PrivateInstanceAAMP::SetLLDLowBufferParam(double latency, double buff, double rate, double bw, double buffLowCount)
{
	profiler.SetLLDLowBufferParam( latency,  buff,  rate,  bw,  buffLowCount);
}

/**
 * @brief Get if pipeline reconfigure required for elementary stream type change status (from stream abstraction)
 * @return true if audio codec has changed
 */
bool PrivateInstanceAAMP::ReconfigureForCodecChange()
{
	if (mpStreamAbstractionAAMP)
	{
		if(!ISCONFIGSET_PRIV(eAAMPConfig_ReconfigPipelineOnDiscontinuity))
		{
			return mpStreamAbstractionAAMP->GetESChangeStatus();
		}
		else
		{
			AAMPLOG_INFO("ReconfigPipelineOnDiscontinuity is enabled, returning false");
			return false;
		}
	}
	else
	{
		AAMPLOG_ERR("ERROR - should not get here. 'mpStreamAbstractionAAMP' is NULL! Assuming ESChangeStatus() is false.");
		return false;
	}
}

/**
 * @brief Sends an ID3 metadata event.
 */
void PrivateInstanceAAMP::SendId3MetadataEvent(aamp::id3_metadata::CallbackData * id3Metadata)
{
	if(id3Metadata) {
		ID3MetadataEventPtr e = std::make_shared<ID3MetadataEvent>(id3Metadata->mData,
			id3Metadata->schemeIdUri,
			id3Metadata->value,
			id3Metadata->timeScale,
			id3Metadata->presentationTime,
			id3Metadata->eventDuration,
			id3Metadata->id,
			id3Metadata->timestampOffset,
			GetSessionId());
		if (ISCONFIGSET_PRIV(eAAMPConfig_ID3Logging))
		{
			std::vector<uint8_t> metadata = e->getMetadata();
			int metadataLen = e->getMetadataSize();
			int printableLen = 0;
			std::ostringstream tag;

			tag << "ID3 tag length: " << metadataLen;

			if (metadataLen > 0 )
			{
				tag << " payload: ";

				for (int i = 0; i < metadataLen; i++)
				{
					if (std::isprint(metadata[i]))
					{
						tag << metadata[i];
						printableLen++;
					}
				}
			}
			// ID3 tag size can be as high as 1055
			std::string tagLog(tag.str());
			AAMPLOG_INFO("%s", tag.str().c_str());
			AAMPLOG_INFO("{schemeIdUri:\"%s\",value:\"%s\",presentationTime:%" PRIu64 ",timeScale:%" PRIu32 ",eventDuration:%" PRIu32 ",id:%" PRIu32 ",timestampOffset:%" PRIu64 "}",e->getSchemeIdUri().c_str(), e->getValue().c_str(), e->getPresentationTime(), e->getTimeScale(), e->getEventDuration(), e->getId(), e->getTimestampOffset());
		}

		// Copying a shared_ptr is an expensive operation, since it's no longer needed, just move it.
		mEventManager->SendEvent(std::move(e),AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 * @brief Flush the stream sink
 * @param[in]  position - playback position
 */
void  PrivateInstanceAAMP::FlushTrack(AampMediaType type,double pos)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->FlushTrack(type, pos);
	}
}

/**
 * @brief Sending a flushing seek to stream sink with given position
 */
void PrivateInstanceAAMP::FlushStreamSink(double position, double rate)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		if(ISCONFIGSET_PRIV(eAAMPConfig_MidFragmentSeek) && position != 0 )
		{
			//Adding midSeekPtsOffset to position value.
			//Enables us to seek to the desired position in the mp4 fragment.
			sink->SeekStreamSink(position + GetMidSeekPosOffset(), rate);
		}
		else
		{
			sink->SeekStreamSink(position, rate);
		}
	}
}

/**
 * @brief PreCachePlaylistDownloadTask Thread function for PreCaching Playlist
 */
void PrivateInstanceAAMP::PreCachePlaylistDownloadTask()
{
	// This is the thread function to download all the HLS Playlist in a
	// differed manner
	int maxWindowForDownload = mPreCacheDnldTimeWindow * 60; // convert to seconds
	int szPlaylistCount = (int)mPreCacheDnldList.size();
	if(szPlaylistCount)
	{
		// First wait for Tune to complete to start this functionality
		{
			std::unique_lock<std::mutex> lock(mMutexPlaystart);
			waitforplaystart.wait(lock);
		}
		// May be Stop is called to release all resources .
		// Before download , check the state
		AAMPPlayerState state = GetState();
		// Check for state not IDLE
		if(state != eSTATE_RELEASED && state != eSTATE_IDLE && state != eSTATE_ERROR)
		{
			CurlInit(eCURLINSTANCE_PLAYLISTPRECACHE, 1, GetNetworkProxy());
			SetCurlTimeout(mPlaylistTimeoutMs, eCURLINSTANCE_PLAYLISTPRECACHE);
			int sleepTimeBetweenDnld = (maxWindowForDownload / szPlaylistCount) * 1000; // time in milliSec
			int idx = 0;
			do
			{
				interruptibleMsSleep(sleepTimeBetweenDnld);
				state = GetState();
				if(DownloadsAreEnabled())
				{
					// First check if the file is already in Cache
					PreCacheUrlStruct newelem = mPreCacheDnldList.at(idx);

					// check if url cached ,if not download
					if( !getAampCacheHandler()->IsPlaylistUrlCached(newelem.url))
					{
						AAMPLOG_WARN("Downloading Playlist Type:%d for PreCaching:%s",
							newelem.type, newelem.url.c_str());
						std::string playlistUrl;
						std::string playlistEffectiveUrl;
						AampGrowableBuffer playlistStore("playlistStore");
						int http_code;
						double downloadTime;
						bool ret = false;
						// Using StreamLock to avoid StreamAbstractionAAMP deletion when external player commands or stop call received
						AcquireStreamLock();
						ret = GetFile(newelem.url, newelem.type, &playlistStore, playlistEffectiveUrl, &http_code, &downloadTime, NULL, eCURLINSTANCE_PLAYLISTPRECACHE, true );
						ReleaseStreamLock();
						if(ret != false)
						{
							// If successful download , then insert into Cache
							getAampCacheHandler()->InsertToPlaylistCache(newelem.url, &playlistStore, playlistEffectiveUrl, false, newelem.type);
							playlistStore.Free();
						}
					}
					idx++;
				}
				else
				{
					// this can come here if trickplay is done or play started late
					if(state == eSTATE_SEEKING || state == eSTATE_PREPARED)
					{
						// wait for seek to complete
						usleep(1000000);
					}
					else if (state != eSTATE_RELEASED && state != eSTATE_IDLE && state != eSTATE_ERROR)
					{
						usleep(500000); // call sleep for other stats except seeking and prepared, otherwise this thread will run in highest priority until the state changes.
					}
				}
			}while (idx < mPreCacheDnldList.size() && state != eSTATE_RELEASED && state != eSTATE_IDLE && state != eSTATE_ERROR);
			mPreCacheDnldList.clear();
			CurlTerm(eCURLINSTANCE_PLAYLISTPRECACHE);
		}
	}
	AAMPLOG_WARN("End of PreCachePlaylistDownloadTask ");
}

/**
 * @brief SetPreCacheDownloadList - Function to assign the PreCaching file list
 */
void PrivateInstanceAAMP::SetPreCacheDownloadList(PreCacheUrlList &dnldListInput)
{
	mPreCacheDnldList = dnldListInput;
	if(mPreCacheDnldList.size())
	{
		AAMPLOG_WARN("Got Playlist PreCache list of Size : %zu", mPreCacheDnldList.size());
	}
}

/**
 * @brief Add an Accessibility node to a cJSON object.
 *
 * This function adds an Accessibility node to a given cJSON object. The scheme key name is specified
 * by the `schemeKey` parameter to address different naming conventions used in different contexts.
 * For example, "schemeId" is used for GetPreferredAudioProperties and GetPreferredTextProperties,
 * while "scheme" is used for GetAvailableAudioTracks,GetAudioTrackInfo,GetTextTrackInfo ,GetAvailableTextTracks.
 *
 * @param obj The cJSON object to which the Accessibility node will be added.
 * @param node The Accessibility node to be added.
 * @param schemeKey The key name for the scheme (e.g., "schemeId" or "scheme").
 */
static void AddAccessibilityNodeToObject(cJSON *obj, const Accessibility &node,const std::string &schemeKey)
{
	cJSON_AddStringToObject( obj, schemeKey.c_str(), node.getSchemeId().c_str());
	int ival = node.getIntValue();
	if( ival>=0 )
	{ // property has non-negative integer value
		cJSON_AddNumberToObject( obj, node.getTypeName(), ival );
	}
	else
	{ // fallback - string encoded value
		cJSON_AddStringToObject( obj, node.getTypeName(), node.getStrValue().c_str());
	}
}

/**
 *   @brief get the current text preference set by user
 *
 *   @return json string with preference data
 */
std::string PrivateInstanceAAMP::GetPreferredTextProperties()
{
	//Convert to JSON format
	std::string preference;
	cJSON *item;
	item = cJSON_CreateObject();
	if(!preferredTextLanguagesString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-languages", preferredTextLanguagesString.c_str());
	}
	if(!preferredTextLabelString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-labels", preferredTextLabelString.c_str());
	}
	if(!preferredTextRenditionString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-rendition", preferredTextRenditionString.c_str());
	}
	if(!preferredTextTypeString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-text-type", preferredTextTypeString.c_str());
	}
	if(!preferredTextAccessibilityNode.getSchemeId().empty())
	{
		cJSON *accessibility = cJSON_AddObjectToObject(item, "preferred-text-accessibility");
		AddAccessibilityNodeToObject( accessibility, preferredTextAccessibilityNode,"schemeId");
	}
	char *jsonStr = cJSON_Print(item);
	if (jsonStr)
	{
		preference.assign(jsonStr);
		free(jsonStr);
	}
	cJSON_Delete(item);
	return preference;
}

/**
 *   @brief get the current audio preference set by user
 */
std::string PrivateInstanceAAMP::GetPreferredAudioProperties()
{
	//Convert to JSON format
	std::string preference;
	cJSON *item;
	item = cJSON_CreateObject();
	if(!preferredLanguagesString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-languages", preferredLanguagesString.c_str());
	}
	if(!preferredLabelsString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-labels", preferredLabelsString.c_str());
	}
	if(!preferredCodecString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-codecs", preferredCodecString.c_str());
	}
	if(!preferredRenditionString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-rendition", preferredRenditionString.c_str());
	}
	if(!preferredTypeString.empty())
	{
		cJSON_AddStringToObject(item, "preferred-audio-type", preferredTypeString.c_str());
	}
	if(!preferredAudioAccessibilityNode.getSchemeId().empty())
	{
		cJSON * accessibility = cJSON_AddObjectToObject(item, "preferred-audio-accessibility");
		AddAccessibilityNodeToObject( accessibility, preferredAudioAccessibilityNode,"schemeId");
	}
	char *jsonStr = cJSON_Print(item);
	if (jsonStr)
	{
		preference.assign(jsonStr);
		free(jsonStr);
	}
	cJSON_Delete(item);
	return preference;
}

/**
 * @brief Get available video tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableVideoTracks()
{
	std::string tracks;
	std::lock_guard<std::recursive_mutex> guard(mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector <StreamInfo*> trackInfo = mpStreamAbstractionAAMP->GetAvailableVideoTracks();
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				for (int i = 0; i < trackInfo.size(); i++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (trackInfo[i]->bandwidthBitsPerSecond != -1)
					{
						cJSON_AddNumberToObject(item, "bandwidth", trackInfo[i]->bandwidthBitsPerSecond);
					}
					if (trackInfo[i]->resolution.width != -1)
					{
						cJSON_AddNumberToObject(item, "width", trackInfo[i]->resolution.width);
					}
					if (trackInfo[i]->resolution.height != -1)
					{
						cJSON_AddNumberToObject(item, "height", trackInfo[i]->resolution.height);
					}
					if (trackInfo[i]->resolution.framerate != -1)
					{
						cJSON_AddNumberToObject(item, "framerate", trackInfo[i]->resolution.framerate);
					}

					cJSON_AddNumberToObject(item, "enabled", trackInfo[i]->enabled);

					if( !trackInfo[i]->codecs.empty() )
					{
						cJSON_AddStringToObject(item, "codec", trackInfo[i]->codecs.c_str() );
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available video track information!");
		}
	}
	return tracks;
}

/**
 * @brief  set bitrate for video tracks selection
 */
void PrivateInstanceAAMP::SetVideoTracks(std::vector<BitsPerSecond> bitrateList)
{
	int bitrateSize = (int)bitrateList.size();
	//clear cached bitrate list
	this->bitrateList.clear();
	// user profile stats enabled only for valid bitrate list, otherwise disabled for empty bitrates
	this->userProfileStatus = (bitrateSize > 0) ? true : false;
	AAMPLOG_INFO("User Profile filtering bitrate size:%d status:%d", bitrateSize, this->userProfileStatus);
	for (int i = 0; i < bitrateSize; i++)
	{
		this->bitrateList.push_back(bitrateList.at(i));
		AAMPLOG_WARN("User Profile Index : %d(%d) Bw : %ld", i, bitrateSize, bitrateList.at(i));
	}
	AAMPPlayerState state = GetState();
	if (state > eSTATE_PREPARING)
	{
		AcquireStreamLock();
		TuneHelper(eTUNETYPE_RETUNE);
		ReleaseStreamLock();
	}
}

/**
 * @brief Get available audio tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	std::string tracks;
	std::lock_guard<std::recursive_mutex> guard(mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector<AudioTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableAudioTracks(allTrack);
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			AudioTrackInfo currentTrackInfo;
			if(root)
			{
				if (IsLocalAAMPTsb())
				{
					bool trackAvailable = mpStreamAbstractionAAMP->GetCurrentAudioTrack(currentTrackInfo);
					if( !trackAvailable )
					{
						AAMPLOG_WARN( "GetCurrentAudioTrack returned false" );
					}
				}
				for (auto iter = trackInfo.begin(); iter != trackInfo.end(); iter++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (!iter->name.empty())
					{
						cJSON_AddStringToObject(item, "name", iter->name.c_str());
					}
					if (!iter->label.empty())
					{
						cJSON_AddStringToObject(item, "label", iter->label.c_str());
					}
					if (!iter->language.empty())
					{
						cJSON_AddStringToObject(item, "language", iter->language.c_str());
					}
					if (!iter->codec.empty())
					{
						cJSON_AddStringToObject(item, "codec", iter->codec.c_str());
					}
					if (!iter->rendition.empty())
					{
						cJSON_AddStringToObject(item, "rendition", iter->rendition.c_str());
					}
					if (!iter->accessibilityType.empty())
					{
						cJSON_AddStringToObject(item, "accessibilityType", iter->accessibilityType.c_str());
					}
					if (!iter->characteristics.empty())
					{
						cJSON_AddStringToObject(item, "characteristics", iter->characteristics.c_str());
					}
					if (iter->channels != 0)
					{
						cJSON_AddNumberToObject(item, "channels", iter->channels);
					}
					if (iter->bandwidth != -1)
					{
						cJSON_AddNumberToObject(item, "bandwidth", iter->bandwidth);
					}
					if (!iter->contentType.empty())
					{
						cJSON_AddStringToObject(item, "contentType", iter->contentType.c_str());
					}
					if (!iter->mixType.empty())
					{
						cJSON_AddStringToObject(item, "mixType", iter->mixType.c_str());
					}
					if (!iter->mType.empty())
					{
						cJSON_AddStringToObject(item, "Type", iter->mType.c_str());
					}
					cJSON_AddBoolToObject(item, "default", iter->isDefault);
					bool isAvailable = iter->isAvailable;
					if (IsLocalAAMPTsb())
					{
						if (iter->index == currentTrackInfo.index)
						{
							//Only the current selected audio track is available in AAMP TSB.
							isAvailable = true;
						}
						else
						{
							isAvailable = false;
						}
						AAMPLOG_INFO("Setting audio track %s isAvailable to %d", iter->index.c_str(), isAvailable);
					}
					cJSON_AddBoolToObject(item, "availability", isAvailable);
					if (!iter->accessibilityItem.getSchemeId().empty())
					{
						cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
						AddAccessibilityNodeToObject( accessibility, iter->accessibilityItem,"scheme");
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available audio track information!");
		}
	}
	return tracks;
}

/**
 *   @brief Get available text tracks.
 */
std::string PrivateInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	std::string tracks;

	std::lock_guard<std::recursive_mutex> guard(mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		std::vector<TextTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableTextTracks(allTrack);

		std::vector<TextTrackInfo> textTracksCopy;
		std::copy_if(begin(trackInfo), end(trackInfo), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});
		std::vector<CCTrackInfo> updatedTextTracks;
		UpdateCCTrackInfo(textTracksCopy,updatedTextTracks);
		PlayerCCManager::GetInstance()->updateLastTextTracks(updatedTextTracks);
		if (!trackInfo.empty())
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			TextTrackInfo currentTrackInfo;
			if(root)
			{
				if (IsLocalAAMPTsb())
				{
					bool trackInfoAvailable = mpStreamAbstractionAAMP->GetCurrentTextTrack(currentTrackInfo);
					if( !trackInfoAvailable )
					{
						AAMPLOG_WARN( "GetCurrentTextTrack returned false" );
					}
				}
				for (auto iter = trackInfo.begin(); iter != trackInfo.end(); iter++)
				{
					cJSON_AddItemToArray(root, item = cJSON_CreateObject());
					if (!iter->name.empty())
					{
						cJSON_AddStringToObject(item, "name", iter->name.c_str());
					}
					if (!iter->label.empty())
					{
						cJSON_AddStringToObject(item, "label", iter->label.c_str());
					}
					if (iter->isCC)
					{
						cJSON_AddStringToObject(item, "sub-type", "CLOSED-CAPTIONS");
					}
					else
					{
						cJSON_AddStringToObject(item, "sub-type", "SUBTITLES");
					}
					if (!iter->language.empty())
					{
						cJSON_AddStringToObject(item, "language", iter->language.c_str());
					}
					if (!iter->rendition.empty())
					{
						cJSON_AddStringToObject(item, "rendition", iter->rendition.c_str());
					}
					if (!iter->accessibilityType.empty())
					{
						cJSON_AddStringToObject(item, "accessibilityType", iter->accessibilityType.c_str());
					}
					if (!iter->instreamId.empty())
					{
						cJSON_AddStringToObject(item, "instreamId", iter->instreamId.c_str());
					}
					if (!iter->characteristics.empty())
					{
						cJSON_AddStringToObject(item, "characteristics", iter->characteristics.c_str());
					}
					if (!iter->mType.empty())
					{
						cJSON_AddStringToObject(item, "type", iter->mType.c_str());
					}
					if (!iter->codec.empty())
					{
						cJSON_AddStringToObject(item, "codec", iter->codec.c_str());
					}
					bool isAvailable = iter->isAvailable;
					if (IsLocalAAMPTsb())
					{
						if (iter->index == currentTrackInfo.index)
						{
							//Only the current selected text track is available in AAMP TSB.
							isAvailable = true;
						}
						else
						{
							isAvailable = false;
						}
						AAMPLOG_INFO("Setting text track %s isAvailable to %d", iter->index.c_str(), isAvailable);
					}
					cJSON_AddBoolToObject(item, "availability", isAvailable);
					if (!iter->accessibilityItem.getSchemeId().empty())
					{
						cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
						AddAccessibilityNodeToObject( accessibility, iter->accessibilityItem,"scheme");
					}
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					tracks.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available text track information!");
		}
	}
	return tracks;
}

/*
 * @brief Get the video window co-ordinates
 */
std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	std::string ret_val = "";
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		ret_val = sink->GetVideoRectangle();
	}
	return ret_val;
}

/**
 * @brief Set the application name which has created PlayerInstanceAAMP, for logging purposes
 */
void PrivateInstanceAAMP::SetAppName(std::string name)
{
	mAppName = name;
}

/**
 *   @brief Get the application name
 */
std::string PrivateInstanceAAMP::GetAppName()
{
	return mAppName;
}

/**
 * @brief DRM individualization callback
 */
void PrivateInstanceAAMP::Individualization(const std::string& payload)
{
	DrmMessageEventPtr event = std::make_shared<DrmMessageEvent>(payload, GetSessionId());
	SendEvent(event,AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Get current initial buffer duration in seconds
 */
int PrivateInstanceAAMP::GetInitialBufferDuration()
{
	mMinInitialCacheSeconds = GETCONFIGVALUE_PRIV(eAAMPConfig_InitialBuffer);
	return mMinInitialCacheSeconds;
}

/**
 *   @brief Check if First Video Frame Displayed Notification
 *          is required.
 */
bool PrivateInstanceAAMP::IsFirstVideoFrameDisplayedRequired()
{
	return mFirstVideoFrameDisplayedEnabled;
}

/**
 *   @brief Notify First Video Frame was displayed
 */
void PrivateInstanceAAMP::NotifyFirstVideoFrameDisplayed()
{
	if(!mFirstVideoFrameDisplayedEnabled)
	{
		return;
	}

	mFirstVideoFrameDisplayedEnabled = false;

	// In the middle of stop processing we can receive state changing callback
	AAMPPlayerState state = GetState();
	if (state == eSTATE_IDLE)
	{
		AAMPLOG_WARN( "skipped as in IDLE state" );
		return;
	}

	// Seek While Paused - pause on first Video frame displayed
	if(mPauseOnFirstVideoFrameDisp)
	{
		mPauseOnFirstVideoFrameDisp = false;
		AAMPPlayerState state = GetState();
		if(state != eSTATE_SEEKING)
		{
			return;
		}

		AAMPLOG_INFO("Pausing Playback on First Frame Displayed");
		if(mpStreamAbstractionAAMP)
		{
			mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
		}
		StopDownloads();
		if(PausePipeline(true, false))
		{
			SetState(eSTATE_PAUSED);
		}
		else
		{
			AAMPLOG_ERR("Failed to pause pipeline for first frame displayed!");
		}
	}
	// Otherwise check for setting BUFFERING state
	else if(!SetStateBufferingIfRequired())
	{
		// If Buffering state was not needed, set PLAYING state
		SetState(eSTATE_PLAYING);
	}
}

/**
 * @brief Set eSTATE_BUFFERING if required
 */
bool PrivateInstanceAAMP::SetStateBufferingIfRequired()
{
	bool bufferingSet = false;
	std::lock_guard<std::recursive_mutex> guard(mFragmentCachingLock);
	if(IsFragmentCachingRequired())
	{
		bufferingSet = true;
		AAMPPlayerState state = GetState();
		if(state != eSTATE_BUFFERING)
		{
			if(mpStreamAbstractionAAMP)
			{
				mpStreamAbstractionAAMP->NotifyPlaybackPaused(true);
			}
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if(sink)
			{
				sink->NotifyFragmentCachingOngoing();
			}
			SetState(eSTATE_BUFFERING);
		}
	}
	return bufferingSet;
}

/**
 * @brief Check to media track downloads are enabled
 */
bool PrivateInstanceAAMP::TrackDownloadsAreEnabled(AampMediaType type)
{
	bool ret = true;
	if (type >= AAMP_TRACK_COUNT)  //CID:142718 - overrun
	{
		AAMPLOG_ERR("type[%d] is un-supported, returning default as false!", type);
		ret = false;
	}
	else
	{
		std::lock_guard<std::recursive_mutex> guard(mLock);
		// If blocked, track downloads are disabled
		ret = !mbTrackDownloadsBlocked[type];
	}
	return ret;
}

/**
 * @brief Stop buffering in AAMP and un-pause pipeline.
 */
void PrivateInstanceAAMP::StopBuffering(bool forceStop)
{
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		sink->StopBuffering(forceStop);
	}
}

/**
 * @brief Get license server url for a drm type
 */
std::string PrivateInstanceAAMP::GetLicenseServerUrlForDrm(DRMSystems type)
{
	std::string url;
	if (type == eDRM_PlayReady)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_PRLicenseServerUrl);
	}
	else if (type == eDRM_WideVine)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_WVLicenseServerUrl);
	}
	else if (type == eDRM_ClearKey)
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_CKLicenseServerUrl);
	}

	if(url.empty())
	{
		url = GETCONFIGVALUE_PRIV(eAAMPConfig_LicenseServerUrl);
	}
	return url;
}

/**
 * @brief Get current audio track index
 */
int PrivateInstanceAAMP::GetAudioTrack()
{
	int idx = -1;
	AcquireStreamLock();
	if (mpStreamAbstractionAAMP)
	{
		idx = mpStreamAbstractionAAMP->GetAudioTrack();
	}
	ReleaseStreamLock();
	return idx;
}

/**
 * @brief Get current audio track index
 */
std::string PrivateInstanceAAMP::GetAudioTrackInfo()
{
	std::string track;
	std::lock_guard<std::recursive_mutex> guard(mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		AudioTrackInfo trackInfo;
		if (mpStreamAbstractionAAMP->GetCurrentAudioTrack(trackInfo))
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				if (!trackInfo.name.empty())
				{
					cJSON_AddStringToObject(item, "name", trackInfo.name.c_str());
				}
				if (!trackInfo.language.empty())
				{
					cJSON_AddStringToObject(item, "language", trackInfo.language.c_str());
				}
				if (!trackInfo.codec.empty())
				{
					cJSON_AddStringToObject(item, "codec", trackInfo.codec.c_str());
				}
				if (!trackInfo.rendition.empty())
				{
					cJSON_AddStringToObject(item, "rendition", trackInfo.rendition.c_str());
				}
				if (!trackInfo.label.empty())
				{
					cJSON_AddStringToObject(item, "label", trackInfo.label.c_str());
				}
				if (!trackInfo.accessibilityType.empty())
				{
					cJSON_AddStringToObject(item, "accessibilityType", trackInfo.accessibilityType.c_str());
				}
				if (!trackInfo.characteristics.empty())
				{
					cJSON_AddStringToObject(item, "characteristics", trackInfo.characteristics.c_str());
				}
				if (trackInfo.channels != 0)
				{
					cJSON_AddNumberToObject(item, "channels", trackInfo.channels);
				}
				if (trackInfo.bandwidth != -1)
				{
					cJSON_AddNumberToObject(item, "bandwidth", trackInfo.bandwidth);
				}
				if (!trackInfo.contentType.empty())
				{
					cJSON_AddStringToObject(item, "contentType", trackInfo.contentType.c_str());
				}
				if (!trackInfo.mixType.empty())
				{
					cJSON_AddStringToObject(item, "mixType", trackInfo.mixType.c_str());
				}
				if (!trackInfo.mType.empty())
				{
					cJSON_AddStringToObject(item, "type", trackInfo.mType.c_str());
				}
				if (!trackInfo.accessibilityItem.getSchemeId().empty())
				{
					cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
					AddAccessibilityNodeToObject( accessibility, trackInfo.accessibilityItem,"scheme");
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					track.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available Text track information!");
		}
	}
	else
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Not in playing state!");
	}
	return track;
}

/**
 * @brief Get current audio track index
 */
std::string PrivateInstanceAAMP::GetTextTrackInfo()
{
	std::string track;
	bool trackInfoAvailable = false;
	std::lock_guard<std::recursive_mutex> guard(mStreamLock);
	if (mpStreamAbstractionAAMP)
	{
		TextTrackInfo trackInfo;

		if (PlayerCCManager::GetInstance()->GetStatus() && mIsInbandCC)
		{
			std::string trackId = PlayerCCManager::GetInstance()->GetTrack();
			if (!trackId.empty())
			{
				std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
				for (auto it = tracks.begin(); it != tracks.end(); it++)
				{
					if (it->instreamId == trackId)
					{
						trackInfo = *it;
						trackInfoAvailable = true;
					}
				}
			}
		}
		if (!trackInfoAvailable)
		{
			trackInfoAvailable = mpStreamAbstractionAAMP->GetCurrentTextTrack(trackInfo);
		}
		if (trackInfoAvailable)
		{
			//Convert to JSON format
			cJSON *root;
			cJSON *item;
			root = cJSON_CreateArray();
			if(root)
			{
				cJSON_AddItemToArray(root, item = cJSON_CreateObject());
				if (!trackInfo.name.empty())
				{
					cJSON_AddStringToObject(item, "name", trackInfo.name.c_str());
				}
				if (!trackInfo.language.empty())
				{
					cJSON_AddStringToObject(item, "language", trackInfo.language.c_str());
				}
				if (!trackInfo.codec.empty())
				{
					cJSON_AddStringToObject(item, "codec", trackInfo.codec.c_str());
				}
				if (!trackInfo.rendition.empty())
				{
					cJSON_AddStringToObject(item, "rendition", trackInfo.rendition.c_str());
				}
				if (!trackInfo.label.empty())
				{
					cJSON_AddStringToObject(item, "label", trackInfo.label.c_str());
				}
				if (!trackInfo.accessibilityType.empty())
				{
					cJSON_AddStringToObject(item, "accessibilityType", trackInfo.accessibilityType.c_str());
				}
				if (!trackInfo.characteristics.empty())
				{
					cJSON_AddStringToObject(item, "characteristics", trackInfo.characteristics.c_str());
				}
				if (!trackInfo.mType.empty())
				{
					cJSON_AddStringToObject(item, "type", trackInfo.mType.c_str());
				}
				if (!trackInfo.instreamId.empty())
				{
					cJSON_AddStringToObject(item, "instreamID", trackInfo.instreamId.c_str());
				}
				if (!trackInfo.accessibilityItem.getSchemeId().empty())
				{
					cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
					AddAccessibilityNodeToObject( accessibility, trackInfo.accessibilityItem,"scheme");
				}
				char *jsonStr = cJSON_Print(root);
				if (jsonStr)
				{
					track.assign(jsonStr);
					free(jsonStr);
				}
				cJSON_Delete(root);
			}
		}
		else
		{
			AAMPLOG_ERR("PrivateInstanceAAMP: No available Text track information!");
		}
	}
	else
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: Not in playing state!");
	}
	return track;
}


/**
 * @brief Create json data from track Info
 */
static char* createJsonData(TextTrackInfo& track)
{
	char *jsonStr = NULL;
	cJSON *item = cJSON_CreateObject();
	if (!track.name.empty())
	{
		cJSON_AddStringToObject(item, "name", track.name.c_str());
	}
	if (!track.language.empty())
	{
		cJSON_AddStringToObject(item, "languages", track.language.c_str());
	}
	if (!track.codec.empty())
	{
		cJSON_AddStringToObject(item, "codec", track.codec.c_str());
	}
	if (!track.rendition.empty())
	{
		cJSON_AddStringToObject(item, "rendition", track.rendition.c_str());
	}
	if (!track.label.empty())
	{
		cJSON_AddStringToObject(item, "label", track.label.c_str());
	}
	if (!track.accessibilityType.empty())
	{
		cJSON_AddStringToObject(item, "accessibilityType", track.accessibilityType.c_str());
	}
	if (!track.characteristics.empty())
	{
		cJSON_AddStringToObject(item, "characteristics", track.characteristics.c_str());
	}
	if (!track.mType.empty())
	{
		cJSON_AddStringToObject(item, "type", track.mType.c_str());
	}
	if (!track.accessibilityItem.getSchemeId().empty())
	{
		cJSON *accessibility = cJSON_AddObjectToObject(item, "accessibility");
		AddAccessibilityNodeToObject( accessibility, track.accessibilityItem ,"scheme");
	}

	jsonStr = cJSON_Print(item);
	cJSON_Delete(item);
	return jsonStr;
}

/**
 * @brief Set text track
 */
void PrivateInstanceAAMP::SetTextTrack(int trackId, char *data)
{
	AAMPLOG_INFO("trackId: %d", trackId);
	if (mpStreamAbstractionAAMP)
	{
		// Passing in -1 as the track ID mutes subs
		if (MUTE_SUBTITLES_TRACKID == trackId)
		{
			SetCCStatus(false);
			if (data != NULL)
			{
				SAFE_DELETE_ARRAY(data);
				data = NULL;
			}
			return;
		}

		if (data == NULL)
		{
			std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
			if (!tracks.empty() && (trackId >= 0 && trackId < tracks.size()))
			{
				TextTrackInfo track = tracks[trackId];
				// Check if CC / Subtitle track
				if (track.isCC)
				{
					mIsInbandCC = true;
					if (!track.instreamId.empty())
					{
						CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT;
						// PlayerCCManager expects the CC type, ie 608 or 708
						// For DASH, there is a possibility that instreamId is just an integer so we infer rendition
						if (mMediaFormat == eMEDIAFORMAT_DASH && (std::isdigit(static_cast<unsigned char>(track.instreamId[0]))) && !track.rendition.empty())
						{
							if (track.rendition.find("608") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_608;
							}
							else if (track.rendition.find("708") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_708;
							}
						}

						// preferredCEA708 overrides whatever we infer from track. USE WITH CAUTION
						int overrideCfg = GETCONFIGVALUE_PRIV(eAAMPConfig_CEAPreferred);
						if (overrideCfg != -1)
						{
							format = (CCFormat)(overrideCfg & 1);
							AAMPLOG_WARN("PrivateInstanceAAMP: CC format override present, override format to: %d", format);
						}
						PlayerCCManager::GetInstance()->SetTrack(track.instreamId, format);
					}
					else
					{
						AAMPLOG_ERR("PrivateInstanceAAMP: Track number/instreamId is empty, skip operation");
					}
				}
				else
				{
					mIsInbandCC = false;
					//Unmute subtitles
					SetCCStatus(true);

					//TODO: Effective handling between subtitle and CC tracks
					int textTrack = mpStreamAbstractionAAMP->GetTextTrack();
					AAMPLOG_WARN("GetPreferredTextTrack %d trackId %d", textTrack, trackId);
					if (trackId != textTrack)
					{
						if(mMediaFormat == eMEDIAFORMAT_DASH)
						{
							const char* jsonData = createJsonData(track);
							if(NULL != jsonData)
							{
								SetPreferredTextLanguages(jsonData);
							}
						}
						else
						{
							SetPreferredTextTrack(track);
							if((ISCONFIGSET_PRIV(eAAMPConfig_useRialtoSink)) && ((mCurrentTextTrackIndex == -1) || (mCurrentTextTrackIndex == trackId)))
							{ // by default text track is enabled and muted for Rialto; notify only if there is change in the subtitles
								AAMPLOG_INFO("useRialtoSink mCurrentTextTrackIndex = %d trackId = %d",mCurrentTextTrackIndex,trackId);
								mpStreamAbstractionAAMP->currentTextTrackProfileIndex = mCurrentTextTrackIndex = trackId;
							}
							else
							{
								discardEnteringLiveEvt = true;
								seek_pos_seconds = GetPositionSeconds();
								AcquireStreamLock();
								TeardownStream(false);
								TuneHelper(eTUNETYPE_SEEK);
								ReleaseStreamLock();
								discardEnteringLiveEvt = false;
							}
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("webvtt data received from application");
			mData.reset(data);
			SetCCStatus(true);

			mpStreamAbstractionAAMP->InitSubtitleParser(data);
			if (!mTextStyle.empty())
			{
				// Restore the subtitle text style after a track change.
				(void)mpStreamAbstractionAAMP->SetTextStyle(mTextStyle);
			}
		}
	}
	else
	{
		AAMPLOG_ERR("null Stream Abstraction AAMP");
		if (data != NULL)
		{
			SAFE_DELETE_ARRAY(data);
			data = NULL;
		}
	}
}


/**
 * @brief Switch the subtitle track following a change to the preferredTextTrack
 */
void PrivateInstanceAAMP::RefreshSubtitles()
{
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->RefreshSubtitles();
	}
}

/**
 * @brief Get current text track index
 */
int PrivateInstanceAAMP::GetTextTrack()
{
	int idx = -1;
	AcquireStreamLock();
	if (PlayerCCManager::GetInstance()->GetStatus() && mpStreamAbstractionAAMP)
	{
		std::string trackId = PlayerCCManager::GetInstance()->GetTrack();
		if (!trackId.empty())
		{
			std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
			for (auto it = tracks.begin(); it != tracks.end(); it++)
			{
				if (it->instreamId == trackId)
				{
					idx = static_cast<int>( std::distance(tracks.begin(), it) );
				}
			}
		}
	}
	if (mpStreamAbstractionAAMP && idx == -1 && !subtitles_muted)
	{
		idx = mpStreamAbstractionAAMP->GetTextTrack();
	}
	ReleaseStreamLock();
	return idx;
}

/**
 * @brief Set CC visibility on/off
 */
void PrivateInstanceAAMP::SetCCStatus(bool enabled)
{
	PlayerCCManager::GetInstance()->SetStatus(enabled);
	AcquireStreamLock();
	subtitles_muted = !enabled;
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->MuteSubtitles(subtitles_muted);
		if (HasSidecarData())
		{ // has sidecar data
			mpStreamAbstractionAAMP->MuteSidecarSubtitles(subtitles_muted);
		}
	}
	SetSubtitleMute(subtitles_muted);
	ReleaseStreamLock();
}

/**
 * @brief Get CC visibility on/off
 */
bool PrivateInstanceAAMP::GetCCStatus(void)
{
	return !(subtitles_muted);
}

/**
 * @brief Function to notify available audio tracks changed
 */
void PrivateInstanceAAMP::NotifyAudioTracksChanged()
{
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_AUDIO_TRACKS_CHANGED, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Function to notify available text tracks changed
 */
void PrivateInstanceAAMP::NotifyTextTracksChanged()
{
	SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_TEXT_TRACKS_CHANGED, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
}

/**
 * @brief Set style options for text track rendering
 *
 */
void PrivateInstanceAAMP::SetTextStyle(const std::string &options)
{
	bool retVal = false;

	// Try setting text style via subtitle parser
	if (mpStreamAbstractionAAMP)
	{
		AAMPLOG_WARN("Calling StreamAbstractionAAMP::SetTextStyle(%s)", options.c_str());
		retVal = mpStreamAbstractionAAMP->SetTextStyle(options);
	}

	if (!retVal)
	{
		// Try setting text style via gstreamer
		AAMPLOG_WARN("Calling StreamSink::SetTextStyle(%s)", options.c_str());
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			retVal = sink->SetTextStyle(options);
		}
	}

	if (retVal)
	{
		// Store current TextStyle in PrivateInstanceAAMP rather than in StreamAbstractionAAMP
		// as StreamAbstractionAAMP object is destroyed on seek
		mTextStyle = options;
	}
	else
	{
			// Try setting text style via CC Manager
		AAMPLOG_WARN("Calling PlayerCCManager::SetTextStyle(%s)", options.c_str());
		PlayerCCManager::GetInstance()->SetStyle(options);
	}
}

/**
 * @brief Get style options for text track rendering
 */
std::string PrivateInstanceAAMP::GetTextStyle()
{
	std::string textStyle = mTextStyle;

	if (textStyle.empty())
	{
		// CCManager is a singleton potentially used by multiple players
		// so should retrieve from CCManager.
		textStyle = PlayerCCManager::GetInstance()->GetStyle();
	}
	return textStyle;
}

/**
 * @brief Check if any active PrivateInstanceAAMP available
 */
bool PrivateInstanceAAMP::IsActiveInstancePresent()
{
	return !gActivePrivAAMPs.empty();
}

/**
 *  @brief Set discontinuity ignored flag for given track
 */
void PrivateInstanceAAMP::SetTrackDiscontinuityIgnoredStatus(AampMediaType track)
{
	mIsDiscontinuityIgnored[track] = true;
}

/**
 *  @brief Check whether the given track discontinuity ignored earlier.
 */
bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForOtherTrack(AampMediaType track)
{
	return (mIsDiscontinuityIgnored[track]);
}

/**
 *  @brief Check whether the given track discontinuity ignored for current track.
 */
bool PrivateInstanceAAMP::IsDiscontinuityIgnoredForCurrentTrack(AampMediaType track)
{
	return (mIsDiscontinuityIgnored[track]);
}

/**
 *  @brief Reset discontinuity ignored flag for audio and video tracks
 */
void PrivateInstanceAAMP::ResetTrackDiscontinuityIgnoredStatus(void)
{
	mIsDiscontinuityIgnored[eTRACK_VIDEO] = false;
	mIsDiscontinuityIgnored[eTRACK_AUDIO] = false;
	mIsDiscontinuityIgnored[eTRACK_SUBTITLE] = false;
	mIsDiscontinuityIgnored[eTRACK_AUX_AUDIO] = false;
}

/**
 *  @brief Reset discontinuity ignored flag for current track
 */
void PrivateInstanceAAMP::ResetTrackDiscontinuityIgnoredStatusForTrack(AampMediaType track )
{
	 mIsDiscontinuityIgnored[track] = false;
}

/**
 *  @brief Check the pipeline is valid for the media type
 */
bool PrivateInstanceAAMP::PipelineValid(AampMediaType track)
{
	bool isValid = false;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		isValid = sink->PipelineConfiguredForMedia(track);
	}
	return isValid;
}

/**
 * @brief Set stream format for audio/video tracks
 */
void PrivateInstanceAAMP::SetStreamFormat(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
	bool reconfigure = false;
	//AAMPLOG_MIL("Got format - videoFormat %d and audioFormat %d", videoFormat, audioFormat);

	// 1. Modified Configure() not to recreate all playbins if there is a change in track's format.
	// 2. For a demuxed scenario, this function will be called twice for each audio and video, so double the trouble.
	// Hence call Configure() only for following scenarios to reduce the overhead,
	// i.e FORMAT_INVALID to any KNOWN/FORMAT_UNKNOWN, FORMAT_UNKNOWN to any KNOWN and any FORMAT_KNOWN to FORMAT_KNOWN if it's not same.
	// Truth table
	// mVideFormat   videoFormat  reconfigure
	// *		  INVALID	false
	// INVALID        INVALID       false
	// INVALID        UNKNOWN       true
	// INVALID	  KNOWN         true
	// UNKNOWN	  INVALID	false
	// UNKNOWN        UNKNOWN       false
	// UNKNOWN	  KNOWN		true
	// KNOWN	  INVALID	false
	// KNOWN          UNKNOWN	false
	// KNOWN          KNOWN         true if format changes, false if same
	std::unique_lock<std::recursive_mutex> lock(mLock);
	if (videoFormat != FORMAT_INVALID && mVideoFormat != videoFormat && (videoFormat != FORMAT_UNKNOWN || mVideoFormat == FORMAT_INVALID))
	{
		reconfigure = true;
		mVideoFormat = videoFormat;
	}
	if (audioFormat != FORMAT_INVALID && mAudioFormat != audioFormat && (audioFormat != FORMAT_UNKNOWN || mAudioFormat == FORMAT_INVALID))
	{
		reconfigure = true;
		mAudioFormat = audioFormat;
	}
	if (auxFormat != mAuxFormat && (mAuxFormat == FORMAT_INVALID || (mAuxFormat != FORMAT_UNKNOWN && auxFormat != FORMAT_UNKNOWN)) && auxFormat != FORMAT_INVALID)
	{
		reconfigure = true;
		mAuxFormat = auxFormat;
	}
	if (IsMuxedStream() && (mVideoComponentCount == 0 || mAudioComponentCount == 0)) //Can be a Muxed stream/Demuxed with either of audio or video-only stream
	{
		AAMPLOG_INFO(" TS Processing Done. Number of Audio Components : %d and Video Components : %d",mAudioComponentCount,mVideoComponentCount);
		if (IsAudioOrVideoOnly(videoFormat, audioFormat, auxFormat))
		{
			bool newTune = IsNewTune();
			lock.unlock();
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if (sink)
			{
				sink->Stop(!newTune);
			}
			lock.lock();
			reconfigure = true;
		}
	}
	if (reconfigure)
	{
		// Configure pipeline as TSProcessor might have detected the actual stream type
		// or even presence of audio
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
		if (sink)
		{
			sink->Configure(mVideoFormat, mAudioFormat, mAuxFormat, mSubtitleFormat, false, mpStreamAbstractionAAMP->GetAudioFwdToAuxStatus());
		}
	}
}

/**
 * @brief To check for audio/video only Playback
 */

bool PrivateInstanceAAMP::IsAudioOrVideoOnly(StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat)
{
	AAMPLOG_WARN("Old Stream format - videoFormat %d and audioFormat %d",mVideoFormat,mAudioFormat);
	bool ret = false;
	if (mVideoComponentCount == 0 && (mVideoFormat != videoFormat && videoFormat == FORMAT_INVALID))
	{
		mAudioOnlyPb = true;
		mVideoFormat = videoFormat;
		AAMPLOG_INFO("Audio-Only PlayBack");
		ret = true;
	}

	else if (mAudioComponentCount == 0)
	{
		if (mAudioFormat != audioFormat && audioFormat == FORMAT_INVALID)
		{
			mAudioFormat = audioFormat;
		}
		else if (mAuxFormat != auxFormat && auxFormat == FORMAT_INVALID)
		{
			mAuxFormat = auxFormat;
		}
		mVideoOnlyPb = true;
		AAMPLOG_INFO("Video-Only PlayBack");
		ret = true;
	}

	return ret;
}
/**
 *  @brief Disable Content Restrictions - unlock
 */
void PrivateInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	AcquireStreamLock();
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->DisableContentRestrictions(grace, time, eventChange);
		if (ISCONFIGSET_PRIV(eAAMPConfig_NativeCCRendering))
		{
			PlayerCCManager::GetInstance()->SetParentalControlStatus(false);
		}
	}
	mApplyContentRestriction = false;
	ReleaseStreamLock();
}

/**
 *  @brief Enable Content Restrictions - lock
 */
void PrivateInstanceAAMP::EnableContentRestrictions()
{
	AcquireStreamLock();
	AAMPPlayerState state = GetState();
	if (mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->EnableContentRestrictions();
		if( mMediaFormat != eMEDIAFORMAT_OTA )
		{
			AAMPLOG_INFO("MediaFormat: %d (%s) is not OTA ,ContentRestrictions is applied on next Tune request to OTA channel",mMediaFormat,mMediaFormatName[mMediaFormat]);
			mApplyContentRestriction = true;
		}
	}
	else
	{
		AAMPLOG_INFO("mpStreamAbstractionAAMP is not Ready, %d", state);
		mApplyContentRestriction = true;
	}
	ReleaseStreamLock();
}


/**
 *  @brief Add async task to scheduler
 */
int PrivateInstanceAAMP::ScheduleAsyncTask(IdleTask task, void *arg, std::string taskName)
{
	int taskId = AAMP_TASK_ID_INVALID;
	if (mScheduler)
	{
		taskId = mScheduler->ScheduleTask(AsyncTaskObj(task, arg, taskName));
		if (taskId == AAMP_TASK_ID_INVALID)
		{
			AAMPLOG_ERR("mScheduler returned invalid ID, dropping the schedule request!");
		}
	}
	else
	{
		taskId = g_idle_add(task, (gpointer)arg);
	}
	return taskId;
}

/**
 * @brief Remove async task scheduled earlier
 */
bool PrivateInstanceAAMP::RemoveAsyncTask(int taskId)
{
	bool ret = false;
	if (mScheduler)
	{
		ret = mScheduler->RemoveTask(taskId);
	}
	else
	{
		ret = g_source_remove(taskId);
	}
	return ret;
}


/**
 *  @brief acquire streamsink lock
 */
void PrivateInstanceAAMP::AcquireStreamLock()
{
	mStreamLock.lock();
}

/**
 * @brief try to acquire streamsink lock
 *
 */
bool PrivateInstanceAAMP::TryStreamLock()
{
	return mStreamLock.try_lock();
}

/**
 * @brief release streamsink lock
 *
 */
void PrivateInstanceAAMP::ReleaseStreamLock()
{
	mStreamLock.unlock();
}

/**
 * @brief To check if auxiliary audio is enabled
 */
bool PrivateInstanceAAMP::IsAuxiliaryAudioEnabled(void)
{
	return !mAuxAudioLanguage.empty();
}

/**
 * @brief Check if discontinuity processed in all tracks
 *
 */
bool PrivateInstanceAAMP::DiscontinuitySeenInAllTracks()
{
	// Check if track is disabled or if mProcessingDiscontinuity is set
	// Split off the logical expression for better clarity
	bool vidDiscontinuity = (mVideoFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	bool auxDiscontinuity = (mAuxFormat == FORMAT_INVALID || mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);

	return (vidDiscontinuity && audDiscontinuity && auxDiscontinuity);
}

/**
 *   @brief Check if discontinuity processed in any track
 */
bool PrivateInstanceAAMP::DiscontinuitySeenInAnyTracks()
{
	// Check if track is enabled and if mProcessingDiscontinuity is set
	// Split off the logical expression for better clarity
	bool vidDiscontinuity = (mVideoFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_VIDEO]);
	bool audDiscontinuity = (mAudioFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUDIO]);
	bool auxDiscontinuity = (mAuxFormat != FORMAT_INVALID && mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO]);

	return (vidDiscontinuity || audDiscontinuity || auxDiscontinuity);
}

/**
 * @brief Reset discontinuity flag for all tracks
 */
void PrivateInstanceAAMP::ResetDiscontinuityInTracks()
{
	mProcessingDiscontinuity[eMEDIATYPE_VIDEO] = false;
	mProcessingDiscontinuity[eMEDIATYPE_AUDIO] = false;
	mProcessingDiscontinuity[eMEDIATYPE_AUX_AUDIO] = false;
}

/**
 *  @brief set preferred Audio Language properties like language, rendition, type, name and codec
 */
void PrivateInstanceAAMP::SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList, const Accessibility *accessibilityItem, const char *preferredName)
{
	/**< First argument is Json data then parse it and and assign the variables properly*/
	AampJsonObject* jsObject = NULL;
	bool isJson = false;
	bool isRetuneNeeded = false;
	bool accessibilityPresent = false;

	try
	{
		jsObject = new AampJsonObject(languageList);
		if (jsObject)
		{
			AAMPLOG_INFO("Preferred Language Properties received as json : %s", languageList);
			isJson = true;
		}
	}
	catch(const std::exception& e)
	{
		/**<Nothing to do exclude it*/
	}

	if (isJson)
	{
		std::vector<std::string> inputLanguagesList;
		std::string inputLanguagesString;

		/** Get language Properties*/
		if (jsObject->isArray("languages"))
		{
			if (jsObject->get("languages", inputLanguagesList))
			{
				for (const auto& preferredLanguage : inputLanguagesList)
				{
					if (!inputLanguagesString.empty())
					{
						inputLanguagesString += ",";
					}
					inputLanguagesString += preferredLanguage;
				}
			}
		}
		else if (jsObject->isString("languages"))
		{
			if (jsObject->get("languages", inputLanguagesString))
			{
				inputLanguagesList.push_back(inputLanguagesString);
			}
		}
		else
		{
			AAMPLOG_ERR("Preferred Audio Language Field Only support String or String Array");
		}

		AAMPLOG_INFO("Number of preferred languages received: %zu", inputLanguagesList.size());
		AAMPLOG_INFO("Preferred language string received: %s", inputLanguagesString.c_str());

		std::vector<std::string> inputLabelList;
		std::string inputLabelsString;
		/** Get Label Properties*/
		if (jsObject->isString("label"))
		{
			if (jsObject->get("label", inputLabelsString))
			{
				inputLabelList.push_back(inputLabelsString);
				AAMPLOG_INFO("Preferred Label string: %s", inputLabelsString.c_str());
			}
		}

		string inputRenditionString;

		/** Get rendition or role Properties*/
		if (jsObject->isString("rendition"))
		{
			if (jsObject->get("rendition", inputRenditionString))
			{
				AAMPLOG_INFO("Preferred rendition string: %s", inputRenditionString.c_str());
			}
		}

		std::vector<std::string> inputCodecList;
		std::string inputCodecString;

		/** Get Codec Properties*/
		if (jsObject->isArray("codec"))
		{
			if (jsObject->get("codec", inputCodecList))
			{
				for (const auto& preferredCodec : inputCodecList)
				{
					if (!inputCodecString.empty())
					{
						inputCodecString += ",";
					}
					inputCodecString += preferredCodec;
				}
				if(!inputCodecString.empty())
				{
					AAMPLOG_INFO("Preferred codec string: %s", inputCodecString.c_str());
				}
			}
		}
		else if (jsObject->isString("codec"))
		{
			if (jsObject->get("codec", inputCodecString))
			{
				inputCodecList.push_back(inputCodecString);
				AAMPLOG_INFO("Preferred codec string: %s", inputCodecString.c_str());
			}
		}

		Accessibility  inputAudioAccessibilityNode;
		/** Get accessibility Properties*/
		if (jsObject->isObject("accessibility"))
		{
			AampJsonObject accessNode;
			if (jsObject->get("accessibility", accessNode))
			{
				inputAudioAccessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode(accessNode);
				if (!inputAudioAccessibilityNode.getSchemeId().empty())
				{
					AAMPLOG_INFO("Preferred accessibility: %s", inputAudioAccessibilityNode.print().c_str() );
				}
			}
			if(preferredAudioAccessibilityNode != inputAudioAccessibilityNode )
			{
				accessibilityPresent = true;
			}
		}

		std::string inputNameString;
		if (jsObject->isString("name"))
		{
			if (jsObject->get("name", inputNameString))
			{
				AAMPLOG_INFO("Preferred name string: %s", inputNameString.c_str());
			}
		}

		/**< Release json object **/
		SAFE_DELETE(jsObject);

		if ((preferredAudioAccessibilityNode != inputAudioAccessibilityNode ) || (preferredRenditionString != inputRenditionString ) ||
		(preferredLabelsString != inputLabelsString) || (inputLanguagesList != preferredLanguagesList ) || (preferredNameString != inputNameString))
		{
			isRetuneNeeded = true;
		}

		/** Clear the cache **/
		preferredAudioAccessibilityNode.clear();
		preferredLabelsString.clear();
		preferredLabelList.clear();
		preferredRenditionString.clear();
		preferredLanguagesString.clear();
		preferredLanguagesList.clear();
		preferredCodecString.clear();
		preferredCodecList.clear();
		preferredNameString.clear();

		/** Reload the new values **/
		preferredAudioAccessibilityNode = inputAudioAccessibilityNode;
		preferredRenditionString = inputRenditionString;
		preferredLabelList = inputLabelList;
		preferredLabelsString = inputLabelsString;
		preferredLanguagesList = inputLanguagesList;
		preferredLanguagesString = inputLanguagesString;
		preferredCodecString = inputCodecString;
		preferredCodecList = inputCodecList;
		preferredNameString = inputNameString;

		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLabel,preferredLabelsString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioCodec,preferredCodecString);
	}
	else
	{
		if((languageList && preferredLanguagesString != languageList) ||
		(preferredRendition && preferredRenditionString != preferredRendition) ||
		(preferredType && preferredTypeString != preferredType) ||
		(codecList && preferredCodecString != codecList) ||
		(labelList && preferredLabelsString != labelList) ||
		(accessibilityItem && !accessibilityItem->getSchemeId().empty() && (preferredAudioAccessibilityNode != *accessibilityItem)) ||
		(preferredName && preferredNameString != preferredName))
		{
			isRetuneNeeded = true;
			if(languageList != NULL)
			{
				preferredLanguagesString.clear();
				preferredLanguagesList.clear();
				preferredLanguagesString = std::string(languageList);
				std::istringstream ss(preferredLanguagesString);
				std::string lng;
				while(std::getline(ss, lng, ','))
				{
					preferredLanguagesList.push_back(lng);
					AAMPLOG_INFO("Parsed preferred lang: %s", lng.c_str());
				}
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLanguage,preferredLanguagesString);
			}

			AAMPLOG_INFO("Number of preferred languages: %zu", preferredLanguagesList.size());

			if(labelList != NULL)
			{
				preferredLabelsString.clear();
				preferredLabelList.clear();
				preferredLabelsString = std::string(labelList);
				std::istringstream ss(preferredLabelsString);
				std::string lab;
				while(std::getline(ss, lab, ','))
				{
					preferredLabelList.push_back(lab);
					AAMPLOG_INFO("Parsed preferred label: %s", lab.c_str());
				}

				preferredLabelsString = std::string(labelList);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioLabel,preferredLabelsString);
				AAMPLOG_INFO("Number of preferred labels: %zu", preferredLabelList.size());
			}

			if( preferredRendition )
			{
				AAMPLOG_INFO("Setting rendition %s", preferredRendition);
				preferredRenditionString = std::string(preferredRendition);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioRendition,preferredRenditionString);
			}
			else
			{
				preferredRenditionString.clear();
			}

			if( preferredType )
			{
				preferredTypeString = std::string(preferredType);
				std::string delim = "_";
				auto pos = preferredTypeString.find(delim);
				auto end = preferredTypeString.length();
				if (pos != std::string::npos)
				{
					preferredTypeString =  preferredTypeString.substr(pos+1, end);
				}
				AAMPLOG_INFO("Setting accessibility type %s", preferredTypeString.c_str());
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING, eAAMPConfig_PreferredAudioType, preferredTypeString);
			}
			else
			{
				preferredTypeString.clear();
			}

			if(codecList != NULL)
			{
				preferredCodecString.clear();
				preferredCodecList.clear();
				preferredCodecString = std::string(codecList);
				std::istringstream ss(preferredCodecString);
				std::string codec;
				while(std::getline(ss, codec, ','))
				{
					preferredCodecList.push_back(codec);
					AAMPLOG_INFO("Parsed preferred codec: %s", codec.c_str());
				}
				preferredCodecString = std::string(codecList);
				SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredAudioCodec,preferredCodecString);
			}
			AAMPLOG_INFO("Number of preferred codecs: %zu", preferredCodecList.size());

			if(accessibilityItem && !accessibilityItem->getSchemeId().empty())
			{
				preferredAudioAccessibilityNode.clear();
				accessibilityPresent = true;
				const std::string &schemeId = accessibilityItem->getSchemeId();
				int ival = accessibilityItem->getIntValue();
				if( ival>=0 )
				{
					preferredAudioAccessibilityNode.setAccessibilityData(schemeId, ival);
				}
				else
				{
					preferredAudioAccessibilityNode.setAccessibilityData(schemeId, accessibilityItem->getStrValue() );
				}
				AAMPLOG_INFO("Preferred accessibility %s", preferredAudioAccessibilityNode.print().c_str() );
			}
			else
			{
				preferredAudioAccessibilityNode.clear();
			}

			if(preferredName)
			{
				AAMPLOG_INFO("Setting Name %s", preferredName);
				preferredNameString = std::string(preferredName);
			}
			else
			{
				preferredNameString.clear();
			}
		}
		else
		{
			AAMPLOG_INFO("Discarding Retune set language(s) (%s) , rendition (%s) and accessibility (%s) since already set",
				languageList?languageList:"", preferredRendition?preferredRendition:"", preferredType?preferredType:"");
		}
	}

	AAMPPlayerState state = GetState();
	AAMPLOG_INFO("state %d, isRetuneNeeded %d", state, isRetuneNeeded);
	if (state != eSTATE_IDLE && state != eSTATE_RELEASED && state != eSTATE_ERROR && isRetuneNeeded)
	{ // active playback session; apply immediately
		if (mpStreamAbstractionAAMP)
		{
			bool languagePresent = false;
			bool renditionPresent = false;
			bool accessibilityTypePresent = false;
			bool codecPresent = false;
			bool labelPresent = false;
			bool namePresent = false;
			int trackIndex = GetAudioTrack();

			bool languageAvailabilityInManifest = false;
			bool renditionAvailabilityInManifest = false;
			bool accessibilityAvailabilityInManifest = false;
			bool labelAvailabilityInManifest = false;
			bool nameAvailabilityInManifest = false;
			std::string trackIndexStr;
			bool codecChange = true;

			if (trackIndex >= 0)
			{
				std::vector<AudioTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableAudioTracks();
				char *currentPrefLanguage = const_cast<char*>(trackInfo[trackIndex].language.c_str());
				char *currentPrefRendition = const_cast<char*>(trackInfo[trackIndex].rendition.c_str());
				char *currentPrefAccessibility = const_cast<char*>(trackInfo[trackIndex].accessibilityType.c_str());
				char *currentPrefCodec = const_cast<char*>(trackInfo[trackIndex].codec.c_str());
				char *currentPrefLabel = const_cast<char*>(trackInfo[trackIndex].label.c_str());
				char *currentPrefName = const_cast<char*>(trackInfo[trackIndex].name.c_str());

				//If codec is already set, check the new codec against the older and ensure any change. If not set, read through the audio track info and found the codec against the new language set
				if(!preferredCodecString.empty())
				{
					if(preferredCodecString == currentPrefCodec)
					{
						codecChange = false;
					}
					AAMPLOG_WARN("PreferredCodecString %s existing Codec %s",preferredCodecString.c_str(),currentPrefCodec);
				}

				// Logic to check whether the given language is present in the available tracks,
				// if available, it should not match with current preferredLanguagesString, then call tune to reflect the language change.
				// if not available, then avoid calling tune.
				if(preferredLanguagesList.size() > 0)
				{
					std::string firstLanguage = preferredLanguagesList.at(0);

					// CID:280504 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.language == firstLanguage) && (temp.language != currentPrefLanguage))
						{
							languagePresent = true;
							if (trackIndexStr.empty())
							{
								trackIndexStr = temp.index;
							}

							if (temp.isAvailable)
							{
								languageAvailabilityInManifest = true;
								break;
							}
						}
					}

					if (preferredLanguagesList.size() > 1)
					{
						/* If multiple value of language is present then retune */
						languagePresent = true;
					}
				}

				// Logic to check whether the given label is present in the available tracks,
				// if available, it should not match with current preferredLabelsString, then call retune to reflect the language change.
				// if not available, then avoid calling tune. Call retune if multiple labels is present
				if(!preferredLabelsString.empty())
				{
					// CID:280504 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.label == preferredLabelsString) && (temp.label != currentPrefLabel))
						{
							labelPresent = true;
							if (temp.isAvailable)
							{
								labelAvailabilityInManifest = true;
								break;
							}
						}
					}

					if (preferredLabelList.size() > 1)
					{
						/* If multiple value of label is present then retune */
						labelPresent = true;
					}
				}


				// Logic to check whether the given rendition is present in the available tracks,
				// if available, it should not match with current preferredRenditionString, then call tune to reflect the rendition change.
				// if not available, then avoid calling tune.
				if(!preferredRenditionString.empty())
				{
					// CID:280504 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.rendition == preferredRenditionString) && (temp.rendition != currentPrefRendition))
						{
							renditionPresent = true;
							if (temp.isAvailable)
							{
								renditionAvailabilityInManifest = true;
								break;
							}
						}
					}
				}

				// Logic to check whether the given accessibility is present in the available tracks,
				// if available, it should not match with current preferredTypeString, then call tune to reflect the accessibility change.
				// if not available, then avoid calling tune.
				if(!preferredTypeString.empty())
				{
					// CID:280504 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.accessibilityType == preferredTypeString) && (temp.accessibilityType != currentPrefAccessibility))
						{
							accessibilityTypePresent = true;
							if (temp.isAvailable)
							{
								accessibilityAvailabilityInManifest = true;
								break;
							}
						}
					}
				}

				// Logic to check whether the given codec is present in the available tracks,
				// if available, it should not match with current preferred codec, then call tune to reflect the codec change.
				// if not available, then avoid calling tune.
				if (preferredCodecList.size() > 1)
				{
					/* If multiple value of codec is present then retune */
					codecPresent = true;
				}
				else if(preferredCodecList.size() > 0)
				{
					std::string firstCodec = preferredCodecList.at(0);

					for (auto &temp : trackInfo)
					{
						if ((temp.codec == firstCodec) && (temp.codec != currentPrefCodec) && (temp.isAvailable))
						{
							codecPresent = true;
							break;
						}
					}

				}
				else
				{
					// Empty preferred codec list.
				}

				// Logic to check whether the given name is present in the available tracks,
				// if available, it should not match with current preferredNameString, then call tune to reflect the name change.
				// if not available, then avoid calling tune.
				if (!preferredNameString.empty())
				{
					// CID:280504 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.name == preferredNameString) && (temp.name != currentPrefName))
						{
							namePresent = true;
							if (temp.isAvailable)
							{
								nameAvailabilityInManifest = true;
								break;
							}
						}
					}
				}
			}

			bool clearPreference = false;
			if(isRetuneNeeded && preferredCodecList.size() == 0 && preferredTypeString.empty() && preferredRenditionString.empty() \
				&& preferredLabelsString.empty() && preferredNameString.empty() && preferredLanguagesList.size() == 0)
			{
				/** Previous preference set and API called to clear all preferences; so retune to make effect **/
				AAMPLOG_INFO("API to clear all preferences; retune to make it affect");
				clearPreference = true;
			}

			if((mMediaFormat == eMEDIAFORMAT_OTA) || (mMediaFormat == eMEDIAFORMAT_RMF))
			{
				mpStreamAbstractionAAMP->SetPreferredAudioLanguages();
			}
			else if((mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE))
			{
				/*Avoid retuning in case of HEMIIN and COMPOSITE IN*/
			}
			else if (languagePresent || renditionPresent || accessibilityTypePresent || codecPresent || labelPresent || accessibilityPresent || namePresent || clearPreference) // call the tune only if there is a change in the language, rendition or accessibility.
			{
				if(!ISCONFIGSET_PRIV(eAAMPConfig_ChangeTrackWithoutRetune))
				{
					discardEnteringLiveEvt = true;
					mOffsetFromTunetimeForSAPWorkaround = (double)(aamp_GetCurrentTimeMS() / 1000) - mLiveOffset;
					mLanguageChangeInProgress = true;
					AcquireStreamLock();
					if(ISCONFIGSET_PRIV(eAAMPConfig_SeamlessAudioSwitch) && !mFirstTune && ( mMediaFormat == eMEDIAFORMAT_HLS_MP4 || mMediaFormat == eMEDIAFORMAT_DASH )  && !codecChange)
					{
						AAMPLOG_WARN("Seamless audio switch has been enabled");
						mpStreamAbstractionAAMP->RefreshTrack(eMEDIATYPE_AUDIO);
					}
					else
					{
						seek_pos_seconds = GetPositionSeconds();
						AAMPLOG_MIL("Retune to change the audio track at pos %fs", seek_pos_seconds);
						if (IsLocalAAMPTsb())
						{
							mAampTsbLanguageChangeInProgress = true;
						}
						TeardownStream(false);
						if(IsFogTSBSupported() &&
								((languagePresent && !languageAvailabilityInManifest) ||
								 (renditionPresent && !renditionAvailabilityInManifest) ||
								 (accessibilityTypePresent && !accessibilityAvailabilityInManifest) ||
								 (labelPresent && !labelAvailabilityInManifest) ||
								 (namePresent && !nameAvailabilityInManifest)))
						{
							ReloadTSB();
						}

						/* If AAMP TSB is enabled, flush the TSB before seeking to live */
						if(IsLocalAAMPTsb())
						{
							if(mTSBSessionManager)
							{
								AAMPLOG_INFO("Recreate the TSB Session Manager");
								CreateTsbSessionManager();
								SetLocalAAMPTsbInjection(false);
								TuneHelper(eTUNETYPE_SEEKTOLIVE);
							}
							else
							{
								AAMPLOG_ERR("TSB Session Manager is NULL");
							}
						}
						else if(mDisableRateCorrection)
						{
							TuneHelper(eTUNETYPE_SEEK);
						}
						else
						{
							TuneHelper(eTUNETYPE_SEEKTOLIVE);
						}
					}
					discardEnteringLiveEvt = false;
					ReleaseStreamLock();
				}
				else if(!trackIndexStr.empty())
				{
					mpStreamAbstractionAAMP->ChangeMuxedAudioTrackIndex(trackIndexStr);
				}
			}
		}
	}
}

/**
 *  @brief Sanitize the given language list by normalizing the codes and removing duplicates.
 *         Order is preserved.
 *
 *         NOTE: if AAMP langCodePreference config is ISO639_NO_LANGCODE_PREFERENCE (=0 the default value),
 *         then "en", "eng" - or other 2/3-digit codes for the *same* language - will not be
 *         normalized and deduplicated to a single value.
 */
void PrivateInstanceAAMP::SanitizeLanguageList(std::vector<std::string>& languages) const
{
	std::transform( languages.begin(), languages.end(),
					languages.begin(),
					[this](std::string& lang)
					{ return Getiso639map_NormalizeLanguageCode(lang, this->GetLangCodePreference()); } );

	// To keep track of the languages that have already been encountered.
	std::unordered_set<std::string> seen;

	auto new_end = std::remove_if(languages.begin(), languages.end(),
								  [&seen](const std::string &value)
								  {
									  if (seen.find(value) != seen.end())
									  {
										  return true;
									  }
									  else
									  {
										  seen.insert(value);
										  return false;
									  }
								  });
	languages.erase(new_end, languages.end());
}

/**
 *  @brief Set Preferred Text Language
 */
void PrivateInstanceAAMP::SetPreferredTextLanguages(const char *param )
{
	/**< First argument is Json data then parse it and and assign the variables properly*/
	AampJsonObject* jsObject = nullptr;
	bool accessibilityPresent = false;
	std::vector<std::string> inputTextLanguagesList;

	try
	{
		jsObject = new AampJsonObject(param);
	}
	catch(const std::exception& e)
	{
		/**<Nothing to do exclude it*/
	}

	if (jsObject)
	{
		AAMPLOG_INFO("Preferred Text Language Properties received as json : %s", param);

		std::string inputTextLanguagesString;

		/** Get language Properties*/
		if(jsObject->isArray("languages"))
		{
			jsObject->get("languages", inputTextLanguagesList);
		}
		else if (jsObject->isString("languages"))
		{ // if starting with string, create simple array
			if (jsObject->get("languages", inputTextLanguagesString))
			{
				inputTextLanguagesList.push_back(inputTextLanguagesString);
			}
		}
		else if (jsObject->isString("language"))
		{
			if (jsObject->get("language", inputTextLanguagesString))
			{
				inputTextLanguagesList.push_back(inputTextLanguagesString);
			}
		}
		else
		{
			AAMPLOG_ERR("Preferred Text Language Field Only support String or String Array");
		}

		std::string inputTextRenditionString;
		/** Get rendition or role Properties*/
		if (jsObject->isString("rendition"))
		{
			if (jsObject->get("rendition", inputTextRenditionString))
			{
				AAMPLOG_INFO("Preferred text rendition string: %s", inputTextRenditionString.c_str());
			}
		}

		std::string inputTextLabelString;
		/** Get label Properties*/
		if (jsObject->isString("label"))
		{
			if (jsObject->get("label", inputTextLabelString))
			{
				AAMPLOG_INFO("Preferred text label string: %s", inputTextLabelString.c_str());
			}
		}

		std::string inputInstreamIdString;
		/** Get instreamId*/
		if (jsObject->isString("instreamId"))
		{
			if (jsObject->get("instreamId", inputInstreamIdString))
			{
				AAMPLOG_INFO("Preferred instreamId string: %s", inputInstreamIdString.c_str());
			}
		}

		std::string inputTextTypeString;
		/** Get accessibility type Properties*/
		if (jsObject->isString("accessibilityType"))
		{
			if (jsObject->get("accessibilityType", inputTextTypeString))
			{
				AAMPLOG_INFO("Preferred text type string: %s", inputTextTypeString.c_str());
			}
		}

		std::string inputTextNameString;
		if (jsObject->isString("name"))
		{
			if (jsObject->get("name", inputTextNameString))
			{
				AAMPLOG_INFO("Preferred name string: %s", inputTextNameString.c_str());
			}
		}

		Accessibility  inputTextAccessibilityNode;
		/** Get accessibility Properties*/
		if (jsObject->isObject("accessibility"))
		{
			AampJsonObject accessNode;
			if (jsObject->get("accessibility", accessNode))
			{
				inputTextAccessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode(accessNode);
				if (!inputTextAccessibilityNode.getSchemeId().empty())
				{
					AAMPLOG_INFO("Preferred accessibility: %s", inputTextAccessibilityNode.print().c_str());
				}
				if(inputTextAccessibilityNode != preferredTextAccessibilityNode)
				{
					accessibilityPresent = true;
				}
			}
		}

		/**< Release json object **/
		SAFE_DELETE(jsObject);

		preferredTextRenditionString = inputTextRenditionString;
		preferredTextAccessibilityNode = inputTextAccessibilityNode;
		preferredTextLabelString = inputTextLabelString;
		preferredTextTypeString = inputTextTypeString;
		preferredInstreamIdString = inputInstreamIdString;
		preferredTextNameString = inputTextNameString;

		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextRendition,preferredTextRenditionString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextLabel,preferredTextLabelString);
		SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextType,preferredTextTypeString);
	}
	else if (param)
	{
		AAMPLOG_INFO("Preferred Text Languages received as comma-delimited string : %s", param);

		std::istringstream ss(param);
		std::string lng;
		while(std::getline(ss, lng, ','))
		{
			inputTextLanguagesList.push_back(lng);
		}
	}
	else
	{
		AAMPLOG_INFO("No valid Parameter received");
		return;
	}

	SanitizeLanguageList(inputTextLanguagesList);
	preferredTextLanguagesList = inputTextLanguagesList;

	// Write the preferred languages back to the string
	preferredTextLanguagesString.clear();
	for (const auto& lang : preferredTextLanguagesList)
	{
		if (!preferredTextLanguagesString.empty())
		{
			preferredTextLanguagesString += ",";
		}
		preferredTextLanguagesString += lang;
	}

	AAMPLOG_INFO("Number of preferred Text languages: %zu", preferredTextLanguagesList.size());
	AAMPLOG_INFO("Preferred Text languages string: %s", preferredTextLanguagesString.c_str());

	SETCONFIGVALUE_PRIV(AAMP_APPLICATION_SETTING,eAAMPConfig_PreferredTextLanguage,preferredTextLanguagesString);

	AAMPPlayerState state = GetState();
	if (state != eSTATE_IDLE && state != eSTATE_RELEASED && state != eSTATE_ERROR )
	{ // active playback session; apply immediately
		if (mpStreamAbstractionAAMP)
		{
			bool languagePresent = false;
			bool renditionPresent = false;
			bool accessibilityTypePresent = false;
			bool labelPresent = false;
			bool instreamIdPresent = false;
			int trackIndex = GetTextTrack();
			bool namePresent = false;

			bool languageAvailabilityInManifest = false;
			bool renditionAvailabilityInManifest = false;
			bool accessibilityAvailabilityInManifest = false;
			bool labelAvailabilityInManifest = false;
			bool nameAvailabilityInManifest = false;
			bool trackNotEnabled = false;

			if (trackIndex >= 0)
			{
				std::vector<TextTrackInfo> trackInfo = mpStreamAbstractionAAMP->GetAvailableTextTracks();
				std::string currentPrefLanguage = Getiso639map_NormalizeLanguageCode(
					trackInfo[trackIndex].language, this->GetLangCodePreference());
				char *currentPrefRendition = const_cast<char*>(trackInfo[trackIndex].rendition.c_str());
				char *currentPrefInstreamId =  const_cast<char*>(trackInfo[trackIndex].instreamId.c_str());
				char *currentPrefName = const_cast<char*>(trackInfo[trackIndex].name.c_str());

				// Logic to check whether the given language is present in the available tracks,
				// if available, it should not match with current preferredLanguagesString, then call tune to reflect the language change.
				// if not available, then avoid calling tune.
				if(preferredTextLanguagesList.size() > 0)
				{
					std::string firstLanguage = preferredTextLanguagesList.at(0);

					for (const auto& track : trackInfo)
					{
						std::string trackLanguage = Getiso639map_NormalizeLanguageCode(
							track.language, this->GetLangCodePreference());

						if ((trackLanguage == firstLanguage) &&
							(trackLanguage != currentPrefLanguage))
						{
							languagePresent = true;
							if (track.isAvailable)
							{
								languageAvailabilityInManifest = true;
								break;
							}
						}
					}

					if (preferredTextLanguagesList.size() > 1)
					{
						/* If multiple value of language is present then retune. */
						languagePresent = true;
					}
				}

				// Logic to check whether the given rendition is present in the available tracks,
				// if available, it should not match with current preferredTextRenditionString, then call tune to reflect the rendition change.
				// if not available, then avoid calling tune.
				if(!preferredTextRenditionString.empty())
				{
					// CID:280501 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.rendition == preferredTextRenditionString) && (temp.rendition != currentPrefRendition))
						{
							renditionPresent = true;
							if (temp.isAvailable)
							{
								renditionAvailabilityInManifest = true;
								break;
							}
						}
					}
				}

				//Logic to check whether the given instreamId is present in the available tracks,
				//if available, it should not match with current preferredInstreamIdString, then call tune to reflect the track change.
				//if not available, then avoid calling tune.
				if(!preferredInstreamIdString.empty())
				{
					std::string curInstreamId = preferredInstreamIdString;
					auto instreamId = std::find_if(trackInfo.begin(), trackInfo.end(),
								[curInstreamId, currentPrefInstreamId] (TextTrackInfo& temp)
								{ return ((temp.instreamId == curInstreamId) && (temp.instreamId != currentPrefInstreamId)); });
					instreamIdPresent = (instreamId != end(trackInfo));
				}
				// Logic to check whether the given name is present in the available tracks,
				// if available, it should not match with current preferredTextNameString, then call tune to reflect the name change.
				// if not available, then avoid calling tune.
				if(!preferredTextNameString.empty())
				{
					// CID:280501 - Using invalid iterator
					for (auto &temp : trackInfo)
					{
						if ((temp.name == preferredTextNameString) && (temp.name != currentPrefName))
						{
							namePresent = true;
							if (temp.isAvailable)
							{
								nameAvailabilityInManifest = true;
								break;
							}
						}
					}
				}

			}
			else
			{
				trackNotEnabled = true;
			}

			if((mMediaFormat == eMEDIAFORMAT_HDMI) || (mMediaFormat == eMEDIAFORMAT_COMPOSITE) || (mMediaFormat == eMEDIAFORMAT_OTA) || \
				(mMediaFormat == eMEDIAFORMAT_RMF))
			{
				/**< Avoid retuning in case of HEMIIN and COMPOSITE IN*/
			}
			else if (languagePresent || renditionPresent || accessibilityPresent || trackNotEnabled || instreamIdPresent || namePresent) /**< call the tune only if there is a change in the language, rendition or accessibility.*/
			{
				discardEnteringLiveEvt = true;
				mOffsetFromTunetimeForSAPWorkaround = (double)(aamp_GetCurrentTimeMS() / 1000) - mLiveOffset;
				mLanguageChangeInProgress = true;
				AcquireStreamLock();

				if (ISCONFIGSET_PRIV(eAAMPConfig_SeamlessAudioSwitch) && !mFirstTune
					&& ((mMediaFormat == eMEDIAFORMAT_HLS_MP4) || (mMediaFormat == eMEDIAFORMAT_DASH)))
				{
					AAMPLOG_WARN("Seamless Text switch has been enabled");
					mpStreamAbstractionAAMP->RefreshTrack(eMEDIATYPE_SUBTITLE);
				}
				else
				{
					if((mMediaFormat == eMEDIAFORMAT_HLS) ||(mMediaFormat == eMEDIAFORMAT_HLS_MP4))
					{
						TextTrackInfo selectedTextTrack;
						if(mpStreamAbstractionAAMP->SelectPreferredTextTrack(selectedTextTrack))
						{
							SetPreferredTextTrack(selectedTextTrack);
						}
					}
					seek_pos_seconds = GetPositionSeconds();

					if (IsLocalAAMPTsb())
					{
						mAampTsbLanguageChangeInProgress = true;
					}

					TeardownStream(false);
					if(IsFogTSBSupported() &&
				 	((languagePresent && !languageAvailabilityInManifest) ||
				 	(renditionPresent && !renditionAvailabilityInManifest) ||
				 	(accessibilityTypePresent && !accessibilityAvailabilityInManifest) ||
					(labelPresent && !labelAvailabilityInManifest) ||
					(namePresent && !nameAvailabilityInManifest)))
					{
						ReloadTSB();
					}

					if(IsLocalAAMPTsb())
					{
						AAMPLOG_WARN("Flush the TSB before seeking to live");

						/* If AAMP TSB is enabled, flush the TSB before seeking to live */
						if(mTSBSessionManager)
						{
							AAMPLOG_INFO("Recreate the TSB Session Manager and Tune to Live");
							CreateTsbSessionManager();
							SetLocalAAMPTsbInjection(false);
							TuneHelper(eTUNETYPE_SEEKTOLIVE);
						}
						else
						{
							AAMPLOG_ERR("TSB Session Manager is NULL");
						}
					}
					else
					{
						TuneHelper(eTUNETYPE_SEEK);
					}

					discardEnteringLiveEvt = false;
				}
				ReleaseStreamLock();

				std::vector<TextTrackInfo> tracks = mpStreamAbstractionAAMP->GetAvailableTextTracks();
				long trackId = -1;
				if (instreamIdPresent || (trackNotEnabled && !preferredInstreamIdString.empty()))
				{
					for (auto it = tracks.begin(); it != tracks.end(); it++)
					{
						if ((it->instreamId == preferredInstreamIdString) && it->isCC)
						{
							trackId = std::distance(tracks.begin(), it);
						}
					}
				}
				else if ((languagePresent || (trackNotEnabled && !preferredTextLanguagesString.empty())) && (preferredRenditionString != "subtitle")) // if no match found for instreamId, check for language string match
				{
					for (auto it = tracks.begin(); it != tracks.end(); it++)
					{
						if ((it->language == preferredTextLanguagesString) && it->isCC)
						{
							trackId = std::distance(tracks.begin(), it);
						}
					}
				}
				if (trackId >= 0 && trackId < tracks.size())
				{
					TextTrackInfo track = tracks[trackId];
					if(!track.instreamId.empty())
					{
						CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT;
						if (mMediaFormat == eMEDIAFORMAT_DASH && (std::isdigit(static_cast<unsigned char>(track.instreamId[0]))) && !track.rendition.empty())
						{
							if (track.rendition.find("608") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_608;
							}
							else if (track.rendition.find("708") != std::string::npos)
							{
								format = eCLOSEDCAPTION_FORMAT_708;
							}
						}
						PlayerCCManager::GetInstance()->SetTrack(track.instreamId, format);
					}
				}

			}
		}
	}
}

/**
 * @brief Enable download activity for individual mediaType
 *
 */
void PrivateInstanceAAMP::EnableMediaDownloads(AampMediaType type)
{
	mMediaDownloadsEnabled[type] = true;
}

/**
 * @brief Disable download activity for individual mediaType
 */
void PrivateInstanceAAMP::DisableMediaDownloads(AampMediaType type)
{
	mMediaDownloadsEnabled[type] = false;
}

/**
 * @brief Enable Download activity for all mediatypes
 */
void PrivateInstanceAAMP::EnableAllMediaDownloads()
{
	for (int i = 0; i <= eMEDIATYPE_DEFAULT; i++)
	{
		// Enable downloads for all mediaTypes
		EnableMediaDownloads((AampMediaType) i);
	}
}

/*
 *   @brief get the WideVine KID Workaround from url
 *
 */
#define WV_KID_WORKAROUND "WideVineKIDWorkaround"

/**
 * @brief workaround for non-compliant partner content
 */
bool PrivateInstanceAAMP::IsWideVineKIDWorkaround(std::string url)
{
	bool enable = false;
	auto pos = url.find(WV_KID_WORKAROUND);
	if (pos != string::npos){
		pos = pos + strlen(WV_KID_WORKAROUND);
		AAMPLOG_INFO("URL found WideVine KID Workaround at %d key = %c",
				(int)pos, url.at(pos));
		enable = (url.at(pos) == '1');
	}

	return enable;
}

//#define ENABLE_DUMP 1 //uncomment this to enable dumping of PSSH Data

/**
 * @brief Replace KeyID from PsshData
 */
unsigned char* PrivateInstanceAAMP::ReplaceKeyIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength)
{
	unsigned char *outputData = NULL;
	unsigned int WIDEVINE_PSSH_KEYID_OFFSET = 36u;
	//unsigned int WIDEVINE_PSSH_DATA_SIZE = 60u;
	unsigned int CK_PSSH_KEYID_OFFSET = 32u;
	unsigned int COMMON_KEYID_SIZE = 16u;
	unsigned char WVSamplePSSH[] = {
		0x00, 0x00, 0x00, 0x3c,
		0x70, 0x73, 0x73, 0x68,
		0x00, 0x00, 0x00, 0x00,
		0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
		0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed,
		0x00, 0x00, 0x00, 0x1c, 0x08, 0x01, 0x12, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x22, 0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x37
	};
	if (InputData){
		AAMPLOG_INFO("Converting system UUID of PSSH data size (%zu)", InputDataLength);
#ifdef ENABLE_DUMP
		AAMPLOG_INFO("PSSH Data (%d) Before Modification : ", InputDataLength);
		DumpBlob(InputData, InputDataLength);
#endif

		/** Replace KeyID of WV PSSH Data with Key ID of CK PSSH Data **/
		int iWVpssh = WIDEVINE_PSSH_KEYID_OFFSET;
		int CKPssh = CK_PSSH_KEYID_OFFSET;
		int size = 0;
		if (CK_PSSH_KEYID_OFFSET+COMMON_KEYID_SIZE <=  InputDataLength){
			for (; size < COMMON_KEYID_SIZE; ++size, ++iWVpssh, ++CKPssh  ){
				/** Transfer KeyID from CK PSSH data to WV PSSH Data **/
				WVSamplePSSH[iWVpssh] = InputData[CKPssh];
			}

			/** Allocate WV PSSH Data memory and transfer local data **/
			outputData = (unsigned char *)malloc(sizeof(WVSamplePSSH));
			if (outputData){
				memcpy(outputData, WVSamplePSSH, sizeof(WVSamplePSSH));
				OutputDataLength = sizeof(WVSamplePSSH);
#ifdef ENABLE_DUMP
				AAMPLOG_INFO("PSSH Data (%d) after Modification : ", OutputDataLength);
				DumpBlob(outputData, OutputDataLength);
#endif
				return outputData;

			}else{
				AAMPLOG_ERR("PSSH Data Memory allocation failed ");
			}
		}else{
			//Invalid PSSH data
			AAMPLOG_ERR("Invalid Clear Key PSSH data ");
		}
	}else{
		//Invalid argument - PSSH Data
		AAMPLOG_ERR("Invalid Argument of PSSH data ");
	}
	return NULL;
}
/*
 * @brief UpdateBufferBasedOnLiveOffset - fn to modify maxbuffer and minbuffer based on liveoffset
 */

void PrivateInstanceAAMP::UpdateBufferBasedOnLiveOffset()
{
	int maxbuffer,minbuffer;
	double liveoffset =0,liveoffset4k=0;
	liveoffset = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset);
	liveoffset4k = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset4K);
	maxbuffer = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp);
	if(liveoffset4k <= maxbuffer)
	{
		const int defaultFragmentDur = 2;
		int maxbuffer,minbuffer;
		double liveoffset =0,liveoffset4k=0;
		liveoffset = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset);
		liveoffset4k = GETCONFIGVALUE_PRIV(eAAMPConfig_LiveOffset4K);
		maxbuffer = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp);
		if(GETCONFIGOWNER_PRIV(eAAMPConfig_LiveOffset4K) > AAMP_DEFAULT_SETTING)
		{
			double netTimeOut =  GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout);
			int calMaxBuff = static_cast<int>(std::min( (liveoffset4k - defaultFragmentDur), (netTimeOut + defaultFragmentDur)));
			mBufferFor4kRampup = std::min(calMaxBuff, maxbuffer);
			mBufferFor4kRampdown = mBufferFor4kRampup -  (defaultFragmentDur*2);
			mBufferFor4kRampdown = mBufferFor4kRampdown < defaultFragmentDur ? defaultFragmentDur : mBufferFor4kRampdown;
		}
		if(GETCONFIGOWNER_PRIV(eAAMPConfig_LiveOffset) > AAMP_DEFAULT_SETTING)
		{
			double netTimeOut =  GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout);
			int calMaxBuff = static_cast<int>(std::min((liveoffset - defaultFragmentDur), (netTimeOut + defaultFragmentDur)));
			maxbuffer = std::min(calMaxBuff, maxbuffer);
			minbuffer = maxbuffer  - (defaultFragmentDur*2);
			minbuffer = minbuffer < defaultFragmentDur*2 ? defaultFragmentDur*2: minbuffer; // minimum 2 fragment needed for non LLD
			AAMPLOG_WARN("Configuring MaxABRNWBufferRampUp (%d) and MinABRNWBufferRampDown (%d)", maxbuffer, minbuffer );
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MaxABRNWBufferRampUp,maxbuffer);
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MinABRNWBufferRampDown,minbuffer);
		}


	}
	if(liveoffset <= maxbuffer)
	{
		maxbuffer = liveoffset -2;
		minbuffer = maxbuffer  -5 ;
		minbuffer = minbuffer < 2 ? 2: minbuffer;
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MaxABRNWBufferRampUp,maxbuffer);
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MinABRNWBufferRampDown,minbuffer);
	}

}

struct curl_slist* PrivateInstanceAAMP::GetCustomHeaders(AampMediaType mediaType)
{
	struct curl_slist* httpHeaders = NULL;
	if (mCustomHeaders.size() > 0)
	{
		std::string customHeader;
		std::string headerValue;
		for (std::unordered_map<std::string, std::vector<std::string>>::iterator it = mCustomHeaders.begin();
				it != mCustomHeaders.end(); it++)
		{
			customHeader.clear();
			headerValue.clear();
			customHeader.insert(0, it->first);
			customHeader.push_back(' ');
			headerValue = it->second.at(0);
			if (it->first.compare("X-MoneyTrace:") == 0)
			{
				if (mFogTSBEnabled && !mIsFirstRequestToFOG)
				{
					continue;
				}
				char buf[512];
				memset(buf, '\0', 512);
				if (it->second.size() >= 2)
				{
					snprintf(buf, 512, "trace-id=%s;parent-id=%s;span-id=%lld",
							(const char*)it->second.at(0).c_str(),
							(const char*)it->second.at(1).c_str(),
							aamp_GetCurrentTimeMS());
				}
				else if (it->second.size() == 1)
				{
					snprintf(buf, 512, "trace-id=%s;parent-id=%lld;span-id=%lld",
							(const char*)it->second.at(0).c_str(),
							aamp_GetCurrentTimeMS(),
							aamp_GetCurrentTimeMS());
				}
				headerValue = buf;
			}
			if (it->first.compare("Wifi:") == 0)
			{
				activeInterfaceWifi =  pPlayerExternalsInterface->GetActiveInterface();
				if (true == activeInterfaceWifi)
				{
					headerValue = "1";
				}
				else
				{
					headerValue = "0";
				}
			}
			customHeader.append(headerValue);
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
		}
		if (ISCONFIGSET_PRIV(eAAMPConfig_LimitResolution) && mIsFirstRequestToFOG && mFogTSBEnabled && eMEDIATYPE_MANIFEST == mediaType)
		{
			std::string customHeader;
			customHeader.clear();
			customHeader = "width: " +  std::to_string(mDisplayWidth);
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
			customHeader.clear();
			customHeader = "height: " + std::to_string(mDisplayHeight);
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
		}

		if(mFogTSBEnabled  && eMEDIATYPE_VIDEO == mediaType)
		{
			double bufferedDuration = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec() * 1000.0;
			std::string customHeader;
			customHeader.clear();
			customHeader = "Buffer: " +std::to_string(bufferedDuration);
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
		}
		if(mFogTSBEnabled  && eMEDIATYPE_AUDIO == mediaType)
		{
			double bufferedAudioDuration = mpStreamAbstractionAAMP->GetBufferedDuration() * 1000.0;
			std::string customHeader;
			customHeader.clear();
			customHeader = "AudioBuffer: " +std::to_string(bufferedAudioDuration);
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
		}
		if(mFogTSBEnabled && (eMEDIATYPE_VIDEO == mediaType || eMEDIATYPE_AUDIO == mediaType))
		{
			MediaTrack* mediaTrack = (eMEDIATYPE_VIDEO == mediaType)?(mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_VIDEO)):(mpStreamAbstractionAAMP->GetMediaTrack(eTRACK_AUDIO));
			if((mediaTrack) && (mediaTrack->GetBufferStatus() == BUFFER_STATUS_RED))
			{
				std::string customHeader;
				customHeader.clear();
				customHeader = "BufferStarvation: ";
				httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
			}
		}

		if (ISCONFIGSET_PRIV(eAAMPConfig_CurlHeader) && (eMEDIATYPE_VIDEO == mediaType || eMEDIATYPE_PLAYLIST_VIDEO == mediaType))
		{
			std::string customheaderstr = GETCONFIGVALUE_PRIV(eAAMPConfig_CustomHeader);
			if(!customheaderstr.empty())
			{
				//AAMPLOG_WARN ("Custom Header Data: Index( %d ) Data( %s )", i, &customheaderstr.at(i));
				httpHeaders = curl_slist_append(httpHeaders, customheaderstr.c_str());
			}
		}

		if (mIsFirstRequestToFOG && mFogTSBEnabled && eMEDIATYPE_MANIFEST == mediaType)
		{
			std::string customHeader = "4k: 1";
			if (ISCONFIGSET_PRIV(eAAMPConfig_Disable4K))
			{
				customHeader = "4k: 0";
			}
			httpHeaders = curl_slist_append(httpHeaders, customHeader.c_str());
		}

		if (httpHeaders != NULL)
		{
			return httpHeaders;
		}
	}
	return httpHeaders;
}


// -------------------------------------------------
// ID3 Metadata

void PrivateInstanceAAMP::UpdatePTSOffsetFromTune(double value_sec, bool is_set)
{
	if (is_set)
	{
		AAMPLOG_INFO(" Setting PTS offset: %f | %f", value_sec, seek_pos_seconds);
		m_PTSOffsetFromTune.store(value_sec);
	}
	else
	{
		// With C++20 we will be able to use the better option:
		// m_PTSOffsetFromTune.fetch_add(value);
		AAMPLOG_INFO(" Updating PTS offset in seconds: %f | %f", value_sec, seek_pos_seconds);
		m_PTSOffsetFromTune.store(m_PTSOffsetFromTune.load() + value_sec);
	}
}

void PrivateInstanceAAMP::ID3MetadataHandler(AampMediaType mediaType, const uint8_t * ptr, size_t pkt_len, const SegmentInfo_t & info, const char * scheme_uri)
{
	const auto is_id3_listener_available = IsEventListenerAvailable(AAMP_EVENT_ID3_METADATA);

	if (is_id3_listener_available)
	{
		namespace aih = aamp::id3_metadata::helpers;
		const auto data_len = aih::DataSize(ptr);

		std::vector<uint8_t> data (ptr, ptr + data_len);

		if (data_len && mId3MetadataCache.CheckNewMetadata(mediaType, data))
		{
			const auto offset = this->GetPTSOffsetFromTune();
			const auto timestamp_ms = static_cast<uint64_t>((info.pts_s + offset) * 1000. + 0.5);

			std::stringstream ss;
			ss << "timestamp: " << timestamp_ms << " - PTS: " << info.pts_s << " "
				<< offset << " [" << seek_pos_seconds << "] || data: " << aih::ToString(ptr, data_len);
			AAMPLOG_WARN(" ID3 tag # %s", ss.str().c_str());

			ReportID3Metadata(mediaType, std::move(data),
				nullptr, nullptr, timestamp_ms,
				0, 0, 1000, 0
			);
		}
	}
}

/**
 * @brief Process the ID3 metadata from segment
 */
void PrivateInstanceAAMP::ProcessID3Metadata(char *segment, size_t size, AampMediaType type, uint64_t timeStampOffset)
{
	namespace aih = aamp::id3_metadata::helpers;

	// Logic for ID3 metadata
	const auto early_processing = mConfig->IsConfigSet(eAAMPConfig_EarlyID3Processing);
	if (!early_processing && segment && mEventManager->IsEventListenerAvailable(AAMP_EVENT_ID3_METADATA))
	{
		uint8_t * seg_buffer = reinterpret_cast<uint8_t *>(segment);

		IsoBmffBuffer buffer;
		buffer.setBuffer(seg_buffer, size);
		buffer.parseBuffer();
		if(!buffer.isInitSegment())
		{
			uint8_t* message = nullptr;
			uint32_t messageLen = 0;
			char * schemeIDUri = nullptr;
			uint8_t* value = nullptr;
			uint64_t presTime = 0;
			uint32_t timeScale = 0;
			uint32_t eventDuration = 0;
			uint32_t id = 0;
			if (buffer.getEMSGData(message, messageLen, schemeIDUri, value, presTime, timeScale, eventDuration, id))
			{
				if (message && messageLen > 0 && aih::IsValidHeader(message, messageLen))
				{
					AAMPLOG_TRACE("PrivateInstanceAAMP: Found ID3 metadata[%d]", type);

					if(mMediaFormat == eMEDIAFORMAT_DASH)
					{
						ReportID3Metadata(type, message, messageLen, schemeIDUri, (char*)(value), presTime, id, eventDuration, timeScale, GetMediaStreamContext(type)->timeStampOffset);
					}else
					{
						ReportID3Metadata(type, message, messageLen, schemeIDUri, (char*)(value), presTime, id, eventDuration, timeScale, timeStampOffset);
					}
				}
			}
		}
	}
}

/**
 * @brief Report ID3 metadata events
 */
void PrivateInstanceAAMP::ReportID3Metadata(AampMediaType mediaType, const uint8_t* ptr, size_t len,
	const char* schemeIdURI, const char* id3Value, uint64_t presTime,
	uint32_t id3ID, uint32_t eventDur, uint32_t tScale, uint64_t tStampOffset)
{
	std::vector<uint8_t> data (ptr, ptr + len);
	ReportID3Metadata(mediaType, std::move(data),
		schemeIdURI, id3Value, presTime,
		id3ID, eventDur, tScale, tStampOffset);
}

void PrivateInstanceAAMP::ReportID3Metadata(AampMediaType mediaType, std::vector<uint8_t> data,
	const char* schemeIdURI, const char* id3Value, uint64_t presTime,
	uint32_t id3ID, uint32_t eventDur, uint32_t tScale, uint64_t tStampOffset)
{
	namespace ai = aamp::id3_metadata;

	mId3MetadataCache.UpdateMetadataCache(mediaType, data);

	ai::CallbackData id3Metadata {
		std::move(data),
		static_cast<const char*>(schemeIdURI),
		static_cast<const char*>(id3Value),
		presTime,
		id3ID,
		eventDur,
		tScale,
		tStampOffset};

	SendId3MetadataEvent(&id3Metadata);
}

/**
 * @brief GetPauseOnFirstVideoFrameDisplay
 */
bool PrivateInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
	return mPauseOnFirstVideoFrameDisp;
}

/**
 * @brief Sets  Low Latency Service Data
 */
void PrivateInstanceAAMP::SetLLDashServiceData(AampLLDashServiceData &stAampLLDashServiceData)
{
	this->mAampLLDashServiceData = stAampLLDashServiceData;
}

/**
 * @brief Gets Low Latency Service Data
 */
AampLLDashServiceData*  PrivateInstanceAAMP::GetLLDashServiceData(void)
{
	return &this->mAampLLDashServiceData;
}


/**
 * @brief Sets Low Video TimeScale
 */
void PrivateInstanceAAMP::SetVidTimeScale(uint32_t vidTimeScale)
{
	this->vidTimeScale = vidTimeScale;
}

/**
 * @brief Gets Video TimeScale
 */
uint32_t  PrivateInstanceAAMP::GetVidTimeScale(void)
{
	return vidTimeScale;
}

/**
 * @brief Sets Low Audio TimeScale
 */
void PrivateInstanceAAMP::SetAudTimeScale(uint32_t audTimeScale)
{
	this->audTimeScale = audTimeScale;
}

/**
 * @brief Gets Audio TimeScale
 */
uint32_t  PrivateInstanceAAMP::GetAudTimeScale(void)
{
	return audTimeScale;
}

/**
 * @brief Sets Subtitle TimeScale
 * @param[in] subTimeScale - Subtitle TimeScale
 */
void PrivateInstanceAAMP::SetSubTimeScale(uint32_t subTimeScale)
{
	this->subTimeScale = subTimeScale;
}

/**
 * @brief Gets Subtitle TimeScale
 * @return uint32_t - Subtitle TimeScale
 */
uint32_t  PrivateInstanceAAMP::GetSubTimeScale(void)
{
	return subTimeScale;
}

/**
 * @brief Sets Speed Cache
 */
void PrivateInstanceAAMP::SetLLDashSpeedCache(struct SpeedCache &speedCache)
{
	this->speedCache = speedCache;
}

/**
 * @brief Gets Speed Cache
 */
struct SpeedCache* PrivateInstanceAAMP::GetLLDashSpeedCache()
{
	return &speedCache;
}

bool PrivateInstanceAAMP::GetLiveOffsetAppRequest()
{
	return mLiveOffsetAppRequest;
}

/**
 * @brief set LiveOffset Request flag Status
 */
void PrivateInstanceAAMP::SetLiveOffsetAppRequest(bool LiveOffsetAppRequest)
{
	this->mLiveOffsetAppRequest = LiveOffsetAppRequest;
}
/**
 *  @brief Get Low Latency Service Configuration Status
 */
bool PrivateInstanceAAMP::GetLowLatencyServiceConfigured()
{
	return bLowLatencyServiceConfigured;
}

/**
 *  @brief Set Low Latency Service Configuration Status
 */
void PrivateInstanceAAMP::SetLowLatencyServiceConfigured(bool bConfig)
{
	bLowLatencyServiceConfigured = bConfig;
	mhAbrManager.SetLowLatencyServiceConfigured(bConfig);
}
/**
 *  @brief Get Current Latency
 */
long PrivateInstanceAAMP::GetCurrentLatency()
{
	return mCurrentLatency;
}

/**
 * @brief Set Current Latency
 */
void PrivateInstanceAAMP::SetCurrentLatency(long currentLatency)
{
	this->mCurrentLatency = currentLatency;
}

/**
 *     @brief Get Media Stream Context
 */
MediaStreamContext* PrivateInstanceAAMP::GetMediaStreamContext(AampMediaType type)
{
	if(mpStreamAbstractionAAMP &&
		(type == eMEDIATYPE_VIDEO ||
		 type == eMEDIATYPE_AUDIO ||
		 type == eMEDIATYPE_SUBTITLE ||
		 type == eMEDIATYPE_AUX_AUDIO))
	{
		MediaStreamContext* context = (MediaStreamContext*)mpStreamAbstractionAAMP->GetMediaTrack((TrackType)type);
		return context;
	}
	return NULL;
}

/**
 *  @brief GetPeriodDurationTimeValue
 */
double PrivateInstanceAAMP::GetPeriodDurationTimeValue(void)
{
	return mNextPeriodDuration;
}

/**
 *  @brief GetPeriodStartTimeValue
 */
double PrivateInstanceAAMP::GetPeriodStartTimeValue(void)
{
	return mNextPeriodStartTime;
}

/**
 *  @brief GetPeriodScaledPtoStartTime
 */
double PrivateInstanceAAMP::GetPeriodScaledPtoStartTime(void)
{
	return mNextPeriodScaledPtoStartTime;
}

/**
 *  @brief Get playback stats for the session so far
 */
std::string PrivateInstanceAAMP::GetPlaybackStats()
{
	std::string strVideoStatsJson;
	long liveLatency = 0;
	//Update liveLatency only when playback is active and live
	if(mpStreamAbstractionAAMP && IsLive())
		liveLatency = mpStreamAbstractionAAMP->GetBufferedVideoDurationSec() * 1000.0;

	if(mVideoEnd)
	{
		mVideoEnd->setPlaybackMode(mPlaybackMode);
		mVideoEnd->setLiveLatency(liveLatency);
		mVideoEnd->SetDisplayResolution(mDisplayWidth,mDisplayHeight);
		//Update VideoEnd Data
		if(mTimeAtTopProfile > 0)
		{
			// Losing millisecond of data in conversion from double to long
			mVideoEnd->SetTimeAtTopProfile(mTimeAtTopProfile);
			mVideoEnd->SetTimeToTopProfile(mTimeToTopProfile);
		}
		mVideoEnd->SetTotalDuration(mPlaybackDuration);
		char * videoStatsPtr = mVideoEnd->ToJsonString(nullptr, true);
		if(videoStatsPtr)
		{
			strVideoStatsJson = videoStatsPtr;
			free(videoStatsPtr);
		}
	}
	else
	{
		AAMPLOG_ERR("GetPlaybackStats failed, mVideoEnd is NULL");
	}

	if(!strVideoStatsJson.empty())
	{
		AAMPLOG_INFO("Playback stats json:%s", strVideoStatsJson.c_str());
	}
	else
	{
		AAMPLOG_ERR("Failed to retrieve playback stats (video stats returned as empty from aamp metrics)");
	}
	return strVideoStatsJson;
}

/**
* @brief LoadFogConfig - Load needed player Config to Fog
*/
long PrivateInstanceAAMP::LoadFogConfig()
{
	std::string jsonStr;
	AampJsonObject jsondata;
	double tmpVar = 0;
	int maxdownload = 0;
	std::string tmpStringVar = "";

	// langCodePreference
	jsondata.add("langCodePreference", (int) GetLangCodePreference());

	// networkTimeout value in sec and convert into MS
	tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout);
	jsondata.add("downloadTimeoutMS", (long)CONVERT_SEC_TO_MS(tmpVar));

	// manifestTimeout value in sec and convert into MS
	tmpVar = GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout);
	jsondata.add("manifestTimeoutMS", (long)CONVERT_SEC_TO_MS(tmpVar));

	//downloadStallTimeout in sec
	jsondata.add("downloadStallTimeout", GETCONFIGVALUE_PRIV(eAAMPConfig_CurlStallTimeout));

	//downloadStartTimeout sec
	jsondata.add("downloadStartTimeout", GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadStartTimeout));

	//downloadLowBWTimeout sec, if default value is 0 sec then derived from network timeout
	int timeout = GETCONFIGVALUE_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout);
	if ((0 == timeout) && (AAMP_DEFAULT_SETTING == GETCONFIGOWNER_PRIV(eAAMPConfig_CurlDownloadLowBWTimeout)))
	{
		timeout = GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout) * LOW_BW_TIMEOUT_FACTOR;
		timeout = std::max(DEFAULT_LOW_BW_TIMEOUT, timeout);
	}
	jsondata.add("downloadLowBWTimeout", timeout);

	//maxConcurrentDownloads
	maxdownload = GETCONFIGVALUE_PRIV(eAAMPConfig_FogMaxConcurrentDownloads);
	jsondata.add("maxConcurrentDownloads", (long)(maxdownload));

	//disableEC3
	//jsondata.add("disableEC3", ISCONFIGSET_PRIV(eAAMPConfig_DisableEC3));

	//disableATMOS
	jsondata.add("disableATMOS", ISCONFIGSET_PRIV(eAAMPConfig_DisableATMOS));

	//disableAC4
	jsondata.add("disableAC4", ISCONFIGSET_PRIV(eAAMPConfig_DisableAC4));

	//persistLowNetworkBandwidth
	jsondata.add("persistLowNetworkBandwidth", ISCONFIGSET_PRIV(eAAMPConfig_PersistLowNetworkBandwidth));

	//disableAC3
	jsondata.add("disableAC3", ISCONFIGSET_PRIV(eAAMPConfig_DisableAC3));

	//persistHighNetworkBandwidth
	jsondata.add("persistHighNetworkBandwidth", ISCONFIGSET_PRIV(eAAMPConfig_PersistHighNetworkBandwidth));

	//enableCMCD
	jsondata.add("enableCMCD", ISCONFIGSET_PRIV(eAAMPConfig_EnableCMCD));

	//info
	jsondata.add("info", ISCONFIGSET_PRIV(eAAMPConfig_InfoLogging));

	//tsbInterruptHandling
	jsondata.add("tsbInterruptHandling", ISCONFIGSET_PRIV(eAAMPConfig_InterruptHandling));

	//minBitrate
	jsondata.add("minBitrate", GETCONFIGVALUE_PRIV(eAAMPConfig_MinBitrate));

	//maxBitrate
	jsondata.add("maxBitrate", GETCONFIGVALUE_PRIV(eAAMPConfig_MaxBitrate));

	//enableABR
	jsondata.add("enableABR", ISCONFIGSET_PRIV(eAAMPConfig_EnableABR));

	//LiveOffset
	if (GETCONFIGOWNER_PRIV(eAAMPConfig_LiveOffset) > AAMP_DEFAULT_SETTING )
	{
		jsondata.add("abrMaxBuffer",GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp));
		jsondata.add("abrMinBuffer",GETCONFIGVALUE_PRIV(eAAMPConfig_MinABRNWBufferRampDown));
	}

	//LiveOffset4k
	//tmpLongVar = 0;
	if(GETCONFIGOWNER_PRIV(eAAMPConfig_LiveOffset4K) > AAMP_DEFAULT_SETTING)
	{
		if(mBufferFor4kRampup != 0)
		{
			jsondata.add("abrMaxBuffer4k",mBufferFor4kRampup);
			jsondata.add("abrMinBuffer4k",mBufferFor4kRampdown);
		}
	}

	// Harvest configuration
	jsondata.add("harvestConfig",GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig));

	tmpStringVar = GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestPath);
	jsondata.add("harvestPath",tmpStringVar);

	/*
	 * Audio and subtitle preference
	 * Disabled this for XRE supported TSB linear
	 */
	if (!ISCONFIGSET_PRIV(eAAMPConfig_XRESupportedTune))
	{
		AampJsonObject jsondataForPreference;
		AampJsonObject audioPreference;
		AampJsonObject subtitlePreference;
		bool aPrefAvail = false;
		bool tPrefAvail = false;
		if((preferredLanguagesList.size() > 0) || !preferredRenditionString.empty() || !preferredLabelsString.empty() || !preferredAudioAccessibilityNode.getSchemeId().empty())
		{
			aPrefAvail = true;
			if ((preferredLanguagesList.size() > 0) && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioLanguage) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("languages", preferredLanguagesList);
			}
			if(!preferredRenditionString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioRendition) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("rendition", preferredRenditionString);
			}
			if(!preferredLabelsString.empty() && (GETCONFIGOWNER_PRIV(eAAMPConfig_PreferredAudioLabel) > AAMP_DEFAULT_SETTING ))
			{
				audioPreference.add("label", preferredLabelsString);
			}
			if(!preferredAudioAccessibilityNode.getSchemeId().empty())
			{
				AampJsonObject accessibility;
				std::string schemeId = preferredAudioAccessibilityNode.getSchemeId();
				accessibility.add("schemeId", schemeId);
				std::string value;
				int ival = preferredAudioAccessibilityNode.getIntValue();
				if( ival>=0 )
				{
					value = std::to_string(ival);
				}
				else
				{
					value = preferredAudioAccessibilityNode.getStrValue();
				}
				accessibility.add("value", value);
				audioPreference.add("accessibility", accessibility);
			}
		}
		bool trackAdded = false;
		if(aPrefAvail)
		{
			jsondataForPreference.add("audio", audioPreference);
			trackAdded = true;
		}
		if(tPrefAvail)
		{
			jsondataForPreference.add("text", subtitlePreference);
			trackAdded = true;
		}

		if(trackAdded)
		{
			jsondata.add("trackPreference", jsondataForPreference);
		}
	}

	jsonStr = jsondata.print_UnFormatted();
	AAMPLOG_TRACE("%s", jsonStr.c_str());
	std::string remoteUrl = "127.0.0.1:9080/playerconfig";
	AampCurlDownloader T1;
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader	= true;
	inpData->eRequestType = eCURL_POST;
	inpData->postData	=	jsonStr;
	T1.Initialize(inpData);
	T1.Download(remoteUrl, respData);

	return respData->iHttpRetValue;
}


/**
 * @brief -To Load needed config from player to aampabr
 */
void PrivateInstanceAAMP::LoadAampAbrConfig()
{
	HybridABRManager::AampAbrConfig mhAampAbrConfig;
	// ABR config values
	mhAampAbrConfig.abrCacheLife = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRCacheLife);
	mhAampAbrConfig.abrCacheLength = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRCacheLength);
	mhAampAbrConfig.abrSkipDuration = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRSkipDuration);
	mhAampAbrConfig.abrNwConsistency = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRNWConsistency);
	mhAampAbrConfig.abrThresholdSize = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRThresholdSize);
	mhAampAbrConfig.abrMaxBuffer = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp);
	mhAampAbrConfig.abrMinBuffer = GETCONFIGVALUE_PRIV(eAAMPConfig_MinABRNWBufferRampDown);
	mhAampAbrConfig.abrCacheOutlier = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRCacheOutlier);
	mhAampAbrConfig.abrBufferCounter = GETCONFIGVALUE_PRIV(eAAMPConfig_ABRBufferCounter);

	// Logging level support on aampabr

	mhAampAbrConfig.infologging  = (ISCONFIGSET_PRIV(eAAMPConfig_InfoLogging)  ? 1 :0);
	mhAampAbrConfig.debuglogging = (ISCONFIGSET_PRIV(eAAMPConfig_DebugLogging) ? 1 :0);
	mhAampAbrConfig.tracelogging = (ISCONFIGSET_PRIV(eAAMPConfig_TraceLogging) ? 1:0);
	mhAampAbrConfig.warnlogging  = (ISCONFIGSET_PRIV(eAAMPConfig_WarnLogging) ? 1:0);

	mhAbrManager.ReadPlayerConfig(&mhAampAbrConfig);
}


/**
 * @brief -To Load needed config from player to TSB Handler
 */
void PrivateInstanceAAMP::LoadLocalTSBConfig()
{
	auto tsbLength				=	GETCONFIGVALUE_PRIV (eAAMPConfig_TsbLength);
	auto tsbLocation			=	GETCONFIGVALUE_PRIV(eAAMPConfig_TsbLocation);
	auto tsbMinFreePercentage	=	GETCONFIGVALUE_PRIV(eAAMPConfig_TsbMinDiskFreePercentage);
	auto tsbMaxDiskStorage = GETCONFIGVALUE_PRIV(eAAMPConfig_TsbMaxDiskStorage);

	mTSBSessionManager->SetTsbLength(tsbLength);
	mTSBSessionManager->SetTsbLocation(tsbLocation);
	mTSBSessionManager->SetTsbMinFreePercentage(tsbMinFreePercentage);
	mTSBSessionManager->SetTsbMaxDiskStorage(tsbMaxDiskStorage);
	// Initialize TSB session manager with configuration set
	mTSBSessionManager->Init();
}

/**
 * @brief Create a new TSB Session Manager
 * The new session manager will be created only for DASH linear content.
 * If one already exists it will be destroyed (wiping the content of the TSB) and a new one created.
 */
void PrivateInstanceAAMP::CreateTsbSessionManager()
{
	if ((ContentType_LINEAR == mContentType) && (eMEDIAFORMAT_DASH == mMediaFormat))
	{
		if(mTSBSessionManager)
		{
			AAMPLOG_INFO("Destroying TSB Session Manager %p", mTSBSessionManager);
			SAFE_DELETE(mTSBSessionManager);
		}
		if(IsLocalAAMPTsbFromConfig())
		{
			if (ISCONFIGSET_PRIV(eAAMPConfig_EnablePTSReStamp))
			{
				mTSBSessionManager = new AampTSBSessionManager(this);
				//TODO unique session id for each
				if(mTSBSessionManager)
				{
					LoadLocalTSBConfig();
					if (mTSBSessionManager->IsActive())
					{
						SetIsIframeExtractionEnabled(true);
						AAMPLOG_INFO("TSB Session Manager %p created and active", mTSBSessionManager);
					}
					if(mTSBStore)
					{
						AAMPLOG_INFO("Refreshing the TSB Store session");
						mTSBStore->Flush();
					}
				}
			}
			else
			{
				AAMPLOG_WARN("Local TSB is not enabled due to PTS Restamp is disabled");
			}
		}
	}
}


/**
 * @brief Get License Custom Data
 */
std::string PrivateInstanceAAMP::GetLicenseCustomData()
{
	std::string customData = GETCONFIGVALUE_PRIV(eAAMPConfig_CustomLicenseData);
	return customData;
}

/**
 * @brief check if sidecar data available
 */
bool PrivateInstanceAAMP::HasSidecarData()
{
	if (mData)
	{
		return true;
	}
	return false;
}

void PrivateInstanceAAMP::UpdateUseSinglePipeline()
{
	if (ISCONFIGSET_PRIV(eAAMPConfig_UseSinglePipeline))
	{
		AampStreamSinkManager::GetInstance().SetSinglePipelineMode(this);
	}
	else
	{
		AAMPLOG_INFO("PLAYER[%d] eAAMPConfig_UseSinglePipeline not set", mPlayerId);
	}
}

/**
 *   @brief To update the max DASH DRM sessions supported in AAMP
 */
void PrivateInstanceAAMP::UpdateMaxDRMSessions()
{
	// drm sessions should be updated only when player is idle
	if (mState == eSTATE_IDLE || mState == eSTATE_RELEASED)
	{
		int maxSessions = GETCONFIGVALUE_PRIV(eAAMPConfig_MaxDASHDRMSessions);
		if(mDRMLicenseManager)
		{
			mDRMLicenseManager->UpdateMaxDRMSessions(maxSessions);
		}
		else
		{
			AAMPLOG_ERR("DRM is not supported");
		}
	}
	else
	{
		AAMPLOG_ERR("Discarded DRM session update as player is in state:%d", mState.load());
	}
}

/**
 * @brief Prepare the manifest download configuration.
 *
 * This function prepares the configuration for downloading the manifest file.
 *
 * @return A shared pointer to the ManifestDownloadConfig containing the configuration.
 */
std::shared_ptr<ManifestDownloadConfig> PrivateInstanceAAMP::prepareManifestDownloadConfig()
{
	// initialize the MPD Downloader instance
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> (mPlayerId);
	inpData->mTuneUrl 	= GetManifestUrl();
	if(!mMPDStichRefreshUrl.empty() && ISCONFIGSET_PRIV(eAAMPConfig_MPDStitchingSupport))
	{
		inpData->mStichUrl	= mMPDStichRefreshUrl;
		inpData->mMPDStichOption	=	mMPDStichOption;
	}

	inpData->mDnldConfig->bNeedDownloadMetrics = true;
	// For Manifest file : Keep default starttimeout and lowBwTimeout - 0 ( no wait for first byte).
	// Playlist/Manifest with DAI contents take more time,hence to avoid frequent timeout, its set as 0
	inpData->mDnldConfig->iStallTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_CurlStallTimeout);
	inpData->mDnldConfig->iDownloadTimeout = GETCONFIGVALUE_PRIV(eAAMPConfig_ManifestTimeout);
	inpData->mDnldConfig->iDownloadRetryCount = DEFAULT_DOWNLOAD_RETRY_COUNT;
	inpData->mHarvestConfig				=	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestConfig);
	inpData->mHarvestCountLimit			=	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestCountLimit);
	inpData->mHarvestPathConfigured		=	GETCONFIGVALUE_PRIV(eAAMPConfig_HarvestPath);
	inpData->mDnldConfig->iCurlConnectionTimeout =  GETCONFIGVALUE_PRIV(eAAMPConfig_Curl_ConnectTimeout);
	inpData->mDnldConfig->iDnsCacheTimeOut =   GETCONFIGVALUE_PRIV(eAAMPConfig_Dns_CacheTimeout);

	std::string uriParameter = GETCONFIGVALUE_PRIV(eAAMPConfig_URIParameter);
	// append custom uri parameter with remoteUrl at the end before curl request if curlHeader logging enabled.
	if (ISCONFIGSET_PRIV(eAAMPConfig_CurlHeader) && (!uriParameter.empty()))
	{
		if (inpData->mTuneUrl.find("?") == std::string::npos)
		{
			uriParameter[0] = '?';
		}

		inpData->mTuneUrl.append(uriParameter.c_str());
		//printf ("URL after appending uriParameter :: %s\n", remoteUrl.c_str());
	}

	inpData->mDnldConfig->pCurl = GetCurlInstanceForURL(inpData->mTuneUrl,eCURLINSTANCE_MANIFEST_MAIN);
	inpData->mDnldConfig->userAgentString = GETCONFIGVALUE_PRIV(eAAMPConfig_UserAgent);
	inpData->mDnldConfig->proxyName       = GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkProxy);
	inpData->mDnldConfig->bSSLVerifyPeer = ISCONFIGSET_PRIV(eAAMPConfig_SslVerifyPeer);
	inpData->mDnldConfig->bVerbose	=      ISCONFIGSET_PRIV(eAAMPConfig_CurlLogging);
	inpData->mDnldConfig->bCurlThroughput = ISCONFIGSET_PRIV(eAAMPConfig_CurlThroughput);

	struct curl_slist* headers = GetCustomHeaders(eMEDIATYPE_MANIFEST);
	std::unordered_map<std::string, std::vector<std::string>> sCustomHeaders;

	//To convert the curl_slist* headers to std::unordered_map<std::string, std::vector<std::string>>
	for (struct curl_slist* node = headers; node != nullptr; node = node->next) {
		std::string header = node->data;
		size_t separator_pos = header.find(':');
		if (separator_pos == std::string::npos) {
			continue; // Invalid header format handling
		}
		std::string name = header.substr(0, separator_pos + 1); // include the ":"
		std::string value = header.substr(separator_pos + 1);
		trim(value); // remove leading whitespace

		sCustomHeaders[name].push_back(value);
	}

	inpData->mDnldConfig->sCustomHeaders = sCustomHeaders;
	inpData->mCMCDCollector = mCMCDCollector;
	inpData->mIsLLDConfigEnabled	=	ISCONFIGSET_PRIV(eAAMPConfig_EnableLowLatencyDash);
	if(!mProvidedManifestFile.empty())
	{
		inpData->mPreProcessedManifest = std::move(mProvidedManifestFile);
	}

	curl_slist_free_all(headers);

	return inpData;
}

/**
 * @brief Get video playback quality data
 */
std::string PrivateInstanceAAMP::GetVideoPlaybackQuality()
{
	std::string playbackQualityStr="";
	PlaybackQualityStruct* playbackQuality = nullptr;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if (sink)
	{
		playbackQuality = sink->GetVideoPlaybackQuality();
	}

	if(playbackQuality)
	{
		cJSON *root;
		cJSON *item;
		root = cJSON_CreateArray();
		if(root)
		{
			cJSON_AddItemToArray(root, item = cJSON_CreateObject());
			if(playbackQuality->rendered != -1)
			{
				cJSON_AddNumberToObject(item, "rendered", playbackQuality->rendered);
			}
			if(playbackQuality->dropped != -1)
			{
				cJSON_AddNumberToObject(item, "dropped", playbackQuality->dropped);
			}
		}
		char *jsonStr = cJSON_Print(root);
		if (jsonStr)
		{
			playbackQualityStr.assign(jsonStr);
			free(jsonStr);
		}
		cJSON_Delete(root);
	}
	else
	{
		AAMPLOG_ERR("PrivateInstanceAAMP: playbackQuality not available");
	}
	return playbackQualityStr;
}

/**
 * @brief Get Last downloaded manifest for DASH
 * @return last downloaded manifest data
 */
void PrivateInstanceAAMP::GetLastDownloadedManifest(std::string& manifestBuffer)
{
	/* verify the request only for DASH content */
	if (mMediaFormat == eMEDIAFORMAT_DASH)
	{
		mMPDDownloaderInstance->GetLastDownloadedManifest(manifestBuffer);
	}
}

/*
 * @brief to check gstsubtec flag and vttcueventlistener
 */
bool PrivateInstanceAAMP::IsGstreamerSubsEnabled(void)
{
	return (ISCONFIGSET_PRIV(eAAMPConfig_GstSubtecEnabled) && !WebVTTCueListenersRegistered());
}

/**
 * @brief Signal the clock to subtitle module
 */
bool PrivateInstanceAAMP::SignalSubtitleClock( void )
{
	bool success = false;
	// Sent clock only if subtitle track injection is unblocked. otherwise this instance might be detached/flushed
	if (!mTrackInjectionBlocked[eTRACK_SUBTITLE] && !pipeline_paused)
	{
		if (IsGstreamerSubsEnabled())
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
			if(sink)
			{
				if (sink->SignalSubtitleClock())
				{
					success=true;
				}
				else
				{
					AAMPLOG_TRACE("Failed to update the subtitle clock");
				};
			}
		}
		// TODO: Implement for subtitle parser. It looks like subtitle parser is using position instead of pts
	}
	else
	{
		AAMPLOG_TRACE("Skipped - mTrackInjectionBlocked=%d, pipeline_paused=%d", mTrackInjectionBlocked[eTRACK_SUBTITLE], pipeline_paused);
	}
	return success;
}

long long PrivateInstanceAAMP::GetVideoPTS()
{
	long long pts = mVideoBasePTS;
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(this);
	if(sink)
	{
		long long sinkPTS = sink->GetVideoPTS();
		if (sinkPTS > 0)
		{
			pts += sinkPTS;
		}
	}
	return pts;
}

/**
 * @brief Apply CC/Subtitle mute but preserve the original status
 * This function should be called after acquiring StreamLock
 * This function is used to mute/unmute CC/Subtitle when video is muted/unmuted
 * @param[in] muted true if CC/Subtitle is to be muted, false otherwise
 */
void PrivateInstanceAAMP::CacheAndApplySubtitleMute(bool muted)
{
	bool subtitles_are_logically_muted = subtitles_muted;
	if (muted)
	{	// hiding video plane
		SetCCStatus(false); // hide subtitle plane (along with video)
		subtitles_muted = subtitles_are_logically_muted;
	}
	else
	{	// we are unmuting video; also unmute subtitles if appropriate
		SetCCStatus(!subtitles_are_logically_muted);
	}
}

/**
 *   @brief To release mWaitForDynamicDRMToUpdate condition wait.
 *
 *   @param[in] void
 */
void PrivateInstanceAAMP::ReleaseDynamicDRMToUpdateWait()
{
	std::lock_guard<std::recursive_mutex> guard(mDynamicDrmUpdateLock);
	mWaitForDynamicDRMToUpdate.notify_one();
	AAMPLOG_INFO("Signal sent for mWaitForDynamicDRMToUpdate");

}

void PrivateInstanceAAMP::SetLocalAAMPTsbInjection(bool value)
{
	std::lock_guard<std::recursive_mutex> guard(mLock);
	mLocalAAMPInjectionEnabled = value;
	AAMPLOG_INFO("Local AAMP TSB injection %d", mLocalAAMPInjectionEnabled);
}

bool PrivateInstanceAAMP::IsLocalAAMPTsbInjection()
{
	return mLocalAAMPInjectionEnabled;
}

void PrivateInstanceAAMP::UpdateLocalAAMPTsbInjection()
{
	bool TSBInjectionActive = false;

	if (mpStreamAbstractionAAMP)
	{
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			auto track = mpStreamAbstractionAAMP->GetMediaTrack(static_cast<TrackType>(i));
			if ((nullptr != track) && (track->Enabled()))
			{
				if (track->IsLocalTSBInjection())
				{
					TSBInjectionActive = true;
					break;
				}
			}
		}

		if (!TSBInjectionActive)
		{
			SetLocalAAMPTsbInjection(false);
		}
	}
}

void PrivateInstanceAAMP::IncreaseGSTBufferSize()
{
	int minVideoBufValue = GST_VIDEOBUFFER_SIZE_BYTES; // 3-4Mb for Non-4K, 12-15 Mb for 4K
	int maxVideoBufValue = GST_VIDEOBUFFER_SIZE_MAX_BYTES; // 25 Mb
	float bufferFactor= GETCONFIGVALUE_PRIV(eAAMPConfig_BWToGstBufferFactor);
	BitsPerSecond maxBitrate = mpStreamAbstractionAAMP->GetMaxBitrate();
	int calcVideoBufValue = maxBitrate * bufferFactor;

	if(calcVideoBufValue < minVideoBufValue)
	{
		calcVideoBufValue = minVideoBufValue;
	}
	else if(calcVideoBufValue > maxVideoBufValue)
	{
		calcVideoBufValue = maxVideoBufValue;
	}
	if(calcVideoBufValue > 0 && GETCONFIGVALUE_PRIV(eAAMPConfig_GstVideoBufBytes) != calcVideoBufValue)	// Update only if different
	{
		AAMPLOG_WARN("Max BW (%ld), calculated Buffer Size (%f) changing Buffer size from %d -->> %d",maxBitrate,maxBitrate * bufferFactor,GETCONFIGVALUE_PRIV(eAAMPConfig_GstVideoBufBytes),calcVideoBufValue);
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING, eAAMPConfig_GstVideoBufBytes, calcVideoBufValue);
	}
}

/**
 * @brief Get the TSB Session manager instance
 */
AampTSBSessionManager *PrivateInstanceAAMP::GetTSBSessionManager()
{
	// Return instance only if its active. Disables TSB workflow if not active
	if (mTSBSessionManager && mTSBSessionManager->IsActive())
	{
		return mTSBSessionManager;
	}
	return NULL;
}

std::string PrivateInstanceAAMP::SendManifestPreProcessEvent()
{
	std::string  bRetManifestData;
	std::lock_guard<std::mutex> guard(mPreProcessLock);
	if(!mProvidedManifestFile.empty())
	{
		bRetManifestData = std::move(mProvidedManifestFile);
		mProvidedManifestFile.clear();
	}
	else
	{
		AAMPLOG_WARN("PreProcessed Manifest not available send Need Manifest data event to application");
		SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_NEED_MANIFEST_DATA, GetSessionId()),AAMP_EVENT_ASYNC_MODE);
	}
	return bRetManifestData;
}

void PrivateInstanceAAMP::updateManifest(const char *manifestData)
{
	if(NULL != manifestData)
	{
		std::lock_guard<std::mutex> guard(mPreProcessLock);
		if(!mProvidedManifestFile.empty())
		{
			AAMPLOG_WARN("Previous preprocessed manifest is not read, update with new manifest info");
			mProvidedManifestFile.clear();
		}
		mProvidedManifestFile = manifestData;
	}
}

bool PrivateInstanceAAMP::isDecryptClearSamplesRequired()
{
	// On some platform decrypt is called by the decryptor gstreamer plugin even for clear samples in order to
	// copy it to a secure buffer. However if Rialto is enabled there should be no copy in the aamp pipeline, as
	// it will be done in the server pipeline
	return !ISCONFIGSET_PRIV(eAAMPConfig_useRialtoSink);
}

void PrivateInstanceAAMP::SetLLDashChunkMode(bool enable)
{
	if (ISCONFIGSET_PRIV(eAAMPConfig_EnableChunkInjection))
	{
		mIsChunkMode = enable;
	}
	else
	{
		AAMPLOG_WARN("Chunk mode injection is disabled");
		mIsChunkMode = false;
	}

	AampLLDashServiceData* stLLServiceData = GetLLDashServiceData();
	if(enable)
	{
		mMPDDownloaderInstance->SetNetworkTimeout(MANIFEST_TIMEOUT_FOR_LLD);
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_ManifestTimeout,MANIFEST_TIMEOUT_FOR_LLD);
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MinABRNWBufferRampDown,AAMP_LOW_BUFFER_BEFORE_RAMPDOWN_FOR_LLD);
		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_MaxABRNWBufferRampUp,AAMP_HIGH_BUFFER_BEFORE_RAMPUP_FOR_LLD);

		SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_NetworkTimeout,TIMEOUT_FOR_LLD); /* Use 3sec for fragment download timout for LLD */
		mNetworkTimeoutMs  = (uint32_t) CONVERT_SEC_TO_MS(GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout));
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			SetCurlTimeout(mNetworkTimeoutMs, (AampCurlInstance)i);
		}
		AAMPLOG_INFO("Updated NetworkTimeout %d for Chunked Mode", mNetworkTimeoutMs);

		if(stLLServiceData != NULL)
		{
			int timeout = ceil(stLLServiceData->fragmentDuration); // workaround: round up 1.92s(float) to 2(int)
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_CurlDownloadStartTimeout,timeout);
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_CurlStallTimeout,timeout);
			SETCONFIGVALUE_PRIV(AAMP_TUNE_SETTING,eAAMPConfig_CurlDownloadLowBWTimeout,timeout);
		}
		else
		{
			AAMPLOG_WARN("LLD Service data is NULL, not updating CURL timeouts "); // should not go here ideally
		}
		AAMPLOG_INFO("ChunkMode enabled");
	}
	else
	{
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_MinABRNWBufferRampDown); // restore only if current owner is AAMP_TUNE_SETTING
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_MaxABRNWBufferRampUp);
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_CurlDownloadStartTimeout);
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_CurlStallTimeout);
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_CurlDownloadLowBWTimeout);
		mConfig->RestoreConfiguration(AAMP_TUNE_SETTING, eAAMPConfig_NetworkTimeout);

		mNetworkTimeoutMs  = (uint32_t) CONVERT_SEC_TO_MS(GETCONFIGVALUE_PRIV(eAAMPConfig_NetworkTimeout));
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			SetCurlTimeout(mNetworkTimeoutMs, (AampCurlInstance)i);
		}
		AAMPLOG_INFO("Updated NetworkTimeout %d for Non Chunked", mNetworkTimeoutMs);
		AAMPLOG_INFO("ChunkMode disabled");
	}

	if(mpStreamAbstractionAAMP)
	{
		mpStreamAbstractionAAMP->SetABRMinBuffer(GETCONFIGVALUE_PRIV(eAAMPConfig_MinABRNWBufferRampDown));
		mpStreamAbstractionAAMP->SetABRMaxBuffer(GETCONFIGVALUE_PRIV(eAAMPConfig_MaxABRNWBufferRampUp));
		LoadAampAbrConfig();
	}
}

bool PrivateInstanceAAMP::GetLLDashChunkMode()
{
	return mIsChunkMode;
}

bool PrivateInstanceAAMP::GetLLDashAdjustSpeed(void)
{
	return bLLDashAdjustPlayerSpeed;
}

double PrivateInstanceAAMP::GetLLDashCurrentPlayBackRate(void)
{
	return mLLDashCurrentPlayRate;
}


/**
 * @fn getStringForPlaybackError
 * @brief Retrieves a human-readable error string for a given playback error type.
 *
 * @param[in] errorType - Errortype of PlaybackErrorType enum.
 * @return A constant character pointer to the error string corresponding to the provided error type.
 */
const char* PrivateInstanceAAMP::getStringForPlaybackError(PlaybackErrorType errorType)
{
	switch (errorType)
	{
		case eGST_ERROR_PTS:
			return "PTS ERROR";
		case eGST_ERROR_UNDERFLOW:
			return "Underflow";
		case eSTALL_AFTER_DISCONTINUITY:
			return "Stall After Discontinuity";
		case eDASH_LOW_LATENCY_MAX_CORRECTION_REACHED:
			return "LL DASH Max Correction Reached";
		case eDASH_LOW_LATENCY_INPUT_PROTECTION_ERROR:
			return "LL DASH Input Protection Error";
		case eDASH_RECONFIGURE_FOR_ENC_PERIOD:
			return "Encrypted period found";
		case eGST_ERROR_GST_PIPELINE_INTERNAL:
			return "GstPipeline Internal Error";
		default:
			return "STARTTIME RESET";
	}
}

/**
 *	@brief Calculates the trick mode EOS position
 *	This function only works for (rate > 1)
 */
void PrivateInstanceAAMP::CalculateTrickModePositionEOS(void)
{
	if (rate > AAMP_NORMAL_PLAY_RATE)
	{
		double positionNow = GetPositionSeconds();
		double livePlayPositionNow = GetLivePlayPosition();
		mTrickModePositionEOS = livePlayPositionNow + (livePlayPositionNow - positionNow)/(rate - 1);
		AAMPLOG_INFO("positionNow %lfs livePlayPositionNow %lfs rate %fs mTrickModePositionEOS %lfs", positionNow, livePlayPositionNow, rate, mTrickModePositionEOS);
	}
}

/**
 * @fn GetLivePlayPosition
 *
 * @brief Get current live play stream position.
 * This is the live edge of the stream minus a configurable offset.
 *
 * @retval current live play position of the stream in seconds.
 */
double PrivateInstanceAAMP::GetLivePlayPosition(void)
{
	return (NOW_STEADY_TS_SECS_FP - mLiveEdgeDeltaFromCurrentTime - mLiveOffset);
}

/**
 *    @brief To increment gaps between periods for dash
 *    return none
 */
void PrivateInstanceAAMP::IncrementGaps()
{
	if(mVideoEnd)
	{
		mVideoEnd->IncrementGaps();
	}
}

/**
 * @fn GetStreamPositionMs
 *
 * @return double, current position in the stream
 */
double PrivateInstanceAAMP::GetStreamPositionMs()
{
	double pos = (double)GetPositionMilliseconds();
	if (mProgressReportOffset >= 0)
	{
		pos -= (mProgressReportOffset * 1000);
	}
	return pos;
}

/**
 * @brief Send MonitorAvEvent
 * @param[in] status - Current MonitorAV status
 * @param[in] videoPositionMS - video position in milliseconds
 * @param[in] audioPositionMS - audio position in milliseconds
 * @param[in] timeInStateMS - time in state in milliseconds
 * @param[in] droppedFrames - dropped frames count
 * @details This function sends a MonitorAVStatusEvent to the event manager.
 * It is used to monitor the audio and video status during playback.
 * It is called when the playback is enabled (mbPlayEnabled is true).
 */
void PrivateInstanceAAMP::SendMonitorAvEvent(const std::string &status, int64_t videoPositionMS, int64_t audioPositionMS, uint64_t timeInStateMS, uint64_t droppedFrames)
{
	if(mbPlayEnabled)
	{
		MonitorAVStatusEventPtr evt = std::make_shared<MonitorAVStatusEvent>(status, videoPositionMS, audioPositionMS, timeInStateMS, GetSessionId(), droppedFrames);
		mEventManager->SendEvent(evt, AAMP_EVENT_SYNC_MODE);
	}
}
/**
 * @fn GetFormatPositionOffsetInMSecs
 * @brief API to get the offset value in msecs for the position values to be reported.
 * @return Offset value in msecs
 */
double PrivateInstanceAAMP::GetFormatPositionOffsetInMSecs()
{
	double offset = 0;
	if ((!ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) || !IsLiveStream()) && mProgressReportOffset > 0)
	{
		// Adjust progress positions for VOD, Linear without absolute timeline
		offset = mProgressReportOffset * 1000;
	}
	else if(ISCONFIGSET_PRIV(eAAMPConfig_UseAbsoluteTimeline) &&
		mProgressReportOffset > 0 && IsLiveStream() &&
		eABSOLUTE_PROGRESS_WITHOUT_AVAILABILITY_START == GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAbsoluteProgressReporting))
	{
		// Adjust progress positions for linear stream with absolute timeline config from AST
		offset = mProgressReportAvailabilityOffset * 1000;
	}
	return offset;
}

/**
 *   @brief Get output format of stream.
 *
 *   @param[out]  primaryOutputFormat - format of primary track
 *   @param[out]  audioOutputFormat - format of audio track
 *   @param[out]  auxAudioOutputFormat - format of aux audio track
 *   @param[out]  subtitleOutputFormat - format of subtitle  track
 *   @return void
 */
void PrivateInstanceAAMP::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
	mpStreamAbstractionAAMP->GetStreamFormat(primaryOutputFormat, audioOutputFormat, auxAudioOutputFormat, subtitleOutputFormat);

	// Limiting the change to just Rialto, until the change has been tested on non-Rialto
	if (ISCONFIGSET_PRIV(eAAMPConfig_useRialtoSink) &&
	    IsLocalAAMPTsbInjection() &&
		(rate != AAMP_NORMAL_PLAY_RATE))
	{
		audioOutputFormat = FORMAT_INVALID;
		auxAudioOutputFormat = FORMAT_INVALID;
		subtitleOutputFormat = FORMAT_INVALID;
		AAMPLOG_TRACE("aamp->rate %f videoFormat %d audioFormat %d auxFormat %d subFormat %d", rate, primaryOutputFormat, audioOutputFormat, auxAudioOutputFormat, subtitleOutputFormat);
	}
}
