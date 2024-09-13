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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "extractcaptions.hpp"
#include "parsemp4.hpp"
#include "ttmlgen.hpp"

static void ParseMp4FileHelper( const char *path, const uint8_t *ptr, size_t len, bool textOnly=false )
{
	const uint8_t *next = ptr+len;
	if( textOnly )
	{
		printf( "%s", path );
		ExtractCaptions(path,ptr,next);
		return;
	}
	else
	{
		printf( "\n- - - - - - - - - - - - - - - - - -\n%s: ", path );
		//mTimeScale = 1;
		ParseBox( ptr, next );
	}
}

static void ParseMp4File( const char *path, bool textOnly=false )
{
	FILE *fIn = fopen(path,"rb");
	if( fIn )
	{
		fseek(fIn, 0, SEEK_END );
		size_t len = ftell(fIn);
		fseek( fIn, 0, SEEK_SET );
		uint8_t *data = (uint8_t *)malloc(len);
		fread(data,1,len,fIn);
		fclose( fIn );
		const uint8_t *ptr = data;
		//ParseMp4FileHelper( path, ptr, len, textOnly ); //commented the parser function as its not used currently
		free( data );
	}
	else
	{
		printf( "file not found!\n" );
		exit(1);
	}
}

int main(int argc, const char * argv[]) 
{

	if (argc > 1)
	{
		int duration = atoi(argv[1]);
		int segmentDurationS = atoi(argv[2]);
		int totalSegments = duration/segmentDurationS;
		std::cout<<"videoDuration "<<duration<<" segmentDuration "<<segmentDurationS<<" totalSegments "<<totalSegments<<std::endl;

		generateTTMLTracks(segmentDurationS,totalSegments);
	
		// recursively walk directories to find and process mp4 segments
		for(auto& p: std::filesystem::recursive_directory_iterator("."))
		{
			if( p.path().extension() == ".mp4" )
			{
				std::string path = p.path().string();
				if( path.find("text")!=std::string::npos ) // text fragments only
				{
					bool textOnly = true;
					ParseMp4File( p.path().string().c_str(), textOnly );
				}
			}
		}
	}
	
	return 0;
}
