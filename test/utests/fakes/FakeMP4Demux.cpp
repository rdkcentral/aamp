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

/**
 * @file FakeMP4Demux.cpp
 * @brief Fake implementation of MP4Demux for unit testing
 */

#include "MP4Demux.h"
#include "MockMp4Demux.h"
#include "AampLogManager.h"
#include <vector>

// Global mock instance used by the testable AampMp4Demuxer
MockMp4Demux *g_mockMp4Demux = nullptr;

/**
 * @brief Fake MP4Demux constructor
 */
Mp4Demux::Mp4Demux()
{
    AAMPLOG_INFO("FakeMP4Demux Constructor");
}

/**
 * @brief Fake MP4Demux destructor
 */
Mp4Demux::~Mp4Demux()
{
    AAMPLOG_INFO("FakeMP4Demux Destructor");
}

/**
 * @brief Fake Parse implementation - delegates to mock if available
 * @param ptr Pointer to MP4 data
 * @param len Length of data
 * @return true if parsing was successful
 */
bool Mp4Demux::Parse(const void *ptr, size_t len)
{
    AAMPLOG_INFO("FakeMP4Demux::Parse called with %zu bytes", len);
    
    // Delegate to mock if available
    if (g_mockMp4Demux) {
        g_mockMp4Demux->Parse(ptr, len);
    }
    // Otherwise, do nothing (fake behavior)
    return true;
}

/**
 * @brief Fake GetTimeScale implementation
 * @return Default timescale value
 */
uint32_t Mp4Demux::GetTimeScale() const
{
    AAMPLOG_INFO("FakeMP4Demux::GetTimeScale called");
    
    // Delegate to mock if available
    if (g_mockMp4Demux) {
        return g_mockMp4Demux->GetTimeScale();
    }
    
    // Default fake value
    return 90000; // Common timescale for video
}

/**
 * @brief Fake GetCodecInfo implementation
 * @return Default codec info
 */
AampCodecInfo Mp4Demux::GetCodecInfo()
{
    AAMPLOG_INFO("FakeMP4Demux::GetCodecInfo called");
    
    // Delegate to mock if available
    if (g_mockMp4Demux) {
        return g_mockMp4Demux->GetCodecInfo();
    }
    
    // Default fake codec info
    AampCodecInfo codecInfo;
    codecInfo.mCodecFormat = FORMAT_INVALID;
    codecInfo.mIsEncrypted = false;
    return codecInfo;
}

/**
 * @brief Fake GetProtectionEvents implementation
 * @return Empty protection events vector
 */
std::vector<AampPsshData> Mp4Demux::GetProtectionEvents()
{
    AAMPLOG_INFO("FakeMP4Demux::GetProtectionEvents called");
    
    // Delegate to mock if available
    if (g_mockMp4Demux) {
        return g_mockMp4Demux->GetProtectionEvents();
    }
    
    // Default fake - no protection events
    return std::vector<AampPsshData>();
}

/**
 * @brief Fake GetSamples implementation
 * @return Empty samples vector or mock-provided samples
 */
std::vector<AampMediaSample> Mp4Demux::GetSamples()
{
    AAMPLOG_INFO("FakeMP4Demux::GetSamples called");
    
    // Delegate to mock if available
    if (g_mockMp4Demux) {
        return g_mockMp4Demux->GetSamples();
    }
    
    // Default fake - no samples
    return std::vector<AampMediaSample>();
}