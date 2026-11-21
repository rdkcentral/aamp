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

#include "MediaCaps.h"

#include <cstring>
#include <map>
#include <string>

namespace rialto_reference
{

using firebolt::rialto::CodecData;
using firebolt::rialto::CodecDataType;
using firebolt::rialto::IMediaPipeline;
using firebolt::rialto::MediaSourceType;
using firebolt::rialto::SegmentAlignment;
using firebolt::rialto::StreamFormat;

namespace
{

struct MimeEntry
{
	const char *mediaType; // GStreamer caps media type
	int mpegVersion;       // 0 when not applicable
	int mpegLayer;         // 0 when not applicable
};

// Rialto MIME type -> caps. The first block matches the Rialto server's
// GstMimeMapping; audio/x-ac3 and audio/x-ac4 are additions (AAMP sends
// audio/x-ac4, which the server's table does not list).
const std::map<std::string, MimeEntry> &mimeTable()
{
	static const std::map<std::string, MimeEntry> table = {
		{"video/h264", {"video/x-h264", 0, 0}},
		{"video/h265", {"video/x-h265", 0, 0}},
		{"video/x-av1", {"video/x-av1", 0, 0}},
		{"video/x-vp9", {"video/x-vp9", 0, 0}},
		{"video/x-vp8", {"video/x-vp8", 0, 0}},
		{"video/mp4", {"video/mpeg", 0, 0}},
		{"audio/mp4", {"audio/mpeg", 4, 0}},
		{"audio/aac", {"audio/mpeg", 4, 0}},
		{"audio/mp3", {"audio/mpeg", 1, 3}},
		{"audio/x-eac3", {"audio/x-eac3", 0, 0}},
		{"audio/x-opus", {"audio/x-opus", 0, 0}},
		{"audio/x-flac", {"audio/x-flac", 0, 0}},
		{"audio/x-vorbis", {"audio/x-vorbis", 0, 0}},
		{"audio/x-raw", {"audio/x-raw", 0, 0}},
		{"audio/b-wav", {"audio/b-wav", 0, 0}},
		{"text/vtt", {"application/x-subtitle-vtt", 0, 0}},
		{"text/ttml", {"application/ttml+xml", 0, 0}},
		{"text/cc", {"subtitle/x-subtitle-cc", 0, 0}},
		{"audio/x-ac3", {"audio/x-ac3", 0, 0}},
		{"audio/x-ac4", {"audio/x-ac4", 0, 0}},
	};
	return table;
}

void setCodecData(GstCaps *caps, const CodecData &codecData)
{
	if (codecData.type == CodecDataType::STRING)
	{
		const std::string text(codecData.data.begin(), codecData.data.end());
		gst_caps_set_simple(caps, "codec_data", G_TYPE_STRING, text.c_str(), nullptr);
		return;
	}
	GstBuffer *buffer = gst_buffer_new_allocate(nullptr, codecData.data.size(), nullptr);
	gst_buffer_fill(buffer, 0, codecData.data.data(), codecData.data.size());
	gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, buffer, nullptr);
	gst_buffer_unref(buffer);
}

bool codecDataEquals(const GstStructure *structure, const CodecData &codecData)
{
	const GValue *value = gst_structure_get_value(structure, "codec_data");
	if (!value)
	{
		return false;
	}
	if (codecData.type == CodecDataType::STRING)
	{
		const char *text = G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : nullptr;
		return text && std::string(text) == std::string(codecData.data.begin(), codecData.data.end());
	}
	if (!GST_VALUE_HOLDS_BUFFER(value))
	{
		return false;
	}
	GstBuffer *buffer = gst_value_get_buffer(value);
	return gst_buffer_get_size(buffer) == codecData.data.size() &&
	       gst_buffer_memcmp(buffer, 0, codecData.data.data(), codecData.data.size()) == 0;
}

const char *toCapsString(SegmentAlignment alignment)
{
	switch (alignment)
	{
		case SegmentAlignment::AU:
			return "au";
		case SegmentAlignment::NAL:
			return "nal";
		default:
			return nullptr;
	}
}

const char *toCapsString(StreamFormat format)
{
	switch (format)
	{
		case StreamFormat::RAW:
			return "raw";
		case StreamFormat::AVC:
			return "avc";
		case StreamFormat::BYTE_STREAM:
			return "byte-stream";
		case StreamFormat::HVC1:
			return "hvc1";
		case StreamFormat::HEV1:
			return "hev1";
		default:
			return nullptr;
	}
}

bool setIntIfChanged(GstStructure *structure, const char *field, int value)
{
	int current = 0;
	if (value <= 0 || (gst_structure_get_int(structure, field, &current) && current == value))
	{
		return false;
	}
	gst_structure_set(structure, field, G_TYPE_INT, value, nullptr);
	return true;
}

} // namespace

GstCaps *buildSourceCaps(const IMediaPipeline::MediaSource &source)
{
	auto it = mimeTable().find(source.getMimeType());
	if (it == mimeTable().end())
	{
		return nullptr;
	}
	const MimeEntry &entry = it->second;
	GstCaps *caps = gst_caps_new_empty_simple(entry.mediaType);
	if (entry.mpegVersion)
	{
		gst_caps_set_simple(caps, "mpegversion", G_TYPE_INT, entry.mpegVersion, nullptr);
	}
	if (entry.mpegLayer)
	{
		gst_caps_set_simple(caps, "layer", G_TYPE_INT, entry.mpegLayer, nullptr);
	}

	const MediaSourceType type = source.getType();
	if (type != MediaSourceType::AUDIO && type != MediaSourceType::VIDEO)
	{
		return caps;
	}

	const auto &av = static_cast<const IMediaPipeline::MediaSourceAV &>(source);
	if (const char *alignment = toCapsString(av.getSegmentAlignment()))
	{
		gst_caps_set_simple(caps, "alignment", G_TYPE_STRING, alignment, nullptr);
	}
	if (const char *format = toCapsString(av.getStreamFormat()))
	{
		gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, format, nullptr);
	}
	if (av.getCodecData())
	{
		setCodecData(caps, *av.getCodecData());
	}

	if (type == MediaSourceType::VIDEO)
	{
		const auto &video = static_cast<const IMediaPipeline::MediaSourceVideo &>(source);
		if (video.getWidth() != firebolt::rialto::kUndefinedSize)
		{
			gst_caps_set_simple(caps, "width", G_TYPE_INT, video.getWidth(), nullptr);
		}
		if (video.getHeight() != firebolt::rialto::kUndefinedSize)
		{
			gst_caps_set_simple(caps, "height", G_TYPE_INT, video.getHeight(), nullptr);
		}
	}
	else
	{
		const auto &config = static_cast<const IMediaPipeline::MediaSourceAudio &>(source).getAudioConfig();
		// Raw AAC cannot be decoded without its AudioSpecificConfig. The server
		// only reads the source codec data; fall back to the audio config's copy.
		if (!av.getCodecData() && !config.codecSpecificConfig.empty())
		{
			setCodecData(caps, CodecData{config.codecSpecificConfig, CodecDataType::BUFFER});
		}
		if (config.numberOfChannels != firebolt::rialto::kInvalidAudioChannels)
		{
			gst_caps_set_simple(caps, "channels", G_TYPE_INT, static_cast<int>(config.numberOfChannels), nullptr);
		}
		if (config.sampleRate != firebolt::rialto::kInvalidAudioSampleRate)
		{
			gst_caps_set_simple(caps, "rate", G_TYPE_INT, static_cast<int>(config.sampleRate), nullptr);
		}
	}
	return caps;
}

GstCaps *updateCapsForSegment(const GstCaps *current, const IMediaPipeline::MediaSegment &segment)
{
	if (!current || gst_caps_is_empty(current))
	{
		return nullptr;
	}
	GstCaps *caps = gst_caps_copy(current);
	GstStructure *structure = gst_caps_get_structure(caps, 0);
	bool changed = false;

	const auto &codecData = segment.getCodecData();
	if (codecData && !codecData->data.empty() && !codecDataEquals(structure, *codecData))
	{
		setCodecData(caps, *codecData);
		structure = gst_caps_get_structure(caps, 0);
		changed = true;
	}

	// The segment's concrete class follows its type (the API constructs
	// MediaSegmentVideo/MediaSegmentAudio for VIDEO/AUDIO).
	if (segment.getType() == MediaSourceType::VIDEO)
	{
		const auto &video = static_cast<const IMediaPipeline::MediaSegmentVideo &>(segment);
		changed |= setIntIfChanged(structure, "width", video.getWidth());
		changed |= setIntIfChanged(structure, "height", video.getHeight());
		const auto frameRate = video.getFrameRate();
		if (frameRate.numerator > 0 && frameRate.denominator > 0)
		{
			int num = 0;
			int den = 0;
			if (!gst_structure_get_fraction(structure, "framerate", &num, &den) || num != frameRate.numerator ||
			    den != frameRate.denominator)
			{
				gst_structure_set(structure, "framerate", GST_TYPE_FRACTION, frameRate.numerator, frameRate.denominator,
				                  nullptr);
				changed = true;
			}
		}
	}
	else if (segment.getType() == MediaSourceType::AUDIO)
	{
		const auto &audio = static_cast<const IMediaPipeline::MediaSegmentAudio &>(segment);
		changed |= setIntIfChanged(structure, "rate", audio.getSampleRate());
		changed |= setIntIfChanged(structure, "channels", audio.getNumberOfChannels());
	}

	if (!changed)
	{
		gst_caps_unref(caps);
		return nullptr;
	}
	return caps;
}

} // namespace rialto_reference
