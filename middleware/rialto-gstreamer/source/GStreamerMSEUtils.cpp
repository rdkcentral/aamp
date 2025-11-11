/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "GStreamerMSEUtils.h"
#include "GStreamerUtils.h"
#include "GstreamerCatLog.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#define GST_CAT_DEFAULT rialtoGStreamerCat

void rialto_mse_sink_setup_supported_caps(GstElementClass *elementClass,
                                          const std::vector<std::string> &supportedMimeTypes)
{
    static const std::unordered_map<std::string, std::vector<std::string>> kMimeToCaps =
        {{"audio/mp4", {"audio/mpeg, mpegversion=1", "audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/mp3", {"audio/mpeg, mpegversion=1", "audio/mpeg, mpegversion=2"}},
         {"audio/aac", {"audio/mpeg, mpegversion=2", "audio/mpeg, mpegversion=4"}},
         {"audio/x-eac3", {"audio/x-ac3", "audio/x-eac3"}},
         {"audio/x-opus", {"audio/x-opus"}},
         {"audio/b-wav", {"audio/b-wav"}},
         {"audio/x-flac", {"audio/x-flac"}},
         {"audio/x-raw", {"audio/x-raw"}},
         {"video/h264", {"video/x-h264"}},
         {"video/h265", {"video/x-h265"}},
         {"video/x-av1", {"video/x-av1"}},
         {"video/x-vp9", {"video/x-vp9"}},
         {"text/vtt", {"text/vtt", "application/x-subtitle-vtt"}},
         {"text/ttml", {"application/ttml+xml"}},
         {"text/cc",
          {"closedcaption/x-cea-608", "closedcaption/x-cea-708", "application/x-cea-608", "application/x-cea-708",
           "application/x-subtitle-cc"}}};

    std::unordered_set<std::string> addedCaps; // keep track what caps were added to avoid duplicates
    GstCaps *caps = gst_caps_new_empty();
    for (const std::string &mime : supportedMimeTypes)
    {
        auto mimeToCapsIt = kMimeToCaps.find(mime);
        if (mimeToCapsIt != kMimeToCaps.end())
        {
            for (const std::string &capsStr : mimeToCapsIt->second)
            {
                if (addedCaps.find(capsStr) == addedCaps.end())
                {
                    GST_INFO("Caps '%s' is supported", capsStr.c_str());
                    GstCaps *newCaps = gst_caps_from_string(capsStr.c_str());
                    gst_caps_append(caps, newCaps);
                    addedCaps.insert(capsStr);
                }
            }
        }
        else
        {
            GST_WARNING("Mime '%s' is not supported", mime.c_str());
        }
    }

    GstPadTemplate *sinktempl = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
    gst_element_class_add_pad_template(elementClass, sinktempl);
    gst_caps_unref(caps);
}

std::optional<firebolt::rialto::Layout> rialto_mse_sink_convert_layout(const gchar *layoutStr)
{
    if (g_strcmp0(layoutStr, "interleaved") == 0)
    {
        return firebolt::rialto::Layout::INTERLEAVED;
    }
    if (g_strcmp0(layoutStr, "non-interleaved") == 0)
    {
        return firebolt::rialto::Layout::NON_INTERLEAVED;
    }
    return std::nullopt;
}

std::optional<firebolt::rialto::Format> rialto_mse_sink_convert_format(const gchar *formatStr)
{
    static const std::unordered_map<std::string, firebolt::rialto::Format>
        kStringToFormat{{"S8", firebolt::rialto::Format::S8},
                        {"U8", firebolt::rialto::Format::U8},
                        {"S16LE", firebolt::rialto::Format::S16LE},
                        {"S16BE", firebolt::rialto::Format::S16BE},
                        {"U16LE", firebolt::rialto::Format::U16LE},
                        {"U16BE", firebolt::rialto::Format::U16BE},
                        {"S24_32LE", firebolt::rialto::Format::S24_32LE},
                        {"S24_32BE", firebolt::rialto::Format::S24_32BE},
                        {"U24_32LE", firebolt::rialto::Format::U24_32LE},
                        {"U24_32BE", firebolt::rialto::Format::U24_32BE},
                        {"S32LE", firebolt::rialto::Format::S32LE},
                        {"S32BE", firebolt::rialto::Format::S32BE},
                        {"U32LE", firebolt::rialto::Format::U32LE},
                        {"U32BE", firebolt::rialto::Format::U32BE},
                        {"S24LE", firebolt::rialto::Format::S24LE},
                        {"S24BE", firebolt::rialto::Format::S24BE},
                        {"U24LE", firebolt::rialto::Format::U24LE},
                        {"U24BE", firebolt::rialto::Format::U24BE},
                        {"S20LE", firebolt::rialto::Format::S20LE},
                        {"S20BE", firebolt::rialto::Format::S20BE},
                        {"U20LE", firebolt::rialto::Format::U20LE},
                        {"U20BE", firebolt::rialto::Format::U20BE},
                        {"S18LE", firebolt::rialto::Format::S18LE},
                        {"S18BE", firebolt::rialto::Format::S18BE},
                        {"U18LE", firebolt::rialto::Format::U18LE},
                        {"U18BE", firebolt::rialto::Format::U18BE},
                        {"F32LE", firebolt::rialto::Format::F32LE},
                        {"F32BE", firebolt::rialto::Format::F32BE},
                        {"F64LE", firebolt::rialto::Format::F64LE},
                        {"F64BE", firebolt::rialto::Format::F64BE}};
    const auto it = kStringToFormat.find(formatStr);
    if (it != kStringToFormat.end())
    {
        return it->second;
    }
    return std::nullopt;
}

std::shared_ptr<firebolt::rialto::CodecData> get_codec_data(const GstStructure *structure)
{
    const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
    if (codec_data)
    {
        GstBuffer *buf = gst_value_get_buffer(codec_data);
        if (buf)
        {
            GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
            if (mappedBuf)
            {
                auto codecData = std::make_shared<firebolt::rialto::CodecData>();
                codecData->data = std::vector<std::uint8_t>(mappedBuf.data(), mappedBuf.data() + mappedBuf.size());
                codecData->type = firebolt::rialto::CodecDataType::BUFFER;
                return codecData;
            }
            else
            {
                GST_ERROR("Failed to read codec_data");
                return nullptr;
            }
        }
        const gchar *str = g_value_get_string(codec_data);
        if (str)
        {
            auto codecData = std::make_shared<firebolt::rialto::CodecData>();
            codecData->data = std::vector<std::uint8_t>(str, str + std::strlen(str));
            codecData->type = firebolt::rialto::CodecDataType::STRING;
            return codecData;
        }
    }

    return nullptr;
}

firebolt::rialto::SegmentAlignment get_segment_alignment(const GstStructure *s)
{
    const gchar *alignment = gst_structure_get_string(s, "alignment");
    if (alignment)
    {
        GST_DEBUG("Alignment found %s", alignment);
        if (strcmp(alignment, "au") == 0)
        {
            return firebolt::rialto::SegmentAlignment::AU;
        }
        else if (strcmp(alignment, "nal") == 0)
        {
            return firebolt::rialto::SegmentAlignment::NAL;
        }
    }

    return firebolt::rialto::SegmentAlignment::UNDEFINED;
}

bool get_dv_profile(const GstStructure *s, uint32_t &dvProfile)
{
    gboolean isDolbyVisionEnabled = false;
    if (gst_structure_get_boolean(s, "dovi-stream", &isDolbyVisionEnabled) && isDolbyVisionEnabled)
    {
        if (gst_structure_get_uint(s, "dv_profile", &dvProfile))
        {
            return true;
        }
    }
    return false;
}

firebolt::rialto::StreamFormat get_stream_format(const GstStructure *structure)
{
    const gchar *streamFormat = gst_structure_get_string(structure, "stream-format");
    firebolt::rialto::StreamFormat format = firebolt::rialto::StreamFormat::UNDEFINED;
    if (streamFormat)
    {
        static const std::unordered_map<std::string, firebolt::rialto::StreamFormat> stringToStreamFormatMap =
            {{"raw", firebolt::rialto::StreamFormat::RAW},
             {"avc", firebolt::rialto::StreamFormat::AVC},
             {"byte-stream", firebolt::rialto::StreamFormat::BYTE_STREAM},
             {"hvc1", firebolt::rialto::StreamFormat::HVC1},
             {"hev1", firebolt::rialto::StreamFormat::HEV1}};

        auto strToStreamFormatIt = stringToStreamFormatMap.find(streamFormat);
        if (strToStreamFormatIt != stringToStreamFormatMap.end())
        {
            format = strToStreamFormatIt->second;
        }
    }

    return format;
}