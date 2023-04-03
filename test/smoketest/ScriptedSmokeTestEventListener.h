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

/**
 * @file ScriptedSmokeTestEventListener.h
 * @brief ScriptedSmokeTestEventListener header file
 */

#ifndef SCRIPTEDSMOKETESTEVENTLISTENER_H
#define SCRIPTEDSMOKETESTEVENTLISTENER_H

#include <gst/gst.h>
#include <main_aamp.h>
#include "priv_aamp.h"
#include <vector>
#include "SmokeTestEventListener.h"


class ScriptedSmokeTestEventListener : public SmokeTestEventListener
{
	public:
		enum class WaitEvent
		{
			NONE,
			TUNED, 			// implies !TUNE_FAILED
			TUNE_FAILED,
			PLAYING,
			PAUSED,
			STOPPED,
			ERROR,
			BLOCKED,
			COMPLETE,
			EOS,
			PROGRESS,
			AUDIO_TRACKS_CHANGED,
			TEXT_TRACKS_CHANGED
		};

		ScriptedSmokeTestEventListener(PlayerInstanceAAMP *player = NULL) :
			protectMonitoredEvents(), 
			eventCondition(), 
			monitoredEvents(), 
			failEvents(),
			waitingForEvents(false),
			aampPlayer(player),
			duration(0)
		{
		}

		void Event(const AAMPEventPtr& e);

		bool WaitForEvent(std::stringstream &args, std::string &status);
		bool SetFailEvents(std::stringstream &argStream);
		bool CheckFailEvents(std::string &status);

		int GetCurrentAudioTrack()
		{
			if (aampPlayer)
			{
				return aampPlayer->GetAudioTrack();
			}
			return -3;
		}

		int GetCurrentTextTrack()
		{
			if (aampPlayer)
			{
				return aampPlayer->GetTextTrack();
			}
			return -1;
		}

		long GetDuration()
		{
			return duration;
		}

	private:
		class monitoredEventStatus
		{
		public:
			#define INVALID_POSITION -1000 // invalid position value (s)
			#define INVALID_SPEED -1000    // invalid speed valueS

			monitoredEventStatus(WaitEvent waitfor = WaitEvent::NONE, const char *eventName = "", bool negate = false) : 
			    event(waitfor), 
				name(eventName), 
				received(false),
				negative(negate),
				data()
			{
				if (event == WaitEvent::PROGRESS)
				{
					data.progress.speed = INVALID_SPEED;
					data.progress.position = INVALID_POSITION;
					data.progress.accuracy = 1000; // default to 1s accuracy (on position)?
				}
				else if (event == WaitEvent::AUDIO_TRACKS_CHANGED)
				{
					data.audio.track = -1;
				}
				else if (event == WaitEvent::TEXT_TRACKS_CHANGED)
				{
					data.audio.track = -1;
				}
			}

			WaitEvent event;
			std::string name;
			bool received;
			bool negative;

			union eventData
			{
				eventData() {}

				struct
				{ 
					double speed;
					double position;
					double accuracy;
				} progress;
				struct
				{ 
					int track;
				} audio;
				struct
				{ 
					int track;
				} text;
			} data;
		};

		bool CheckEventList(const AAMPEventPtr& e, std::vector<monitoredEventStatus> &eventList, bool &failed);
		bool createEventList(std::stringstream &argStream, std::vector<monitoredEventStatus> &eventList, bool defaultNegate = false);
		bool checkEventList(std::vector<monitoredEventStatus> &eventList, std::string &status);

		std::mutex protectMonitoredEvents;
		std::condition_variable eventCondition;
		std::vector<monitoredEventStatus> monitoredEvents;
		std::vector<monitoredEventStatus> failEvents;
		bool waitingForEvents;

		PlayerInstanceAAMP *aampPlayer;
		long duration;

};

#endif // SCRIPTEDSMOKETESTEVENTLISTENER_H
