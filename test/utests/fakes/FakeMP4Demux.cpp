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

// Global mock instance used by the testable AampMp4Demuxer
MockMp4Demux *g_mockMp4Demux = nullptr;

/**
 * @brief Fake MP4Demux constructor
 */
Mp4Demux::Mp4Demux()
{
}

/**
 * @brief Fake MP4Demux destructor
 */
Mp4Demux::~Mp4Demux()
{
}

/**
 * @brief Fake Parse implementation - delegates to mock if available
 * @param segment Shared ownership of the buffer to parse
 * @return true if parsing was successful
 */
bool Mp4Demux::Parse(std::shared_ptr<std::vector<uint8_t>>&& segment)
{
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->Parse(std::move(segment));
	}
	return true;
}

/**
 * @brief Fake GetTimeScale implementation
 * @return Default timescale value
 */
uint32_t Mp4Demux::GetTimeScale() const
{
	// Delegate to mock if available
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->GetTimeScale();
	}
	
	// Default fake value
	return 1; // Common timescale for video
}

/**
 * @brief Fake GetCodecInfo implementation
 * @return Default codec info
 */
MediaCodecInfo Mp4Demux::GetCodecInfo()
{
	// Delegate to mock if available
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->GetCodecInfo();
	}
	
	// Default fake codec info
	MediaCodecInfo codecInfo;
	codecInfo.mCodecFormat = GST_FORMAT_INVALID;
	codecInfo.mIsEncrypted = false;
	return codecInfo;
}

/**
 * @brief Fake GetProtectionEvents implementation
 * @return Empty protection events vector
 */
std::vector<MediaProtectionInfo> Mp4Demux::GetProtectionEvents()
{
	// Delegate to mock if available
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->GetProtectionEvents();
	}
	
	// Default fake - no protection events
	return std::vector<MediaProtectionInfo>();
}

/**
 * @brief Fake GetSamples implementation
 * @return Empty samples vector or mock-provided samples
 */
std::vector<AampMediaSample> Mp4Demux::GetSamples()
{
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->GetSamples();
	}
	return std::vector<AampMediaSample>();
}

/**
 * @brief Fake GetLastError implementation
 * @return Default parse error code
 */
Mp4ParseError Mp4Demux::GetLastError() const
{
	if (g_mockMp4Demux) {
		return g_mockMp4Demux->GetLastError();
	}
	return MP4_PARSE_OK;
}