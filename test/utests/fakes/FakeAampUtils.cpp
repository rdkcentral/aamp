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

long long aamp_GetCurrentTimeMS(void)
{
    return 0;
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

std::size_t GetPrintableThreadID( const std::thread &t )
{
    return 0;
}

const FormatMap * GetAudioFormatForCodec( const char *codecs )
{
    return NULL;
}

double ISO8601DateTimeToUTCSeconds(const char *ptr)
{
    return 0;
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

double CURL_EASY_GETINFO_DOUBLE( CURL *handle, CURLINFO info )
{
	return 0.0;
}

int CURL_EASY_GETINFO_LONG( CURL *handle, CURLINFO info )
{
	return -1;
}

char *CURL_EASY_GETINFO_STRING( CURL *handle, CURLINFO info )
{
	return NULL;
}

double aamp_CurlEasyGetinfoDouble( CURL *handle, CURLINFO info )
{
	return 0.0;
}

int aamp_CurlEasyGetinfoInt( CURL *handle, CURLINFO info )
{
	return 0;
}

long aamp_CurlEasyGetinfoLong( CURL *handle, CURLINFO info )
{
	return 0;
}

char *aamp_CurlEasyGetinfoString( CURL *handle, CURLINFO info )
{
	return NULL;
}

