/*
 *   Copyright 2024 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
*/
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cstdlib>
#include "extractcaptions.hpp"

static void ExtractCaptionHelper( const uint8_t *ptr, const uint8_t *next )
{
	printf(",\"");
	while( ptr<next )
	{
		char c = *ptr++;
		if( c == '>' )
		{
			while( ptr<next )
			{
				if( memcmp(ptr,"</span>",7)==0 )
				{
					printf( "\"" );
					return;
				}
				char c = *ptr++;
				if( c=='-' )
				{
					c = '~';
				}
				else if( c=='\"' )
				{
					printf( "\"" );
				}
				printf( "%c", c );
			}
			break;
		}
	}

}

void ExtractCaptions( const char *path, const uint8_t *ptr, const uint8_t *next )
{
	const char *delim = strstr(path,".mp4");
	assert( delim );
	while( *delim!='-' ) delim--;
	printf( ",%d", atoi(delim+1) );
	if( strstr(path,"TTMLdeCC") )
	{ // track#1
		printf( ",1" );
	}
	else if( strstr(path,"TTMLde") )
	{ // track#0
		printf(",0" );
	}
	else
	{ // test track
		printf( ",2");
	}
	
	while( ptr<next )
	{
		if( memcmp(ptr,"<span ",6)==0 )
		{
			ExtractCaptionHelper( ptr,next );
		}
		ptr++;
	}
	printf("\n");
	return;
}
