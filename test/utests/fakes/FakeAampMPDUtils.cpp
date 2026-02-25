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
 * @brief Computes the fragment duration
 * @param duration of the fragment.
 * @param timeScale value.
 * @return - computed fragment duration in double.
 */
double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale )
{
	return 0;
}

/**
 * @brief Check if mime type is compatible with media type
 * @param mimeType mime type
 * @param mediaType media type
 * @retval true if compatible
 */
bool IsCompatibleMimeType(const std::string& mimeType, AampMediaType mediaType)
{
	return false;
}

/**
 * @fn ConstructFragmentURL
 * @param[out] fragmentUrl fragment url
 * @param[in] fragmentDescriptor descriptor
 * @param[in] media media information string
 * @param[in] config AAMP configuration
 */
void ConstructFragmentURL( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media, AampConfig *config)
{
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
bool ParseSegmentIndexBox( const uint8_t *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset)
{
	return true;
}

/**
 * @brief Read 16 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
unsigned int Read16( const char **pptr)
{
	return 0;
}

/**
 * @brief Read 32 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
unsigned int Read32( const char **pptr)
{
	return 0;
}

/**
 * @brief Read 64 word helper function
 * @param pptr pointer to read from
 * @retval word value
 */
uint64_t Read64( const char **pptr)
{
	return 0;
}

/**
 * @brief read unsigned multi-byte value and update buffer pointer
 * @param[in] pptr buffer
 * @param[in] n word size in bytes
 * @retval 32 bit value
 */
uint64_t ReadWordHelper( const char **pptr, int n )
{
	return 0;
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
	return 0;
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
	return 0;
}
