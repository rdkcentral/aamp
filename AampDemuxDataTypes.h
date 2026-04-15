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
 * mDataPtr and mDataSize give zero-copy access to raw bytes inside the owning
 * segment buffer.  mSegment holds the shared_ptr that keeps that buffer alive
 * for as long as any sample derived from it exists.
 *
 * In future, we can consider unifying this with MediaSample in DemuxDataTypes.h
 */
struct AampMediaSample
{
	const uint8_t* mDataPtr{nullptr};                  /**< Raw pointer into the segment buffer (zero-copy) */
	size_t mDataSize{0};                               /**< Byte count of the sample payload */
	std::shared_ptr<std::vector<uint8_t>> mSegment{};  /**< Keeps the segment buffer alive */
	double mPts{0.0};
	double mDts{0.0};
	double mDuration{0.0};
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