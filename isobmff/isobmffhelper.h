/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

#ifndef __ISOBMFFHELPER_H__
#define __ISOBMFFHELPER_H__

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "AampLogManager.h"

class IsoBmffBuffer;

class IsoBmffHelper
{
	public:
		IsoBmffHelper(){};
		~IsoBmffHelper() = default;

		/**
		 * @brief Convert an ISOBMFF segment to a single key frame
		 * @param[in,out] buffer ISOBMFF segment, contains a single key frame when the function returns
		 * @retval false in case of failure, true otherwise
		 */
		bool ConvertToKeyFrame(std::vector<uint8_t> &buffer);

		/**
		 * @brief Streaming MOOF probe for VOD iframe synthesis partial download.
		 *
		 *        Given a partially-received segment buffer, compute the minimum
		 *        byte count required to cover:
		 *            [MOOF box (complete)] [MDAT header (8 bytes)] [first-sample payload]
		 *
		 *        Intended to be called from the CURL write callback on every chunk
		 *        until the MOOF is fully buffered (returns 0 until then).  Once the
		 *        cap is returned, the caller should truncate the download at that
		 *        offset and then call ConvertToKeyFrame() to fix the MOOF metadata.
		 *
		 *        No heap allocation is performed; safe for the write-callback hot path.
		 *
		 * @param[in] buf  Raw bytes received so far (segment start = offset 0)
		 * @param[in] len  Valid byte count in buf
		 * @return Total bytes needed (moofSize + 8 + firstSampleSize), or 0 if the
		 *         MOOF is not yet fully buffered or the first-sample size cannot be
		 *         determined (caller should fall back to full download).
		 */
		static size_t GetIframeByteCap(const uint8_t *buf, size_t len);

		/**
		 * @fn RestampPts
		 *
		 * @brief Restamp the PTS in the ISO BMFF boxes in the buffer, by adding an offset
		 *
		 * @param[in] buffer - buffer containing the ISO BMFF boxes to restamp
		 * @param[in] ptsOffset - Offset to be added to PTS values
		 * @param[in] fragmentUrl - Fragment URL, used in logging
		 * @param[in] trackName - Media track name, used in logging
		 * @param[in] timeScale - Timescale, used in logging
		 *
		 * @retval true  - PTS values were restamped
		 * @retval false - There was a problem restamping PTS values
		 */
		bool RestampPts(std::vector<uint8_t> &buffer, int64_t ptsOffset, std::string const &fragmentUrl, const char* trackName, uint32_t timeScale);

		/**
		 * @fn SetTimescale
		 *
		 * @brief Set the timescale in the mdhd box, to implement trick modes
		 *
		 * @param[in,out] buffer - ISOBMFF segment
		 * @param[in] timeScale - Number of time units that pass in one second
		 *
		 * @retval true  - Timescale was set in the ISO BMFF box
		 * @retval false - There was a problem setting the timescale
		 */
		bool SetTimescale(std::vector<uint8_t> &buffer, uint32_t timeScale);

		/**
		 * @fn SetPtsAndDuration
		 *
		 * @brief Set the PTS (base media decode time) and sample duration.
		 *        This function assumes that the buffer contains an I-frame media segment,
		 *        consisting of a single sample, so is suitable for trick mode re-stamping.
		 *        If the buffer contains multiple samples or truns, only the first sample
		 *        duration will be set (if flagged as present).
		 *
		 * @param[in,out] buffer - buffer containing ISOBMFF I-frame media segment
		 * @param[in] pts - Base media decode time to set
		 * @param[in] duration - Sample duration to set (uint32_t per ISOBMFF spec)
		 *
		 * @retval true  - PTS and duration were set in the ISOBMFF boxes
		 * @retval false - Setting failed
		 */
		bool SetPtsAndDuration(std::vector<uint8_t> &buffer, uint64_t pts, uint32_t duration);

		/**
		 * @fn ClearMediaHeaderDuration
		 *
		 * @brief Clear the sample duration in the mdhd box
		 *
		 * @param[in,out] buffer - buffer containing ISOBMFF initialization header
		 *
		 * @retval true  - Sample duration was cleared in the ISO BMFF box
		 * @retval false - Failed to parse the buffer or clear the sample duration
		 */
		bool ClearMediaHeaderDuration(std::vector<uint8_t> &buffer);

	private:
		/**
		 * @brief Create an IsoBmffBuffer from a data buffer and parse it
		 *
		 * @param[out] isoBmffBuffer - The IsoBmffBuffer to initialise and parse
		 * @param[in]  buffer - The raw data buffer
		 * @return true if parsing succeeded, false otherwise
		 */
		bool InitAndParse(IsoBmffBuffer& isoBmffBuffer, std::vector<uint8_t> &buffer);
};

#endif /* __ISOBMFFHELPER_H__ */
