/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#ifndef FIREBOLT_RIALTO_SERVER_GST_MIME_MAPPING_H_
#define FIREBOLT_RIALTO_SERVER_GST_MIME_MAPPING_H_

#include "GstCapabilities.h"
#include "IGstWrapper.h"
#include "IMediaPipeline.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace firebolt::rialto::server
{

/**
 * @brief Converts MIME types to simple caps.
 *
 * @param[in] m_gstWrapper        : Member variable that is a shared pointer to an object of type IGstWrapper
 * @param[in] m_attachedSource    : A const reference to an object of type MediaSource
 *
 * @retval the pointer to the GstCaps object, which represents the simple caps for the given MIME type. If MIME type is
 * not found, an empty GstCaps object is returned
 */

inline GstCaps *createSimpleCapsFromMimeType(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper,
                                             const IMediaPipeline::MediaSource &m_attachedSource)
{
    static const std::unordered_map<std::string, std::string> mimeToMediaType =
        {{"video/h264", "video/x-h264"},        {"video/h265", "video/x-h265"},
         {"video/x-av1", "video/x-av1"},        {"video/x-vp9", "video/x-vp9"},
         {"video/mp4", "video/mpeg"},           {"audio/mp4", "audio/mpeg"},
         {"audio/mp3", "audio/mpeg"},           {"audio/aac", "audio/mpeg"},
         {"audio/x-eac3", "audio/x-eac3"},      {"audio/x-opus", "audio/x-opus"},
         {"audio/b-wav", "audio/b-wav"},        {"audio/x-raw", "audio/x-raw"},
         {"audio/x-flac", "audio/x-flac"},      {"text/vtt", "application/x-subtitle-vtt"},
         {"text/ttml", "application/ttml+xml"}, {"text/cc", "subtitle/x-subtitle-cc"}};
    auto mimeToMediaTypeIt = mimeToMediaType.find(m_attachedSource.getMimeType());
    if (mimeToMediaTypeIt != mimeToMediaType.end())
    {
        return m_gstWrapper->gstCapsNewEmptySimple(mimeToMediaTypeIt->second.c_str());
    }

    return m_gstWrapper->gstCapsNewEmpty();
}

/**
 * @brief Converts simple caps to MIME types.
 *
 * @param[in] supportedCaps    : A const reference to a vector of pointers to GstCaps objects representing the supported caps
 * @param[in] m_gstWrapper     : Member variable that is a shared pointer to an object of type IGstWrapper
 *
 * @retval an unordered set of strings representing the supported MIME types
 */

inline std::unordered_set<std::string>
convertFromCapsVectorToMimeSet(const std::vector<GstCaps *> &supportedCaps,
                               std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper)
{
    std::vector<std::pair<GstCaps *, std::vector<std::string>>> capsToMimeVec =
        {{m_gstWrapper->gstCapsFromString("audio/mpeg, mpegversion=(int)4"), {"audio/mp4", "audio/aac", "audio/x-eac3"}},
         {m_gstWrapper->gstCapsFromString("audio/mpeg, mpegversion=(int)1, layer=(int)3"), {"audio/mp3"}},
         {m_gstWrapper->gstCapsFromString("audio/x-eac3"), {"audio/x-eac3"}},
         {m_gstWrapper->gstCapsFromString("audio/b-wav"), {"audio/b-wav"}},
         {m_gstWrapper->gstCapsFromString("audio/x-raw"), {"audio/x-raw"}},
         {m_gstWrapper->gstCapsFromString("audio/x-opus"), {"audio/x-opus"}},
         {m_gstWrapper->gstCapsFromString("audio/x-opus, channel-mapping-family=(int)0"), {"audio/x-opus"}},
         {m_gstWrapper->gstCapsFromString("audio/x-flac"), {"audio/x-flac"}},
         {m_gstWrapper->gstCapsFromString("video/x-av1"), {"video/x-av1"}},
         {m_gstWrapper->gstCapsFromString("video/x-h264"), {"video/h264"}},
         {m_gstWrapper->gstCapsFromString("video/x-h265"), {"video/h265"}},
         {m_gstWrapper->gstCapsFromString("video/x-vp9"), {"video/x-vp9"}},
         {m_gstWrapper->gstCapsFromString("video/mpeg, mpegversion=(int)4"), {"video/mp4"}},
         {m_gstWrapper->gstCapsFromString("video/x-h264(memory:DMABuf)"), {"video/h264"}},
         {m_gstWrapper->gstCapsFromString("video/x-h265(memory:DMABuf)"), {"video/h265"}},
         {m_gstWrapper->gstCapsFromString("video/x-av1(memory:DMABuf)"), {"video/x-av1"}},
         {m_gstWrapper->gstCapsFromString("video/x-vp9(memory:DMABuf)"), {"video/x-vp9"}}};

    std::unordered_set<std::string> supportedMimes;

    for (GstCaps *caps : supportedCaps)
    {
        for (const auto &capsToMime : capsToMimeVec)
        {
            if (m_gstWrapper->gstCapsCanIntersect(capsToMime.first, caps))
            {
                supportedMimes.insert(capsToMime.second.begin(), capsToMime.second.end());
            }
        }
    }

    for (auto &capsToMime : capsToMimeVec)
    {
        if (capsToMime.first)
            m_gstWrapper->gstCapsUnref(capsToMime.first);
    }

    return supportedMimes;
}

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_MIME_MAPPING_H_
