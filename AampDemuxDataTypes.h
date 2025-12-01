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
#include "StreamOutputFormat.h" // for StreamOutputFormat
#include "AampMediaType.h" // for AampMediaType

/*
 * @struct AampPsshData
 * @brief PSSH data structure
 */
struct AampPsshData
{
	std::string systemID; // 16 bytes UUID
	std::vector<uint8_t> pssh; // variable length

	// Default constructor
	AampPsshData(): systemID(), pssh()
	{
	}

	// Constructor with parameters
	AampPsshData(std::string id, std::vector<uint8_t> data): systemID(std::move(id)), pssh(std::move(data))
	{
	}

	// Move constructor and move assignment (allow efficient transfers)
	AampPsshData(AampPsshData&&) = default;
	AampPsshData& operator=(AampPsshData&&) = default;

	// delete copy constructor and copy assignment to prevent accidental copies
	AampPsshData(const AampPsshData&) = delete;
	AampPsshData& operator=(const AampPsshData&) = delete;
};

/*
 * @struct AampCodecInfo
 * @brief Codec information structure
 */
struct AampCodecInfo
{
	StreamOutputFormat mCodecFormat; // FORMAT_VIDEO_ES_H264, etc
	std::vector<uint8_t> mCodecData; // codec private data, e.g. avcC box
	bool mIsEncrypted;
	union
	{
		struct
		{
			uint16_t mChannelCount;
			uint16_t mSampleSize;
			uint16_t mSampleRate;
			uint8_t mObjectTypeId;
			uint8_t mStreamType;
			uint8_t mUpStream;
			uint16_t mBufferSize;
			uint32_t mMaxBitrate;
			uint32_t mAvgBitrate;
		} audio;
		
		struct
		{
			uint16_t mWidth;
			uint16_t mHeight;
			uint16_t mFrameCount;
			uint16_t mDepth;
			uint32_t mHorizontalResolution;
			uint32_t mVerticalResolution;
		} video;
	} mInfo;

	/**
	 * @brief Constructor for AampCodecInfo
	 */
	AampCodecInfo() : mCodecFormat(FORMAT_INVALID), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
	}

	/**
	 * @brief Constructor for AampCodecInfo with format
	 * @param format Stream output format
	 */
	AampCodecInfo(StreamOutputFormat format) : mCodecFormat(format), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	AampCodecInfo(const AampCodecInfo&) = delete;
	AampCodecInfo& operator=(const AampCodecInfo&) = delete;

	/**
	 * @brief Move constructor for AampCodecInfo
	 * @param other Source AampCodecInfo to move from
	 */
	AampCodecInfo(AampCodecInfo&& other) noexcept
        : mCodecFormat(std::move(other.mCodecFormat))
        , mCodecData(std::move(other.mCodecData))
        , mIsEncrypted(std::move(other.mIsEncrypted))
        , mInfo(std::move(other.mInfo))
    {
        // Explicitly reset the source object to default state after move
        other.mCodecFormat = FORMAT_INVALID;
        other.mIsEncrypted = false;
        std::memset(&other.mInfo, 0, sizeof(other.mInfo));
        // mCodecData is already empty after std::move
    }

	/** Move assignment operator for AampCodecInfo
	 * @param other Source AampCodecInfo to move from
	 */
	AampCodecInfo& operator=(AampCodecInfo&& other) noexcept
	{
		if (this != &other)
		{
			mCodecFormat = std::move(other.mCodecFormat);
			mCodecData = std::move(other.mCodecData);
			mIsEncrypted = std::move(other.mIsEncrypted);
			mInfo = std::move(other.mInfo);

			// Explicitly reset the source object to default state after move
			other.mCodecFormat = FORMAT_INVALID;
			other.mIsEncrypted = false;
			std::memset(&other.mInfo, 0, sizeof(other.mInfo));
			// mCodecData is already empty after std::move
		}
		return *this;
	}
};

/*
 * @struct AampDrmMetadata
 * @brief DRM metadata for encrypted samples
 */
struct AampDrmMetadata
{
	bool mIsEncrypted;
	std::string mKeyId; // 16 bytes UUID
	std::vector<uint8_t> mIV; // 8 or 16 bytes
	std::string mCipher; // e.g. 'cenc', 'cbcs'
	std::vector<uint8_t> mSubSamples; // optional subsample encryption data
	uint8_t mCryptByteBlock;
	uint8_t mSkipByteBlock;

	/**
	 * @brief Constructor for AampDrmMetadata
	 */
	AampDrmMetadata() : mIsEncrypted(false), mKeyId(), mIV(), mCipher(),
		mSubSamples(), mCryptByteBlock(0), mSkipByteBlock(0)
	{
	}

	/**
	 * @brief Move constructor for AampDrmMetadata
	 * @param other Source AampDrmMetadata to move from
	 */
	AampDrmMetadata(AampDrmMetadata&& other) noexcept
		: mIsEncrypted(other.mIsEncrypted),
		  mKeyId(std::move(other.mKeyId)),
		  mIV(std::move(other.mIV)),
		  mCipher(std::move(other.mCipher)),
		  mSubSamples(std::move(other.mSubSamples)),
		  mCryptByteBlock(other.mCryptByteBlock),
		  mSkipByteBlock(other.mSkipByteBlock)
	{
		// Reset source object to default state after move
		other.mIsEncrypted = false;
		other.mCryptByteBlock = 0;
		other.mSkipByteBlock = 0;
	}

	/**
	 * @brief Move assignment operator for AampDrmMetadata
	 * @param other Source AampDrmMetadata to move from
	 * @return Reference to this object
	 */
	AampDrmMetadata& operator=(AampDrmMetadata&& other) noexcept
	{
		if (this != &other)
		{
			mIsEncrypted = other.mIsEncrypted;
			mKeyId = std::move(other.mKeyId);
			mIV = std::move(other.mIV);
			mCipher = std::move(other.mCipher);
			mSubSamples = std::move(other.mSubSamples);
			mCryptByteBlock = other.mCryptByteBlock;
			mSkipByteBlock = other.mSkipByteBlock;

			// Reset source object to default state after move
			other.mIsEncrypted = false;
			other.mCryptByteBlock = 0;
			other.mSkipByteBlock = 0;
		}
		return *this;
	}

	// Delete copy constructor and copy assignment to prevent accidental copies
	AampDrmMetadata(const AampDrmMetadata&) = delete;
	AampDrmMetadata& operator=(const AampDrmMetadata&) = delete;
};

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

    AampDrmMetadata mDrmMetadata; // empty if not encrypted

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