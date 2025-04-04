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

#include "WebVttSubtecParser.hpp"
#include "TextStyleAttributes.h"

WebVTTSubtecParser::WebVTTSubtecParser(SubtitleMimeType type, int width, int height) : SubtitleParser(type, width, height), m_channel(nullptr)
{
	m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::WEBVTT);
	if (!m_channel->InitComms())
	{
		MW_LOG_INFO("Init failed - subtitle parsing disabled");
		throw std::runtime_error("PacketSender init failed");
	}
	MW_LOG_INFO("Sending RESET ALL");
	m_channel->SendResetAllPacket();
	int screenWidth = 1920, screenHeight = 1080;
	if(width != 0 && height != 0)
	{
		screenWidth = width;
		screenHeight = height;
	}
	m_channel->SendMutePacket();
	m_channel->SendSelectionPacket(screenWidth, screenHeight);
}

void WebVTTSubtecParser::updateTimestamp(unsigned long long positionMs)
{
	MW_LOG_INFO("positionMs %lld",  positionMs);
	m_channel->SendTimestampPacket(positionMs);
}

void WebVTTSubtecParser::reset()
{
	m_channel->SendMutePacket();
	m_channel->SendResetChannelPacket();
}

bool WebVTTSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
	MW_LOG_INFO("startPos %f basePTS %lld",  startPosSeconds, basePTS);

	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPosSeconds*1000.0));
 	if(playerResumeTrackDownloads_CB)
 	{
		playerResumeTrackDownloads_CB();
 	}

	return true;
}

bool WebVTTSubtecParser::processData(const char* buffer, size_t bufferLen, double position, double duration)
{
	std::string str(const_cast<const char*>(buffer), bufferLen);
	std::vector<uint8_t> data(str.begin(), str.end());

	m_channel->SendDataPacket(std::move(data), 0);

	return true;
}

void WebVTTSubtecParser::mute(bool mute)
{
	if (mute)
		m_channel->SendMutePacket();
	else
		m_channel->SendUnmutePacket();
}

void WebVTTSubtecParser::pause(bool pause)
{
	if (pause)
		m_channel->SendPausePacket();
	else
		m_channel->SendResumePacket();
}


void WebVTTSubtecParser::setTextStyle(const std::string &options)
{
	TextStyleAttributes textStyleAttributes;
	uint32_t attributesMask = 0;
	attributesType attributesValues = {0};

	int ccType = 0;			/* Value not used by WebVTT */

	if (!textStyleAttributes.getAttributes(options, attributesValues, attributesMask))
	{
		if (attributesMask)
		{
			m_channel->SendCCSetAttributePacket(ccType, attributesMask, attributesValues);
		}
	}
}
