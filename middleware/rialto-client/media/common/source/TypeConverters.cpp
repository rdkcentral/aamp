/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#include "TypeConverters.h"
#include <unordered_map>

namespace firebolt::rialto::common
{
const char *convertMediaSourceType(const MediaSourceType &mediaSourceType)
{
    static const std::unordered_map<MediaSourceType, const char *>
        kMediaSourceTypeToStr{{MediaSourceType::AUDIO, "Audio"},
                              {MediaSourceType::VIDEO, "Video"},
                              {MediaSourceType::SUBTITLE, "Subtitle"},
                              {MediaSourceType::UNKNOWN, "Unknown"}};
    const auto kIt = kMediaSourceTypeToStr.find(mediaSourceType);
    if (kMediaSourceTypeToStr.end() != kIt)
    {
        return kIt->second;
    }
    return "Unknown";
}

const char *convertLayout(const Layout &layout)
{
    static const std::unordered_map<Layout, const char *> kLayoutToStr{{Layout::INTERLEAVED, "interleaved"},
                                                                       {Layout::NON_INTERLEAVED, "non-interleaved"}};
    const auto kIt = kLayoutToStr.find(layout);
    if (kLayoutToStr.end() != kIt)
    {
        return kIt->second;
    }
    return "";
}

const char *convertFormat(const Format &format)
{
    static const std::unordered_map<Format, const char *> kFormatToStr{{Format::S8, "S8"},
                                                                       {Format::U8, "U8"},
                                                                       {Format::S16LE, "S16LE"},
                                                                       {Format::S16BE, "S16BE"},
                                                                       {Format::U16LE, "U16LE"},
                                                                       {Format::U16BE, "U16BE"},
                                                                       {Format::S24_32LE, "S24_32LE"},
                                                                       {Format::S24_32BE, "S24_32BE"},
                                                                       {Format::U24_32LE, "U24_32LE"},
                                                                       {Format::U24_32BE, "U24_32BE"},
                                                                       {Format::S32LE, "S32LE"},
                                                                       {Format::S32BE, "S32BE"},
                                                                       {Format::U32LE, "U32LE"},
                                                                       {Format::U32BE, "U32BE"},
                                                                       {Format::S24LE, "S24LE"},
                                                                       {Format::S24BE, "S24BE"},
                                                                       {Format::U24LE, "U24LE"},
                                                                       {Format::U24BE, "U24BE"},
                                                                       {Format::S20LE, "S20LE"},
                                                                       {Format::S20BE, "S20BE"},
                                                                       {Format::U20LE, "U20LE"},
                                                                       {Format::U20BE, "U20BE"},
                                                                       {Format::S18LE, "S18LE"},
                                                                       {Format::S18BE, "S18BE"},
                                                                       {Format::U18LE, "U18LE"},
                                                                       {Format::U18BE, "U18BE"},
                                                                       {Format::F32LE, "F32LE"},
                                                                       {Format::F32BE, "F32BE"},
                                                                       {Format::F64LE, "F64LE"},
                                                                       {Format::F64BE, "F64BE"}};
    const auto kIt = kFormatToStr.find(format);
    if (kFormatToStr.end() != kIt)
    {
        return kIt->second;
    }
    return "";
}
} // namespace firebolt::rialto::common
