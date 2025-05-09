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

#pragma once

#include "subtitleParser.h"
#include "SubtecChannel.hpp"

class WebVTTSubtecParser : public SubtitleParser
{
public:
	WebVTTSubtecParser(SubtitleMimeType type, int width, int height);
	
	WebVTTSubtecParser(const WebVTTSubtecParser&) = delete;
	WebVTTSubtecParser& operator=(const WebVTTSubtecParser&) = delete;

	
	bool init(double startPosSeconds, unsigned long long basePTS) override;
	bool processData(const char* buffer, size_t bufferLen, double position, double duration) override;
	bool close() override { return true; }
	void reset() override;
	void setProgressEventOffset(double offset) override {}
	void updateTimestamp(unsigned long long positionMs) override;
	void pause(bool pause) override;
	void mute(bool mute) override;
	void setTextStyle(const std::string &options) override;
protected:
	std::unique_ptr<SubtecChannel> m_channel;
private:
	std::uint64_t time_offset_ms_ = 0;
	std::uint64_t start_ms_ = 0;
};
