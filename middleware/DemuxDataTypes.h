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

#include <string>
#include <vector>
#include <cstring> // for std::memset
#include "GstUtils.h" // for GstStreamOutputFormat

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
	 */
	MediaCodecInfo() : mCodecFormat(GST_FORMAT_INVALID), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
	}

	/**
	 * @brief Constructor for MediaCodecInfo with format
	 * @param format Stream output format
	 */
	MediaCodecInfo(GstStreamOutputFormat format) : mCodecFormat(format), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaCodecInfo(const MediaCodecInfo&) = delete;
	MediaCodecInfo& operator=(const MediaCodecInfo&) = delete;

	/**
	 * @brief Move constructor for MediaCodecInfo
	 * @param other Source MediaCodecInfo to move from
	 */
	MediaCodecInfo(MediaCodecInfo&& other) noexcept
        : mCodecFormat(std::move(other.mCodecFormat))
        , mCodecData(std::move(other.mCodecData))
        , mIsEncrypted(other.mIsEncrypted)
        , mInfo(std::move(other.mInfo))
    {
        // Explicitly reset the source object to default state after move
        other.mCodecFormat = GST_FORMAT_INVALID;
        other.mIsEncrypted = false;
    }

	/** Move assignment operator for MediaCodecInfo
	 * @param other Source MediaCodecInfo to move from
	 */
	MediaCodecInfo& operator=(MediaCodecInfo&& other) noexcept
	{
		if (this != &other)
		{
			mCodecFormat = std::move(other.mCodecFormat);
			mCodecData = std::move(other.mCodecData);
			mIsEncrypted = other.mIsEncrypted;
			mInfo = std::move(other.mInfo);

			// Explicitly reset the source object to default state after move
			other.mCodecFormat = GST_FORMAT_INVALID;
			other.mIsEncrypted = false;
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
		: mIsEncrypted(other.mIsEncrypted),
		  mKeyId(std::move(other.mKeyId)),
		  mIV(std::move(other.mIV)),
		  mCipher(other.mCipher),
		  mSubSamples(std::move(other.mSubSamples)),
		  mNumSubSamples(other.mNumSubSamples),
		  mCryptByteBlock(other.mCryptByteBlock),
		  mSkipByteBlock(other.mSkipByteBlock)
	{
		// Reset source object to default state after move
		other.mIsEncrypted = false;
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
			mIsEncrypted = other.mIsEncrypted;
			mKeyId = std::move(other.mKeyId);
			mIV = std::move(other.mIV);
			mCipher = other.mCipher;
			mSubSamples = std::move(other.mSubSamples);
			mNumSubSamples = other.mNumSubSamples;
			mCryptByteBlock = other.mCryptByteBlock;
			mSkipByteBlock = other.mSkipByteBlock;

			// Reset source object to default state after move
			other.mIsEncrypted = false;
		}
		return *this;
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaDrmMetadata(const MediaDrmMetadata&) = delete;
	MediaDrmMetadata& operator=(const MediaDrmMetadata&) = delete;
};

/*
 * @struct MediaSample
 * @brief Media sample structure
 */
struct MediaSample
{
	const void* mData;
	size_t mDataSize;
	double mPts;
	double mDts;
	double mDuration;
	double mPtsOffset;

    MediaDrmMetadata mDrmMetadata; // DRM metadata for encrypted samples
	/**
	 * @brief Constructor for MediaSample
	 */
	MediaSample()
		: mData(nullptr)
		, mDataSize(0)
		, mPts(0)
		, mDts(0)
		, mDuration(0)
		, mPtsOffset(0)
		, mDrmMetadata()
	{
	}

	/* @brief Move constructor for MediaSample
	 * @param other Source MediaSample to move from
	 */
	MediaSample(MediaSample&& other) noexcept
		: mData(other.mData)
		, mDataSize(other.mDataSize)
		, mPts(other.mPts)
		, mDts(other.mDts)
		, mDuration(other.mDuration)
		, mDrmMetadata(std::move(other.mDrmMetadata))
	{
		// Reset source object to default state after move
		other.mData = nullptr;
		other.mDataSize = 0;
	}
	/* @brief Move assignment operator for MediaSample
	 * @param other Source MediaSample to move from
	 * @return Reference to this object
	 */
	MediaSample& operator=(MediaSample&& other) noexcept
	{
		if (this != &other)
		{
			mData = other.mData;
			mDataSize = other.mDataSize;
			mPts = other.mPts;
			mDts = other.mDts;
			mDuration = other.mDuration;
			mDrmMetadata = std::move(other.mDrmMetadata);

			// Reset source object to default state after move
			other.mData = nullptr;
			other.mDataSize = 0;
		}
		return *this;
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	MediaSample(const MediaSample&) = delete;
	MediaSample& operator=(const MediaSample&) = delete;
};

#endif /* __DEMUX_DATA_TYPES_H__ */