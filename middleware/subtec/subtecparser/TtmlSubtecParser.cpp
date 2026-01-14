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

#include "TtmlSubtecParser.hpp"
#include <regex>

// #define TTML_DEBUG

TtmlSubtecParser::TtmlSubtecParser(SubtitleMimeType type, int width, int height) : SubtitleParser(type, width, height), m_channel(nullptr)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::TtmlSubtecParser\n" );
#endif
	m_channel = SubtecChannel::SubtecChannelFactory(SubtecChannel::ChannelType::TTML);
	if (!m_channel->InitComms())
	{
		MW_LOG_INFO("Init failed - subtitle parsing disabled");
		throw std::runtime_error("PacketSender init failed");
	}
	m_channel->SendResetAllPacket();

	int screenWidth = 1920, screenHeight = 1080;
	if(width != 0 && height != 0)
	{
		screenWidth = width;
		screenHeight = height;
	}
	m_channel->SendSelectionPacket(screenWidth, screenHeight);
	m_channel->SendMutePacket();
 	if(playerResumeTrackDownloads_CB)
 	{
		playerResumeTrackDownloads_CB();
 	}
}

bool TtmlSubtecParser::init(double startPosSeconds, unsigned long long basePTS)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::init(startPosSeconds=%.3fs,basePTS=%llu\n", startPosSeconds, basePTS );
#endif
	MW_LOG_INFO("startPosSeconds %.3fs basePTS=%llu", startPosSeconds, basePTS);
	m_channel->SendTimestampPacket(static_cast<uint64_t>(startPosSeconds * 1000.0));
 	if(playerResumeTrackDownloads_CB)
 	{
		playerResumeTrackDownloads_CB();
 	}

	m_parsedFirstPacket = false;
	m_sentOffset = false;
	m_firstBeginOffset = 0.0;

	return true;
}

void TtmlSubtecParser::updateTimestamp(unsigned long long positionMs)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::updateTimestamp(positionMs=%llu\n", positionMs );
#endif
	m_channel->SendTimestampPacket(positionMs);
}

void TtmlSubtecParser::reset()
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::reset\n" );
#endif
	m_channel->SendResetChannelPacket();
}

static std::int64_t parseFirstBegin(std::stringstream &ss)
{
	std::int64_t firstBegin = std::numeric_limits<std::int64_t>::max();
	std::string line;
	static const std::regex beginRegex(R"(begin="([0-9]+):([0-9][0-9]?):([0-9][0-9]?)\.?([0-9]+)?")");
	
	while(std::getline(ss, line))
	{
		try {
			bool matched = false;
			//Regex still works with no hours and/or no ms. Mins and secs are required
			std::smatch match;
			matched = std::regex_search(line, match, beginRegex);

			if (matched) {
				std::int64_t hours = 0, minutes = 0, seconds = 0, milliseconds = 0;

				if (!match.str(1).empty()) MW_LOG_WARN("h:%s", match.str(1).c_str()); hours = std::stol(match.str(1));
				if (!match.str(2).empty()) MW_LOG_WARN("m:%s", match.str(2).c_str()); minutes = std::stol(match.str(2));
				if (!match.str(3).empty()) MW_LOG_WARN("s:%s", match.str(3).c_str()); seconds = std::stol(match.str(3));
				if (!match.str(4).empty()) MW_LOG_WARN("ms:%s", match.str(4).c_str()); milliseconds = std::stol(match.str(4));

				firstBegin = milliseconds + (1000 * (seconds + (60 * (minutes + (60 * hours)))));
				break;
			}
		}
		catch (const std::regex_error& e) {
			MW_LOG_WARN("Regex error %s from line %s", std::to_string(e.code()).c_str(), line.c_str());
		}
	}
	
	return firstBegin;
}

bool TtmlSubtecParser::processData(const char* buffer, size_t bufferLen, double position, double duration)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::processData(bufferLen=%zu,position=%f,duration=%f)\n", bufferLen, position, duration );
#endif

	PlayerIsoBmffBuffer isobuf;

	isobuf.setBuffer( (uint8_t *)buffer, bufferLen);
	isobuf.parseBuffer();

	if (!isobuf.isInitSegment())
	{
		uint8_t *mdat;
		size_t mdatLen;

		//isobuf.printBoxes();
		isobuf.getMdatBoxSize(mdatLen);

		mdat = (uint8_t *)malloc(mdatLen);
		isobuf.parseMdatBox(mdat, mdatLen);

		std::vector<uint8_t> data(mdatLen);
		data.assign(mdat, mdat+mdatLen);
		
		//necessary because the offset into the TTML
		//is not available in the linear manifest
		//Take the first instance of the "begin" tag as the time offset for subtec
		if (!m_parsedFirstPacket && m_isLinear)
		{
			m_firstBeginOffset = position;
			m_parsedFirstPacket = true;
		}

		if (!m_sentOffset && m_parsedFirstPacket && m_isLinear)
		{
			MW_LOG_TRACE("Linear content - parsing first begin as offset - pos %.3f dur %.3f m_firstBeginOffset %.3f", 
				 position, duration, m_firstBeginOffset);
			std::stringstream ss(std::string(data.begin(), data.end()));
			std::int64_t offset = parseFirstBegin(ss);
			
			if (offset != std::numeric_limits<std::int64_t>::max())
			{
				auto positionDeltaSecs = (position - m_firstBeginOffset);
				long long getPositionMS = 0;
				double seekPositionSeconds = 0.0;
				if(playerGetPositions_CB)
 				{
					playerGetPositions_CB(getPositionMS, seekPositionSeconds);
 				}

				auto timeFromStartMs = getPositionMS - (seekPositionSeconds * 1000.0);
				std::int64_t totalOffset = offset - (positionDeltaSecs * 1000.0) + timeFromStartMs;

				std::stringstream output;
				output << "setting totalOffset " << totalOffset << " positionDeltaSecs " << positionDeltaSecs <<
					" timeFromStartMs " << timeFromStartMs;
				MW_LOG_TRACE("%s",  output.str().c_str());
				m_sentOffset = true;
				m_channel->SendTimestampPacket(totalOffset);
			}
		}

		m_channel->SendDataPacket(std::move(data), 0);

		free(mdat);
		MW_LOG_TRACE("Sent buffer with size %zu position %.3f", bufferLen, position);
	}
	else
	{
		MW_LOG_INFO("Init Segment");
	}
	return true;
}

void TtmlSubtecParser::mute(bool mute)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::mute(mute=%d)\n", mute );
#endif
	if (mute)
	{
		m_channel->SendMutePacket();
	}
	else
	{
		m_channel->SendUnmutePacket();
	}
}

void TtmlSubtecParser::pause(bool pause)
{
#ifdef TTML_DEBUG
	printf( "TtmlSubtecParser::pause(pause=%d)\n", pause );
#endif
	if (pause)
	{
		m_channel->SendPausePacket();
	}
	else
	{
		m_channel->SendResumePacket();
	}
}
