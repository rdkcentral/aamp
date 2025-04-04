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
 * @file  WebvttSubtecDevInterface.hpp
 * 
 * @brief This file provides interfaces for a subtitle parser in Player
 *
 */


#ifndef __WEBVTT_SUBTEC_DEV_INTERFACE_HPP__
#define __WEBVTT_SUBTEC_DEV_INTERFACE_HPP__

#include "SubtecChannel.hpp"

class WebvttSubtecDevInterface
{
public:
	WebvttSubtecDevInterface(int width, int height);
	~WebvttSubtecDevInterface(){}

	bool init(unsigned long long basePTS);
	void reset() ;
	void sendCueData(const std::string& ttml);
	void updateTimestamp(unsigned long long positionMs);
	void pause(bool pause);
	void mute(bool mute);
protected:
	std::unique_ptr<SubtecChannel> m_channel;
};

#endif /* __WEBVTT_SUBTEC_DEV_INTERFACE_HPP__ */
