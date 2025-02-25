/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file PlayerUtils.cpp
 * @brief Common utility functions
 */
#include "PlayerUtils.h"
#include "base64.h"

/**
 * @brief Check if string start with a prefix
 *
 * @retval TRUE if substring is found in bigstring
 */
bool player_StartsWith( const char *inputStr, const char *prefix )
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
char *base64_URL_Encode(const unsigned char *src, size_t len)
{
	char *rc = Base64Utils::base64Encode(src,len);
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
unsigned char *base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
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
		rc = Base64Utils::base64Decode(temp, len );
		free(temp);
	}
	else
	{
		*len = 0;
	}
	return rc;
}

static std::hash<std::thread::id> std_thread_hasher;

std::size_t GetThreadID( const std::thread &t )
{
	return std_thread_hasher( t.get_id() );
}

std::size_t GetThreadID( void )
{
	return std_thread_hasher( std::this_thread::get_id() );
}

/**
 * @brief support for POSIX threads
 */
std::size_t GetThreadID( const pthread_t &t )
{
	static std::hash<pthread_t> pthread_hasher;
	return pthread_hasher( t );
}

/**
 * @brief Get current time from epoch is milliseconds
 * @retval - current time in milliseconds
 */
long long GetCurrentTimeMS(void)
{
        struct timeval t;
        gettimeofday(&t, NULL);
        return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
}

