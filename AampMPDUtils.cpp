/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include "AampMPDUtils.h"
#include <inttypes.h>

/**
 * @brief Get xml node form reader
 *
 * @retval xml node
 */
Node* MPDProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd)
{
	static int UNIQ_PID = 0;
	int type = xmlTextReaderNodeType(*reader);

	if (type != WhiteSpace && type != Text && type != XML_CDATA_SECTION_NODE)
	{
		while (type == Comment || type == WhiteSpace)
		{
			if(!xmlTextReaderRead(*reader))
			{
				AAMPLOG_WARN("xmlTextReaderRead  failed");
			}
			type = xmlTextReaderNodeType(*reader);
		}

		Node *node = new Node();
		node->SetType(type);
		node->SetMPDPath(Path::GetDirectoryPath(url));

		const char *name = (const char *)xmlTextReaderConstName(*reader);
		if (name == NULL)
		{
			SAFE_DELETE(node);
			return NULL;
		}

		int	isEmpty = xmlTextReaderIsEmptyElement(*reader);
		node->SetName(name);
		AddAttributesToNode(reader, node);

		if(!strcmp("Period", name))
		{
			if(!node->HasAttribute("id"))
			{
				// Add a unique period id. AAMP needs these in multi-period
				// static DASH assets to identify the current period.
				std::string periodId = std::to_string(UNIQ_PID++) + "-";
				node->AddAttribute("id", periodId);
			}
			else if(isAd)
			{
				// Make ad period ids unique. AAMP needs these for playing the same ad back to back.
				std::string periodId = std::to_string(UNIQ_PID++) + "-" + node->GetAttributeValue("id");
				node->AddAttribute("id", periodId);
			}
			else
			{
				// Non-ad period already has an id. Don't change dynamic period ids.
			}
		}

		if (isEmpty)
		{
			return node;
		}

		Node    *subnode = NULL;
		int     ret = xmlTextReaderRead(*reader);
		int subnodeType = xmlTextReaderNodeType(*reader);

		while (ret == 1)
		{
			if (!strcmp(name, (const char *)xmlTextReaderConstName(*reader)))
			{
				return node;
			}

			if(subnodeType != Comment && subnodeType != WhiteSpace)
			{
				subnode = MPDProcessNode(reader, url, isAd);
				if (subnode != NULL)
					node->AddSubNode(subnode);
			}

			ret = xmlTextReaderRead(*reader);
			subnodeType = xmlTextReaderNodeType(*reader);
		}
		return node;
	}
	else if (type == Text || type == XML_CDATA_SECTION_NODE) 
	{
		xmlChar* text = nullptr;
		if (type == XML_CDATA_SECTION_NODE) 
		{
			text = xmlTextReaderValue(*reader); // CDATA section
		} 
		else 
		{
			text = xmlTextReaderReadString(*reader); // Regular text node
		}
		if (text != NULL)
		{
			Node *node = new Node();
			node->SetType(type);
			node->SetText((const char*)text);
			xmlFree(text);
			return node;
		}
	}
	return NULL;
}


/**
 * @brief Add attributes to xml node
 * @param reader xmlTextReaderPtr
 * @param node xml Node
 */
void AddAttributesToNode(xmlTextReaderPtr *reader, Node *node)
{
	if (xmlTextReaderHasAttributes(*reader))
	{
		while (xmlTextReaderMoveToNextAttribute(*reader))
		{
			std::string key = (const char *)xmlTextReaderConstName(*reader);
			if(!key.empty())
			{
				std::string value = (const char *)xmlTextReaderConstValue(*reader);
				node->AddAttribute(key, value);
			}
			else
			{
				AAMPLOG_WARN("key   is null");  //CID:85916 - Null Returns
			}
		}
	}
}


/**
 * @brief Check if mime type is compatible with media type
 * @param mimeType mime type
 * @param mediaType media type
 * @retval true if compatible
 */
bool IsCompatibleMimeType(const std::string& mimeType, AampMediaType mediaType)
{
	bool isCompatible = false;

	switch ( mediaType )
	{
		case eMEDIATYPE_VIDEO:
			if (mimeType == "video/mp4")
				isCompatible = true;
			break;

		case eMEDIATYPE_AUDIO:
			if ((mimeType == "audio/webm") ||
				(mimeType == "audio/mp4"))
				isCompatible = true;
			break;

		case eMEDIATYPE_SUBTITLE:
			if ((mimeType == "application/ttml+xml") ||
				(mimeType == "text/vtt") ||
				(mimeType == "application/mp4"))
				isCompatible = true;
			break;

		default:
			break;
	}

	return isCompatible;
}

/**
 * @brief Computes the fragment duration
 * @param duration of the fragment.
 * @param timeScale value.
 * @return - computed fragment duration in double.
 */
double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale )
{
	double newduration = 2.0;
	if( duration && timeScale )
	{
		newduration =  (double)duration / (double)timeScale;
	}
	else
	{
		AAMPLOG_ERR("Invalid %u %u",duration,timeScale);
	}
	return newduration;
}

/**
 * @brief Parse segment index box
 * @note The SegmentBase indexRange attribute points to Segment Index Box location with segments and random access points.
 * @param start start of box
 * @param size size of box
 * @param segmentIndex segment index
 * @param[out] referenced_size referenced size
 * @param[out] referenced_duration referenced duration
 * @retval true on success
 */
bool ParseSegmentIndexBox( const char *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset)
{
	if (!start)
	{
		// If the fragment pointer is NULL then return from here, no need to process it further.
		return false;
	}

	const char **f = &start;

	unsigned int len = Read32(f);
	if (len != size)
	{
		AAMPLOG_WARN("Wrong size in ParseSegmentIndexBox %d found, %zu expected", len, size);
		if (firstOffset) *firstOffset = 0;
		return false;
	}

	unsigned int type = Read32(f);
	if (type != 'sidx')
	{
		AAMPLOG_WARN("Wrong type in ParseSegmentIndexBox %c%c%c%c found, %zu expected",
					 (type >> 24) % 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, size);
		if (firstOffset) *firstOffset = 0;
		return false;
	}

	unsigned int version = Read32(f); (void) version;
	unsigned int reference_ID = Read32(f); (void)reference_ID;
	unsigned int timescale = Read32(f);
	uint64_t earliest_presentation_time;
	uint64_t first_offset;
	if( version==0 )
	{
		earliest_presentation_time = Read32(f);
		(void)earliest_presentation_time; // unused
		first_offset = Read32(f);
	}
	else
	{
		earliest_presentation_time = Read64(f);
		(void)earliest_presentation_time; // unused
		first_offset = Read64(f);
	}
	unsigned int reserved = Read16(f); (void)reserved;
	unsigned int reference_count = Read16(f);
	if (firstOffset)
	{
		*firstOffset = (unsigned int)first_offset;
		return true;
	}
	if( segmentIndex<reference_count )
	{
		start += 12*segmentIndex;
		*referenced_size = Read32(f)&0x7fffffff;
		// top bit is "reference_type"

		*referenced_duration = Read32(f)/(float)timescale;

		unsigned int flags = Read32(f);
		(void)flags;
		// starts_with_SAP (1 bit)
		// SAP_type (3 bits)
		// SAP_delta_time (28 bits)

		return true;
	}
	return false;
}

/**
 * @brief read unsigned multi-byte value and update buffer pointer
 * @param[in] pptr buffer
 * @param[in] n word size in bytes
 * @retval 32 bit value
 */
uint64_t ReadWordHelper( const char **pptr, int n )
{
	const char *ptr = *pptr;
	uint64_t rc = 0;
	while( n-- )
	{
		rc <<= 8;
		rc |= (unsigned char)*ptr++;
	}
	*pptr = ptr;
	return rc;
}

/**
 * @brief Read 16 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
unsigned int Read16( const char **pptr)
{
	return (unsigned int)ReadWordHelper(pptr,2);
}

/**
 * @brief Read 32 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
unsigned int Read32( const char **pptr)
{
	return (unsigned int)ReadWordHelper(pptr,4);
}

/**
 * @brief Read 64 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
uint64_t Read64( const char **pptr)
{
	return ReadWordHelper(pptr,8);
}

/**
 * @brief Replace matching token with given number
 * @param str String in which operation to be performed
 * @param from token
 * @param toNumber number to replace token
 * @retval position
 */
int replace(std::string &str, const std::string &from, uint64_t toNumber)
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if (next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					size_t formatLen = next - pos - tokenLength - 1;
					char buf[256];
					if (formatLen > 0)
					{
						std::string format = str.substr(pos + tokenLength + 1, formatLen - 1);
						char type = str[pos + tokenLength + formatLen];
						switch (type)
						{ // don't use the number-formatting string from dash manifest as-is; map to uint64_t equivalent
						case 'd':
							format += PRIu64;
							break;
						case 'x':
							format += PRIx64;
							break;
						case 'X':
							format += PRIX64;
							break;
						default:
							AAMPLOG_WARN("unsupported template format: %s%c", format.c_str(), type);
							format += type;
							break;
						}

						snprintf(buf, sizeof(buf), format.c_str(), toNumber);
						tokenLength += formatLen;
					}
					else
					{
						snprintf(buf, sizeof(buf), "%" PRIu64 "", toNumber);
					}
					str.replace(pos, tokenLength + 2, buf);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_WARN("next is not found "); // CID:81252 - checked return
				break;
			}
		}
		if (done)
			break;
	}

	return rc;
}

/**
 * @brief Replace matching token with given string
 * @param str String in which operation to be performed
 * @param from token
 * @param toString string to replace token
 * @retval position
 */
int replace(std::string &str, const std::string &from, const std::string &toString)
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if (next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					str.replace(pos, tokenLength + 2, toString);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_ERR("Error at  next"); // CID:81346 - checked return
				break;
			}
		}

		if (done)
			break;
	}

	return rc;
}

/**
 * @fn ConstructFragmentURL
 * @param[out] fragmentUrl fragment url
 * @param[in] fragmentDescriptor descriptor
 * @param[in] media media information string
 * @param[in] config Aamp configuration
 */
void ConstructFragmentURL(std::string &fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media, AampConfig *config)
{
	std::string constructedUri = fragmentDescriptor->GetMatchingBaseUrl();
	if (media.empty())
	{
	}
	else if (aamp_IsAbsoluteURL(media))
	{ // don't pre-pend baseurl if media starts with http:// or https://
		constructedUri.clear();
	}
	else if (!constructedUri.empty())
	{
		if (config->IsConfigSet(eAAMPConfig_DASHIgnoreBaseURLIfSlash))
		{
			if (constructedUri == "/")
			{
				AAMPLOG_WARN("ignoring baseurl /");
				constructedUri.clear();
			}
		}
		// append '/' suffix to BaseURL if not already present
		if (aamp_IsAbsoluteURL(constructedUri))
		{
			if (constructedUri.back() != '/')
			{
				constructedUri += '/';
			}
		}
	}
	else
	{
		AAMPLOG_TRACE("BaseURL not available");
	}
	constructedUri += media;
	replace(constructedUri, "Bandwidth", fragmentDescriptor->Bandwidth);
	replace(constructedUri, "RepresentationID", fragmentDescriptor->RepresentationID);
	replace(constructedUri, "Number", fragmentDescriptor->Number);
	replace(constructedUri, "Time", (uint64_t)fragmentDescriptor->Time );
	aamp_ResolveURL(fragmentUrl, fragmentDescriptor->manifestUrl, constructedUri.c_str(), config->IsConfigSet(eAAMPConfig_PropagateURIParam));
}


