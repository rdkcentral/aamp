/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#ifdef USE_WATERMARK_JSBINDINGS
#include "PersistentWatermarkEventHandler.h"
#include "jsutils.h"

std::string PersistentWatermark::EventHandler::getEventName(PersistentWatermark::EventHandler::eventType type)
{
	switch(type)
	{
		case eventType::ShowSuccess:
			return "ShowSuccess";
		case eventType::ShowFailed:
			return "ShowFailed";
		case eventType::HideSuccess:
			return "HideSuccess";
		case eventType::HideFailed:
			return "HideFailed";
		default:
			LOG_ERROR_EX("PersistentWatermark:unknown event");
			return "";
	}
}

PersistentWatermark::EventHandler::eventType PersistentWatermark::EventHandler::getEventType(std::string eventName)
{
	if(eventName=="ShowSuccess")
	{
		return eventType::ShowSuccess;
	}
	else if(eventName=="ShowFailed")
	{
		return eventType::ShowFailed;
	}
	else if(eventName=="HideSuccess")
	{
		return eventType::HideSuccess;
	}
	else if(eventName=="HideFailed")
	{
		return eventType::HideFailed;
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:unknown event '%s'.", eventName.c_str());
		return eventType::null;
	}
}

bool PersistentWatermark::EventHandler::isValidEvent(PersistentWatermark::EventHandler::eventType event)
{
	return static_cast<unsigned int>(event) < static_cast<unsigned int>(PersistentWatermark::EventHandler::eventType::size);
}


PersistentWatermark::EventHandler::EventHandler():mMutex(), mCallbacks()
{
	//empty
}

void PersistentWatermark::EventHandler::Send(PersistentWatermark::EventHandler::eventType event, std::string msg)
{
	if(!isValidEvent(event))
	{
		LOG_ERROR_EX("PersistentWatermark:Invalid event supplied. %s", msg.c_str());
	}
	else
	{
		auto nameOfEvent = getEventName(event).c_str();
		std::lock_guard<std::mutex>lock(mMutex);
		auto eventCallbacks = mCallbacks[static_cast<int>(event)];
		LOG_WARN_EX("PersistentWatermark:Calling %d Callbacks for %s event. %s", eventCallbacks.size(), nameOfEvent, msg.c_str());
		for(auto callbackData: eventCallbacks)
		{
			JSObjectCallAsFunction(callbackData.context, callbackData.callback, nullptr, 0, nullptr, nullptr);
		}
	}
}

bool PersistentWatermark::EventHandler::addEventHandler(std::string eventName, JSObjectRef callback, JSContextRef context)
{
	eventType event = getEventType(eventName);
	if(isValidEvent(event))
	{
		std::lock_guard<std::mutex>lock(mMutex);
		LOG_WARN_EX("PersistentWatermark:Adding callback to %s event", eventName.c_str());
		mCallbacks[static_cast<int>(event)].push_back(callbackData{callback, context});
		return true;
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:Invalid event '%s'", eventName.c_str());
		return false;
	}
}

int PersistentWatermark::EventHandler::RemoveEventHandler(std::string eventName)
{
	LOG_TRACE("Enter %s", eventName.c_str());
	eventType event = getEventType(eventName);
	if(isValidEvent(event))
	{
		std::lock_guard<std::mutex>lock(mMutex);
		int removed = mCallbacks[static_cast<int>(event)].size();
		mCallbacks[static_cast<int>(event)].clear();
		LOG_TRACE("removed %d", removed);
		return removed;
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:Invalid event '%s'", eventName.c_str());
		return 0;
	}
}
#endif