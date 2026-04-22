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

#ifndef __DEMUX_DATA_TYPES_H__
#define __DEMUX_DATA_TYPES_H__

#include <memory>
#include <string>
#include <vector>
#include <utility> // for std::exchange
#include "GstUtils.h" // for GstStreamOutputFormat

// C++11/14 compatible exchange utility (std::exchange is C++14)
// This atomically replaces old_val with new_val and returns the old value
#if __cplusplus < 201402L
namespace detail {
	template<typename T, typename U = T>
	T exchange(T& obj, U&& new_value) noexcept
	{
		T old_value = std::move(obj);
		obj = std::forward<U>(new_value);
		return old_value;
	}
}
using detail::exchange;
#else
using std::exchange;
#endif

/*
 * @enum CipherType
 * @brief Enumeration of supported cipher types
 */
enum CipherType
{
	CIPHER_TYPE_CENC,
	CIPHER_TYPE_CBCS,
	CIPHER_TYPE_CBC1,
	CIPHER_TYPE_CENS,
	CIPHER_TYPE_NONE
};

/*
 * @struct MediaProtectionInfo
 * @brief Protection data structure which holds the system ID and PSSH blob
 */
struct MediaProtectionInfo
{
	std::string systemID; // 16 bytes UUID
	std::vector<uint8_t> pssh; // variable length

	// Default constructor
	MediaProtectionInfo(): systemID(), pssh()
	{
	}

	// Constructor with parameters
	MediaProtectionInfo(std::string id, std::vector<uint8_t> data): systemID(std::move(id)), pssh(std::move(data))
	{
	}

	// Move constructor and move assignment (allow efficient transfers)
	// Using = default is fine here as both members are standard containers
	// that properly handle move semantics with guaranteed empty state
	MediaProtectionInfo(MediaProtectionInfo&&) = default;
	MediaProtectionInfo& operator=(MediaProtectionInfo&&) = default;

	// delete copy constructor and copy assignment to prevent accidental copies
	MediaProtectionInfo(const MediaProtectionInfo&) = delete;
	MediaProtectionInfo& operator=(const MediaProtectionInfo&) = delete;
};

/*
 * @struct MediaCodecInfo
 * @brief Codec information structure for a media track
 */
struct MediaCodecInfo
{
	GstStreamOutputFormat mCodecFormat; // GST_FORMAT_VIDEO_ES_H264, etc
	std::vector<uint8_t> mCodecData; // codec private data, e.g. avcC box
	bool mIsEncrypted;
	union
	{
		struct
		{
			uint16_t mChannelCount;
			uint16_t mSampleRate;
		} audio;
		
		struct
		{
			uint16_t mWidth;
			uint16_t mHeight;
		} video;
	} mInfo;

	/**
	 * @brief Constructor for MediaCodecInfo
	 * 
	 * @note Uses uniform initialization (mInfo{}) instead of std::memset for zero-initialization.
	 *       Uniform initialization is preferred for C++ types as it's type-safe, clearer in intent,
	 *       and works correctly with all C++ types including those with constructors.
	 */
	MediaCodecInfo() : mCodecFormat(GST_FORMAT_INVALID), mIsEncrypted(false), mCodecData(), mInfo{0}
	{
	}

	/**
	 * @brief Constructor for MediaCodecInfo with format
	 * @param format Stream output format
	 * 
	 * @note Uses uniform initialization (mInfo{}) instead of std::memset for zero-initialization.
	 *       Uniform initialization is preferred for C++ types as it's type-safe, clearer in intent,
	 *       and works correctly with all C++ types including those with constructors.
	 */
	MediaCodecInfo(GstStreamOutputFormat format) : mCodecFormat(format), mIsEncrypted(false), mCodecData(), mInfo{0}
	{
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaCodecInfo(const MediaCodecInfo&) = delete;
	MediaCodecInfo& operator=(const MediaCodecInfo&) = delete;

	/**
	 * @brief Move constructor for MediaCodecInfo
	 * @param other Source MediaCodecInfo to move from
	 */
	MediaCodecInfo(MediaCodecInfo&& other) noexcept
        : mCodecFormat(exchange(other.mCodecFormat, GST_FORMAT_INVALID))
        , mCodecData(exchange(other.mCodecData, {}))
        , mIsEncrypted(exchange(other.mIsEncrypted, false))
        , mInfo(exchange(other.mInfo, {})) // POD union - exchange with zero-initialized union
    {
    }

	/**
	 * @brief Move assignment operator for MediaCodecInfo
	 * @param other Source MediaCodecInfo to move from
	 * @return Reference to this object
	 */
	MediaCodecInfo& operator=(MediaCodecInfo&& other) noexcept
	{
		if (this != &other)
		{
			mCodecFormat = exchange(other.mCodecFormat, GST_FORMAT_INVALID);
			mCodecData = exchange(other.mCodecData, {});
			mIsEncrypted = exchange(other.mIsEncrypted, false);
			mInfo = exchange(other.mInfo, {}); // POD union - exchange with zero-initialized union
		}
		return *this;
	}
};

/*
 * @struct MediaDrmMetadata
 * @brief DRM metadata for encrypted samples
 */
struct MediaDrmMetadata
{
	bool mIsEncrypted;
	std::vector<uint8_t> mKeyId; // 16 bytes UUID
	std::vector<uint8_t> mIV; // 8 or 16 bytes
	CipherType mCipher;
	std::vector<uint8_t> mSubSamples; // optional subsample encryption data
	uint16_t mNumSubSamples; // number of subsamples
	uint8_t mCryptByteBlock;
	uint8_t mSkipByteBlock;

	/**
	 * @brief Constructor for MediaDrmMetadata
	 */
	MediaDrmMetadata() : mIsEncrypted(false), mKeyId(), mIV(), mCipher(CIPHER_TYPE_NONE),
		mSubSamples(), mNumSubSamples(0), mCryptByteBlock(0), mSkipByteBlock(0)
	{
	}

	/**
	 * @brief Move constructor for MediaDrmMetadata
	 * @param other Source MediaDrmMetadata to move from
	 */
	MediaDrmMetadata(MediaDrmMetadata&& other) noexcept
		: mIsEncrypted(exchange(other.mIsEncrypted, false))
		, mKeyId(exchange(other.mKeyId, {}))
		, mIV(exchange(other.mIV, {}))
		, mCipher(exchange(other.mCipher, CIPHER_TYPE_NONE))
		, mSubSamples(exchange(other.mSubSamples, {}))
		, mNumSubSamples(exchange(other.mNumSubSamples, 0))
		, mCryptByteBlock(exchange(other.mCryptByteBlock, 0))
		, mSkipByteBlock(exchange(other.mSkipByteBlock, 0))
	{
	}

	/**
	 * @brief Move assignment operator for MediaDrmMetadata
	 * @param other Source MediaDrmMetadata to move from
	 * @return Reference to this object
	 */
	MediaDrmMetadata& operator=(MediaDrmMetadata&& other) noexcept
	{
		if (this != &other)
		{
			mIsEncrypted = exchange(other.mIsEncrypted, false);
			mKeyId = exchange(other.mKeyId, {});
			mIV = exchange(other.mIV, {});
			mCipher = exchange(other.mCipher, CIPHER_TYPE_NONE);
			mSubSamples = exchange(other.mSubSamples, {});
			mNumSubSamples = exchange(other.mNumSubSamples, 0);
			mCryptByteBlock = exchange(other.mCryptByteBlock, 0);
			mSkipByteBlock = exchange(other.mSkipByteBlock, 0);
		}
		return *this;
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaDrmMetadata(const MediaDrmMetadata&) = delete;
	MediaDrmMetadata& operator=(const MediaDrmMetadata&) = delete;
};

/*
 * @struct MediaSample
 * @brief Media sample structure.
 *
 * The media payload is held in mData, a shared_ptr<uint8_t> pointing at the
 * raw byte array, with mDataSize holding the byte count.
 *
 * Ownership model:
 *  - Zero-copy MP4 path (AampMp4Demuxer): AampMediaSample::mData is a
 *    shared_ptr<const uint8_t> built with the aliasing constructor so that
 *    it points at the raw sample bytes inside the segment buffer while
 *    keeping that buffer alive.  AAMPGstPlayer::SendSample bridges to
 *    MediaSample via const_pointer_cast (required by GStreamer's C API which
 *    takes gpointer/void* rather than const void*; the buffer is passed as
 *    GST_MEMORY_FLAG_READONLY so GStreamer will not mutate the data).
 *    The segment buffer is kept alive until the last GstBuffer referencing
 *    it is released.
 *  - When constructed from std::vector&&: the vector is moved to the heap
 *    (owned by a shared_ptr<vector>), and mData aliases that control block
 *    while pointing at vector::data().  The vector is freed when the last
 *    mData copy is released.
 *  - When constructed from a raw ptr/size: a private byte array is allocated
 *    and freed via delete[] when the last mData copy is released.
 */
struct MediaSample
{
	/// Shared ownership of the raw payload bytes.  nullptr means no data.
	std::shared_ptr<uint8_t> mData{};
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
	MediaSample(std::shared_ptr<uint8_t> data, size_t dataSize,
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
		mData = std::shared_ptr<uint8_t>(heapVec, heapVec->data());
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

	/// @brief Mutable pointer to payload bytes, or nullptr if no data.
	uint8_t* data() noexcept { return mData.get(); }

	/// @brief Byte count of the payload.
	size_t size() const noexcept { return mDataSize; }

	/// @brief True if there is no payload data.
	bool empty() const noexcept { return mDataSize == 0; }
};

#endif /* __DEMUX_DATA_TYPES_H__ */
