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

/**
 * @file AampcliVirtualChannelMap.cpp
 * @brief Aampcli VirtualChannelMap handler
 */

#include "AampcliVirtualChannelMap.h"

VirtualChannelMap mVirtualChannelMap;

// coverity[ +tainted_string_sanitize_content : arg-0 ]
static bool sanitize(const char *data, size_t size)
{
	size_t length = strnlen(data, size);
	return ((length > 0) && (length < size));
}

void VirtualChannelMap::add(VirtualChannelInfo& channelInfo)
{
	if( !channelInfo.name.empty() )
	{
		if( !channelInfo.uri.empty() )
		{
			if( !VIRTUAL_CHANNEL_VALID(channelInfo.channelNumber) )
			{ // No explicit channel number set; automatically populate
				channelInfo.channelNumber = mMostRecentlyImportedVirtualChannelNumber+1;
			}

			if (isPresent(channelInfo) )
			{ // skip if duplicate
				return;
			}
			mMostRecentlyImportedVirtualChannelNumber = channelInfo.channelNumber;
		}
	}
	mVirtualChannelMap.push_back(channelInfo);
}

VirtualChannelInfo* VirtualChannelMap::find(const int channelNumber)
{
	VirtualChannelInfo *found = NULL;
	for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (channelNumber == existingChannelInfo.channelNumber)
		{
			found = &existingChannelInfo;
			break;
		}
	}
	return found;
}
bool VirtualChannelMap::isPresent(const VirtualChannelInfo& channelInfo)
{
	bool didWarn = false;
	for( auto it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		const VirtualChannelInfo &existingChannelInfo = *it;
		if(channelInfo.channelNumber == existingChannelInfo.channelNumber)
		{ // forbid repeated virtual channel number!
			printf("[AAMPCLI] duplicate channel number: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
			return true;
		}
		if(channelInfo.uri == existingChannelInfo.uri )
		{
			if( !didWarn )
			{ // warn for same url appearing more than once
				printf("[AAMPCLI] duplicate URL: %d: '%s'\n", channelInfo.channelNumber, channelInfo.uri.c_str() );
				didWarn = true;
			}
		}
	}
	return false;
}

// NOTE: prev() and next() are IDENTICAL other than the direction of the iterator. They could be collapsed using a template,
// but will all target compilers support this, it wouldn't save much code space, and may make the code harder to understand.
// Can not simply use different runtime iterators, as the types of each in C++ are not compatible (really).
VirtualChannelInfo* VirtualChannelMap::prev()
{
	VirtualChannelInfo *pPrevChannel = NULL;
	VirtualChannelInfo *pLastChannel = NULL;
	bool prevFound = false;

	// mCurrentlyTunedChannel is 0 for manually entered urls, not having used mVirtualChannelMap yet or empty
	if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
	{
		prevFound = true;  // return the last valid channel
	}

	for(std::list<VirtualChannelInfo>::reverse_iterator it = mVirtualChannelMap.rbegin(); it != mVirtualChannelMap.rend(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
		{
			if ( pLastChannel == NULL )
			{ // remember this channel for wrap case
				pLastChannel = &existingChannelInfo;
			}
			if ( prevFound )
			{
				pPrevChannel = &existingChannelInfo;
				break;
			}
			else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
			{
				prevFound = true;
			}
		}
	}
	if (prevFound && pPrevChannel == NULL)
	{
		pPrevChannel = pLastChannel;  // if we end up here we are probably at the first channel -- wrap to back
	}
	return pPrevChannel;
}

VirtualChannelInfo* VirtualChannelMap::next()
{
	VirtualChannelInfo *pNextChannel = NULL;
	VirtualChannelInfo *pFirstChannel = NULL;
	bool nextFound = false;

	// mCurrentlyTunedChannel is 0 for manually entered urls, not using mVirtualChannelMap
	if (mCurrentlyTunedChannel == 0 && mVirtualChannelMap.size() > 0)
	{
		nextFound = true; // return the first valid channel
	}

	for (std::list<VirtualChannelInfo>::iterator it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it)
	{
		VirtualChannelInfo &existingChannelInfo = *it;
		if (VIRTUAL_CHANNEL_VALID(existingChannelInfo.channelNumber) ) // skip group headings
		{
			if ( pFirstChannel == NULL )
			{ // remember this channel for wrap case
				pFirstChannel = &existingChannelInfo;
			}
			if ( nextFound )
			{
				pNextChannel = &existingChannelInfo;
				break;
			}
			else if ( existingChannelInfo.channelNumber == mCurrentlyTunedChannel )
			{
				nextFound = true;
			}
		}
	}
	if (nextFound && pNextChannel == NULL)
	{
		pNextChannel = pFirstChannel;  // if we end up here we are probably at the last channel -- wrap to front
	}
	return pNextChannel;
}

void VirtualChannelMap::print(unsigned long start, unsigned long end, unsigned long tail)
{
	if (mVirtualChannelMap.empty())
	{
		return;
	}

	printf("[AAMPCLI] aampcli.cfg virtual channel map:\n");

	int numCols = 0;
	unsigned long lineCount = 0;
	unsigned long mapSize = mVirtualChannelMap.size();
	if(end == ULLONG_MAX || end >= mapSize)
	{
		end = mapSize - 1;
	}
	if(start >= mapSize)
	{
		start = 0;
	}
	if(tail)
	{
		if(tail >= mapSize)
		{
			start = 0;
		}
		else
		{
			start = mapSize - tail;
		}
	}

	for ( auto it = mVirtualChannelMap.begin(); it != mVirtualChannelMap.end(); ++it )
	{
		if(lineCount < start)
		{
			//Skip over lines we don't want to display when a range or tail is specified.
			lineCount++;
			continue;
		}
		const VirtualChannelInfo &pChannelInfo = *it;
		std::string channelName = pChannelInfo.name.c_str();
		size_t len = channelName.length();
		int maxNameLen = 20;
		if( len>maxNameLen )
		{
			len = maxNameLen;
			channelName = channelName.substr(0,len);
		}
		if( pChannelInfo.uri.empty() )
		{
			if( numCols!=0 )
			{
				printf( "\n" );
			}
			printf( "%s\n", channelName.c_str() );
			numCols = 0;
			//Increment displayed lines here & check, as the "continue" bypasses the normal check.
			lineCount++;
			if(lineCount > end)
			{
				break;
			}			
			continue;
		}
		printf("%4d: %s", pChannelInfo.channelNumber, channelName.c_str() );
		if( numCols>=4 )
		{ // four virtual channels per row
			printf("\n");
			numCols = 0;
		}
		else
		{
			while( len<maxNameLen )
			{ // pad each column to 20 characters, for clean layout
				printf( " " );
				len++;
			}
			numCols++;
		}
		//Check if we've reached the end of lines we want to display
		lineCount++;
		if(lineCount > end)
		{
			break;
		}
	}
	printf("\n\n");
}

void VirtualChannelMap::setCurrentlyTunedChannel(int value)
{
	mCurrentlyTunedChannel = value;
}

void VirtualChannelMap::showList(unsigned long start, unsigned long end, unsigned long tail)
{
	printf("******************************************************************************************\n");
	printf("*   Virtual Channel Map\n");
	printf("******************************************************************************************\n");
	print(start, end, tail);
}

void VirtualChannelMap::tuneToChannel( VirtualChannelInfo &channel, PlayerInstanceAAMP *playerInstanceAamp, bool bAutoPlay)
{
	setCurrentlyTunedChannel(channel.channelNumber);
	const char *name = channel.name.c_str();
	const char *locator = channel.uri.c_str();
	printf( "TUNING to '%s' %s\n", name, locator );
	playerInstanceAamp->Tune(locator,bAutoPlay);
}

std::string VirtualChannelMap::getNextFieldFromCSV( const char **pptr )
{
	const char *ptr = *pptr;
	const char *delim = ptr;
	const char *next = ptr;

	if (!isprint(*ptr) && *ptr != '\0')
	{  // Skip BOM UTF-8 start codes and not end of string
		while (!isprint(*ptr) && *ptr != '\0')
		{
			ptr++;
		}
		delim = ptr;
	}

	if( *ptr=='\"' )
	{ // Skip startquote
		ptr++;
		delim  = strchr(ptr,'\"');
		if( delim )
		{
			next = delim+1; // skip endquote
		}
		else
		{
			delim = ptr;
		}
	}
	else
	{  // Include space and greater chars and not , and not end of string
		while( *delim >= ' ' && *delim != ',' && *delim != '\0')
		{
			delim++;
		}
		next = delim;
	}

	if( *next==',' ) next++;
	*pptr = next;

	return std::string(ptr,delim-ptr);
}

void VirtualChannelMap::loadVirtualChannelMapFromCSV( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		// CID:280549 - Untrusted loop bound
		if (sanitize(buf, sizeof(buf)))
		{
			VirtualChannelInfo channelInfo;
			const char *ptr = buf;
			std::string channelNumber = getNextFieldFromCSV( &ptr );
			// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
			channelInfo.channelNumber = atoi(channelNumber.c_str());
			channelInfo.name = getNextFieldFromCSV(&ptr);
			channelInfo.uri = getNextFieldFromCSV(&ptr);
			if (!channelInfo.name.empty() && !channelInfo.uri.empty())
			{
				add( channelInfo );
			}
			else
			{ // no name, no uri, no service
				//printf("[AAMPCLI] can not parse virtual channel '%s'\n", buf);
			}
		}
	}
}

/**
 * @brief Parse config entries for aamp-cli, and update mVirtualChannelMap
 *        based on the config.
 * @param f File pointer to config to process
 */
void VirtualChannelMap::loadVirtualChannelMapLegacyFormat( FILE *f )
{
	char buf[MAX_BUFFER_LENGTH];
	while (fgets(buf, sizeof(buf), f))
	{
		// CID:280433 - Untrusted loop bound
		if (!sanitize(buf, sizeof(buf)))
		{
			// Skip blank or overlong line
			continue;
		}

		const char *ptr = buf;
		ptr = skipwhitespace(ptr);
		if( *ptr=='#' )
		{ // comment line
			continue;
		}

		if( *ptr=='*' )
		{ // skip "*" character, if present
			ptr = skipwhitespace(ptr+1);
		}
		else
		{ // not a virtual channel
			continue;
		}

		VirtualChannelInfo channelInfo;		// extract channel number
		// invalid input results in 0 -- !VIRTUAL_CHANNEL_VALID, will be auto assigned
		channelInfo.channelNumber = atoi(ptr);
		while( *ptr>='0' && *ptr<='9' ) ptr++;
		ptr = skipwhitespace(ptr);

		// extract name
		const char *delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.name = std::string(ptr,delim-ptr);

		// extract locator
		ptr = skipwhitespace(delim);
		delim = ptr;
		while( *delim>' ' )
		{
			delim++;
		}
		channelInfo.uri = std::string(ptr,delim-ptr);

		add( channelInfo );
	}
} // loadVirtualChannelMapLegacyFormat

const char *VirtualChannelMap::skipwhitespace( const char *ptr )
{
	while( *ptr==' ' ) ptr++;
	return ptr;
}

