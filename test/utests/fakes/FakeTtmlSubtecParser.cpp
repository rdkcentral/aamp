/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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
#include "TtmlSubtecParser.hpp"

TtmlSubtecParser::TtmlSubtecParser(SubtitleMimeType type, int width, int height) : SubtitleParser(type, width, height), m_channel(nullptr)
{
}

bool TtmlSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
	return true;
}

void TtmlSubtecParser::updateTimestamp(unsigned long long positionMs)
{
}

void TtmlSubtecParser::reset()
{
}

//static std::int64_t parseFirstBegin(std::stringstream &ss)
//{
//	return 0;
//}

bool TtmlSubtecParser::processData(const char* buffer, size_t bufferLen, double position, double duration)
{
	return true;
}

void TtmlSubtecParser::mute(bool mute)
{
}

void TtmlSubtecParser::pause(bool pause)
{
}
