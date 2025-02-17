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
			return node;

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
		case eMEDIATYPE_AUX_AUDIO:
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

