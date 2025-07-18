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
#ifndef dash_adapter_hpp
#define dash_adapter_hpp

#include <string>
#include <stdint.h>
#include <vector>
#include <map>
#include "turbo_xml.hpp"
#include <cinttypes>
#include <sstream>
#include "downloader.hpp"

class MediaData
{ // associated with a presenting Representation, but can be inherited from an AdaptationSet
public:
	std::string initialization;
	std::vector<std::string> media; // for SegmentList, will be vector
	std::vector<uint64_t> duration; // for SegmentTimeline, will be vector
	std::vector<uint64_t> time; // for SegmentTimeline, will be vector
	uint64_t startNumber;
	uint64_t presentationTimeOffset;
	uint64_t availabilityTimeOffset;
	uint32_t timescale;
	
	void Debug( void ) const
	{
		std::stringstream stream;
		
		stream << "initialization: " << initialization << "\n";
		
		stream << "media:";
		for( auto it : media ) stream << " " << it;
		stream << "\n";
		
		stream << "duration:";
		for( auto it : duration ) stream << " " << it;
		stream << "\n";
		
		stream << "time:";
		for( auto it : time ) stream << " " << it;
		stream << "\n";
		
		stream << "startNumber=" << startNumber << "\n";
		stream << "presentationTimeOffset=" << presentationTimeOffset << "\n";
		stream << "availabilityTimeOffset=" << availabilityTimeOffset << "\n";
		stream << "timescale=" << timescale << "\n";
		
		puts( stream.str().c_str() );
	}
};

class Representation
{
public:
	std::string BaseURL;
	std::string id;
	long bandwidth;
	long width;
	long height;
	long frameRate;
	long audioSamplingRate;
	
	class MediaData data;
	
	void Debug( void ) const
	{
		std::stringstream stream;
		stream << "Representation:\n";
		stream << "id=" << id << "\n";
		stream << "BaseURL=" << BaseURL << "\n";
		stream << "bandwidth=" << bandwidth << "\n";
		stream << "width=" << width << "\n";
		stream << "height=" << height << "\n";
		stream << "frameRate=" << frameRate << "\n";
		stream << "audioSamplingRate=" << audioSamplingRate << "\n";
		puts( stream.str().c_str() );
		data.Debug();
	}
};

class AdaptationSet
{
public:
	std::string id;
	std::string lang;
	std::string contentType;
	std::string codecs;
	std::string mimeType;
	std::string licenseURL;
	long maxWidth;
	long maxHeight;
	long frameRate;
	long audioSamplingRate;
	
	class MediaData data;
	
	std::vector<Representation> representation;
	
	void Debug( void ) const
	{
		std::stringstream stream;
		stream << "AdaptationSet:\n";
		stream << "id=" << id << "\n";
		stream << "lang=" << lang << "\n";
		stream << "contentType=" << contentType << "\n";
		stream << "codecs=" << codecs << "\n";
		stream << "mimeType=" << mimeType << "\n";
		stream << "audioSamplingRate=" << audioSamplingRate << "\n";
		puts( stream.str().c_str() );
		data.Debug();
		for( auto it : representation )
		{
			it.Debug();
		}
	}
};

class PeriodObj
{
public:
	std::string id;
	double start;
	double duration;
	//double timestampOffset;
	double firstPts;
	
	std::map<std::string, AdaptationSet> adaptationSet;
	
	void Debug( void ) const
	{
		printf( "\tPeriodObj\n" );
		for( auto it : adaptationSet )
		{
			it.second.Debug();
		}
	}
	
	PeriodObj(): id(),start(-1),duration(-1)//, timestampOffset(0.0)
	{
	}
	
	~PeriodObj()
	{
	}
};

class Timeline
{
public:
	long long tuneUTC;
	
	long pending;
	std::string url;
	std::vector<PeriodObj> period;
	
	// TODO: use long long (ms) here?
	double maxSegmentDuration;
	double minBufferTime;
	double mediaPresentationDuration;
	double minimumUpdatePeriod;
	double timeShiftBufferDepth;
	
	long long availabilityStartTime;
	long long publishTime;
	
	std::string type;
	
	void Debug( void ) const
	{
		printf( "Timeline:\n" );
		for( auto p : period )
		{
			p.Debug();
		}
	}
};

class ArrayBuffer
{
public:
	gpointer ptr;
	gsize len;

	ArrayBuffer( gpointer ptr, gsize len ) : ptr(ptr), len(len)
	{
	}
	~ArrayBuffer()
	{
	}
};

class DataView
{
private:
	const unsigned char *baseAddr;
	const unsigned char *fin;
	
public:
	size_t byteLength;

	DataView( const class ArrayBuffer &arrayBuffer )
	{
		byteLength = arrayBuffer.len;
		baseAddr = (const unsigned char *)arrayBuffer.ptr;
		fin = baseAddr + byteLength;
	}
	
	~DataView()
	{
	}
	
	uint32_t getUint32(long idx )
	{
		uint32_t rc = 0;
		const unsigned char *src = &baseAddr[idx];
		for( int i=0; i<4; i++ )
		{
			if( src<fin )
			{
				rc <<= 8;
				rc |= *src++;
			}
		}
		return rc;
	}
	
	uint16_t getUint16( long idx )
	{
		uint16_t rc = 0;
		const unsigned char *src = &baseAddr[idx];
		for( int i=0; i<2; i++ )
		{
			if( src<fin )
			{
				rc <<= 8;
				rc |= *src++;
			}
		}
		return rc;
	}
};

class Console
{
public:
	Console()
	{
	};
	
	~Console()
	{
	};

	static void log( const std::string &string )
	{
	}
};

void alert( const std::string &string );

class DOMParser
{
public:
	DOMParser(){}
	~DOMParser(){}
	
	XmlNode parseFromString( const std::string &data, std::string type )
	{ // stub
		XmlNode *xml = new XmlNode("document",NULL,0);
		return *xml;
	}
};

//void LoadManifest( const std::string &url, CompletionHandler completion );

Timeline parseManifest( const XmlNode &MPD, const std::string url );

#endif /* dash_adapter_hpp */
