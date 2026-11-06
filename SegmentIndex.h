/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file SegmentIndex.h
 * @brief Thread-safe holder for a DASH SegmentBase index (sidx) buffer.
 */

#ifndef SEGMENT_INDEX_H
#define SEGMENT_INDEX_H

#include <cstdint>
#include <mutex>
#include <vector>
#include "AampMPDUtils.h"
#include "AampUtils.h"

/**
 * @brief The packed index (sidx) of a SegmentBase representation, and the
 *        byte offset of segment 0 in the file it describes.
 *
 * The FetcherLoop loads and clears the index while download worker threads
 * read it, so every member function takes the internal lock. Callers never
 * lock. Clear() releases the buffer and zeroes the base offset together.
 */
class SegmentIndex
{
public:
	/**
	 * @brief Check whether no index is loaded.
	 */
	bool Empty() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mData.empty();
	}

	/**
	 * @brief Store the index only if none is loaded.
	 * @param[in] data Index bytes; moved from only when stored
	 * @return true if stored
	 */
	bool LoadIfEmpty(std::vector<uint8_t> &&data)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (!mData.empty())
		{
			return false;
		}
		mData = std::move(data);
		return true;
	}

	/**
	 * @brief Copy of the index bytes, for parsing outside the lock.
	 */
	std::vector<uint8_t> Snapshot() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mData;
	}

	/**
	 * @brief Look up one sidx entry. See ParseSegmentIndexBox.
	 *
	 * referencedSize and referencedDuration must be valid unless firstOffset
	 * is given, in which case only firstOffset is written.
	 *
	 * @param[in]  segmentIndex    Entry to read
	 * @param[out] referencedSize  Size in bytes
	 * @param[out] referencedDuration Duration in seconds
	 * @param[out] firstOffset     Offset of the first entry, or nullptr
	 * @return true if the entry was parsed
	 */
	bool Lookup(int segmentIndex, unsigned int *referencedSize, float *referencedDuration, unsigned int *firstOffset) const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return ParseSegmentIndexBox(mData.data(), mData.size(), segmentIndex, referencedSize, referencedDuration, firstOffset);
	}

	/**
	 * @brief Byte offset of segment 0 in the file for the loaded index.
	 */
	uint64_t BaseOffset() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mBaseOffset;
	}

	/**
	 * @brief Record the byte offset of segment 0 for the loaded index.
	 */
	void SetBaseOffset(uint64_t baseOffset)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mBaseOffset = baseOffset;
	}

	/**
	 * @brief Record where segment 0 starts and find where a segment starts.
	 *
	 * Does nothing to the base offset if no index is loaded.
	 *
	 * @param[in] offsetAfterIndex Offset of the first byte following the packed index
	 * @param[in] segmentIndex     Segment whose start offset is wanted
	 * @return Byte offset of segment segmentIndex, or offsetAfterIndex if no index is loaded
	 */
	uint64_t AnchorAndOffsetOf(uint64_t offsetAfterIndex, int segmentIndex)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mData.empty())
		{
			return offsetAfterIndex;
		}
		uint64_t offset = offsetAfterIndex;
		unsigned int firstOffset = 0;
		if (ParseSegmentIndexBox(mData.data(), mData.size(), 0, nullptr, nullptr, &firstOffset))
		{
			offset += firstOffset;
		}
		mBaseOffset = offset;
		offset = OffsetOfLocked(segmentIndex);
		return offset;
	}

	/**
	 * @brief Byte range of a segment relative to the recorded base offset.
	 * @param[in]  segmentIndex Segment to find
	 * @param[out] offset       Start of the segment; the sum of the preceding
	 *                          segment sizes added to the base offset, even if
	 *                          the entry itself is missing
	 * @param[out] size         Size in bytes of the segment
	 * @param[out] duration     Duration in seconds of the segment
	 * @return true if the entry was parsed
	 */
	bool RangeOf(int segmentIndex, uint64_t &offset, unsigned int &size, float &duration) const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		offset = OffsetOfLocked(segmentIndex);
		return ParseSegmentIndexBox(mData.data(), mData.size(), segmentIndex, &size, &duration, nullptr);
	}

	/**
	 * @brief Release the index and reset the base offset.
	 */
	void Clear()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		aamp_utils::ClearAndRelease(mData);
		mBaseOffset = 0;
	}

private:
	/**
	 * @brief Sum the sizes of the segments before segmentIndex onto the base
	 *        offset. The caller holds mMutex.
	 */
	uint64_t OffsetOfLocked(int segmentIndex) const
	{
		uint64_t offset = mBaseOffset;
		for (int i = 0; i < segmentIndex; i++)
		{
			unsigned int size = 0;
			float duration = 0.0f;
			if (ParseSegmentIndexBox(mData.data(), mData.size(), i, &size, &duration, nullptr))
			{
				offset += size;
			}
		}
		return offset;
	}

	mutable std::mutex mMutex;
	std::vector<uint8_t> mData{};
	uint64_t mBaseOffset{0};
};

#endif // SEGMENT_INDEX_H
