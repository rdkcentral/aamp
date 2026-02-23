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

#ifndef MOCK_MP4_DEMUX_H
#define MOCK_MP4_DEMUX_H

#include <gmock/gmock.h>
#include <vector>
#include "AampDemuxDataTypes.h"

class MockMp4Demux
{
public:
    MOCK_METHOD(bool, Parse, (const void *ptr, size_t len));
    MOCK_METHOD(uint32_t, GetTimeScale, (), (const));
    MOCK_METHOD(MediaCodecInfo, GetCodecInfo, ());
    MOCK_METHOD(std::vector<MediaProtectionInfo>, GetProtectionEvents, ());
    MOCK_METHOD(std::vector<AampMediaSample>, GetSamples, ());
    MOCK_METHOD(Mp4ParseError, GetLastError, (), (const));
};

extern MockMp4Demux *g_mockMp4Demux;

#endif /* MOCK_MP4_DEMUX_H */