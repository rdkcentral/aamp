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
#include <type_traits>
#include "AampLogManager.h"
#include "AampUtils.h"
#include "AampMPDPeriodInfo.h"
#include "AampFragmentDescriptor.hpp"
#include "AampConfig.h"

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
bool ParseSegmentIndexBox( const uint8_t *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset);

/**
 * @brief Lightweight cursor for reading big-endian ISOBMFF box fields.
 *
 * Provides typed Read<T>() and Skip<T>() operations that replace the
 * legacy Read16/Read32/Read64 free functions and raw pointer arithmetic.
 * The caller is responsible for validating the box size field before
 * reading individual fields.
 */
class BoxReader final
{
public:
	constexpr explicit BoxReader(const uint8_t *data) noexcept
		: mCursor{data}
	{
	}

	/**
	 * @brief Read a big-endian integer field and advance the cursor.
	 * @tparam T  Integral type whose sizeof determines the field width.
	 * @return    The value read in host byte order.
	 */
	template <typename T>
	[[nodiscard]] T Read() noexcept
	{
		static_assert(std::is_integral_v<T>,
					  "Read<T> requires an integral type");
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
					  sizeof(T) == 4 || sizeof(T) == 8,
					  "Read<T> only supports 1/2/4/8-byte types");

		uint64_t val{0};
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			val = (val << 8) | static_cast<uint8_t>(mCursor[i]);
		}
		mCursor += sizeof(T);
		return static_cast<T>(val);
	}

	/**
	 * @brief Skip a field without reading it.
	 * @tparam T  Integral type whose sizeof determines the skip width.
	 */
	template <typename T>
	void Skip() noexcept
	{
		static_assert(std::is_integral_v<T>,
					  "Skip<T> requires an integral type");
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
					  sizeof(T) == 4 || sizeof(T) == 8,
					  "Skip<T> only supports 1/2/4/8-byte types");

		mCursor += sizeof(T);
	}

	/**
	 * @brief Skip a runtime number of bytes.
	 * @param n  Number of bytes to advance.
	 */
	void Skip(size_t n) noexcept
	{
		mCursor += n;
	}

private:
	const uint8_t *mCursor;
};

/**
 * @brief Replace matching token with given number
 * @param str String in which operation to be performed
 * @param from token
 * @param toNumber number to replace token
 * @retval position
 */
int replace(std::string& str, const std::string& from, uint64_t toNumber );

/**
 * @brief Replace matching token with given string
 * @param str String in which operation to be performed
 * @param from token
 * @param toString string to replace token
 * @retval position
 */
int replace(std::string& str, const std::string& from, const std::string& toString );

#endif /* __AAMP_MPD_UTILS_H__ */
