/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include "downloader.hpp"
#include "gst-test.h"
#include <stdlib.h>
#include <curl/curl.h>
#include "string_utils.hpp"
#include <assert.h>

/**
 * @class Curl context class
 *
 * Use a static instance of this class to enable connection reuse by the Curl
 * library.
 */
class CurlContext
{
public:
	/**
	 * @brief Constructor
	 */
	CurlContext () : curl(),capacity(),size(),buffer()
	{
		clear();
		
		curl = curl_easy_init();
		if (curl == NULL)
		{
			exit(1);
		}
	}
	
	/**
	 * @brief Destructor
	 */
	~CurlContext()
	{
		curl_easy_cleanup(curl);
	}
	
	/**
	 * @brief Clear downloaded data
	 */
	void clear()
	{
		capacity = 0;
		size = 0;
		buffer = NULL;
	}
	
	/**
	 * @brief Curl write callback
	 *
	 * Downloaded data is written into the context data buffer. The buffer is
	 * extended if required.
	 *
	 * @param[in] ptr Downloaded data
	 * @param[in] size Size of downloaded data element
	 * @param[in] nmemb Number of downloaded data elements
	 * @param[in] context Curl context
	 * @return The number of bytes written
	 */
	static size_t write_callback(char *ptr, size_t size, size_t nmemb, CurlContext *context)
	{
		gsize total = size*nmemb;
		if ((context->size + total) > context->capacity)
		{
			context->capacity = context->size + total;
			context->buffer = (char *)g_realloc(context->buffer, context->capacity);
			if (context->buffer == NULL) {
				throw TestHarnessException("CurlContext::write_callback - g_realloc failed");
			}
		}
		(void)memcpy(&context->buffer[context->size], ptr, total);
		context->size += total;
		return total;
	}
	
	CURL *curl;			/**< @brief Curl library context handle */
	gsize capacity;		/**< @brief Buffer capacity */
	gsize size;			/**< @brief Number of buffered bytes */
	char *buffer;		/**< @brief Downloaded data buffer */
	
	//copy constructor
	CurlContext(const CurlContext&)=delete;
	//copy assignment operator
	CurlContext& operator=(const CurlContext&)=delete;
};

/**
 * C file loader
 * TODO: HttpRequestEnd telemetry
 * TODO: curlstore integration
 */
gpointer LoadUrl( const std::string &url, gsize *pLen )
{
	printf( "LoadUrl(%s)\n", url.c_str() );
	gpointer ptr = NULL;
	gsize len = 0;
	
	static CurlContext context; // static to enable connection reuse
	
	std::string range;
	size_t delim = 0;
	for(;;)
	{
		delim = url.find('@', delim);
		if( delim == std::string::npos )
		{
			break;
		}
		char c = url[delim+1];
		if( isdigit(c) )
		{ // sanity check - @ followed by number to avoid bogus parsing of non-range directives
			range = url.substr(delim+1);
			break;
		}
		delim++;
	}
	
	if( starts_with(url,"http://") || starts_with(url,"https://" ) )
	{
		if( range.empty() )
		{
			(void)curl_easy_setopt(context.curl, CURLOPT_URL, url.c_str() );
			(void)curl_easy_setopt(context.curl, CURLOPT_RANGE, NULL);
		}
		else
		{
			std::string prefix = url.substr(0,delim);
			(void)curl_easy_setopt(context.curl, CURLOPT_URL, prefix.c_str() );
			(void)curl_easy_setopt(context.curl, CURLOPT_RANGE, range.c_str() );
		}
		(void)curl_easy_setopt(context.curl, CURLOPT_BUFFERSIZE, 4096L);
		(void)curl_easy_setopt(context.curl, CURLOPT_FOLLOWLOCATION, 1L);
		(void)curl_easy_setopt(context.curl, CURLOPT_WRITEFUNCTION, CurlContext::write_callback);
		(void)curl_easy_setopt(context.curl, CURLOPT_WRITEDATA, &context);
		(void)curl_easy_setopt(context.curl, CURLOPT_TCP_KEEPALIVE, 1L);
		(void)curl_easy_setopt(context.curl, CURLOPT_USERAGENT, "gstTestHarness/1.0");

		context.clear();
		CURLcode rc = curl_easy_perform(context.curl);
		if (CURLE_OK == rc)
		{
			long response_code = 0;
			(void)curl_easy_getinfo(context.curl, CURLINFO_RESPONSE_CODE, &response_code);
			switch( response_code )
			{
				case 200:
				case 204:
				case 206:
					ptr = (gpointer)context.buffer;
					len = context.size;
					break;
				default:
					printf( "http error: %ld\n", response_code );
					g_free(context.buffer);
					context.buffer = NULL;
					break;
			}
		}
		else
		{
			printf( "curl error: %d\n", rc );
			g_free(context.buffer);
			context.buffer = NULL;
		}
	}
	else
	{
		int start = 0;
		if( starts_with(url,"file://") )
		{ // strip file:// prefix
			start = 7;
		}
		FILE *f = NULL;
		long offs = 0;
		if( range.empty() )
		{
			std::string prefix = url.substr(start);
			f = fopen( prefix.c_str(), "rb" );
			if( !f )
			{ // file not found
				printf( "failed to open file\n" );
				return NULL;
			}
			fseek( f, 0, SEEK_END );
			len = ftell(f);
		}
		else
		{
			std::string prefix = url.substr(start,delim-start);
			f = fopen( prefix.c_str(), "rb" );
			if (!f) {
				throw TestHarnessException("failed to open file: " + prefix);
			}
			delim = range.find('-');
			if (delim == std::string::npos) {
				fclose(f);
				throw TestHarnessException("Invalid range format (missing '-'): " + range);
			}
			try {
				offs = std::stol(range.substr(0,delim));
				len = std::stol(range.substr(delim+1)) + 1 - offs;
			} catch (const std::exception& e) {
				fclose(f);
				throw TestHarnessException("Failed to parse range values: " + range + " - " + e.what());
			}
		}
		if( f )
		{
			ptr = g_malloc(len);
			if( ptr )
			{
				fseek( f, offs, SEEK_SET );
				if( fread(ptr, 1, len, f ) != len )
				{
					g_free( ptr );
					ptr = NULL;
				}
			}
			fclose( f );
		}
	}
	*pLen = len;
	return ptr;
}
