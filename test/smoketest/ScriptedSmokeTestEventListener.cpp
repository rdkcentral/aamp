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

	// Extract some meta data for furure use *duration etc.)
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

		// Update any events that we have configured for monitoring
		for ( auto &eventItem : eventList )
		{
			if ((eventItem.event == AAMP_EVENT_TUNED) &&
				(e->getType() == AAMP_EVENT_TUNE_FAILED))
			{
				// Special case - if we are waiting for 'tuned' and get 'tune failed'
				// then just fail the check
				failed = true;
			}
			else if (eventItem.event == e->getType())
			{
				switch (eventItem.event)
				{
					//
					// Events with, potentially, some extra checks 
					//
					case AAMP_EVENT_PROGRESS:
						// We can get multiple progress events so if we have already got the one we want don't check again
						if (!eventItem.received)
						{
							eventItem.received = true;

							ProgressEventPtr ev = std::dynamic_pointer_cast<ProgressEvent>(e);
							if (eventItem.data.progress.speed > INVALID_VALUE)
							{
								eventItem.actual.progress.speed = ev->getSpeed();
								if (eventItem.data.progress.speed != eventItem.actual.progress.speed)
								{
									eventItem.received = false;
								}
							}
							if (eventItem.data.progress.position > INVALID_VALUE)
							{
								eventItem.actual.progress.position = ev->getPosition();
								double diff = std::abs(eventItem.actual.progress.position - eventItem.data.progress.position);
								if (diff > eventItem.data.progress.accuracy)
								{
									eventItem.received = false;
								}
							}
						}
						break;
					case AAMP_EVENT_SEEKED:
						eventItem.received = true;
						if (eventItem.data.seek.position > INVALID_VALUE)
						{
							SeekedEventPtr ev = std::dynamic_pointer_cast<SeekedEvent>(e);
							eventItem.actual.seek.position = ev->getPosition();
							double diff = std::abs(eventItem.actual.seek.position - eventItem.data.seek.position);
							if (diff > eventItem.data.seek.accuracy)
							{
								eventItem.received = false;
							}
						}
						break;
					case AAMP_EVENT_SPEED_CHANGED:
						eventItem.received = true;
						if (eventItem.data.progress.speed > INVALID_VALUE)
						{
							SpeedChangedEventPtr ev = std::dynamic_pointer_cast<SpeedChangedEvent>(e);
							eventItem.actual.progress.speed = ev->getRate();
							if (eventItem.data.progress.speed != eventItem.actual.progress.speed)
							{
								eventItem.received = false;
							}
						}
						break;

					//
					// For state change vents, check this is the state change we are waiting for
					//
					case AAMP_EVENT_STATE_CHANGED:
						{
							StateChangedEventPtr ev = std::dynamic_pointer_cast<StateChangedEvent>(e);
							if (eventItem.state == ev->getState())
							{
								eventItem.received = true;
							}
						}
						break;

					//
					// For most events, if we got the event then we are done
					//
					default:
						eventItem.received = true;
						break;

				}
			}

			if (!eventItem.received)
			{
				allEventsReceived = false;
			}
			else if (eventItem.negative)
			{
				failed = true; // we didn't want this one!
			}
		}
	}

	return allEventsReceived;
}

/**
 * @brief Helper function to extract the event position arguments from a stream
 * @param[in] argStream - string stream of argumants
 * @param[out] position - the extracted position
 * @param[out] accuracy - the extracted accuracy
 * @retval - true if a position value was extracted
 */
bool ScriptedSmokeTestEventListener::extractPositionArgs(std::stringstream &argStream, double &position, double &accuracy)
{
	bool retval = false;
	std::string positionArg;
	std::string accuracyArg;

	// Extract the expected position - in the form 'position' or 'position(accuracy)'
	if (ScriptedSmokeTest::getValueParameter(argStream, positionArg, accuracyArg, false, false))
	{
		if (!positionArg.empty())
		{
			retval = ScriptedSmokeTest::getInteger(positionArg, position);
			if (!accuracyArg.empty())
			{
				retval = ScriptedSmokeTest::getInteger(accuracyArg, accuracy);
			}
		}
	}
	else
	{
		printf("%s:%d: Unable to get position from stream\n", __FUNCTION__,__LINE__);
	}
	return retval;
}

/**
 * @brief Helper function to extract the event speed argument from a stream
 * @param[in] argStream - string stream of argumants
 * @param[out] speed - the extracted speed
 * @retval - true if a speed value was extracted
 */
bool ScriptedSmokeTestEventListener::extractSpeedArgs(std::stringstream &argStream, double &speed)
{
	bool retval = false;
	std::string speedArg;

	// Extract the expected speed
	if (ScriptedSmokeTest::getValueParameter(argStream, speedArg, false))
	{
		if (!speedArg.empty())
		{
			retval = ScriptedSmokeTest::getInteger(speedArg, speed);
		}
	}
	else
	{
		printf("%s:%d: Unable to get speed from stream\n", __FUNCTION__,__LINE__);
	}
	return retval;
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
	while (ScriptedSmokeTest::getValueParameter(argStream, event, eventArgs, true, false))
	{
		bool negate = defaultNegate;
		if (event[0] == '!')
		{
			negate = !defaultNegate;
			event.erase(0, 1);
		}

		//
		// Events with, potentially, some extra checks 
		//
		if (event == "PROGRESS")
		{
			monitoredEventStatus status = monitoredEventStatus(AAMP_EVENT_PROGRESS, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				// We expect a comma separated list of progress arguments
				std::stringstream progressArgStream(eventArgs);

				status.name += "(";
				if (extractSpeedArgs(progressArgStream, status.data.progress.speed))
				{
					status.name += "speed ";
					status.name += std::to_string(status.data.progress.speed);
				}
				status.name += ",";
				if (extractPositionArgs(progressArgStream, status.data.progress.position, status.data.progress.accuracy))
				{
					status.name += " position ";
					status.name += std::to_string(status.data.progress.position);
					status.name += "+-";
					status.name += std::to_string(status.data.progress.accuracy);
				}
				status.name += ")";
			}
			eventList.push_back(status);
		}
		else if (event == "SEEKED")
		{
			monitoredEventStatus status = monitoredEventStatus(AAMP_EVENT_SEEKED, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				status.name += "(";
				std::stringstream seekedArgStream(eventArgs);
				if (extractPositionArgs(seekedArgStream, status.data.seek.position, status.data.seek.accuracy))
				{
					status.name += " position ";
					status.name += std::to_string(status.data.seek.position);
					status.name += "+-";
					status.name += std::to_string(status.data.seek.accuracy);
				}
				status.name += ")";
			}
			eventList.push_back(status);
		}
		else if (event == "SPEED_CHANGED")
		{
			monitoredEventStatus status = monitoredEventStatus(AAMP_EVENT_SPEED_CHANGED, event.c_str(), negate);
			if (!eventArgs.empty())
			{
				status.name += "(";
				std::stringstream speedArgStream(eventArgs);
				if (extractSpeedArgs(speedArgStream, status.data.speed.speed))
				{
					status.name += "speed ";
					status.name += std::to_string(status.data.speed.speed);
				}
				status.name += ")";
			}
			eventList.push_back(status);
		}
		else if (event == "AUDIO_TRACKS_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AUDIO_TRACKS_CHANGED, event.c_str(), negate));
		}
		else if (event == "TEXT_TRACKS_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_TEXT_TRACKS_CHANGED, event.c_str(), negate));
		}
		else if (event == "TUNED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_TUNED, event.c_str(), negate));
		}
		else if (event == "EOS")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_EOS, event.c_str(), negate));
		}
		else if (event == "TUNE_FAILED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_TUNE_FAILED, event.c_str(), negate));
		}
		else if (event == "BLOCKED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_BLOCKED, event.c_str(), negate));
		}
		else if (event == "PLAYLIST_INDEXED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_PLAYLIST_INDEXED, event.c_str(), negate));
		}
		else if (event == "CC_HANDLE_RECEIVED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_CC_HANDLE_RECEIVED, event.c_str(), negate));
		}
		else if (event == "JS_EVENT")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_JS_EVENT, event.c_str(), negate));
		}
		else if (event == "MEDIA_METADATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_MEDIA_METADATA, event.c_str(), negate));
		}
		else if (event == "ENTERING_LIVE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_ENTERING_LIVE, event.c_str(), negate));
		}
		else if (event == "BITRATE_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_BITRATE_CHANGED, event.c_str(), negate));
		}
		else if (event == "TIMED_METADATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_TIMED_METADATA, event.c_str(), negate));
		}
		else if (event == "BULK_TIMED_METADATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_BULK_TIMED_METADATA, event.c_str(), negate));
		}
		else if (event == "SPEEDS_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_SPEEDS_CHANGED, event.c_str(), negate));
		}
		else if (event == "TUNE_PROFILING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_TUNE_PROFILING, event.c_str(), negate));
		}
		else if (event == "BUFFERING_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_BUFFERING_CHANGED, event.c_str(), negate));
		}
		else if (event == "DURATION_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_DURATION_CHANGED, event.c_str(), negate));
		}
		else if (event == "AD_BREAKS_CHANGED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_BREAKS_CHANGED, event.c_str(), negate));
		}
		else if (event == "AD_STARTED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_STARTED, event.c_str(), negate));
		}
		else if (event == "AD_COMPLETED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_COMPLETED, event.c_str(), negate));
		}
		else if (event == "DRM_METADATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_DRM_METADATA, event.c_str(), negate));
		}
		else if (event == "REPORT_ANOMALY")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_REPORT_ANOMALY, event.c_str(), negate));
		}
		else if (event == "WEBVTT_CUE_DATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_WEBVTT_CUE_DATA, event.c_str(), negate));
		}
		else if (event == "AD_RESOLVED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_RESOLVED, event.c_str(), negate));
		}
		else if (event == "AD_RESERVATION_START")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_RESERVATION_START, event.c_str(), negate));
		}
		else if (event == "AD_RESERVATION_END")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_RESERVATION_END, event.c_str(), negate));
		}
		else if (event == "AD_PLACEMENT_START")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_PLACEMENT_START, event.c_str(), negate));
		}
		else if (event == "AD_PLACEMENT_END")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_PLACEMENT_END, event.c_str(), negate));
		}
		else if (event == "AD_PLACEMENT_ERROR")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_PLACEMENT_ERROR, event.c_str(), negate));
		}
		else if (event == "AD_PLACEMENT_PROGRESS")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_AD_PLACEMENT_PROGRESS, event.c_str(), negate));
		}
		else if (event == "REPORT_METRICS_DATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_REPORT_METRICS_DATA, event.c_str(), negate));
		}
		else if (event == "ID3_METADATA")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_ID3_METADATA, event.c_str(), negate));
		}
		else if (event == "DRM_MESSAGE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_DRM_MESSAGE, event.c_str(), negate));
		}
		else if (event == "CONTENT_GAP")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_CONTENT_GAP, event.c_str(), negate));
		}
		else if (event == "HTTP_RESPONSE_HEADER")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_HTTP_RESPONSE_HEADER, event.c_str(), negate));
		}
		else if (event == "WATERMARK_SESSION_UPDATE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_WATERMARK_SESSION_UPDATE, event.c_str(), negate));
		}
		else if (event == "CONTENT_PROTECTION_DATA_UPDATE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE, event.c_str(), negate));
		}

		//
		// State change events
		//
		else if (event == "IDLE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_IDLE, event.c_str(), negate));
		}
		else if (event == "PLAYING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_PLAYING, event.c_str(), negate));
		}
		else if (event == "PAUSED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_PAUSED, event.c_str(), negate));
		}
		else if (event == "STOPPED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_STOPPED, event.c_str(), negate));
		}
		else if (event == "ERROR")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_ERROR, event.c_str(), negate));
		}
		else if (event == "INITIALIZING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_INITIALIZING, event.c_str(), negate));
		}
		else if (event == "INITIALIZED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_INITIALIZED, event.c_str(), negate));
		}
		else if (event == "PREPARING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_PREPARING, event.c_str(), negate));
		}
		else if (event == "PREPARED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_PREPARED, event.c_str(), negate));
		}
		else if (event == "BUFFERING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_BUFFERING, event.c_str(), negate));
		}
		else if (event == "SEEKING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_SEEKING, event.c_str(), negate));
		}
		else if (event == "STOPPING")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_STOPPING, event.c_str(), negate));
		}
		else if (event == "COMPLETE")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_COMPLETE, event.c_str(), negate));
		}
		else if (event == "RELEASED")
		{
			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_RELEASED, event.c_str(), negate));
		}
//		else if (event == "BLOCKED")
//		{
//			eventList.push_back(monitoredEventStatus(AAMP_EVENT_STATE_CHANGED, eSTATE_BLOCKED, event.c_str(), negate));
//		}

		// unrecognised
		else
		{
			printf("%s:%d: ERROR - unrecognised event '%s'\n",__FUNCTION__,__LINE__, event.c_str());
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
		if (event.received == event.negative) // wanted event not received, or not wanted event received
		{
			status += "NOT OK";
			if (event.event == AAMP_EVENT_PROGRESS)
			{
				status += ", speed ";
				status += std::to_string(event.actual.progress.speed);
				status += ", position ";
				status += std::to_string(event.actual.progress.position);
			}
			retval = false;
		}
		else
		{
			status += "OK";
		}
		status += ") ";
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
