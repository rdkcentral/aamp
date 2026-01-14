/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file  subtitleParser.h
 * 
 * @brief This file provides interfaces for a subtitle parser in Player
 *
 */

#ifndef __SUBTITLE_PARSER_H__
#define __SUBTITLE_PARSER_H__

#include <string>
#include <functional>
#include "vttCue.h"
#include "PlayerLogManager.h"
#include <glib.h>
#include <iostream>
#include <sstream>

/**
* \enum       SubtitleMimeType
* \brief      Subtitle data types
*/
typedef enum
{
	eSUB_TYPE_WEBVTT,
	eSUB_TYPE_MP4,
	eSUB_TYPE_TTML,
	eSUB_TYPE_UNKNOWN
} SubtitleMimeType;

/**
 * @brief Structure holding the player callback.
 */
struct PlayerCallbacks {
    std::function<void()> resumeTrackDownloads_CB; /**< Callback to resume track downloads */
    std::function<void()> stopTrackDownloads_CB;   /**< Callback to stop track downloads */
    std::function<void(VTTCue*)> sendVTTCueData_CB; /**< Callback to send VTT cue data */
    std::function<void(long long&, double&)> getPlayerPositions_CB; /**< Callback to get player positions */
};

/**
* \class       SubtitleParser
* \brief       Subtitle parser class
*
* This is the base class for a subtitle parser impl in Player
*/
class SubtitleParser
{
public:

	SubtitleParser(SubtitleMimeType type, int width, int height) : mType(type),
				mHeight(height), mWidth(width)
	{
	}

	/// Copy Constructor
	SubtitleParser(const SubtitleParser&) = delete;

	virtual ~SubtitleParser()
	{
		UnRegisterCallback();
	}

	/// Assignment operator Overloading
	SubtitleParser& operator=(const SubtitleParser&) = delete;

	virtual bool init(double startPosSeconds, unsigned long long basePTS) { return false; }
	virtual bool processData(const char* buffer, size_t bufferLen, double position, double duration) = 0;
	virtual bool close() = 0;
	virtual void reset() = 0;
	virtual void setProgressEventOffset(double offset) = 0;
	virtual void updateTimestamp(unsigned long long positionMs) = 0;
	virtual void pause(bool pause) {}
	virtual void mute(bool mute) {}
	virtual void isLinear(bool isLinear) {}
	virtual void setTextStyle(const std::string &options){}
	void RegisterCallback(const PlayerCallbacks& playerCallBack)
	{
		playerResumeTrackDownloads_CB = playerCallBack.resumeTrackDownloads_CB;
		playerGetPositions_CB = playerCallBack.getPlayerPositions_CB;
		playerStopTrackDownloads_CB = playerCallBack.stopTrackDownloads_CB;
		playerSendVTTCueData_CB = playerCallBack.sendVTTCueData_CB;
	}
	void UnRegisterCallback( )
	{
		playerResumeTrackDownloads_CB = NULL;
		playerGetPositions_CB = NULL;
		playerStopTrackDownloads_CB = NULL;
		playerSendVTTCueData_CB = NULL;
	}
protected:

	SubtitleMimeType mType;
	int mHeight;
	int mWidth;
	std::function<void()> playerResumeTrackDownloads_CB;
	std::function<void()> playerStopTrackDownloads_CB;
	std::function<void(long long&, double&)> playerGetPositions_CB;
	std::function<void(VTTCue*)> playerSendVTTCueData_CB;
};

#endif /* __SUBTITLE_PARSER_H__ */
