/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
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
 * @file AampSmokeTestPlayer.cpp
 * @brief Aamp SmokeTest standalone Player 
 */

#include "AampSmokeTestPlayer.h"
#include "ScriptedSmokeTest.h"


/**
 * @brief Implementation of event callback
 * @param e Event
 */
void ScriptedSmokeTestEventListener::Event(const AAMPEventPtr& e)
{
    SmokeTestEventListener::Event(e);

	std::unique_lock<std::mutex> lock(protectMonitoredEvents);
	bool waitFailed = false;

	if (e->getType() == AAMP_EVENT_MEDIA_METADATA)
	{
		MediaMetadataEventPtr ev = std::dynamic_pointer_cast<MediaMetadataEvent>(e);
		duration = ev->getDuration();
		//... todo add more info?
	}

	if ( waitingForEvents)
	{
		bool allEventsReceived = CheckEventList(e, monitoredEvents, waitFailed);
		if (allEventsReceived || waitFailed )
		{
			// If all the events have passed (or recognised as failed) then unblock the script
			eventCondition.notify_all();
		}
	}

	// Update any fail events
	CheckEventList(e, failEvents, waitFailed);
}

/**
 * @brief Update given event list with received event info
 * @param[in] e - the received AAMPEventPtr
 * @param[in, out] eventList - the event list to update
 * @param[out] failed - true if the received event triggers failure
 * @retval - true if all events in the list have been received
 */
bool ScriptedSmokeTestEventListener::CheckEventList(const AAMPEventPtr& e, std::vector<monitoredEventStatus> &eventList, bool &failed)
{
	bool allEventsReceived = true;

	if (eventList.size())
	{
		failed = false;

		// Update any events that we have confured for monitoring
		for ( auto &state : eventList )
		{
			switch (state.event)
			{
				case WaitEvent::TUNED:
					if (e->getType() == AAMP_EVENT_TUNED)
					{
						state.received = true;
					}
					else if (e->getType() == AAMP_EVENT_TUNE_FAILED)
					{
						failed = true;
					}
					break;
				case WaitEvent::TUNE_FAILED:
					if (e->getType() == AAMP_EVENT_TUNE_FAILED)
					{
						state.received = true;
					}
					break;
				case WaitEvent::PLAYING:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_PLAYING)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::STOPPED:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_IDLE)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::PAUSED:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_PAUSED)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::ERROR:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_ERROR)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::BLOCKED:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_BLOCKED)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::COMPLETE:
					if (e->getType() == AAMP_EVENT_STATE_CHANGED)
					{
						StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
						if (ev->getState() == PrivAAMPState::eSTATE_COMPLETE)
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::EOS:
					if (e->getType() == AAMP_EVENT_EOS)
					{
						state.received = true;
					}
					break;
				case WaitEvent::PROGRESS:
					if (e->getType() == AAMP_EVENT_PROGRESS)
					{
						// We can get multiple progress events so if we have already got the one we want don't check again
						if (!state.received)
						{
							state.received = true;

							ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
							if (state.data.progress.speed > INVALID_SPEED)
							{
								if (state.data.progress.speed != ev->getSpeed())
								{
									state.received = false;
								}
							}
							if (state.data.progress.position > INVALID_POSITION)
							{
								double diff = std::abs(ev->getPosition() - state.data.progress.position);
								if (diff > state.data.progress.accuracy)
								{
									state.received = false;
								}
							}
						}
					}
					break;
				case WaitEvent::AUDIO_TRACKS_CHANGED:
					if (e->getType() == AAMP_EVENT_AUDIO_TRACKS_CHANGED)
					{
						if (state.data.audio.track != -1)
						{
							state.received = (state.data.audio.track == GetCurrentAudioTrack());
						}
						else
						{
							state.received = true;
						}
					}
					break;
				case WaitEvent::TEXT_TRACKS_CHANGED:
					if (e->getType() == AAMP_EVENT_TEXT_TRACKS_CHANGED)
					{
						if (state.data.text.track != -1)
						{
							state.received = (state.data.text.track == GetCurrentTextTrack());
						}
						else
						{
							state.received = true;
						}
					}
					break;
			}

			if (!state.received)
			{
				allEventsReceived = false;
			}
			else if (state.negative)
			{
				failed = true; // we didn't want this one!
			}
		}
	}

	return allEventsReceived;
}

/**
 * @brief Create a list of monitored events from text input
 * @param[in] argStream - string stream event list
 * @param[in, out] eventList - the created event list
 * @param[in] defaultNegate - true if the events in the list are to be checked for NOT received
 * @retval - true if event list was reated successfully
 */
bool ScriptedSmokeTestEventListener::createEventList(std::stringstream &argStream, std::vector<monitoredEventStatus> &eventList, bool defaultNegate)
{
	bool retval = true;
	eventList.clear();

	std::string event;
	std::string eventArgs;
	while (ScriptedSmokeTest::getValueParameter(argStream, event, eventArgs, false))
	{
		bool negate = defaultNegate;
		if (event[0] == '!')
		{
			negate = !defaultNegate;
			event.erase(0, 1);
		}

		if (event == "TUNED")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::TUNED, event.c_str(), negate));
		}
		else if (event == "PLAYING")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::PLAYING, event.c_str(), negate));
		}
		else if (event == "PAUSED")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::PAUSED, event.c_str(), negate));
		}
		else if (event == "STOPPED")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::STOPPED, event.c_str(), negate));
		}
		else if (event == "EOS")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::EOS, event.c_str(), negate));
		}
		else if (event == "TUNE_FAILED")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::TUNE_FAILED, event.c_str(), negate));
		}
		else if (event == "ERROR")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::ERROR, event.c_str(), negate));
		}
		else if (event == "BLOCKED")
		{
			eventList.push_back(monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::BLOCKED, event.c_str(), negate));
		}
		else if (event == "AUDIO_TRACKS_CHANGED")
		{
			monitoredEventStatus status = monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::AUDIO_TRACKS_CHANGED, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				retval = ScriptedSmokeTest::getInteger(eventArgs, status.data.audio.track);
			}
			eventList.push_back(status);
		}
		else if (event == "TEXT_TRACKS_CHANGED")
		{
			monitoredEventStatus status = monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::TEXT_TRACKS_CHANGED, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				retval = ScriptedSmokeTest::getInteger(eventArgs, status.data.text.track);
			}
			eventList.push_back(status);
		}
		else if (event == "PROGRESS")
		{
			monitoredEventStatus status = monitoredEventStatus(ScriptedSmokeTestEventListener::WaitEvent::PROGRESS, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				// We expect a comma separated list of progress arguments
				// first of all remove any spaces that have been put in
				auto start = std::remove(eventArgs.begin(), eventArgs.end(), ' ');
				eventArgs.erase(start, eventArgs.end());
				std::stringstream positionStream(eventArgs);

				// Extract the expected speed
				{
					std::string speed;
					if (std::getline(positionStream, speed, ','))
					{
						if (!speed.empty())
						{
							status.name += " speed ";
							status.name += speed;
							retval = ScriptedSmokeTest::getInteger(speed, status.data.progress.speed);
						}
					}
					else
					{
						printf("%s:%d: Unable to get speed from '%s'\n", __FUNCTION__,__LINE__, eventArgs.c_str());
						retval = false;
					}
				}

				// Extract the expected position - in the form 'position' or 'position(accuracy)'
				{
					std::string position;
					std::string accuracy;
					if (ScriptedSmokeTest::getValueParameter(positionStream, position, accuracy, false, ','))
					{
						if (!position.empty())
						{
							status.name += " position ";
							status.name += position;
							retval = ScriptedSmokeTest::getInteger(position, status.data.progress.position);

							if (!accuracy.empty())
							{
								status.name += "+-";
								status.name += accuracy;
								retval = ScriptedSmokeTest::getInteger(accuracy, status.data.progress.accuracy);
							}
						}
					}
					else
					{
						printf("%s:%d: Unable to get position from '%s'\n", __FUNCTION__,__LINE__, eventArgs.c_str());
						retval = false;
					}
				}
			}
			eventList.push_back(status);
			break; // Don't allow any more events after PROGRESS
		}
		else
		{
			retval = false;
		}
	}

	return retval;
}

/**
 * @brief Check the status of the events in the given event list
 * @param[in] eventList - the list of events to check
 * @param[in, out] status - a text description of the event list status
 * @retval - true if either all wanted events have been received, or any NOT wanted events have been received
 */
bool ScriptedSmokeTestEventListener::checkEventList(std::vector<monitoredEventStatus> &eventList, std::string &status)
{
	bool retval = true;

	status = "check for ";
	for ( auto event : eventList )
	{
		if (event.negative)
		{
			status += "!";	
		}
		status += event.name;
		status += "(";
		status += (event.received != event.negative)?"OK":"NOT OK";
		status += ") ";

		if (event.received == event.negative) // wanted event not received, or not wanted event received
		{
			retval = false;
		}
	}

	return retval;
}

/**
 * @brief Process 'waitfor' command - wait for specified events to be received
 * @param[in] argStream - input command containing parameters
 * @param[in, out] status - string description of result
 * @retval true if specified events were received
 */
bool ScriptedSmokeTestEventListener::WaitForEvent(std::stringstream &argStream, std::string &status)
{
// This is a bit convoluted but 'wait for events' can be used as:
//     waitfor [timeout (!= 0)] [... list of events in any order ...]
// Block and wait for the events to arrive
//
//     waitfor 0 [... list of events in any order ...]
// Configure the events to look for but do not block
//
//     waitfor [timeout]
// Check to see if previously configured events arrived (0 == check now, non-zero will block and wait)
//
	std::unique_lock<std::mutex> lock(protectMonitoredEvents);
	uint32_t timeout = 0;
	auto sec = std::chrono::seconds(1);
	bool retval = true;

	// First param is a timeout value
	if (!ScriptedSmokeTest::getUintParameter(argStream, timeout))
	{
		printf("%s:%d: Failed to get timeout\n",__FUNCTION__,__LINE__);
		return false;
	}
	// If we are not already monitoring for some events then configure the events
	// that are passed here
	else if (!waitingForEvents)
	{
		if (!createEventList(argStream, monitoredEvents))
		{
			printf("%s:%d: ERROR - failed to get ctreate event list\n",__FUNCTION__,__LINE__);
			return false;
		}
	}

	// At this point we should have some events configured to look for
	if ( !monitoredEvents.size())
	{
		printf("%s:%d: ERROR - no events in monitored list\n",__FUNCTION__,__LINE__);
		return false;
	}

	if (timeout > 0)
	{
		// If the timeout is not zero then block till the events are received or we timeout
		waitingForEvents = true;
		eventCondition.wait_for(lock, timeout * sec);
		waitingForEvents = false;
	}
	else if (waitingForEvents)
	{
		// If the timeout is zero but we are waiting for events then we must have configured some events 
		// to look out for previously so check the status of the events we are look for
		waitingForEvents = false;
	}
	else
	{
		// If the timeout is zero and we are not waiting for events then we have just configured some events
		// to monitor for so don't check them now
		waitingForEvents = true;
	}

	if (!waitingForEvents)
	{
		// Monitoring is done so check the received stratus of any events that have been configured
		retval = checkEventList(monitoredEvents, status);

		status += retval?"SUCCEEDED":"FAILED";
		printf("%s:%d: %s\n",__FUNCTION__,__LINE__, status.c_str());
	}

	return retval;
}

/**
 * @brief Create a list of events to fail the test on
 * @param[in] argStream - input command containing parameters
 * @retval true if the l ist was reated successfully
 */
bool ScriptedSmokeTestEventListener::SetFailEvents(std::stringstream &argStream)
{
	std::unique_lock<std::mutex> lock(protectMonitoredEvents);
	// These are 'negative' events (we do NOT want to recieve them and wil fail if we do)
	return createEventList(argStream, failEvents, true);
}

/**
 * @brief Check the status of the fail event list
 * @param[in, out] status - string description of result
 * @retval true if fail events have NOT been recieved
 */
bool ScriptedSmokeTestEventListener::CheckFailEvents(std::string &status)
{
	bool checksOk = checkEventList(failEvents, status);
	if (!checksOk)
	{
		status += "FAILED";
	}
	return checksOk;
}
