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
* @file Aamputils.h
* @brief Context-free common utility functions.
*/

#ifndef __AAMP_UTILS_H__
#define __AAMP_UTILS_H__

#include "DrmSystems.h"
#include "StreamOutputFormat.h"
#include "AampMediaType.h"
#include <thread>
#include "iso639map.h"
#include <string>
#include <sstream>
#include <chrono>
#include "TsbApi.h"


#define NOW_SYSTEM_TS_SECS std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()     /**< Getting current system clock in seconds */
#define NOW_STEADY_TS_SECS std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()     /**< Getting current steady clock in seconds */

#define NOW_SYSTEM_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()     /**< Getting current system clock in milliseconds */
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()     /**< Getting current steady clock in milliseconds */

#define NOW_SYSTEM_TS_SECS_FP std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count()     /**< Getting current system clock in floating point seconds */
#define NOW_STEADY_TS_SECS_FP std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now().time_since_epoch()).count()     /**< Getting current steady clock in floating point seconds */

#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))

//Delete non-array object
#define SAFE_DELETE(ptr) { delete(ptr); ptr = NULL; }
//Delete Array object
#define SAFE_DELETE_ARRAY(ptr) { delete [] ptr; ptr = NULL; }

/**HTTP SUccess*/
#define IS_HTTP_SUCCESS(code) ((code) == 200 || (code) == 204 || (code) == 206)

/** FHD height*/
#define AAMP_FHD_HEIGHT (1080)
#define FLOATING_POINT_EPSILON 0.1 // workaround for floating point math precision issues


/**
* @struct	FormatMap
* @brief	FormatMap structure for stream codec/format information
*/
struct FormatMap
{
	const char* codec;
	StreamOutputFormat format;
};


/*
* @fn GetAudioFormatStringForCodec
* @brief Function to get audio codec string from the map.
*
* @param[in] input Audio codec type
* @return Audio codec string
*/
const char * GetAudioFormatStringForCodec ( StreamOutputFormat input);

/*
* @fn GetAudioFormatForCodec
* @brief Function to get audio codec from the map.
*
* @param[in] Audio codec string
* @return Audio codec map
*/
const FormatMap * GetAudioFormatForCodec( const char *codecs );

/*
* @fn GetVideoFormatForCodec
* @brief Function to get video codec from the map.
*
* @param[in] Video codec string
* @return Video codec map
*/
const FormatMap * GetVideoFormatForCodec( const char *codecs );

/**
 * @fn aamp_GetCurrentTimeMS
 *
 */
long long aamp_GetCurrentTimeMS(void); //TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead

/**
 * @fn aamp_IsAbsoluteURL
 *
 * @param[in] url - Input URL
 */
 bool aamp_IsAbsoluteURL( const std::string &url );

/**
 * @fn aamp_getHostFromURL
 *
 * @param[in] url - Input URL
 * @retval host of input url
 */
std::string aamp_getHostFromURL(std::string url);

/**
 * @fn aamp_IsLocalHost
 *
 * @param[in] Hostname - Hostname parsed from url
 * @retval true if localhost false otherwise.
 */
bool aamp_IsLocalHost ( std::string Hostname );

/**
 * @fn aamp_ResolveURL
 *
 * @param[out] dst - Created URL
 * @param[in] base - Base URL
 * @param[in] uri - File path
 * @param[in] bPropagateUriParams - flag to use base uri params
 * @retval void
 */
void aamp_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams);

/**
 * @fn aamp_StartsWith
 *
 * @param[in] inputStr - Input string
 * @param[in] prefix - substring to be searched
 */
bool aamp_StartsWith( const char *inputStr, const char *prefix);

/**
 * @fn aamp_Base64_URL_Encode
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 */
char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len);

/**
 * @fn aamp_Base64_URL_Decode
 * @param src pointer to cstring containing base64-URL-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 * @param srcLen source data length
 */
unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen);

/**
 * @brief unescape uri-encoded uri parameter
 * @param uriParam string to un-escape
 */
void aamp_DecodeUrlParameter( std::string &uriParam );
/**
 * @fn ISO8601DateTimeToUTCSeconds
 * @param ptr ISO8601 string
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr);

/**
 * @fn aamp_PostJsonRPC
 * @param[in] id string containing player id
 * @param[in] method string containing JSON method
 * @param[in] params string containing params:value pair list
 */
std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params );

/**
 * @fn aamp_GetDeferTimeMs
 *
 * @param  maxTimeSeconds Maximum time allowed for deferred license acquisition
 */
int aamp_GetDeferTimeMs(long maxTimeSeconds);

/**
 * @fn GetDrmSystemName
 * @param drmSystem drm system
 */
const char * GetDrmSystemName(DRMSystems drmSystem);

/**
 * @fn GetDrmSystem
 * @param drmSystemID - Id of the DRM system, empty string if not supported
 */
DRMSystems GetDrmSystem(std::string drmSystemID);

/**
 * @fn GetDrmSystemID
 * @param drmSystem - drm system
 */
const char * GetDrmSystemID(DRMSystems drmSystem);

/**
 * @fn UrlEncode
 *
 * @param[in] inStr - Input URL
 * @param[out] outStr - Encoded URL
 */
void UrlEncode(std::string inStr, std::string &outStr);

/**
 * @fn trim
 * @param[in][out] src Buffer containing string
 */
void trim(std::string& src);

/**
 * @fn Getiso639map_NormalizeLanguageCode 
 * @param[in] lang - Language in string format
 * @param[in] preferFormat - Preferred language format
 */
std::string Getiso639map_NormalizeLanguageCode( const std::string lang, LangCodePreference preferFormat );

/**
 * @fn aamp_GetTimespec
 * @param[in] timeInMs 
 */
struct timespec aamp_GetTimespec(int timeInMs);

/**
 * @fn aamp_WriteFile
 * @param fileName - out file name
 * @param data - buffer
 * @param len - length of buffer
 * @param mediaType - Media type of file
 * @param count - for manifest or playlist update
 * @param prefix - prefix name
 */
bool aamp_WriteFile(std::string fileName, const char* data, size_t len, AampMediaType mediaType, unsigned int count,const char *prefix);

/**
 * @fn getHarvestConfigForMedia
 * @param mediaType - media file type
 */
int getHarvestConfigForMedia(AampMediaType mediaType);
/**
 * @fn getWorkingTrickplayRate
 * @param rate input rate
 */
float getWorkingTrickplayRate(float rate);

/**
 * @fn getPseudoTrickplayRate
 * @param rate working rate
 */
float getPseudoTrickplayRate(float rate);

/**
 * @fn getDefaultHarvestPath
 * @param[in] value - harvest path
 * @return void
 */

void getDefaultHarvestPath(std::string &value);

/**
 * @fn stream2hex
 * @param[in] str input string
 * @param[out] hexstr output hex string
 * @param[in] capital - Boolean to enable capital letter conversion flag
 */
void stream2hex(const std::string str, std::string& hexstr, bool capital = false);

/**
 * @fn mssleep
 * @param milliseconds Time to sleep
 */
void mssleep(int milliseconds);

/**
 * @fn GetNetworkTime
 *
 * @param[in] remoteUrl - File URL
 * @param[in] http_error - HTTP error code
 * @return double time value
 */
double GetNetworkTime(const std::string& remoteUrl, int *http_error, std::string NetworkProxy);

/**
 * @fn GetMediaTypeName
 * @param[in] mediaType - Media type
 * @return
 */
const char * GetMediaTypeName(AampMediaType mediaType);

std::size_t GetPrintableThreadID( const std::thread &t );
std::size_t GetPrintableThreadID( const pthread_t &t );
std::size_t GetPrintableThreadID();

/**
 * @brief Parse duration from ISO8601 string
 * @param ptr ISO8601 string
 * @return durationMs duration in milliseconds
 */
double ParseISO8601Duration(const char *ptr);

/**
 * @brief Computes the fragment duration.
 * @param duration of the fragment.
 * @param timeScale value.
 * @return - computed fragment duration in double.
 */
double ComputeFragmentDuration(uint32_t duration, uint32_t timeScale);

/**
 * @brief Get 32 bit MPEG CRC value
 * @param[in] data buffer containing data
 * @param[in] size length of data
 * @param[in] initial initial CRC
 * @retval 32 bit CRC value
 */
uint32_t aamp_ComputeCRC32(const uint8_t *data, uint32_t size, uint32_t initial = 0xffffffff);

namespace aamp_utils
{
    template<typename T, typename ...Args>
    std::unique_ptr<T> make_unique(Args&& ...args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}

/**
 * @fn ConvertTsbLogLevel
 * @param[in] int Log leve set by user
 * @return allowed log level by TSB module
 */
TSB::LogLevel ConvertTsbLogLevel(int logLev);

#define MAX_RANGE_STRING_CHARS 128

#define WRITE_HASCII( DST, BYTE ) \
{ \
	*DST++ = "0123456789abcdef"[BYTE>>4]; \
	*DST++ = "0123456789abcdef"[BYTE&0xf]; \
}

/**
 * @brief Account for various overrides and env variables to give path to file
 * @param[in] filename A file which may not exist if the intention is to write
 * On simulator
 * "/opt/somefile.txt -> "$AAMP_CFG_DIR/somefile.txt" or "$HOME/somefile.txt"
 * "a_file.txt"       ->  "$AAMP_CFG_DIR/a_file.txt" or "$HOME/a_file.txt"
 * "/abc/a_file.txt"  ->  "$AAMP_CFG_DIR/abc/a_file.txt" or "$HOME/abc/a_file.txt"
 *
 * On production build
 * filename is not modified
 *
 * @retval a full path
 */
std::string aamp_GetConfigPath( const std::string &filename );


/**
 * Parses and confirms the SCTE35 data is a valid DAI event.
 *
 * @param scte35Data The SCTE35 data to be checked.
 * @return True if the SCTE35 data is valid DAI event, false otherwise.
 */
bool parseAndValidateSCTE35(const std::string &scte35Data);


/**
 * @brief convert time in HH:SS:MM.ms format to milliseconds
 */
long long convertHHMMSSToTime(const char * str);

/**
 * @brief convert time in milliseconds to HH:SS:MM.ms format
 */
std::string convertTimeToHHMMSS( long long t );

/**
 * @brief strstr variant that doesn't require the string being searched to end with NUL terminator
 *
 * @param haystack_ptr start of character range to search
 * @param haystack_fin pointer just past end of character range to search (haystack_len = haystack_fin-haystack_ptr)
 * @param needle cstring for character to find
 *
 * @retval pointer within haystack if needle found as a substring
 * @retval NULL if needle not present within haystack
 */
const char *mystrstr(const char *haystack_ptr, const char *haystack_fin, const char *needle_ptr);

/**
 * @brief wrapper for pthread_setname_np (pthreads used under the hood for std::thread)
 * @param[in] name human readable thread name
 */
void aamp_setThreadName(const char *name);

/**
 * @brief wrapper for pthread_setschedparam (pthreads used under the hood for std::thread)
 * @param[in] policy scheduling policy
 * @param[in] priority scheduling priority
 * @retval 0 on success
 * @retval -1 on failure
 */
int aamp_SetThreadSchedulingParameters(int policy, int priority);

/**
 * @fn isTuneScheme
 *
 * @param[in] uri
 *
 * @retval true iff uri starts with a recognized protocol representing an IP Video Locator
 */
bool aamp_isTuneScheme( const char *cmdBuf );

#endif  /* __AAMP_UTILS_H__ */
