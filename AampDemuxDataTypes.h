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

#include <string>
#include <vector>
#include <cstring> // for std::memset
#include "AampGrowableBuffer.h" // for AampGrowableBuffer
#include "AampTime.h" // for AampTime
#include "DemuxDataTypes.h" // for MediaDrmMetadata

/*
 * @struct AampMediaSample
 * @brief Media sample structure
 */
struct AampMediaSample
{
	AampGrowableBuffer mData;
	AampTime mPts;
	AampTime mDts;
	AampTime mDuration;

    MediaDrmMetadata mDrmMetadata; // DRM metadata for encrypted samples

	/**
	 * @brief Constructor for AampMediaSample
	 */
	AampMediaSample() : mData("AampMediaSample"), mPts(0), mDts(0), mDuration(0), mDrmMetadata()
	{
	}

	// Move constructor and move assignment (allow efficient transfers)
	AampMediaSample(AampMediaSample&&) = default;
	AampMediaSample& operator=(AampMediaSample&&) = default;

	// Delete copy constructor and copy assignment to prevent accidental copies
	AampMediaSample(const AampMediaSample&) = delete;
	AampMediaSample& operator=(const AampMediaSample&) = delete;
};

#endif /* __AAMP_DEMUX_DATA_TYPES_H__ */