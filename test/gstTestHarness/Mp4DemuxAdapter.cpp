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
 * @file Mp4DemuxAdapter.cpp
 * @brief Implementation of MP4Demux adapter for gst test harness
 */

#include "Mp4DemuxAdapter.h"
#include "GstUtils.h"
#include <cassert>
#include <cstdio>
#include <inttypes.h>

// MultiChar_Constant is already defined in MP4Demux.h
// But we need it for the AdjustMediaDecodeTime static method
#ifndef MultiChar_Constant
#define MultiChar_Constant(TEXT) ( \
	(static_cast<uint32_t>(TEXT[0]) << 0x18) | \
	(static_cast<uint32_t>(TEXT[1]) << 0x10) | \
	(static_cast<uint32_t>(TEXT[2]) << 0x08) | \
	(static_cast<uint32_t>(TEXT[3]) << 0x00) )
#endif

Mp4DemuxAdapter::Mp4DemuxAdapter()
	: mDemux(new Mp4Demux())
	, mSamples()
	, mProtectionEvents()
	, mCodecInfo()
	, mTimeScale(0)
	, mGstProtectionEvents()
{
}

Mp4DemuxAdapter::~Mp4DemuxAdapter()
{
	// Clean up any cached GstEvents
	for (auto* event : mGstProtectionEvents)
	{
		if (event)
		{
			gst_event_unref(event);
		}
	}
	mGstProtectionEvents.clear();
}

bool Mp4DemuxAdapter::Parse(const void* ptr, size_t len)
{
	if (!mDemux)
	{
		return false;
	}

	bool result = mDemux->Parse(ptr, len);
	if (result)
	{
		// Cache the parsed data
		mSamples = mDemux->GetSamples();
		mProtectionEvents = mDemux->GetProtectionEvents();
		mCodecInfo = mDemux->GetCodecInfo();
		mTimeScale = mDemux->GetTimeScale();

		// Clear any cached GstEvents since we have new data
		for (auto* event : mGstProtectionEvents)
		{
			if (event)
			{
				gst_event_unref(event);
			}
		}
		mGstProtectionEvents.clear();
	}

	return result;
}

uint32_t Mp4DemuxAdapter::getTimeScale() const
{
	return mTimeScale;
}

int Mp4DemuxAdapter::count() const
{
	return static_cast<int>(mSamples.size());
}

const uint8_t* Mp4DemuxAdapter::getPtr(int part) const
{
	if (part < 0 || static_cast<size_t>(part) >= mSamples.size())
	{
		return nullptr;
	}
	return reinterpret_cast<const uint8_t*>(mSamples[part].mData.GetPtr());
}

size_t Mp4DemuxAdapter::getLen(int part) const
{
	if (part < 0 || static_cast<size_t>(part) >= mSamples.size())
	{
		return 0;
	}
	return mSamples[part].mData.GetLen();
}

double Mp4DemuxAdapter::getPts(int part) const
{
	if (part < 0 || static_cast<size_t>(part) >= mSamples.size())
	{
		return 0.0;
	}
	return mSamples[part].mPts;
}

double Mp4DemuxAdapter::getDts(int part) const
{
	if (part < 0 || static_cast<size_t>(part) >= mSamples.size())
	{
		return 0.0;
	}
	return mSamples[part].mDts;
}

double Mp4DemuxAdapter::getDuration(int part) const
{
	if (part < 0 || static_cast<size_t>(part) >= mSamples.size())
	{
		return 0.0;
	}
	return mSamples[part].mDuration;
}

GstBuffer* Mp4DemuxAdapter::CreateGstBuffer(gconstpointer data, gsize size)
{
	// This is compatible with gstreamer 1.20+
	// For older versions, we need to allocate and copy manually
	GstBuffer* buffer = gst_buffer_new_and_alloc(size);
	if (buffer)
	{
		GstMapInfo map;
		gst_buffer_map(buffer, &map, GST_MAP_WRITE);
		std::memcpy(map.data, data, size);
		gst_buffer_unmap(buffer, &map);
	}
	return buffer;
}

const char* Mp4DemuxAdapter::CipherTypeToString(CipherType cipher)
{
	switch (cipher)
	{
		case CIPHER_TYPE_CENC: return "cenc";
		case CIPHER_TYPE_CBCS: return "cbcs";
		case CIPHER_TYPE_CBC1: return "cbc1";
		case CIPHER_TYPE_CENS: return "cens";
		case CIPHER_TYPE_NONE:
		default:
			return "none";
	}
}

GstStructure* Mp4DemuxAdapter::getDrmMetadata(int sampleIndex) const
{
	if (sampleIndex < 0 || static_cast<size_t>(sampleIndex) >= mSamples.size())
	{
		return nullptr;
	}

	const auto& sample = mSamples[sampleIndex];
	const auto& drmMeta = sample.mDrmMetadata;

	if (!drmMeta.mIsEncrypted)
	{
		return nullptr;
	}

	// Create metadata structure similar to old mp4demux.hpp implementation
	GstStructure* metadata = gst_structure_new(
		"application/x-cenc",
		"encrypted", G_TYPE_BOOLEAN, TRUE,
		nullptr);

	// Add KID
	if (!drmMeta.mKeyId.empty())
	{
		GstBuffer* kid_buf = CreateGstBuffer(drmMeta.mKeyId.data(), drmMeta.mKeyId.size());
		if (kid_buf)
		{
			gst_structure_set(metadata, "kid", GST_TYPE_BUFFER, kid_buf, nullptr);
			gst_buffer_unref(kid_buf);
		}
	}

	// Add cipher mode
	const char* cipher_mode = CipherTypeToString(drmMeta.mCipher);
	gst_structure_set(metadata, "cipher-mode", G_TYPE_STRING, cipher_mode, nullptr);

	// Add IV
	if (!drmMeta.mIV.empty())
	{
		GstBuffer* iv_buf = CreateGstBuffer(drmMeta.mIV.data(), drmMeta.mIV.size());
		if (iv_buf)
		{
			gst_structure_set(metadata,
				"iv_size", G_TYPE_UINT, static_cast<guint>(drmMeta.mIV.size()),
				"iv", GST_TYPE_BUFFER, iv_buf,
				nullptr);
			gst_buffer_unref(iv_buf);
		}
	}

	// Add subsample information
	if (!drmMeta.mSubSamples.empty())
	{
		GstBuffer* subsamples_buf = CreateGstBuffer(drmMeta.mSubSamples.data(), drmMeta.mSubSamples.size());
		if (subsamples_buf)
		{
			gst_structure_set(metadata,
				"subsample_count", G_TYPE_UINT, static_cast<guint>(drmMeta.mNumSubSamples),
				"subsamples", GST_TYPE_BUFFER, subsamples_buf,
				nullptr);
			gst_buffer_unref(subsamples_buf);
		}
	}
	else
	{
		gst_structure_set(metadata, "subsample_count", G_TYPE_UINT, 0, nullptr);
	}

	// Add pattern encryption fields for CBCS
	if (drmMeta.mCipher == CIPHER_TYPE_CBCS)
	{
		gst_structure_set(metadata,
			"crypt_byte_block", G_TYPE_UINT, static_cast<guint>(drmMeta.mCryptByteBlock),
			"skip_byte_block", G_TYPE_UINT, static_cast<guint>(drmMeta.mSkipByteBlock),
			nullptr);
	}

	return metadata;
}

void Mp4DemuxAdapter::setCaps(GstAppSrc* appsrc) const
{
	if (!appsrc)
	{
		return;
	}

	GstCaps* caps = nullptr;
	const auto& codecInfo = mCodecInfo;

	// Create codec data buffer if available
	GstBuffer* codec_data_buf = nullptr;
	if (!codecInfo.mCodecData.empty())
	{
		codec_data_buf = CreateGstBuffer(codecInfo.mCodecData.data(), codecInfo.mCodecData.size());
	}

	// Create caps based on codec format
	switch (codecInfo.mCodecFormat)
	{
		case GST_FORMAT_VIDEO_ES_H265:
			if (codec_data_buf)
			{
				caps = gst_caps_new_simple(
					"video/x-h265",
					"stream-format", G_TYPE_STRING, "hvc1",
					"alignment", G_TYPE_STRING, "au",
					"codec_data", GST_TYPE_BUFFER, codec_data_buf,
					"width", G_TYPE_INT, codecInfo.mInfo.video.mWidth,
					"height", G_TYPE_INT, codecInfo.mInfo.video.mHeight,
					"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					nullptr);
			}
			break;

		case GST_FORMAT_VIDEO_ES_H264:
			if (codec_data_buf)
			{
				caps = gst_caps_new_simple(
					"video/x-h264",
					"stream-format", G_TYPE_STRING, "avc",
					"alignment", G_TYPE_STRING, "au",
					"codec_data", GST_TYPE_BUFFER, codec_data_buf,
					"width", G_TYPE_INT, codecInfo.mInfo.video.mWidth,
					"height", G_TYPE_INT, codecInfo.mInfo.video.mHeight,
					"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					nullptr);
			}
			break;

		case GST_FORMAT_AUDIO_ES_AAC:
			if (codec_data_buf)
			{
				caps = gst_caps_new_simple(
					"audio/mpeg",
					"mpegversion", G_TYPE_INT, 4,
					"framed", G_TYPE_BOOLEAN, TRUE,
					"stream-format", G_TYPE_STRING, "raw",
					"codec_data", GST_TYPE_BUFFER, codec_data_buf,
					nullptr);
			}
			break;

		case GST_FORMAT_AUDIO_ES_EC3:
			caps = gst_caps_new_simple(
				"audio/x-eac3",
				"framed", G_TYPE_BOOLEAN, TRUE,
				"rate", G_TYPE_INT, codecInfo.mInfo.audio.mSampleRate,
				"channels", G_TYPE_INT, codecInfo.mInfo.audio.mChannelCount,
				nullptr);
			break;

		case GST_FORMAT_AUDIO_ES_AC3:
			caps = gst_caps_new_simple(
				"audio/x-ac3",
				"framed", G_TYPE_BOOLEAN, TRUE,
				"rate", G_TYPE_INT, codecInfo.mInfo.audio.mSampleRate,
				"channels", G_TYPE_INT, codecInfo.mInfo.audio.mChannelCount,
				nullptr);
			break;

		case GST_FORMAT_AUDIO_ES_AC4:
			caps = gst_caps_new_simple(
				"audio/x-ac4",
				"framed", G_TYPE_BOOLEAN, TRUE,
				"rate", G_TYPE_INT, codecInfo.mInfo.audio.mSampleRate,
				"channels", G_TYPE_INT, codecInfo.mInfo.audio.mChannelCount,
				nullptr);
			break;

		default:
			g_print("Unsupported codec format: %d\n", codecInfo.mCodecFormat);
			if (codec_data_buf)
			{
				gst_buffer_unref(codec_data_buf);
			}
			return;
	}

	if (caps)
	{
		// Add encryption caps if needed
		if (codecInfo.mIsEncrypted)
		{
			GstStructure* s = gst_caps_get_structure(caps, 0);
			gst_structure_set(s,
				"original-media-type", G_TYPE_STRING, gst_structure_get_name(s),
				GST_PROTECTION_SYSTEM_ID_CAPS_FIELD, G_TYPE_STRING, "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
				nullptr);
			gst_structure_set_name(s, "application/x-cenc");
		}

		gst_app_src_set_caps(appsrc, caps);
		gst_caps_unref(caps);
	}

	if (codec_data_buf)
	{
		gst_buffer_unref(codec_data_buf);
	}
}

size_t Mp4DemuxAdapter::getNumProtectionEvents() const
{
	return mProtectionEvents.size();
}

GstEvent* Mp4DemuxAdapter::getProtectionEvent(int which) const
{
	if (which < 0 || static_cast<size_t>(which) >= mProtectionEvents.size())
	{
		return nullptr;
	}

	// Lazily create GstEvents from protection data
	// Ensure the cache vector is sized appropriately
	if (mGstProtectionEvents.size() != mProtectionEvents.size())
	{
		// Clear and rebuild cache
		for (auto* event : mGstProtectionEvents)
		{
			if (event)
			{
				gst_event_unref(event);
			}
		}
		mGstProtectionEvents.clear();
		mGstProtectionEvents.resize(mProtectionEvents.size(), nullptr);
	}

	// Create event if not already cached
	if (!mGstProtectionEvents[which])
	{
		const auto& protInfo = mProtectionEvents[which];
		GstBuffer* pssh = CreateGstBuffer(protInfo.pssh.data(), protInfo.pssh.size());
		if (pssh)
		{
			mGstProtectionEvents[which] = gst_event_new_protection(
				protInfo.systemID.c_str(),
				pssh,
				"isobmff/moov");
			gst_buffer_unref(pssh);
		}
	}

	// Return a referenced event (caller should unref when done)
	if (mGstProtectionEvents[which])
	{
		return gst_event_ref(mGstProtectionEvents[which]);
	}

	return nullptr;
}

uint64_t Mp4DemuxAdapter::AdjustMediaDecodeTime(uint8_t* ptr, size_t len, int64_t pts_restamp_delta)
{
	uint64_t baseMediaDecodeTime = 0;
	const uint8_t* fin = &ptr[len];

	while (ptr < fin && !baseMediaDecodeTime)
	{
		uint32_t size = (uint32_t)(ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
		uint8_t* next = ptr + size;
		ptr += 4;
		uint32_t type = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
		ptr += 4;

		if (type == MultiChar_Constant("tfdt")) // Track Fragment Base Media Decode Time Box
		{
			uint8_t version = *ptr++;
			int sz = (version == 1) ? 8 : 4;
			ptr += 3; // skip flags
			baseMediaDecodeTime = 0;
			for (int i = 0; i < sz; i++)
			{
				baseMediaDecodeTime <<= 8;
				baseMediaDecodeTime |= ptr[i];
			}
			baseMediaDecodeTime += pts_restamp_delta;
			for (int i = 0; i < sz; i++)
			{
				ptr[i] = (baseMediaDecodeTime >> ((sz - 1 - i) * 8)) & 0xff;
			}
			break;
		}
		else
		{
			// Walk children
			switch (type)
			{
				case MultiChar_Constant("traf"): // Track Fragment Box
				case MultiChar_Constant("moov"): // Movie Box
				case MultiChar_Constant("trak"): // Track Box
				case MultiChar_Constant("minf"): // Media Information Box
				case MultiChar_Constant("dinf"): // Data Information Box
				case MultiChar_Constant("stbl"): // Sample Table Box
				case MultiChar_Constant("mvex"): // Movie Extends Box
				case MultiChar_Constant("moof"): // Movie Fragment Boxes
				case MultiChar_Constant("mdia"): // Media Box
					baseMediaDecodeTime = AdjustMediaDecodeTime(ptr, next - ptr, pts_restamp_delta);
					break;

				default:
					break;
			}
		}
		ptr = next;
	}

	return baseMediaDecodeTime;
}
