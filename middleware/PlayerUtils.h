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

#ifndef __PLAYER_UTILS_H__
#define __PLAYER_UTILS_H__

/**
 * @file PlayerUtils.h
 * @brief Context-free common utility functions.
 */

#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <sys/time.h>
#include <inttypes.h>
#include <iostream>
#include <cstring>

//Delete non-array object
#define MW_SAFE_DELETE(ptr) { delete(ptr); ptr = NULL; }
//Delete Array object
#define MW_SAFE_DELETE_ARRAY(ptr) { delete [] ptr; ptr = NULL; }


#define WRITE_HASCII( DST, BYTE ) \
{ \
	*DST++ = "0123456789abcdef"[BYTE>>4]; \
	*DST++ = "0123456789abcdef"[BYTE&0xf]; \
}

/**
 * @fn player_StartsWith
 *
 * @param[in] inputStr - Input string
 * @param[in] prefix - substring to be searched
 */
bool player_StartsWith( const char *inputStr, const char *prefix);

/**
 * @fn base64_URL_Encode
 * @param src pointer to first byte of binary data to be encoded
 * @param len number of bytes to encode
 */
char *base64_URL_Encode(const unsigned char *src, size_t len);

/**
 * @fn base64_URL_Decode
 * @param src pointer to cstring containing base64-URL-encoded data
 * @param len receives byte length of returned pointer, or zero upon failure
 * @param srcLen source data length
 */
unsigned char *base64_URL_Decode(const char *src, size_t *len, size_t srcLen);

std::size_t GetThreadID( const std::thread &t );
std::size_t GetThreadID( void );
std::size_t GetThreadID( const pthread_t &t );

std::size_t GetPrintableThreadID( const pthread_t &t );
std::size_t GetPrintableThreadID();
/**
 * @fn ResolveURL
 *
 * @param[out] dst - Created URL
 * @param[in] base - Base URL
 * @param[in] uri - File path
 * @param[in] bPropagateUriParams - flag to use base uri params
 * @retval void
 */
void ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams);
/**
 * @fn GetCurrentTimeMS
 * @brief Get the current time in milliseconds
 *
 * @return The current time in milliseconds
 */
long long GetCurrentTimeMS(void);
/**
 * @brief Resolve file URL from the base and file path
 */
void player_ResolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams);

/**
 * @brief parse leading protocol from uri if present
 * @param[in] uri manifest/ fragment uri
 * @retval return pointer just past protocol (i.e. http://) if present (or) return NULL uri doesn't start with protcol
 */
static const char * ParseUriProtocol(const char *uri);

#endif  /* __PLAYER_UTILS_H__ */

