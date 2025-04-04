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
#include "playerisobmffbuffer.h"
#include "SubtecChannel.hpp"

class TtmlSubtecParser : public SubtitleParser
{
public:
	TtmlSubtecParser(SubtitleMimeType type, int width, int height);
	
	TtmlSubtecParser(const TtmlSubtecParser&) = delete;
	TtmlSubtecParser& operator=(const TtmlSubtecParser&) = delete;

	
	bool init(double startPosSeconds, unsigned long long basePTS) override;
	bool processData(const char* buffer, size_t bufferLen, double position, double duration) override;
	bool close() override { return true; }
	void reset() override;
	void setProgressEventOffset(double offset) override {}
	void updateTimestamp(unsigned long long positionMs) override;
	void pause(bool pause) override;
	void mute(bool mute) override;

	void isLinear(bool isLinear) override { m_isLinear = isLinear; }

protected:
	std::unique_ptr<SubtecChannel> m_channel;
	bool m_isLinear = false;
	bool m_parsedFirstPacket = false;
	bool m_sentOffset = false;
	double m_firstBeginOffset = 0.0;
};
