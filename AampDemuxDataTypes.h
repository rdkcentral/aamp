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

	AampCodecInfo() : mCodecFormat(FORMAT_INVALID), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
	}

	AampCodecInfo(StreamOutputFormat format) : mCodecFormat(format), mIsEncrypted(false), mCodecData()
	{
		std::memset(&mInfo, 0, sizeof(mInfo));
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

	AampDrmMetadata() : mIsEncrypted(false), mKeyId(), mIV(), mCipher(),
		mSubSamples(), mCryptByteBlock(0), mSkipByteBlock(0)
	{
	}
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

	AampMediaSample() : mData("AampMediaSample"), mPts(0), mDts(0), mDuration(0), mDrmMetadata()
	{
	}
};

#endif /* __AAMP_DEMUX_DATA_TYPES_H__ */