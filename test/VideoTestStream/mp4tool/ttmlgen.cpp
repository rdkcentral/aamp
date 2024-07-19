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
#include "ttmlgen.hpp"

static void PackInt( std::string &out, int value )
{
	out += (char)(value>>24);
	out += (char)(value>>16);
	out += (char)(value>>8);
	out += (char)(value>>0);
}

static void PackBox( std::string &out, size_t len, int name )
{
	PackInt( out, (int)len );
	PackInt( out, name );
}

static void PackBytes( std::string &out, int len ,... )
{
	va_list args;
	va_start(args, len);
	for( int i=0; i<len; i++ )
	{
		int b = va_arg(args, int);
		assert( b>=0 && b<=0xff );
		out += (unsigned char)b;
	}
	va_end(args);
}

static std::string PackTime( int ms )
{;
	int sec = ms/1000;
	int min = sec/60;
	int hour = min/60;
	ms %= 1000;
	min%=60;
	sec%=60;
	char temp[256];
	snprintf( temp, sizeof(temp), "%02d:%02d:%02d.%03d", hour, min, sec, ms );
	return std::string(temp);
}

void GenerateTTMLSegment( const char *path, int segmentIndex, int segmentDurationS, const std::string &track )
{
	int timeScale = 48000;
	std::string ttml =
	"<?xml version=\"1.0\"?><tt xml:lang=\"de\" xmlns=\"http://www.w3.org/ns/ttml\" xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\" xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\" ttp:profile=\"http://www.w3.org/ns/ttml/profile/imsc1/text\" ttp:cellResolution=\"40 24\"><head><metadata/><layout><region xml:id=\"r1\" tts:origin=\"0.000% 95.833%\" tts:extent=\"100.000% 4.167%\" tts:textAlign=\"center\" tts:fontSize=\"80.000%\" tts:lineHeight=\"125.000%\"/></layout></head><body><div>";
	
	for( int i=0; i<segmentDurationS; i++ )
	{
		int ms = (segmentIndex*segmentDurationS + i)*1000;
		std::string startTime = PackTime(ms);
		ttml += "<p begin=\"" + startTime + "\" end=\"" + PackTime(ms+800) + "\" region=\"r1\"><span tts:color=\"#ffffffff\" tts:backgroundColor=\"#000000ff\">" + track + " " + startTime + "</span><br/></p>";
	}
	ttml += "</div></body></tt>";
	
	std::string out;
	PackBox( out, 108, 'moof' );
	PackBox( out, 16, 'mfhd' );
	PackBytes( out, 8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 );
	PackBox( out, 84, 'traf' );
	PackBox( out, 24, 'tfhd' );
	PackBytes( out, 16, 0x00,0x02,0x00,0x0a,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x15,0xf9,0x00 );
	PackBox( out, 20, 'tfdt' );
	PackBytes( out, 8, 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00 );
	int decodeTime = segmentIndex*segmentDurationS*timeScale;
	PackInt( out, decodeTime );
	PackBox( out, 32, 'trun' );
	PackInt( out, 0x00000b01 ); // version, flags
	PackInt( out, 0x00000001 ); // sample count
	PackInt( out, 0x00000074 ); // data offset
	PackInt( out, segmentDurationS*timeScale ); // 0x0015f900
	PackInt( out, (int)ttml.length() ); // sample size // ?
	PackInt( out, 0x00000000 ); // sample_composition_time_offset
	PackBox( out, ttml.length()+8, 'mdat' );
	out += ttml;
	FILE *f = fopen(path,"wb");
	assert( f );
	if( f )
	{
		fwrite(out.c_str(), 1, out.length(), f );
		fclose( f );
	}
}

void getTextTrackDetails(std::vector<std::string>& langVector) 
{
    FILE *file;
    char line[1024];

    const char *filename = "../../helper/config.sh";
    if (access(filename, F_OK) == -1) 
    {
        perror("Error: helper/config.sh file not found");
        return;
    }

    file = fopen(filename, "r"); 
    if (file == NULL) {
        perror("Error opening file");
	return;
    }

    while (fgets(line, sizeof(line), file) != NULL) 
    {
	    if (strstr(line, "LANG_639_2") != NULL) 
	    {
		    char lang[20];
		    char *token;
		    const char *delimiters = " ";
		    int i = 0;

		    sscanf(line, "LANG_639_2=(%[^)])", lang);
		    token = strtok(lang, delimiters);
		    while (token != NULL) 
		    {
			    langVector.push_back(std::string(token));
			    i++;
			    token = strtok(NULL, delimiters);
		    }
	    }
    }

    fclose(file);
}

void generateTTMLTracks(int segmentDurationS, int totalSegments)
{
	std::vector<std::string> langVector;
	getTextTrackDetails(langVector);
	for (int track = 0; track < langVector.size(); track++) 
	{
		for( int segmentIndex=0; segmentIndex<totalSegments; segmentIndex++ )
		{
			char path[50];
			snprintf( path, sizeof(path), "ttml_%s_%02d.mp4",langVector[track].c_str(), segmentIndex+1 );
			GenerateTTMLSegment( path, segmentIndex, segmentDurationS, langVector[track] );
		}
	}
}
