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

/**************************************
* @file AampMPDUtils.h
* @brief MPD Utils for Aamp
**************************************/

#ifndef __AAMP_MPD_UTILS_H__
#define __AAMP_MPD_UTILS_H__

#include "libdash/xml/Node.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>
#include <thread>
#include "AampLogManager.h"
#include "AampUtils.h"
#include "AampMPDPeriodInfo.h"

using namespace dash;
using namespace std;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash::helpers;


/**
 * @brief Get xml node form reader
 *
 * @retval xml node
 */
Node* MPDProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd=false);


/**
 * @brief Add attributes to xml node
 * @param reader xmlTextReaderPtr
 * @param node xml Node
 */
void AddAttributesToNode(xmlTextReaderPtr *reader, Node *node);


/**
 * @brief Check if mime type is compatible with media type
 * @param mimeType mime type
 * @param mediaType media type
 * @retval true if compatible
 */
bool IsCompatibleMimeType(const std::string& mimeType, AampMediaType mediaType);

/**
 * @brief Computes the fragment duration
 * @param duration of the fragment.
 * @param timeScale value.
 * @return - computed fragment duration in double.
 */
double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale );

/**
 * @fn ConstructFragmentURL
 * @param[out] fragmentUrl fragment url
 * @param[in] fragmentDescriptor descriptor
 * @param[in] media media information string
 * @param[in] config AAMP configuration
 */
void ConstructFragmentURL( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media, AampConfig *config);

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
bool ParseSegmentIndexBox( const char *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset);

#endif /* __AAMP_MPD_UTILS_H__ */
