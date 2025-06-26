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
 * @file _base64.cpp
 * @brief optimized pair of base64 encode/decode implementations
 */

#include "_base64.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief convert blob of binary data to ascii base64-encoded equivalent
 * @retval pointer to malloc'd cstring containing base64 encoded version of string
 * @retval NULL if insufficient memory to allocate base64-encoded copy
 * @note caller responsible for freeing returned cstring
 */
char *base64_Encode(const unsigned char *src, size_t len)
{
	const unsigned char *fin = &src[len];
	const static char *encode = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // base64
	char *rc = (char *)malloc(((len+2)/3)*4+1);
	if( rc )
	{
		char *dst = rc;
		unsigned int temp;
		int pad = 0;
		for( int i=0; i<len; i+=3 )
		{
			temp = (*src++) << 16;
			if( src<fin ) temp |= (*src++) << 8; else pad++;
			if( src<fin ) temp |= (*src++); else pad++;
			*dst++ = encode[(temp & 0x00FC0000) >> 18];
			*dst++ = encode[(temp & 0x0003F000) >> 12];
			*dst++ = (pad>=2)?'=':encode[(temp & 0x00000FC0) >> 6 ];
			*dst++ = (pad>=1)?'=':encode[(temp & 0x0000003F)];
		}
		*dst++ = 0x00;
	}
	return rc;
}

/**
 * @brief decode base64 encoded data to binary equivalent
 * @retval return value from base64_Decode value
 */
unsigned char *base64_Decode(const char *src, size_t *outLen, size_t srcLen)
{
	static const signed char decode[256] =
	{
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 63, -1, 63,
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
		-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
		-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};
	unsigned char *rc = (unsigned char *)malloc(srcLen*3/4);
	*outLen = 0; // default
	if( rc )
	{
		unsigned char *dst = rc;
		while( srcLen>0 && src[srcLen-1] == '=' )
		{ // strip padding
			srcLen--;
		}
		const char *fin = &src[srcLen];
		while( src<fin )
		{
			unsigned int buf = 0;
			int count = 0;
			for( int i=0; i<4; i++ )
			{
				buf<<=6;
				if( src<fin )
				{
					unsigned char c = (unsigned char)*src++;
					int digit64 = decode[c];
					if( digit64<0 )
					{ // invalid character
						free( rc );
						return NULL;
					}
					buf |= digit64;
					count++;
				}
			}
			if( count>=2 ) *dst++ = (buf>>(8*2))&0xff;
			if( count>=3 ) *dst++ = (buf>>(8*1))&0xff;
			if( count>=4 ) *dst++ = (buf>>(8*0))&0xff;
		}
		*outLen = dst-rc;
	}
	return rc;
}

unsigned char *base64_Decode(const char *src, size_t *len)
{
	return base64_Decode(src, len, strlen(src));
}
