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

		long GetDuration()
		{
			return duration;
		}

	private:
		class monitoredEventStatus
		{
		public:
			#define INVALID_VALUE 				-1000 // invalid position/speed value
			#define DEFAULT_POSITION_ACCURACY 	 1000 // Default position accuracy of +-1s

			monitoredEventStatus(AAMPEventType waitfor = AAMP_MAX_NUM_EVENTS, PrivAAMPState newState = eSTATE_IDLE, 
			                     const char *eventName = "", bool negate = false) : 
			    event(waitfor), 
				state(newState),
				name(eventName), 
				received(false),
				negative(negate),
				data()
			{
				if (event == AAMP_EVENT_PROGRESS)
				{
					data.progress.speed = INVALID_VALUE;
					data.progress.position = INVALID_VALUE;
					data.progress.accuracy = DEFAULT_POSITION_ACCURACY;
				}
				else if (event == AAMP_EVENT_SEEKED)
				{
					data.seek.position = INVALID_VALUE;
					data.seek.accuracy = DEFAULT_POSITION_ACCURACY;
				}
				else if (event == AAMP_EVENT_SPEED_CHANGED)
				{
					data.speed.speed = INVALID_VALUE;
				}
			}

			monitoredEventStatus(AAMPEventType waitfor = AAMP_MAX_NUM_EVENTS, const char *eventName = "", bool negate = false) : 
				monitoredEventStatus(waitfor, eSTATE_IDLE, eventName, negate)
			{
			}

			AAMPEventType event;
			PrivAAMPState state;
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
					double position;
					double accuracy;
				} seek;
				struct
				{ 
					double speed;
				} speed;
			} data;

			eventData actual;
		};

		bool CheckEventList(const AAMPEventPtr& e, std::vector<monitoredEventStatus> &eventList, bool &failed);
		bool createEventList(std::stringstream &argStream, std::vector<monitoredEventStatus> &eventList, bool defaultNegate = false);
		bool checkEventList(std::vector<monitoredEventStatus> &eventList, std::string &status);

		bool extractPositionArgs(std::stringstream &argStream, double &position, double &accuracy);
		bool extractSpeedArgs(std::stringstream &argStream, double &speed);

		std::mutex protectMonitoredEvents;
		std::condition_variable eventCondition;
		std::vector<monitoredEventStatus> monitoredEvents;
		std::vector<monitoredEventStatus> failEvents;
		bool waitingForEvents;

		PlayerInstanceAAMP *aampPlayer;
		long duration;

};

#endif // SCRIPTEDSMOKETESTEVENTLISTENER_H
