/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef __AAMP_DEMUX_DATA_TYPES_H__
#define __AAMP_DEMUX_DATA_TYPES_H__

#include <vector>
#include <cstdint>
#include <memory>
#include "DemuxDataTypes.h" // for MediaDrmMetadata

/**
 * @struct AampMediaSample
 * @brief Media sample structure.
 *
 * mData is a shared_ptr<const uint8_t> built with the aliasing constructor
 * so that it points at the raw sample bytes inside the owning segment buffer
 * while the segment buffer's reference count keeps that storage alive.
 * mDataSize gives the byte count of the payload.
 *
 * AampMediaSample is the demuxer-domain sample; MediaSample
 * (middleware/MediaSample.h) is the sink-domain sample.  Both now hold
 * shared_ptr<const uint8_t>, so AAMPGstPlayer::SendSample bridges between
 * them without any const_pointer_cast.
 */
struct AampMediaSample
{
	std::shared_ptr<const uint8_t> mData{};  /**< Aliased pointer into the segment buffer (zero-copy) */
	size_t mDataSize{0};                     /**< Byte count of the sample payload */
	double mPts{0.0};                        /**< Presentation timestamp in seconds */
	double mDts{0.0};                        /**< Decode timestamp in seconds */
	double mDuration{0.0};                   /**< Sample duration in seconds */
	int64_t mDisplayOffsetMs{0};             /**< Display timing offset in milliseconds. Set by AampMp4Demuxer when PTS restamping is enabled; 0 otherwise. Used by AampRialtoPlayer for subtitle presentation timing. */
	MediaDrmMetadata mDrmMetadata{}; /**< DRM metadata for encrypted samples */

	// Move constructor and move assignment (allow efficient transfers)
	AampMediaSample() = default;
	AampMediaSample(AampMediaSample&&) = default;
	AampMediaSample& operator=(AampMediaSample&&) = default;

	// Delete copy constructor and copy assignment to prevent accidental copies
	AampMediaSample(const AampMediaSample&) = delete;
	AampMediaSample& operator=(const AampMediaSample&) = delete;
};

#endif /* __AAMP_DEMUX_DATA_TYPES_H__ */