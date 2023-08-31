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
	struct timespec tspec;
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

int getHarvestConfigForMedia(MediaType fileType)
{
    return 0;
}

void getDefaultHarvestPath(std::string &value)
{
}

bool aamp_WriteFile(std::string fileName, const char* data, size_t len, MediaType &fileType, unsigned int count,const char *prefix)
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
		//Find out offset from utc by convering epoch
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
 * @brief parse leading protcocol from uri if present
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
    const char *codec = "UNKNOWN";
    return codec;
}

std::string Getiso639map_NormalizeLanguageCode(std::string  lang,LangCodePreference preferLangFormat )
{
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
	return 0.0;
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

void trim(std::string& src)
{

}

/**
 * @brief Return the name corresponding to the Media Type
 * @param mediaType media type
 * @retval the name of the mediaType
 */
const char* getMediaTypeName( MediaType mediaType )
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
	switch(mediaType)
	{
		case eMEDIATYPE_VIDEO:
			return MEDIATYPE_VIDEO;
		case eMEDIATYPE_AUDIO:
			return MEDIATYPE_AUDIO;
		case eMEDIATYPE_SUBTITLE:
			return MEDIATYPE_TEXT;
		case eMEDIATYPE_IMAGE:
			return MEDIATYPE_IMAGE;
		case eMEDIATYPE_AUX_AUDIO:
			return MEDIATYPE_AUX_AUDIO;
		default:
			return "UNKNOWN";
	}
}

/**
 * @brief Check if mime type is compatible with media type
 * @param mimeType mime type
 * @param mediaType media type
 * @retval true if compatible
 */
bool IsCompatibleMimeType(const std::string& mimeType, MediaType mediaType)
{
        //FN_TRACE_F_MPD( __FUNCTION__ );
	bool isCompatible = false;

	switch ( mediaType )
	{
		case eMEDIATYPE_VIDEO:
			if (mimeType == "video/mp4")
				isCompatible = true;
			break;

		case eMEDIATYPE_AUDIO:
		case eMEDIATYPE_AUX_AUDIO:
			if ((mimeType == "audio/webm") ||
				(mimeType == "audio/mp4"))
				isCompatible = true;
			break;

		case eMEDIATYPE_SUBTITLE:
			if ((mimeType == "application/ttml+xml") ||
				(mimeType == "text/vtt") ||
				(mimeType == "application/mp4"))
				isCompatible = true;
			break;

		default:
			break;
	}

	return isCompatible;
}

/**
 * @brief Computes the fragment duratioN.
 * @param duration of the fragment.
 * @param timeScale value.
 * @return - computed fragment duration in double.
 */
double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale )
{
	double newduration = 2.0;
	if( duration && timeScale )
	{
		newduration =  (double)duration / (double)timeScale;
	}
	else
	{
		AAMPLOG_ERR("Invalid %u %u",duration,timeScale);
	}
	return newduration;
}

uint32_t aamp_ComputeCRC32(const uint8_t *data, uint32_t size, uint32_t initial)
{
	return 0;
}
