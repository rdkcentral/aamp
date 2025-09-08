/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "AampUtils.h"
#include "MockAampUtils.h"

MockAampUtils *g_mockAampUtils = nullptr;

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

long long aamp_GetCurrentTimeMS(void)
{
	long long timeMS = 0;

	if (g_mockAampUtils)
	{
		timeMS = g_mockAampUtils->aamp_GetCurrentTimeMS();
	}

	return timeMS;
}

float getWorkingTrickplayRate(float rate)
{
    return 0.0;
}

void aamp_DecodeUrlParameter( std::string &uriParam )
{
}

bool replace(std::string &str, const char *existingSubStringToReplace, const char *replacementString)
{
    return false;
}

bool aamp_IsLocalHost ( std::string Hostname )
{
    return false;
}

struct timespec aamp_GetTimespec(int timeInMs)
{
	static struct timespec tspec;
	return tspec;
}

float getPseudoTrickplayRate(float rate)
{
    return 0.0;
}

std::string aamp_getHostFromURL(std::string url)
{
    return "";
}

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

void getDefaultHarvestPath(std::string &value)
{
}

bool aamp_WriteFile(std::string fileName, const char* data, size_t len, AampMediaType mediaType, unsigned int count,const char *prefix)
{
    return false;
}

std::string aamp_PostJsonRPC( std::string id, std::string method, std::string params )
{
	return "";
}

std::size_t GetPrintableThreadID( const std::thread &t )
{
	return 0;
}

std::size_t GetPrintableThreadID()
{
	return 0;
}

const FormatMap * GetAudioFormatForCodec( const char *codecs )
{
    return NULL;
}

/**
 * @brief Parse date time from ISO8601 string and return value in seconds
 * @retval date time in seconds
 */
double ISO8601DateTimeToUTCSeconds(const char *ptr)
{
	double timeSeconds = 0;
	if(ptr)
	{
		std::tm timeObj = { 0 };
		//Find out offset from utc by converting epoch
		std::tm baseTimeObj = { 0 };
		strptime("1970-01-01 T00:00:00.", "%Y-%m-%d T%H:%M:%S.", &baseTimeObj);
		time_t offsetFromUTC = timegm(&baseTimeObj);
		//Convert input string to time
		const char *msString = strptime(ptr, "%Y-%m-%d T%H:%M:%S.", &timeObj);
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
 * @brief parse leading protocol from uri if present
 * @param[in] uri manifest/ fragment uri
 * @retval return pointer just past protocol (i.e. http://) if present (or) return NULL uri doesn't start with protcol
 */
static const char * ParseUriProtocol(const char *uri)
{
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

const char * GetAudioFormatStringForCodec ( StreamOutputFormat input)
{
    return "UNKNOWN";
}

std::string Getiso639map_NormalizeLanguageCode(std::string lang, LangCodePreference preferLangFormat)
{
	if (g_mockAampUtils)
	{
		return g_mockAampUtils->Getiso639map_NormalizeLanguageCode(lang, preferLangFormat);
	}

    return lang;
}

const FormatMap * GetVideoFormatForCodec( const char *codecs )
{
    return NULL;
}

bool aamp_IsAbsoluteURL( const std::string &url )
{
	return url.compare(0, 7, "http://")==0 || url.compare(0, 8, "https://")==0;
}

double GetNetworkTime(const std::string& remoteUrl, int *http_error , std::string NetworkProxy)
{
	double networkTime = 0.0;

	if (g_mockAampUtils)
	{
		networkTime = g_mockAampUtils->GetNetworkTime(remoteUrl, http_error, NetworkProxy);
	}

	return networkTime;
}

char *aamp_Base64_URL_Encode(const unsigned char *src, size_t len)
{
	return NULL;
}

unsigned char *aamp_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
{
	return NULL;
}

inline double safeMultiply(const unsigned int first, const unsigned int second)
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
				printf("years %d", years);
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
		printf("Invalid input %s", ptr);
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

void trim(std::string& src)
{

}

/**
 * @brief Return the name corresponding to the Media Type
 * @param mediaType media type
 * @retval the name of the mediaType
 */
const char* GetMediaTypeName( AampMediaType mediaType )
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

uint32_t aamp_ComputeCRC32(const uint8_t *data, uint32_t size, uint32_t initial)
{
	return 0;
}

const char *GetDrmSystemName(DRMSystems drmSystem)
{
	return "";
}

DRMSystems GetDrmSystem(std::string drmSystemID)
{
	return eDRM_NONE;
}
void mssleep(int milliseconds)
{
}

const char *GetDrmSystemID(DRMSystems drmSystem)
{
	return "";
}

bool aamp_StartsWith(const char *inputStr, const char *prefix)
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

std::string aamp_GetConfigPath(const std::string &filename)
{
	if (g_mockAampUtils != nullptr)
	{
		return g_mockAampUtils->aamp_GetConfigPath(filename);
	}

	return "FakeAampUtils.cpp";
}

TSB::LogLevel ConvertTsbLogLevel(int logLev)
{
	return static_cast<TSB::LogLevel>(0);
}

void UrlEncode(std::string inStr, std::string &outStr)
{
}

bool parseAndValidateSCTE35(const std::string &scte35Data)
{
	if (g_mockAampUtils)
	{
		return g_mockAampUtils->parseAndValidateSCTE35(scte35Data);
	}
	return false;
}

long long convertHHMMSSToTime(const char * str)
{
	return 0;
}

std::string convertTimeToHHMMSS( long long t )
{
	return "";
}

const char *mystrstr(const char *haystack_ptr, const char *haystack_fin, const char *needle_ptr)
{
	return NULL;
}

void aamp_setThreadName(const char *name)
{
}

int aamp_SetThreadSchedulingParameters(int policy, int priority)
{
	return 0;
}

bool aamp_isTuneScheme( const char *cmdBuf ){ return false; }

// aamp_ApplyPageHttpHeaders not actually part of AampUtils.cpp, but fake declared here for convenience
extern "C" void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP *aamp){}
