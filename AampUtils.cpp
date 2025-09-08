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
 * @file AampUtils.cpp
 * @brief Common utility functions
 */

#include "AampUtils.h"
#include "_base64.h"
#include "AampConfig.h"
#include "AampConstants.h"
#include "AampCurlStore.h"
#include "AampCurlDownloader.h"
#include "isobmff/isobmffbuffer.h"
#include "scte35/AampSCTE35.h"

#include <sys/time.h>
#include <string.h>
#include <assert.h>
#include <ctime>
#include <cctype>
#include <curl/curl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <algorithm>
#include <inttypes.h>

#define DEFER_DRM_LIC_OFFSET_FROM_START 5
#define DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND 5
#define MAX_THREAD_NAME_LENGTH 16 // Linux is least common denominator, with up to 15 characters + null terminator

/*
 * Variable initialization for various audio formats
 * There are no registered codecs parameters for mp1, mp2, mp3, mpg, mpeg
 */
static const FormatMap mAudioFormatMap[] =
{
	{ "mp4a.40.2", FORMAT_AUDIO_ES_AAC },
	{ "mp4a.40.5", FORMAT_AUDIO_ES_AAC },
	{ "ac-3", FORMAT_AUDIO_ES_AC3 },
	{ "mp4a.a5", FORMAT_AUDIO_ES_AC3 },
	{ "ac-4.02.01.01", FORMAT_AUDIO_ES_AC4 },
	{ "ac-4.02.01.02", FORMAT_AUDIO_ES_AC4 },
	{ "ec-3", FORMAT_AUDIO_ES_EC3 },
	{ "ec+3", FORMAT_AUDIO_ES_ATMOS },
	{ "eac3", FORMAT_AUDIO_ES_EC3 }
};
#define AAMP_AUDIO_FORMAT_MAP_LEN ARRAY_SIZE(mAudioFormatMap)

/*
 * Variable initialization for various video formats
 */
static const FormatMap mVideoFormatMap[] =
{
	{ "avc1.", FORMAT_VIDEO_ES_H264 },
	{ "hvc1.", FORMAT_VIDEO_ES_HEVC },
	{ "hev1.", FORMAT_VIDEO_ES_HEVC },
	{ "mpeg2v", FORMAT_VIDEO_ES_MPEG2 }//For testing.
};
#define AAMP_VIDEO_FORMAT_MAP_LEN ARRAY_SIZE(mVideoFormatMap)

/**
 * @brief Get current time from epoch is milliseconds
 *
 * @retval - current time in milliseconds
 */
long long aamp_GetCurrentTimeMS(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}

/**
 * @brief Get harvest path to dump the files
 */
void getDefaultHarvestPath(std::string &value)
{
	value = aamp_GetConfigPath("/opt/aamp");
}

/**
 * @brief parse leading protocol from uri if present
 * @param[in] uri manifest/ fragment uri
 * @retval return pointer just past protocol (i.e. http://) if present (or) return NULL uri doesn't start with protcol
 */
static const char * ParseUriProtocol(const char *uri)
{
	if(NULL == uri)
	{
		AAMPLOG_ERR("Empty URI");
		return NULL;
	}
	for(;;)
	{
		char ch = *uri++;
		if( ch ==':' )
		{
			if (uri[0] == '/' && uri[1] == '/')
			{
				return uri + 2;
			}
			break;
		}
		else if (isalnum (ch) || ch == '.' || ch == '-' || ch == '+') // other valid (if unlikely) characters for protocol
		{ // legal characters for uri protocol - continue
			continue;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

/**
 * @brief Resolve file URL from the base and file path
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams)
{
	if( ParseUriProtocol(uri) )
	{
		dst = uri;
	}
	else
	{
		if(base.empty())
		{
			AAMPLOG_WARN("Empty base");
			return;
		}
		const char *baseStart = base.c_str();
		const char *basePtr = ParseUriProtocol(baseStart);
		const char *baseEnd;
		for(;;)
		{
			char c = *basePtr;
			if( c==0 || c=='/' || c=='?' )
			{
				baseEnd = basePtr;
				break;
			}
			basePtr++;
		}

		if( uri[0]!='/' && uri[0]!='\0' )
		{
			for(;;)
			{
				char c = *basePtr;
				if( c=='/' )
				{
					baseEnd = basePtr;
				}
				else if( c=='?' || c==0 )
				{
					break;
				}
				basePtr++;
			}
		}
		dst = base.substr(0,baseEnd-baseStart);
		if( uri[0]!='/' )
		{
			dst += "/";
		}
		dst += uri;
		if( bPropagateUriParams )
		{
			if (strchr(uri,'?') == 0)
			{ // uri doesn't have url parameters; copy from parents if present
				const char *baseParams = strchr(basePtr,'?');
				if( baseParams )
				{
					std::string params = base.substr(baseParams-baseStart);
					dst.append(params);
				}
			}
		}
	}
}

/**
 * @brief distinguish between absolute and relative urls
 *
 * @return true iff url starts with http:// or https://
 */
bool aamp_IsAbsoluteURL( const std::string &url )
{
	return url.compare(0, 7, "http://")==0 || url.compare(0, 8, "https://")==0;
	// note: above slightly faster than equivalent url.rfind("http://",0)==0 || url.rfind("https://",0)==0;
}

/**
 * @brief Extract host string from url
 *
 * @retval host of input url
 */
std::string aamp_getHostFromURL(std::string url)
{
	std::string host = "";
	std::size_t start_pos = std::string::npos;
	if(url.rfind("http://", 0) == 0)
	{ // starts with http://
		start_pos = 7;
	}
	else if(url.rfind("https://", 0) == 0)
	{ // starts with https://
		start_pos = 8;
	}
	if(start_pos != std::string::npos)
	{
		std::size_t pos = url.find('/', start_pos);
		if(pos != std::string::npos)
		{
			host = url.substr(start_pos, (pos - start_pos));
		}
	}
	return host;
}

/**
 * @brief check is local or not from given hostname
 *
 * @retval true if localhost, false otherwise.
 */
bool aamp_IsLocalHost ( std::string Hostname )
{
	bool isLocalHost = false;
	if( std::string::npos != Hostname.find("127.0.0.1") || \
		std::string::npos != Hostname.find("localhost") )
	{
		isLocalHost = true;
	}

	return isLocalHost;
}

/**
 * @brief Check if string start with a prefix
 *
 * @retval TRUE if substring is found in bigstring
 */
bool aamp_StartsWith( const char *inputStr, const char *prefix )
{
	bool rc = true;
	while( *prefix )
	{
		if( *inputStr++ != *prefix++ )
		{
			rc = false;
			break;
		}
	}
	return rc;
}

/**
 * @brief convert blob of binary data to ascii base64-URL-encoded equivalent
 * @retval pointer to malloc'd cstring containing base64 URL encoded version of string
 * @retval NULL if insufficient memory to allocate base64-URL-encoded copy
 * @note caller responsible for freeing returned cstring
 */
char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len)
{
	char *rc = base64_Encode(src,len);
	if( rc )
	{
		char *dst = rc;
		while( *dst )
		{
			switch( *dst )
			{
				case '+':
					*dst = '-';
					break;
				case '/':
					*dst = '_';
					break;
				case '=':
					*dst = '\0';
					break;
				default:
					break;
			}
			dst++;
		}
	}
	return rc;
}

/**
 * @brief decode base64 URL encoded data to binary equivalent
 * @retval pointer to malloc'd memory containing decoded binary data
 * @retval NULL if insufficient memory to allocate decoded data
 * @note caller responsible for freeing returned data
 */
unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
{
	unsigned char * rc = NULL;
	char *temp = (char *)malloc(srcLen+3);
	if( temp )
	{
		temp[srcLen+2] = '\0';
		temp[srcLen+1] = '=';
		temp[srcLen+0] = '=';
		for( int iter = 0; iter < srcLen; iter++ )
		{
			char c = src[iter];
			switch( c )
			{
				case '_':
					c = '/';
					break;
				case '-':
					c = '+';
					break;
				default:
					break;
			}
			temp[iter] = c;
		}
		rc = base64_Decode(temp, len );
		free(temp);
	}
	else
	{
		*len = 0;
	}
	return rc;
}

/**
 * @brief unescape uri-encoded uri parameter
 */
void aamp_DecodeUrlParameter( std::string &uriParam )
{
	std::string rc;
	CURL *curl = curl_easy_init();
	if (curl != NULL)
	{
		int unescapedLen;
		const char* unescapedData = curl_easy_unescape(curl, uriParam.c_str(), (int)uriParam.size(), &unescapedLen);
		if (unescapedData != NULL)
		{
			uriParam = std::string(unescapedData, unescapedLen);
			curl_free((void*)unescapedData);
		}
		curl_easy_cleanup(curl);
	}
}

/**
 * @brief Parse date time from ISO8601 string and return value in seconds
 * @retval durationMs duration in milliseconds
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr)
{
	double timeSeconds = 0;
	if(ptr)
	{
		std::tm timeObj = { 0 };
		//Find out offset from utc by converting epoch
		std::tm baseTimeObj = { 0 };
		strptime("1970-01-01T00:00:00.", "%Y-%m-%dT%H:%M:%S.", &baseTimeObj);
		time_t offsetFromUTC = timegm(&baseTimeObj);
		//Convert input string to time
		const char *msString = strptime(ptr, "%Y-%m-%dT%H:%M:%S.", &timeObj);
		timeSeconds = timegm(&timeObj) - offsetFromUTC;
		
		if( msString && *msString )
		{ // at least one character following decimal point
			double ms = atof(msString-1); // back up and parse as float
			timeSeconds += ms; // include ms granularity
		}
	}
	return timeSeconds;
}

/**
 * @brief aamp_PostJsonRPC posts JSONRPC data
 */
std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params )
{
	std::string remoteUrl = "http://127.0.0.1:9998/jsonrpc";
	AampCurlDownloader T1;
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader	= true;
	inpData->eRequestType = eCURL_POST;
	inpData->postData	=	"{\"jsonrpc\":\"2.0\",\"id\":"+id+",\"method\":\""+method+"\",\"params\":"+params+"}";
	inpData->sCustomHeaders["Content-Type:"] = std::vector<std::string> {"application/json"};
	T1.Initialize(inpData);
	T1.Download(remoteUrl, respData);
	
	std::string response;
	if( respData->curlRetValue == CURLE_OK )
	{
		AAMPLOG_WARN("JSONRPC data: %s", inpData->postData.c_str() );
		AAMPLOG_WARN("HTTP %d", respData->iHttpRetValue);
		response =  std::string( respData->mDownloadData.begin(), respData->mDownloadData.end());
	}
	else
	{
		AAMPLOG_ERR("failed: %d", respData->curlRetValue);
	}

	return response;
	
}

/**
 * @brief Get time to defer DRM acquisition
 *
 * @return Time in MS to defer DRM acquisition
 */
int aamp_GetDeferTimeMs(long maxTimeSeconds)
{
	int ret = 0;
	ret = (DEFER_DRM_LIC_OFFSET_FROM_START + rand()%(maxTimeSeconds - DEFER_DRM_LIC_OFFSET_FROM_START - DEFER_DRM_LIC_OFFSET_TO_UPPER_BOUND))*1000;
	AAMPLOG_WARN("Added time for deferred license acquisition  %d ", (int)ret);
	return ret;
}

/**
 * @brief Get DRM system from ID
 * @retval drmSystem drm system
 */
DRMSystems GetDrmSystem(std::string drmSystemID)
{
	if(drmSystemID == WIDEVINE_UUID)
	{
		return eDRM_WideVine;
	}
	else if(drmSystemID == PLAYREADY_UUID)
	{
		return eDRM_PlayReady;
	}
	else if(drmSystemID == CLEARKEY_UUID)
	{
		return eDRM_ClearKey;
	}
	else
	{
		return eDRM_NONE;
	}
}

/**
 * @brief Get name of DRM system
 * @retval Name of the DRM system, empty string if not supported
 */
const char * GetDrmSystemName(DRMSystems drmSystem)
{
	switch(drmSystem)
	{
		case eDRM_WideVine:
			return "Widevine";
		case eDRM_PlayReady:
			return "PlayReady";
		// Deprecated
		case eDRM_CONSEC_agnostic:
			return "Consec Agnostic";
		// Deprecated and removed Adobe Access and Vanilla AES
		case eDRM_NONE:
		case eDRM_ClearKey:
		case eDRM_MAX_DRMSystems:
		default:
			return "";
	}
}

/**
 * @brief Get ID of DRM system
 * @retval ID of the DRM system, empty string if not supported
 */
const char * GetDrmSystemID(DRMSystems drmSystem)
{
	if(drmSystem == eDRM_WideVine)
	{
		return WIDEVINE_UUID;
	}
	else if(drmSystem == eDRM_PlayReady)
	{
		return PLAYREADY_UUID;
	}
	else if (drmSystem == eDRM_ClearKey)
	{
		return CLEARKEY_UUID;
	}
	else if(drmSystem == eDRM_CONSEC_agnostic)
	{
		return CONSEC_AGNOSTIC_UUID;
	}
	else
	{
		return "";
	}
}

/**
 * @brief Encode URL
 *
 * @return Encoding status
 */
void UrlEncode(std::string inStr, std::string &outStr)
{
	outStr.clear();
	const char *src = inStr.c_str();
	const char *hex = "0123456789ABCDEF";
	for(;;)
	{
		char ch = *src++;
		if( !ch )
		{
			break;
		}
		if (isalnum (ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			outStr.push_back( ch );
		}
		else
		{
			outStr.push_back( '%' );
			outStr.push_back( hex[ch >> 4] );
			outStr.push_back( hex[ch & 0x0F] );
		}
	}
}

/**
 * @brief Trim a string
 */
void trim(std::string& src)
{
	size_t first = src.find_first_not_of(" \n\r\t\f\v");
	if (first != std::string::npos)
	{
		size_t last = src.find_last_not_of(" \n\r\t\f\v");
		std::string dst = src.substr(first, (last - first + 1));
		src = dst;
	}
}

/**
 * @brief To get the preferred iso639mapped language code
 * @retval[out] preferred iso639 mapped language.
 */
std::string Getiso639map_NormalizeLanguageCode( const std::string lang, LangCodePreference preferLangFormat )
{
	std::string rc;
	if (preferLangFormat == ISO639_NO_LANGCODE_PREFERENCE)
	{
		rc = std::move(lang);
	}
	else
	{
		char lang2[3+1]; // max 3 characters, i.e. 'eng' with cstring NUL terminator
		strncpy(lang2, lang.c_str(), sizeof(lang2) );
		lang2[sizeof(lang2)-1]=0x00; // ensure NUL termination (not guaranteed by strncpy)
		iso639map_NormalizeLanguageCode(lang2, preferLangFormat); // modifies lang2
		rc = lang2;
	}
	return rc;
}

/**
 * @brief To get the timespec
 * @retval[out] timespec.
 */
struct timespec aamp_GetTimespec(int timeInMs)
{
	struct timespec tspec;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        tspec.tv_sec = tv.tv_sec + timeInMs / 1000;
        tspec.tv_nsec = (long)(tv.tv_usec * 1000 + 1000 * 1000 * (timeInMs % 1000));
        tspec.tv_sec += tspec.tv_nsec / (1000 * 1000 * 1000);
        tspec.tv_nsec %= (1000 * 1000 * 1000);

	return tspec;
}

/** 
 * @enum HarvestConfigType
 * @brief Harvest Configuration type 
 */
enum HarvestConfigType
{
	eHARVEST_DISABLE_DEFAULT = 0x00000000,            /**< Disable harvesting for unknown type */
	eHARVEST_ENABLE_VIDEO = 0x00000001,              /**< Enable Harvest Video fragments - set 1st bit*/
	eHARVEST_ENABLE_AUDIO = 0x00000002,              /**< Enable Harvest audio - set 2nd bit*/
	eHARVEST_ENABLE_SUBTITLE = 0x00000004,           /**< Enable Harvest subtitle - set 3rd bit */
	eHARVEST_ENABLE_AUX_AUDIO = 0x00000008,          /**< Enable Harvest auxiliary audio - set 4th bit*/
	eHARVEST_ENABLE_MANIFEST = 0x00000010,           /**< Enable Harvest manifest - set 5th bit */
	eHARVEST_ENABLE_LICENCE = 0x00000020,            /**< Enable Harvest license - set 6th bit  */
	eHARVEST_ENABLE_IFRAME = 0x00000040,             /**< Enable Harvest iframe - set 7th bit  */
	eHARVEST_ENABLE_INIT_VIDEO = 0x00000080,         /**< Enable Harvest video init fragment - set 8th bit*/
	eHARVEST_ENABLE_INIT_AUDIO = 0x00000100,         /**< Enable Harvest audio init fragment - set 9th bit*/
	eHARVEST_ENABLE_INIT_SUBTITLE = 0x00000200,      /**< Enable Harvest subtitle init fragment - set 10th bit*/
	eHARVEST_ENABLE_INIT_AUX_AUDIO = 0x00000400,     /**< Enable Harvest auxiliary audio init fragment - set 11th bit*/
	eHARVEST_ENABLE_PLAYLIST_VIDEO = 0x00000800,     /**< Enable Harvest video playlist - set 12th bit*/
	eHARVEST_ENABLE_PLAYLIST_AUDIO = 0x00001000,     /**< Enable Harvest audio playlist - set 13th bit*/
	eHARVEST_ENABLE_PLAYLIST_SUBTITLE = 0x00002000,  /**< Enable Harvest subtitle playlist - set 14th bit*/
	eHARVEST_ENABLE_PLAYLIST_AUX_AUDIO = 0x00004000, /**< Enable Harvest auxiliary audio playlist - set 15th bit*/
	eHARVEST_ENABLE_PLAYLIST_IFRAME = 0x00008000,    /**< Enable Harvest Iframe playlist - set 16th bit*/
	eHARVEST_ENABLE_INIT_IFRAME = 0x00010000,        /**< Enable Harvest IFRAME init fragment - set 17th bit*/
	eHARVEST_ENABLE_DSM_CC = 0x00020000,             /**< Enable Harvest digital storage media command and control (DSM-CC)- set 18th bit */
	eHARVEST_ENABLE_DEFAULT = 0xFFFFFFFF             /**< Harvest unknown - Enable all by default */
};

/**
 * @brief Inline function to create directory
 * @param dirpath - path name
 */
static inline void createdir(const char *dirpath)
{
	DIR *d = opendir(dirpath);
	if (!d)
	{
		if(mkdir(dirpath, 0777) == -1)
		{
			
			AAMPLOG_ERR("Error :  %s",strerror(errno));
		}
	}
	else
	{
		closedir(d);
	}
}

/**
 * @brief Get harvest config corresponds to Media type
 * @return harvestType
 */
int getHarvestConfigForMedia(AampMediaType mediaType)
{
	enum HarvestConfigType harvestType = eHARVEST_ENABLE_DEFAULT;
	switch(mediaType)
	{
		case eMEDIATYPE_VIDEO:
			harvestType = eHARVEST_ENABLE_VIDEO;
			break; 

		case eMEDIATYPE_INIT_VIDEO:
			harvestType = eHARVEST_ENABLE_INIT_VIDEO;
			break;

		case eMEDIATYPE_AUDIO:
			harvestType = eHARVEST_ENABLE_AUDIO;
			break; 
		
		case eMEDIATYPE_INIT_AUDIO:
			harvestType = eHARVEST_ENABLE_INIT_AUDIO;
			break; 
		
		case eMEDIATYPE_SUBTITLE:
			harvestType = eHARVEST_ENABLE_SUBTITLE;
			break; 

		case eMEDIATYPE_INIT_SUBTITLE:
			harvestType = eHARVEST_ENABLE_INIT_SUBTITLE;
			break; 

		case eMEDIATYPE_MANIFEST:
			harvestType = eHARVEST_ENABLE_MANIFEST;
			break; 

		case eMEDIATYPE_LICENCE:
			harvestType = eHARVEST_ENABLE_LICENCE;
			break; 

		case eMEDIATYPE_IFRAME:
			harvestType = eHARVEST_ENABLE_IFRAME;
			break; 
		
		case eMEDIATYPE_INIT_IFRAME:
			harvestType = eHARVEST_ENABLE_INIT_IFRAME;
			break;

		case eMEDIATYPE_PLAYLIST_VIDEO:
			harvestType = eHARVEST_ENABLE_PLAYLIST_VIDEO;
			break; 

		case eMEDIATYPE_PLAYLIST_AUDIO:
			harvestType = eHARVEST_ENABLE_PLAYLIST_AUDIO;
			break; 

		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			harvestType = eHARVEST_ENABLE_PLAYLIST_SUBTITLE;
			break; 
		
		case eMEDIATYPE_PLAYLIST_IFRAME:
			harvestType = eHARVEST_ENABLE_PLAYLIST_IFRAME;
			break;  

		case eMEDIATYPE_DSM_CC: 
			harvestType = eHARVEST_ENABLE_DSM_CC;
			break; 

		default:
			harvestType = eHARVEST_DISABLE_DEFAULT;
			break; 
	}
	return (int)harvestType;
}

/**
 * @brief Write - file to storage
 */
bool aamp_WriteFile(std::string fileName, const char* data, size_t len, AampMediaType mediaType, unsigned int count,const char *prefix)
{
	bool retVal=false;	
	{
		//check if query parameter(s) present if yes then remove it.This is creating problem for file / folder path 
		std::size_t queryParamStartPos = fileName.find_first_of('?');
		if( queryParamStartPos != std::string::npos )
		{
			fileName = fileName.substr(0,queryParamStartPos);
		}

		std::size_t pos = fileName.find("://");
		if( pos != std::string::npos )
		{
			fileName = fileName.substr(pos+3); // strip off leading http://
		
			/* Avoid chance of overwriting , in case of manifest and playlist, name will be always same */
			if(mediaType == eMEDIATYPE_PLAYLIST_AUDIO
			|| mediaType == eMEDIATYPE_PLAYLIST_IFRAME || mediaType == eMEDIATYPE_PLAYLIST_SUBTITLE || mediaType == eMEDIATYPE_PLAYLIST_VIDEO )
			{ // add suffix to give unique name for each downloaded playlist
				fileName = fileName + "." + std::to_string(count);
			}
			else if(mediaType == eMEDIATYPE_MANIFEST)
			{
				std::size_t manifestPos = fileName.find_last_of('/');
				std::size_t extPos = fileName.find_last_of('.');
				if(manifestPos == std::string::npos || extPos == std::string::npos)
				{
					return retVal;
				}
				std::string ext = fileName.substr(extPos);
				fileName = fileName.substr(0,manifestPos+1); 
				fileName = fileName + "manifest" + ext +  "." + std::to_string(count);
			}
			
			// create subdirectories lazily as needed, preserving CDN folder structure
			std::string dirpath = std::string(prefix);
			const char *subdir = fileName.c_str();
			for(;;)
			{
				createdir(dirpath.c_str() );
				dirpath += '/';
				const char *delim = strchr(subdir,'/');
				if( delim )
				{
					dirpath += std::string(subdir,delim-subdir);
					subdir = delim+1;
				}
				else
				{
					dirpath += std::string(subdir);
					break;
				}
			}
			std::ofstream f(dirpath, std::ofstream::binary);
			if (f.good())
			{
				f.write(data, len);
				f.close();
				retVal = true;
			}
			else
			{
				AAMPLOG_ERR("File open failed. outfile = %s ", (dirpath + fileName).c_str());
			}
		}
	}
	return retVal;
}

/**
 * @brief Get compatible trickplay for 6s cadence of iframe track from the given rates
 */
float getWorkingTrickplayRate(float rate)
{
	float workingRate;
	switch ((int)rate){
		case 4:
			workingRate = 25;
			break;
		case 16:
			workingRate = 32;
			break;
		case 32:
			workingRate = 48;
			break;
		case -4:
			workingRate = -25;
			break;
		case -16:
			workingRate = -32;
			break;
		case -32:
			workingRate = -48;
			break;
		default:
			workingRate = rate;
	}
	return workingRate;
}

/**
 * @brief Get reverse map the working rates to the rates given by platform player
 */
float getPseudoTrickplayRate(float rate)
{
	float psudoRate;
	switch ((int)rate){
		case 25:
			psudoRate = 4;
			break;
		case 32:
			psudoRate = 16;
			break;
		case 48:
			psudoRate = 32;
			break;
		case -25:
			psudoRate = -4;
			break;
		case -32:
			psudoRate = -16;
			break;
		case -48:
			psudoRate = -32;
			break;
		default:
			psudoRate = rate;
	}
	return psudoRate;
}

/**
 * @brief Convert string of chars to its representative string of hex numbers
 */
void stream2hex(const std::string str, std::string& hexstr, bool capital)
{
	hexstr.resize(str.size() * 2);
	const size_t a = capital ? 'A' - 1 : 'a' - 1;

	for (size_t i = 0, c = str[0] & 0xFF; i < hexstr.size(); c = str[i / 2] & 0xFF)
	{
		hexstr[i++] = c > 0x9F ? (c / 16 - 9) | a : c / 16 | '0';
		hexstr[i++] = (c & 0xF) > 9 ? (c % 16 - 9) | a : c % 16 | '0';
	}
}

/**
 * @brief Sleep for given milliseconds
 */
void mssleep(int milliseconds)
{
	struct timespec req, rem;
	if (milliseconds > 0)
	{
		req.tv_sec = milliseconds / 1000;
		req.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&req, &rem);
	}
}

/*
* @fn GetAudioFormatStringForCodec
* @brief Function to get audio codec string from the map.
*
* @param[in] input Audio codec type
* @return Audio codec string
*/
const char * GetAudioFormatStringForCodec ( StreamOutputFormat input)
{
	const char *codec = "UNKNOWN";
	if(input < FORMAT_UNKNOWN)
	{
		for( int i=0; i<AAMP_AUDIO_FORMAT_MAP_LEN; i++ )
		{
			if(mAudioFormatMap[i].format == input )
			{
				codec =  mAudioFormatMap[i].codec;
				break;
			}
		}
	}
	return codec;
}

/*
* @fn GetAudioFormatForCodec
* @brief Function to get audio codec from the map.
*
* @param[in] Audio codec string
* @return Audio codec map
*/
const FormatMap * GetAudioFormatForCodec( const char *codecs )
{
	if( codecs )
	{
		for( int i=0; i<AAMP_AUDIO_FORMAT_MAP_LEN; i++ )
		{
			if( strstr( codecs, mAudioFormatMap[i].codec) )
			{
				return &mAudioFormatMap[i];
			}
		}
	}
	return NULL;
}

/*
* @fn GetVideoFormatForCodec
* @brief Function to get video codec from the map.
*
* @param[in] Video codec string
* @return Video codec map
*/
const FormatMap * GetVideoFormatForCodec( const char *codecs )
{
	if( codecs )
	{
		for( int i=0; i<AAMP_VIDEO_FORMAT_MAP_LEN; i++ )
		{
			if( strstr( codecs, mVideoFormatMap[i].codec) )
			{
				return &mVideoFormatMap[i];
			}
		}
	}
	return NULL;
}

static std::hash<std::thread::id> std_thread_hasher;

std::size_t GetPrintableThreadID( const std::thread &t )
{
	return std_thread_hasher( t.get_id() );
}

std::size_t GetPrintableThreadID( void )
{
	return std_thread_hasher( std::this_thread::get_id() );
}

/**
 * @brief support for POSIX threads
 */
std::size_t GetPrintableThreadID( const pthread_t &t )
{
	static std::hash<pthread_t> pthread_hasher;
	return pthread_hasher( t );
}

/**
 * @brief Download a file from the server
 */
double GetNetworkTime(const std::string& remoteUrl, int *http_error , std::string NetworkProxy)
{
	double retValue = 0;
	AampCurlDownloader T1;
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bIgnoreResponseHeader	= true;
	inpData->eRequestType = eCURL_GET;
	inpData->iStallTimeout = 2; // 2sec
	inpData->iStartTimeout = 2; // 2sec
	inpData->iDownloadTimeout = 3; // 3sec
	inpData->proxyName 	  = NetworkProxy;
	
	inpData->bNeedDownloadMetrics = true;
	T1.Initialize(inpData);
	T1.Download(remoteUrl, respData);
		
	if (respData->curlRetValue == CURLE_OK)
	{
		if ((respData->iHttpRetValue == 204) || (respData->iHttpRetValue == 200))
		{
			std::string dataStr =  std::string( respData->mDownloadData.begin(), respData->mDownloadData.end());
			if(dataStr.size())
			{
				//2021-06-15T18:11:39Z - UTC Zulu
				//2021-06-15T19:03:48.795Z - <ProducerReferenceTime> WallClk UTC Zulu
				//const char* format = "%Y-%m-%dT%H:%M:%SZ";
				//mTime = convertTimeToEpoch((const char*)dataStr.c_str(), format);
				retValue = ISO8601DateTimeToUTCSeconds((const char*)dataStr.c_str());
				AAMPLOG_INFO("ProducerReferenceTime Wallclock (Epoch): [%f] TimeTaken[%f]", retValue, respData->downloadCompleteMetrics.total);
			}
		}
		else
		{
			AAMPLOG_ERR("Http Error Returned [%d]", respData->iHttpRetValue);
		}
	}
	else
	{
		AAMPLOG_ERR("Failed to perform curl request [%d]", respData->curlRetValue);
	}
	
	if(http_error)
	{
		*http_error = respData->iHttpRetValue;
	}
	return retValue;
	
}

//Multiply two ints without overflow
inline double safeMultiply(const  unsigned int first, const unsigned int second)
{
    return static_cast<double>(first * second);
}
/**
 * @brief Parse duration from ISO8601 string
 * @param ptr ISO8601 string
 * @return durationMs duration in milliseconds
 */
double ParseISO8601Duration(const char *ptr)
{
	int years = 0;
	int months = 0;
	int days = 0;
	int hour = 0;
	int minute = 0;
	double seconds = 0.0;
	double returnValue = 0.0;
	int indexforM = 0,indexforT=0;

	//ISO 8601 does not specify specific values for months in a day
	//or days in a year, so use 30 days/month and 365 days/year
	static constexpr auto kMonthDays = 30;
	static constexpr auto kYearDays = 365;
	static constexpr auto kMinuteSecs = 60;
	static constexpr auto kHourSecs = kMinuteSecs * 60;
	static constexpr auto kDaySecs = kHourSecs * 24;
	static constexpr auto kMonthSecs = kMonthDays * kDaySecs;
	static constexpr auto kYearSecs = kDaySecs * kYearDays;

	// ISO 8601 allow for number of years(Y), months(M), days(D) before the "T"
	// and hours(H), minutes(M), and seconds after the "T"

	const char* durationPtr = strchr(ptr, 'T');
	indexforT = (int)(durationPtr - ptr);
	const char* pMptr = strchr(ptr, 'M');
	if(NULL != pMptr)
	{
		indexforM = (int)(pMptr - ptr);
	}

	if (ptr[0] == 'P')
	{
		ptr++;
		if (ptr != durationPtr)
		{
			const char *temp = strchr(ptr, 'Y');
			if (temp)
			{	sscanf(ptr, "%dY", &years);
				AAMPLOG_WARN("years %d", years);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'M');
			if (temp && ( indexforM < indexforT ) )
			{
				sscanf(ptr, "%dM", &months);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'D');
			if (temp)
			{
				sscanf(ptr, "%dD", &days);
				ptr = temp + 1;
			}
		}
		if (ptr == durationPtr)
		{
			ptr++;
			const char* temp = strchr(ptr, 'H');
			if (temp)
			{
				sscanf(ptr, "%dH", &hour);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'M');
			if (temp)
			{
				sscanf(ptr, "%dM", &minute);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'S');
			if (temp)
			{
				sscanf(ptr, "%lfS", &seconds);
				ptr = temp + 1;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("Invalid input %s", ptr);
	}

	returnValue += seconds;

	//Guard against overflow by casting first term
	returnValue += safeMultiply(kMinuteSecs, minute);
	returnValue += safeMultiply(kHourSecs, hour);
	returnValue += safeMultiply(kDaySecs, days);
	returnValue += safeMultiply(kMonthSecs, months);
	returnValue += safeMultiply(kYearSecs, years);

	(void)ptr; // Avoid a warning as the last set value is unused.

	return returnValue * 1000;
}

const char *GetMediaTypeName(AampMediaType mediaType)
{
	static const char *name[] =
	{
		"video",//eMEDIATYPE_VIDEO
		"audio",//eMEDIATYPE_AUDIO
		"text",//eMEDIATYPE_SUBTITLE
		"aux_audio",//eMEDIATYPE_AUX_AUDIO
		"manifest",//eMEDIATYPE_MANIFEST
		"licence",//eMEDIATYPE_LICENCE
		"iframe",//eMEDIATYPE_IFRAME
		"init_video",//eMEDIATYPE_INIT_VIDEO
		"init_audio",//eMEDIATYPE_INIT_AUDIO
		"init_text",//eMEDIATYPE_INIT_SUBTITLE
		"init_aux_audio",//eMEDIATYPE_INIT_AUX_AUDIO
		"playlist_video",//eMEDIATYPE_PLAYLIST_VIDEO
		"playlist_audio",//eMEDIATYPE_PLAYLIST_AUDIO
		"playlist_text",//eMEDIATYPE_PLAYLIST_SUBTITLE
		"playlist_aux_audio",//eMEDIATYPE_PLAYLIST_AUX_AUDIO
		"playlist_iframe",//eMEDIATYPE_PLAYLIST_IFRAME
		"init_iframe",//eMEDIATYPE_INIT_IFRAME
		"dsm_cc",//eMEDIATYPE_DSM_CC
		"image",//eMEDIATYPE_IMAGE
	};
	if( mediaType < eMEDIATYPE_DEFAULT )
	{
		return name[mediaType];
	}
	else
	{
		return "UNKNOWN";
	}
}

TSB::LogLevel ConvertTsbLogLevel(int logLev)
{
	TSB::LogLevel ret = TSB::LogLevel::WARN; //default value
	if ((logLev < 0) || (logLev > static_cast<int>(TSB::LogLevel::ERROR)))
	{
		AAMPLOG_ERR("Bad TSB Log level Set by user: %d!! using default value : %d", logLev, static_cast<int>(ret));
	}
	else
	{
		AAMPLOG_INFO("TSB Log level Set as : %d", logLev);
		ret = static_cast<TSB::LogLevel>(logLev);
	}

	return ret;
}



/**
 * @brief Get 32 bit MPEG CRC value
 * @param[in] data buffer containing data
 * @param[in] size length of data
 * @param[in] initial optional initial CRC
 * @retval 32 bit CRC value
 */
uint32_t aamp_ComputeCRC32(const uint8_t *data, uint32_t size, uint32_t initial)
{
	static uint32_t crc32_table[256];
	static bool crc32_initialized;
	uint32_t i;
	uint32_t j;
	uint32_t k;
	uint32_t result = initial;

	if (!crc32_initialized)
	{
		for (i = 0; i < 256; i++)
		{
			k = 0;
			for (j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1)
			{
				k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);
			}
			crc32_table[i] = k;
		}

		crc32_initialized = true;
	}

	for (i = 0; i < size; i++)
	{
		result = (uint32_t)((result << 8) ^ crc32_table[(result >> 24) ^ data[i]]);
	}

	return result;
}

std::string aamp_GetConfigPath( const std::string &filename )
{
	std::string cfgPath;

#ifdef AAMP_SIMULATOR_BUILD					  // OSX or Ubuntu
	char *ptr{};

	if ((ptr = getenv("AAMP_CFG_DIR")) != nullptr)
	{
		cfgPath = ptr;
	}
	else if ((ptr = getenv("HOME")) != nullptr)
	{
		cfgPath = ptr;
	}

	if (filename.rfind("/opt/", 0) == 0)       //filename.starts_with("/opt") pre C++20
	{ // skip leading /opt in simulator
		cfgPath += filename.substr(4);
	}
	else
	{
		cfgPath += filename;
	}

#elif defined(AAMP_CPC)
	char *env_aamp_enable_opt = getenv("AAMP_ENABLE_OPT_OVERRIDE");
	/*
	 * defined(AAMP_CPC) 
	 * AAMP_ENABLE_OPT_OVERRIDE set         returns filename E.G /opt/aamp.cfg
	 * AAMP_ENABLE_OPT_OVERRIDE not set     returns "" E.G no .cfg file is read
	 */
	if (env_aamp_enable_opt)
	{
		cfgPath = filename;
	}
#else
	cfgPath = filename;
#endif

	return cfgPath;
}

/**
 * Parses and confirms the SCTE35 data is a valid DAI event.
 *
 * @param scte35Data The SCTE35 data to be checked.
 * @return True if the SCTE35 data is valid DAI event, false otherwise.
 */
bool parseAndValidateSCTE35(const std::string &scte35Data)
{
	/* Decode any SCTE35 splice info event. */
	bool isValidDAIEvent = false;
	std::vector<SCTE35SpliceInfo::Summary> spliceInfoSummary;
	SCTE35SpliceInfo spliceInfo(scte35Data);

	spliceInfo.getSummary(spliceInfoSummary);
	for (auto &splice : spliceInfoSummary)
	{
		AAMPLOG_DEBUG("[CDAI] splice info type %d, time %f, duration %f, id 0x%" PRIx32,
			(int)splice.type, splice.time, splice.duration, splice.event_id);

		if ((splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_ADVERTISEMENT_START) ||
			(splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_PLACEMENT_OPPORTUNITY_START) ||
			(splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::DISTRIBUTOR_PLACEMENT_OPPORTUNITY_START) ||
			(splice.type == SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_AD_BLOCK_START))
		{
			isValidDAIEvent = true;
			break;
		}
	}
	return isValidDAIEvent;
}



long long convertHHMMSSToTime(const char * str)
{ // parse HH:MM:SS.ms
	long long timeValueMs = 0;
	const int multiplier[4] = { 0,60,60,1000 };
	for( int part=0; part<4; part++ )
	{
		int num = 0;
		for(;;)
		{
			int c = *str++;
			if( c>='0' && c<='9' )
			{
				num*=10;
				num+=(c-'0');
			}
			else
			{
				timeValueMs *= multiplier[part];
				timeValueMs += num;
				break;
			}
		}
	}
	return timeValueMs;
}

static std::string numberToString( long number, int minDigits=2 )
{
	std::string rc = std::to_string(number);
	while( rc.length() < minDigits )
	{
		rc = '0' + rc;
	}
	return rc;
}

std::string convertTimeToHHMMSS( long long t )
{ // pack HH:MM:SS.ms
	std::string rc;
	int ms = t%1000;
	int sec = (int)(t/1000);
	int minute = sec/60;
	int hour = minute/60;
	minute %= 60;
	sec %= 60;
	rc = numberToString(hour) + ":" + numberToString(minute) + ":" + numberToString(sec) + "." + numberToString(ms,3);
	return rc;
}

const char *mystrstr(const char *haystack_ptr, const char *haystack_fin, const char *needle_ptr)
{
	size_t needle_len = strlen(needle_ptr);
	haystack_fin -= needle_len;
	while( haystack_ptr<=haystack_fin )
	{
		if( memcmp(needle_ptr,haystack_ptr,needle_len)==0 )
		{
			return haystack_ptr;
		}
		haystack_ptr++;
	}
	return NULL;
}

/**
 * @brief To set the thread name
 * The thread name should be 16 characters or less, including null terminator.
 * If the name is longer than 15 characters, it will be truncated.
 * @note This function is only supported on POSIX threads.
 * @param[in] name thread name
 */
void aamp_setThreadName(const char *name)
{
	if (name == NULL)
	{
		AAMPLOG_ERR("invalid name");
	}
	else
	{
		char truncatedThreadName[MAX_THREAD_NAME_LENGTH];
		size_t len = strlen(name);
		if( len>=MAX_THREAD_NAME_LENGTH )
		{ // clamp, avoiding ERANGE error
			len = MAX_THREAD_NAME_LENGTH-1;
		}
		memcpy( truncatedThreadName, name, len );
		truncatedThreadName[len] = '\0';
#ifdef __APPLE__
		int ret = pthread_setname_np(truncatedThreadName); // different API signature on OSX
#else
		int ret = pthread_setname_np(pthread_self(), truncatedThreadName);
#endif
		if( ret != 0 )
		{ // Not exactly an error, but log it for information
			AAMPLOG_WARN( "pthread_setname_np failed with error code[%d]", ret );
		}
	}
}

/**
 * @brief Set the thread scheduling parameters
 * @param[in] policy scheduling policy
 * @param[in] priority scheduling priority
 * @retval 0 on success
 * @retval -1 on failure
 */
int aamp_SetThreadSchedulingParameters(int policy, int priority)
{
	// Set up the scheduling parameters
	struct sched_param param;
	param.sched_priority = priority;

	// Set the scheduling policy and parameters for the current thread
	int result = pthread_setschedparam(pthread_self(), policy, &param);

	// Handle errors
	if (result != 0)
	{
		AAMPLOG_ERR ("Error: pthread_setschedparam failed with error code[%d] ", result);
		return result;
	}
	AAMPLOG_INFO("Thread scheduling parameters set successfully.");
	return result; // Success
}

bool aamp_isTuneScheme( const char *cmdBuf )
{
    size_t cmdLen = strlen(cmdBuf);
    bool isTuneScheme = false;
    static const char *protocol[]  = { "http:","https:","live:","hdmiin:","file:","mr:","tune:" };
    for( int i=0; i<sizeof(protocol)/sizeof(protocol[0]); i++ )
    {
        size_t protocolLen = strlen(protocol[i]);
        if( cmdLen>=protocolLen && memcmp( cmdBuf, protocol[i], protocolLen )==0 )
        {
            isTuneScheme=true;
            break;
        }
    }
    return isTuneScheme;
}

