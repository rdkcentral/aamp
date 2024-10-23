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
#include "dash_adapter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include "stream_utils.hpp"
#include "string_utils.hpp"
#include "downloader.hpp"

static class Console console;
typedef enum
{
	eMEDIATYPE_UNKNOWN,
	eMEDIATYPE_MANIFEST,
	eMEDIATYPE_PLAYLIST_VIDEO,
	eMEDIATYPE_PLAYLIST_AUDIO,
} MediaType;

void parseSIDX( MediaData &obj, const ArrayBuffer &arrayBuffer, uint64_t baseContentOffset )
{
	uint16_t reference_count = 0;
	DataView dataView(arrayBuffer);
	
	auto len = dataView.getUint32(0);
	if( len != dataView.byteLength )
	{
		console.log( "unexpected SIDX length!" );
	}
	else
	{
		auto type = dataView.getUint32(4);
		if( type != 0x73696478 ) // 'sidx'
		{
			console.log( "unexpected SIDX type!" );
		}
		else
		{
			auto version = dataView.getUint32(8);
			auto reference_ID = dataView.getUint32(12);
			(void)reference_ID;
			int idx = 0;
			obj.timescale = dataView.getUint32(16);
			//obj.duration = [];
			//obj.time = [];
			//obj.media = [];
			if( version )
			{
				uint64_t earliest_presentation_time = ((uint64_t)dataView.getUint32(20)<<32)|dataView.getUint32(24);
				(void)earliest_presentation_time;
				baseContentOffset += ((uint64_t)dataView.getUint32(28)<<32)|dataView.getUint32(32);
				// skip 16 bit 'reserved' field
				reference_count = dataView.getUint16(38);
				idx = 40;
			}
			else
			{
				auto earliest_presentation_time = dataView.getUint32(20);
				(void) earliest_presentation_time;
				baseContentOffset += dataView.getUint32(24);
				// skip 16 bit 'reserved' field
				reference_count = dataView.getUint16(30);
				idx = 32;
			}
			uint64_t t = 0;
			for( auto segmentIndex=0; segmentIndex<reference_count; segmentIndex++ )
			{
				auto byteLength = dataView.getUint32(idx) & 0x7fffffff; // top bit is "reference_type"
				uint32_t d = dataView.getUint32(idx+4);
				auto flags = dataView.getUint32(idx+8);
				(void)flags;
				auto next = baseContentOffset+byteLength;
				std::string range = "@";
				range += std::to_string(baseContentOffset);
				range += '-';
				range += std::to_string(next-1);
				obj.media.push_back( range );
				//obj.media.push( "@"+baseContentOffset+"-"+(next-1) );
				baseContentOffset = next;
				obj.time.push_back(t);
				obj.duration.push_back(d);
				t += d;
				idx += 12;
			}
		}
	}
}

void unsuportedTag( const XmlNode &child, const XmlNode &parent )
{
	static const char *tag[] =
	{
		"Accessibility",
		"AssetIdentifier",
		"AudioChannelConfiguration",
		"AvailableBitrates"
		"body",
		"BufferLevel",
		"EssentialProperty",
		"EventStream",
		"InbandEventStream",
		"Label",
		"Metrics",
		"parsererror",
		"ProducerReferenceTime",
		"ProgramInformation",
		"Resync",
		"sand:Channel",
		"ServiceDescription",
		"SupplementalProperty",
		"Switching",
		"UTCTiming",
		"Viewpoint",
	};
	for( int i=0; i<sizeof(tag)/sizeof(tag[0]); i++ )
	{
		if( child.tagName == tag[i] )
		{
			//std::cout << child.tagName << "\n";
			return;
		}
	}
	std::cout << parent.tagName << " has unsupported tag: " << child.tagName << "\n";
}

std::string getXmlNodeText( const XmlNode &node )
{
	return node.innerHTML; // turboXML
	//return node.childNodes[0].nodeValue; // HTML
}

std::string appendURL( std::string base, std::string next )
{
	if( starts_with(next,"http:") || starts_with(next,"https:") )
	{
		return next;
	}
	return base+next;
}

void parseContentType( AdaptationSet &obj, const XmlNode &node )
{
	if( node.hasAttribute("contentType") )
	{
		obj.contentType = node.getAttribute("contentType");
	}
	if( node.hasAttribute("codecs") )
	{
		obj.codecs = node.getAttribute("codecs");
		if( obj.contentType.empty() && obj.codecs == "stpp" )
		{
			obj.contentType = "text";
		}
	}
	if( node.hasAttribute("mimeType") )
	{
		obj.mimeType = node.getAttribute("mimeType");
		if( obj.contentType.empty() )
		{
			auto delim = obj.mimeType.find("/");
			obj.contentType = obj.mimeType.substr(0,delim);
		}
	}
}

void parseSegmentTiming( MediaData &obj, const XmlNode &node )
{
	if( node.hasAttribute("timescale") )
	{ // units per second
		obj.timescale = (uint32_t)Number(node.getAttribute("timescale"));
	}
	else
	{
		obj.timescale = 1; // default
	}
	if( node.hasAttribute("startNumber") )
	{
		obj.startNumber = Number(node.getAttribute("startNumber"));
	}
	else
	{
		obj.startNumber = 1; // default
	}
	if( node.hasAttribute("duration") )
	{
		obj.duration.push_back( Number(node.getAttribute("duration") ) );
	}
}

void parseSegmentTimeline( MediaData &obj, const XmlNode &SegmentTimeline )
{
	uint64_t t = 0;
	for( const auto pSegment : SegmentTimeline.children )
	{
		const XmlNode &Segment = *pSegment; // hack for . syntax
		if( Segment.tagName == "S" )
		{
			if( Segment.hasAttribute("t") )
			{
				t = Number(Segment.getAttribute("t"));
			}
			uint64_t d = Number(Segment.getAttribute("d"));
			uint64_t repeat = 0;
			if( Segment.hasAttribute("r") )
			{
				repeat = Number(Segment.getAttribute("r"));
			}
			if( repeat<0 )
			{
				repeat = 20; // HACK!
			}
			for( auto i=0; i<=repeat; i++ )
			{
				obj.time.push_back( t );
				obj.duration.push_back( d );
				t += d;
			}
		}
		else
		{
			unsuportedTag( Segment, SegmentTimeline );
		}
	}
}

void parsePTO( MediaData &obj, const XmlNode &node ) // may be present on SegmentBase or SegmentTemplate
{
	if( node.hasAttribute("presentationTimeOffset") )
	{
		//manifest_characteristics |= FLAG_PTO;
		obj.presentationTimeOffset = Number(node.getAttribute("presentationTimeOffset"));
	}
	if( node.hasAttribute("availabilityTimeOffset") )
	{
		obj.availabilityTimeOffset = Number(node.getAttribute("availabilityTimeOffset"));
	}
}

void parseSegmentBase( MediaData &obj, const XmlNode &SegmentBase, MediaType mediaType, std::string BaseURL, Timeline timeline )
{
	//manifest_characteristics |= FLAG_SEGMENT_BASE;
	timeline.pending++;
	parsePTO( obj, SegmentBase );
	auto indexRange = SegmentBase.getAttribute("indexRange");
	auto indexSplit = splitString(indexRange,'-');
	auto baseContentOffset = 1+Number(indexSplit[1]);
	(void)baseContentOffset;
	for( const auto pInitialization : SegmentBase.children )
	{
		const XmlNode &Initialization = *pInitialization; // hack for . syntax
		if( Initialization.tagName == "Initialization" )
		{
			obj.initialization = "@"+Initialization.getAttribute("range");
		}
		else
		{
			unsuportedTag( Initialization, SegmentBase );
		}
	}
	if( obj.initialization.empty() )
	{
		obj.initialization = "@0-"+std::to_string(Number(indexSplit[0])-1);
	}
#if 1
	auto url = BaseURL + "@"+indexRange;
	gsize len = 0;
	gpointer ptr = LoadUrl( url, &len );
	auto arrayBuffer = new ArrayBuffer(ptr,len);
	parseSIDX( obj, *arrayBuffer, baseContentOffset );
	g_free( ptr );
#else
	auto xhr = new XMLHttpRequest();
	xhr.responseType = 'arraybuffer';
	auto url = BaseURL + "@"+indexRange;
	GetFile( xhr, url, mediaType, obj.bandwidth,
			function() {
		parseSIDX( obj, xhr.response, baseContentOffset );
		timeline.pending--;
		if( timeline.pending == 0 )
		{
			console.log( "async SIDX loading complete!" );
			completion( timeline );
		}
	} );
#endif
}

void parseSegmentTemplate( MediaData &obj, const XmlNode &SegmentTemplate )
{
	//manifest_characteristics |=  FLAG_SEGMENT_TEMPLATE;
	parsePTO( obj, SegmentTemplate );
	if( SegmentTemplate.hasAttribute("initialization") )
	{
		obj.initialization = SegmentTemplate.getAttribute("initialization");
	}
	if( SegmentTemplate.hasAttribute("media") )
	{
		obj.media.push_back( SegmentTemplate.getAttribute("media") );
	}
	parseSegmentTiming( obj, SegmentTemplate );
	for( const auto pSegmentTimeline : SegmentTemplate.children  )
	{
		const XmlNode &SegmentTimeline = *pSegmentTimeline; // hack for . syntax
		if( SegmentTimeline.tagName == "SegmentTimeline" )
		{
			parseSegmentTimeline( obj, SegmentTimeline );
		}
		else
		{
			unsuportedTag( SegmentTimeline, SegmentTemplate );
		}
	}
}

void parseSegmentList( MediaData &obj, const XmlNode &SegmentList )
{
	//manifest_characteristics |=  FLAG_SEGMENT_LIST;
	//obj.media = [];
	parseSegmentTiming( obj, SegmentList );
	for( const auto pChild : SegmentList.children )
	{
		const XmlNode &child = *pChild; // hack for . syntax
		if( child.tagName == "Initialization" )
		{
			if( child.hasAttribute("sourceURL") )
			{
				obj.initialization = child.getAttribute("sourceURL");
			}
			else
			{
				obj.initialization = "@"+child.getAttribute("range");
			}
		}
		else if( child.tagName == "SegmentURL" )
		{
			if( child.hasAttribute("media") )
			{
				obj.media.push_back( child.getAttribute("media") );
			}
			else
			{
				auto mediaRange = child.getAttribute("mediaRange");
				auto indexRange = child.getAttribute("indexRange"); // TODO
				obj.media.push_back( "@" + mediaRange );
			}
		}
		else
		{
			unsuportedTag( child, SegmentList );
		}
	}
}

void parseRepresentation( Representation &representation, const XmlNode &Representation, std::string BaseURL, AdaptationSet &adaptationSet, Timeline &timeline )
{
	//JsonObj representation;// = {};
	
	representation.BaseURL = BaseURL;
	if( Representation.hasAttribute("id") )
	{
		representation.id = Representation.getAttribute("id");
	}
	representation.bandwidth = Number(Representation.getAttribute("bandwidth"));
	
	// inherit from adaptationSet
	representation.data = adaptationSet.data;
	
	parseContentType( adaptationSet, Representation );
	if( adaptationSet.contentType == "video" )
	{
		if( Representation.hasAttribute("width") )
		{
			representation.width = Number(Representation.getAttribute("width"));
		}
		else
		{
			representation.width = adaptationSet.maxWidth;
		}
		if( Representation.hasAttribute("height") )
		{
			representation.height = Number(Representation.getAttribute("height"));
		}
		else
		{
			representation.height = adaptationSet.maxHeight;
		}
		if( Representation.hasAttribute("frameRate") )
		{
			representation.frameRate = Number(Representation.getAttribute("frameRate"));
		}
		else
		{
			representation.frameRate = adaptationSet.frameRate;
		}
	}
	else if( adaptationSet.contentType == "audio" )
	{
		if( Representation.hasAttribute("audioSamplingRate") )
		{
			representation.audioSamplingRate = Number(Representation.getAttribute("audioSamplingRate"));
		}
		else
		{
			representation.audioSamplingRate = adaptationSet.audioSamplingRate;
		}
	}
	
	for( const auto pChild : Representation.children )
	{
		const XmlNode &child = *pChild; // hack for . syntax
		auto tagName = child.tagName;
		if( tagName == "SegmentList" )
		{
			parseSegmentList( representation.data, child );
			representation.BaseURL = BaseURL;
		}
		else if( tagName == "SegmentBase" )
		{
			MediaType mediaType = eMEDIATYPE_UNKNOWN;
			if( adaptationSet.contentType == "video"  )
			{
				mediaType = eMEDIATYPE_PLAYLIST_VIDEO;
			}
			else if( adaptationSet.contentType == "audio" )
			{
				mediaType = eMEDIATYPE_PLAYLIST_AUDIO;
			}
			parseSegmentBase( representation.data, child, mediaType, BaseURL, timeline );
			representation.BaseURL = BaseURL;
		}
		else if( tagName == "SegmentTemplate" )
		{
			parseSegmentTemplate( representation.data, child );
			representation.BaseURL = BaseURL;
		}
		else if( tagName == "BaseURL" )
		{
			if( child.hasAttribute("availabilityTimeOffset") )
				console.log( "Representation BaseURL has availabiltyTimeOffset!" );
			BaseURL = appendURL( BaseURL,getXmlNodeText(child) );
		}
		else if( tagName == "AudioChannelConfiguration" )
		{
			auto schemeIdUri = child.getAttribute("schemeIdUri"); // urn:mpeg:dash:23003:3:audio_channel_configuration:2011
			auto value = child.getAttribute("value"); // 1
		}
		else
		{
			unsuportedTag( child, Representation );
		}
	}
}

void parseContentProtection( const XmlNode &ContentProtection, std::string BaseURL, AdaptationSet &adaptationSet )
{
	//manifest_characteristics |= FLAG_ENCRYPTED;
	
	auto type = ContentProtection.getAttribute("schemeIdUri");
	if( type == "urn:uuid:e2719d58-a985-b3c9-781a-b030af78d30e" )
	{ // CLEAR_KEY_DASH_IF
		for( const auto pChild : ContentProtection.children )
		{
			const XmlNode &child = *pChild; // hack for . syntax
			auto tagName = child.tagName;
			if( tagName == "dashif:laurl" )
			{
				adaptationSet.licenseURL = appendURL( BaseURL, child.innerHTML );
			}
		}
	}
}

bool parseAdaptationSet( AdaptationSet &adaptationSet, const XmlNode &AdaptationSet, std::string BaseURL, Timeline &timeline )
{
	parseContentType( adaptationSet, AdaptationSet );
	if( AdaptationSet.hasAttribute("id") )
	{
		adaptationSet.id = AdaptationSet.getAttribute("id");
	}
	if( AdaptationSet.hasAttribute("maxWidth") )
	{
		adaptationSet.maxWidth = Number(AdaptationSet.getAttribute("maxWidth"));
	}
	if( AdaptationSet.hasAttribute("maxHeight") )
	{
		adaptationSet.maxHeight = Number(AdaptationSet.getAttribute("maxHeight"));
	}
	if( AdaptationSet.hasAttribute("frameRate") )
	{
		adaptationSet.frameRate = Number(AdaptationSet.getAttribute("frameRate"));
	}
	if( AdaptationSet.hasAttribute("audioSamplingRate") )
	{
		adaptationSet.audioSamplingRate = Number(AdaptationSet.getAttribute("audioSamplingRate"));
	}
	if( AdaptationSet.hasAttribute("lang") )
	{
		adaptationSet.lang = AdaptationSet.getAttribute("lang"); // audio only?
	}
	for( const auto pChild : AdaptationSet.children )
	{
		const XmlNode &child = *pChild; // hack for . syntax
		auto tagName = child.tagName;
		if( tagName == "Representation" )
		{
			Representation representation;
			parseRepresentation( representation, child, BaseURL, adaptationSet, timeline );
			adaptationSet.representation.push_back( representation );
		}
		else if( tagName == "BaseURL" )
		{
			if( child.hasAttribute("availabilityTimeOffset") )
			{
				console.log( "AdaptationSet BaseURL has availabiltyTimeOffset!" );
			}
			BaseURL = appendURL( BaseURL,getXmlNodeText(child) );
		}
		else if( tagName == "ContentProtection" )
		{
			parseContentProtection( child, BaseURL, adaptationSet );
		}
		else if( tagName == "Role" )
		{
			auto schemeIdUri = child.getAttribute("schemeIdUri"); // urn:mpeg:dash:role:2011
			auto value = child.getAttribute("value"); // main
		}
		else if( tagName == "EssentialProperty" )
		{
			auto schemeIdUri = child.getAttribute("schemeIdUri");
			auto value = child.getAttribute("value");
			printf( "***EssentialProperty: %s\n", schemeIdUri.c_str() );
			if( schemeIdUri == "http://dashif.org/guidelines/trickmode" && Number(value) != 0 )
			{ // skip iframe track
				return false;
			}
		}
		else if( tagName == "SegmentTemplate" )
		{
			parseSegmentTemplate( adaptationSet.data, child );
		}
		else
		{
			unsuportedTag( child, AdaptationSet );
		}
	}
	return true;
}

void parsePeriod( PeriodObj &period, const XmlNode &Period, std::string BaseURL, Timeline &timeline )
{
	if( Period.hasAttribute("id") )
	{
		period.id = Period.getAttribute("id");
	}
	if( Period.hasAttribute("start") )
	{
		period.start = parseDuration(Period.getAttribute("start"));
	}
	if( Period.hasAttribute("duration") )
	{
		period.duration = parseDuration(Period.getAttribute("duration"));
	}
	for( const auto pChild : Period.children )
	{
		const XmlNode &child = *pChild; // hack for . syntax
		auto tagName = child.tagName;
		if( tagName == "AdaptationSet" )
		{
			AdaptationSet adaptationSet;
			if( parseAdaptationSet( adaptationSet, child, BaseURL, timeline ) )
			{
				period.adaptationSet[adaptationSet.contentType] = adaptationSet;
			}
		}
		else if( child.tagName == "BaseURL" )
		{
			if( child.hasAttribute("availabilityTimeOffset") )
				console.log( "Period BaseURL has availabiltyTimeOffset!" );
			BaseURL = appendURL( BaseURL,getXmlNodeText(child) );
		}
		else
		{
			unsuportedTag( child, Period );
		}
	}
}

/**
let AvailabilityWindowStart = tNow - MPD@timeShiftBufferDepth;
let TotalAvailabilityTimeOffset = sum of all @availabilityTimeOffset from SegmentBase, SegmentTemplate or BaseURL elements
availability window: time span from AvailabilityWindowStart to tNow + TotalAvailabilityTimeOffset.
 */
void parseManifestAttributes( Timeline &obj, const XmlNode &node )
{
	if( node.hasAttribute("maxSegmentDuration") )
	{
		obj.maxSegmentDuration = parseDuration(node.getAttribute("maxSegmentDuration"));
	}
	if( node.hasAttribute("minBufferTime") )
	{
		obj.minBufferTime = parseDuration(node.getAttribute("minBufferTime"));
	}
	if( node.hasAttribute("mediaPresentationDuration") )
	{
		obj.mediaPresentationDuration = parseDuration(node.getAttribute("mediaPresentationDuration"));
	}
	if( node.hasAttribute("minimumUpdatePeriod") )
	{
		obj.minimumUpdatePeriod = parseDuration(node.getAttribute("minimumUpdatePeriod"));
	}
	if( node.hasAttribute("timeShiftBufferDepth") )
	{
		obj.timeShiftBufferDepth = parseDuration(node.getAttribute("timeShiftBufferDepth"));
	}
	
	if( node.hasAttribute("availabilityStartTime") )
	{
		obj.availabilityStartTime = parseDate(node.getAttribute("availabilityStartTime"));
	}
	if( node.hasAttribute("publishTime") )
	{
		obj.publishTime = parseDate(node.getAttribute("publishTime"));
	}
	
	if( node.hasAttribute("type") )
	{
		obj.type = node.getAttribute("type");
	}
}

#include "turbo_xml.hpp"

void parseManifest( const std::string manifest, const std::string url )
{
	DOMParser parser;
	auto xmlDoc = parser.parseFromString(manifest,"text/xml");
}

Timeline parseManifest( const XmlNode &MPD, const std::string url )
{
	Timeline timeline;// = []; // sequence of non-overlapping periods
	auto BaseURL = url.substr(0,url.find_last_of("/")+1);
	printf( "BaseURL: %s\n", BaseURL.c_str() );
//	auto BaseURL = url.substr(0,url.lastIndexOf("/")+1);
	timeline.url = url;
	timeline.pending = 0;
	
	parseManifestAttributes( timeline, MPD );
	
	for( const auto pChild : MPD.children )
	{
		const XmlNode &child = *pChild; // hack for . syntax
		auto tagName = child.tagName;
		if( tagName == "Period" )
		{
			PeriodObj period;
			parsePeriod( period, child, BaseURL, timeline );
			timeline.period.push_back( period );
		}
		else if( tagName == "BaseURL" )
		{
			BaseURL = appendURL( BaseURL,getXmlNodeText(child) );
		}
		else
		{
			unsuportedTag( child, MPD );
		}
	}
	return timeline;
}

#if 0
void LoadManifest( const std::string &url, CompletionHandler completion )
{
	//gErrorState = false;
	auto xhr = new XMLHttpRequest();
	GetFile( xhr, url, eMEDIATYPE_MANIFEST, 0, function(){
		auto url = xhr.responseURL;
		parseManifest(xhr.responseText,url,completion)
	} );
}
#endif
