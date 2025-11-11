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

#include "WebvttSubtecDevInterface.hpp"
#include "PlayerLogManager.h"
#include <sstream>

WebvttSubtecDevInterface::WebvttSubtecDevInterface(int width, int height) : m_channel(nullptr)
{
	m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::TTML);
	if (!m_channel->InitComms())
	{
		MW_LOG_INFO("Init failed - subtitle parsing disabled");
		throw std::runtime_error("PacketSender init failed");
	}
	m_channel->SendResetAllPacket();
	m_channel->SendSelectionPacket(width, height);
	m_channel->SendMutePacket();
}

void WebvttSubtecDevInterface::sendCueData(const std::string& ttml)
{
	std::vector<uint8_t> data(ttml.begin(), ttml.end());
	
	m_channel->SendDataPacket(std::move(data), 0);
}

void WebvttSubtecDevInterface::reset()
{
	m_channel->SendResetChannelPacket();
}

void WebvttSubtecDevInterface::updateTimestamp(unsigned long long positionMs)
{
	MW_LOG_TRACE("timestamp: %lldms", positionMs );
	m_channel->SendTimestampPacket(positionMs);
}

bool WebvttSubtecDevInterface::init(unsigned long long basePTS)
{
	bool ret = true;
	m_channel->SendTimestampPacket(static_cast<uint64_t>(basePTS));

	return ret;
}

void WebvttSubtecDevInterface::mute(bool mute)
{
	if (mute)
		m_channel->SendMutePacket();
	else
		m_channel->SendUnmutePacket();
}

void WebvttSubtecDevInterface::pause(bool pause)
{
	if (pause)
		m_channel->SendPausePacket();
	else
		m_channel->SendResumePacket();
}

