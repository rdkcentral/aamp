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

#ifndef __MEDIA_SAMPLE_H__
#define __MEDIA_SAMPLE_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include "DemuxDataTypes.h" // for MediaDrmMetadata and the exchange helper

/*
 * @struct MediaSample
 * @brief Media sample transport structure delivered to the stream sink.
 *
 * The media payload is held in mData, a shared_ptr<const uint8_t> pointing
 * at the raw byte array, with mDataSize holding the byte count.  The sample
 * is a read-only view of the underlying storage; mutation is only permitted
 * at the C-API boundary to GStreamer (which takes gpointer/void* but is
 * given GST_MEMORY_FLAG_READONLY so it will not mutate the data).
 *
 * Ownership model:
 *  - Zero-copy MP4 path (AampMp4Demuxer): AampMediaSample::mData is a
 *    shared_ptr<const uint8_t> built with the aliasing constructor so that
 *    it points at the raw sample bytes inside the segment buffer while
 *    keeping that buffer alive.  The segment buffer stays alive until the
 *    last GstBuffer referencing it is released.
 *  - When constructed from std::vector&&: the vector is moved to the heap
 *    (owned by a shared_ptr<vector>), and mData aliases that control block
 *    while pointing at vector::data().  The vector is freed when the last
 *    mData copy is released.
 */
struct MediaSample
{
	/// Shared read-only ownership of the raw payload bytes.  nullptr means no data.
	std::shared_ptr<const uint8_t> mData{};
	/// Byte count of the payload pointed to by mData.
	size_t mDataSize{0};
	double mPts{0.0};
	double mDts{0.0};
	double mDuration{0.0};
	double mPtsOffset{0.0};
	MediaDrmMetadata mDrmMetadata{}; // DRM metadata for encrypted samples

	/**
	 * @brief Default constructor for MediaSample
	 */
	MediaSample() = default;

	/**
	 * @brief Constructor from shared_ptr (zero-copy; for pre-allocated buffers).
	 *
	 * Takes shared ownership of an existing byte buffer (e.g. an aliased
	 * pointer produced by the MP4 demuxer) without any byte-level copy.
	 * Typically used when bridging from AampMediaSample to MediaSample across
	 * the GStreamer domain boundary.
	 *
	 * @param data      Shared ownership of the raw payload bytes.
	 * @param dataSize  Byte count of the payload.
	 * @param pts       Presentation timestamp
	 * @param dts       Decode timestamp
	 * @param duration  Sample duration
	 * @param drm       DRM metadata (consumed)
	 * @param ptsOffset PTS offset (default 0.0)
	 */
	MediaSample(std::shared_ptr<const uint8_t> data, size_t dataSize,
	            double pts, double dts, double duration,
	            MediaDrmMetadata&& drm, double ptsOffset = 0.0)
		: mData(std::move(data))
		, mDataSize(dataSize)
		, mPts(pts)
		, mDts(dts)
		, mDuration(duration)
		, mPtsOffset(ptsOffset)
		, mDrmMetadata(std::move(drm))
	{
	}

	/**
	 * @brief Constructor from vector (zero-copy via aliasing shared_ptr).
	 *
	 * Moves the vector to the heap and uses the shared_ptr aliasing constructor
	 * so that mData points at the raw bytes while the control block owns the
	 * heap-allocated vector.  No byte-level copy is performed.
	 *
	 * @param data      Vector to take ownership of (moved).
	 * @param pts       Presentation timestamp
	 * @param dts       Decode timestamp
	 * @param duration  Sample duration
	 * @param ptsOffset PTS offset
	 */
	MediaSample(std::vector<uint8_t>&& data, double pts, double dts, double duration, double ptsOffset = 0.0)
		: mPts(pts)
		, mDts(dts)
		, mDuration(duration)
		, mPtsOffset(ptsOffset)
	{
		auto heapVec = std::make_shared<std::vector<uint8_t>>(std::move(data));
		mDataSize = heapVec->size();
		// Aliasing constructor: mData shares the refcount with heapVec but
		// points directly at the raw bytes.  The vector is destroyed when
		// the last mData copy is released.
		mData = std::shared_ptr<const uint8_t>(heapVec, heapVec->data());
	}

	/**
	 * @brief Move constructor for MediaSample.
	 * @param other Source MediaSample to move from
	 */
	MediaSample(MediaSample&& other) noexcept
		: mData(exchange(other.mData, nullptr))
		, mDataSize(exchange(other.mDataSize, 0u))
		, mPts(exchange(other.mPts, 0.0))
		, mDts(exchange(other.mDts, 0.0))
		, mDuration(exchange(other.mDuration, 0.0))
		, mPtsOffset(exchange(other.mPtsOffset, 0.0))
		, mDrmMetadata(exchange(other.mDrmMetadata, {}))
	{
	}

	/**
	 * @brief Move assignment operator for MediaSample.
	 * @param other Source MediaSample to move from
	 * @return Reference to this object
	 */
	MediaSample& operator=(MediaSample&& other) noexcept
	{
		if (this != &other)
		{
			mData     = exchange(other.mData, nullptr);
			mDataSize = exchange(other.mDataSize, 0u);
			mPts      = exchange(other.mPts, 0.0);
			mDts      = exchange(other.mDts, 0.0);
			mDuration = exchange(other.mDuration, 0.0);
			mPtsOffset = exchange(other.mPtsOffset, 0.0);
			mDrmMetadata = exchange(other.mDrmMetadata, {});
		}
		return *this;
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaSample(const MediaSample&) = delete;
	MediaSample& operator=(const MediaSample&) = delete;

	/// @brief Read-only pointer to payload bytes, or nullptr if no data.
	const uint8_t* data() const noexcept { return mData.get(); }

	/// @brief Byte count of the payload.
	size_t size() const noexcept { return mDataSize; }
};

#endif /* __MEDIA_SAMPLE_H__ */
