/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#ifndef __AAMP_CURL_DEFINE_H__
#define __AAMP_CURL_DEFINE_H__

/**
* @file AampCurlDefine.h
* @brief Curl config defines
*/

#include <string>
#include <sstream>
#include <chrono>
#include <curl/curl.h>

#define CURL_EASY_SETOPT(curl, CURLoption, option)\
		if (curl_easy_setopt(curl, CURLoption, option) != 0) {\
			AAMPLOG_WARN("Failed at curl_easy_setopt ");\
		}

#define DEFAULT_CURL_TIMEOUT 5L		/**< Default timeout for Curl downloads */
#define DEFAULT_CURL_CONNECTTIMEOUT 3L	/**< Curl socket connection timeout */
#define DEFAULT_DNS_CACHE_TIMEOUT 3*60L	/***< Name resolve results for this number of seconds*/
/**
 * @enum AampCurlStoreErrorCode
 * @brief Error codes returned by curlstore
 */
 

/**
 * @brief Http Header Type
 */
enum HttpHeaderType
{
	eHTTPHEADERTYPE_COOKIE,       /**< Cookie Header */
	eHTTPHEADERTYPE_XREASON,      /**< X-Reason Header */
	eHTTPHEADERTYPE_FOG_REASON,   /**< X-Reason Header */
	eHTTPHEADERTYPE_EFF_LOCATION, /**< Effective URL location returned */
	eHTTPHEADERTYPE_UNKNOWN=-1    /**< Unknown Header */
};

enum CurlAbortReason
{
	eCURL_ABORT_REASON_NONE = 0,
	eCURL_ABORT_REASON_STALL_TIMEDOUT,
	eCURL_ABORT_REASON_START_TIMEDOUT,
	eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT
};

/**
 *
 * @enum Curl Request
 *
 */
enum CurlRequest
{
	eCURL_GET,
	eCURL_POST,
	eCURL_DELETE
};

/**
 * @brief Enumeration for Curl Instances
 */
enum AampCurlInstance
{
	eCURLINSTANCE_VIDEO,
	eCURLINSTANCE_AUDIO,
	eCURLINSTANCE_SUBTITLE,
	eCURLINSTANCE_AUX_AUDIO,
	eCURLINSTANCE_MANIFEST_MAIN,
	eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO,
	eCURLINSTANCE_MANIFEST_PLAYLIST_AUDIO,
	eCURLINSTANCE_MANIFEST_PLAYLIST_SUBTITLE,
	eCURLINSTANCE_MANIFEST_PLAYLIST_AUX_AUDIO,
	eCURLINSTANCE_DAI,
	eCURLINSTANCE_AES,
	eCURLINSTANCE_PLAYLISTPRECACHE,
	eCURLINSTANCE_MAX
};

#define CURL_SHARE_SETOPT( SH, OPT, PARAM ) \
	{ \
		CURLSHcode rc = curl_share_setopt( SH, OPT, PARAM ); \
		if( rc != CURLSHE_OK ) \
		{ \
			AAMPLOG_WARN( "curl_share_setopt fail %d", rc ); \
		} \
	}
#define CURL_EASY_SETOPT_POINTER( handle, option, parameter )\
	if (curl_easy_setopt( handle, option, parameter ) != CURLE_OK ) {\
		AAMPLOG_WARN("CURL_EASY_SETOPT_POINTER failure" );\
	}
#define CURL_EASY_SETOPT_STRING( handle, option, parameter)\
	if (curl_easy_setopt( handle, option, parameter ) != CURLE_OK ) {\
		AAMPLOG_WARN("CURL_EASY_SETOPT_STRING failure" );\
	}
#define CURL_EASY_SETOPT_LONG( handle, option, parameter )\
	if (curl_easy_setopt( handle, option, (long)parameter ) != CURLE_OK ) {\
		AAMPLOG_WARN("CURL_EASY_SETOPT_LONG failure" );\
	}
#define CURL_EASY_SETOPT_FUNC( handle, option, parameter )\
	if (curl_easy_setopt( handle, option, parameter ) != CURLE_OK) {\
		AAMPLOG_WARN("CURL_EASY_SETOPT_FUNC failure" );\
	}
#define CURL_EASY_SETOPT_LIST( handle, option, parameter )\
	if (curl_easy_setopt( handle, option, parameter ) != CURLE_OK) {\
		AAMPLOG_WARN("CURL_EASY_SETOPT_LIST failure" );\
}

curl_off_t aamp_CurlEasyGetinfoOffset( CURL *handle, CURLINFO info );
double aamp_CurlEasyGetinfoDouble( CURL *handle, CURLINFO info );
long aamp_CurlEasyGetinfoLong( CURL *handle, CURLINFO info );
char *aamp_CurlEasyGetinfoString( CURL *handle, CURLINFO info );

#define FOG_REASON_STRING			"Fog-Reason:"
#define CURLHEADER_X_REASON			"X-Reason:"
#define BITRATE_HEADER_STRING		"X-Bitrate:"
#define CONTENTLENGTH_STRING		"Content-Length:"
#define SET_COOKIE_HEADER_STRING	"Set-Cookie:"
#define LOCATION_HEADER_STRING		"Location:"
#define CONTENT_ENCODING_STRING		"Content-Encoding:"
#define FOG_RECORDING_ID_STRING		"Fog-Recording-Id:"
#define CAPPED_PROFILE_STRING		"Profile-Capped:"
#define TRANSFER_ENCODING_STRING	"Transfer-Encoding:"


#endif  //__AAMP_CURL_DEFINE_H__

