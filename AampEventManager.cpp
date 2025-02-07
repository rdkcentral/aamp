/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file AampEventManager.cpp
 * @brief Event Manager operations for Aamp
 */

#include "AampEventManager.h"


//#define EVENT_DEBUGGING 1


/**
 * @brief GetSourceID - Get the idle task's source ID
 * @return Source Id
 */
guint AampEventManager::GetSourceID()
{
	guint callbackId = 0;
	GSource *source = g_main_current_source();
	if (source != NULL)
	{
		callbackId = g_source_get_id(source);
	}
	return callbackId;
}

/**
 * @brief Default Constructor
 */
AampEventManager::AampEventManager(int playerId): mIsFakeTune(false),
					mAsyncTuneEnabled(false),mEventPriority(G_PRIORITY_DEFAULT_IDLE),mMutexVar(),
					mPlayerState(eSTATE_IDLE),mEventWorkerDataQue(),mPendingAsyncEvents(),mPlayerId(playerId)
{
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		mEventListeners[i]	= NULL;
		mEventStats[i]		=	 0;
	}
}

/**
 * @brief Destructor Function
 */
AampEventManager::~AampEventManager()
{

	// Clear all event listeners and pending events
	FlushPendingEvents();
	std::lock_guard<std::mutex> guard(mMutexVar);
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		while (mEventListeners[i] != NULL)
		{
			ListenerData* pListener = mEventListeners[i];
			mEventListeners[i] = pListener->pNext;
			SAFE_DELETE(pListener);
		}
	}
}

/**
 * @brief FlushPendingEvents - Clear all pending events from EventManager
 */
void AampEventManager::FlushPendingEvents()
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	while(!mEventWorkerDataQue.empty())
	{
		// Remove each AampEventPtr from the queue , not deleting the Shard_ptr
		mEventWorkerDataQue.pop();
	}

	if (mPendingAsyncEvents.size() > 0)
	{
		AAMPLOG_WARN("mPendingAsyncEvents.size - %zu", mPendingAsyncEvents.size());
		for (AsyncEventListIter it = mPendingAsyncEvents.begin(); it != mPendingAsyncEvents.end(); it++)
		{
			if ((it->first != 0) && (it->second))
			{
				AAMPLOG_WARN("remove id - %d", (int) it->first);
				g_source_remove(it->first);

			}
		}
		mPendingAsyncEvents.clear();
	}

	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
		mEventStats[i] = 0;
#ifdef EVENT_DEBUGGING
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
	{
		AAMPLOG_WARN("EventType[%d]->[%d]",i,mEventStats[i]);
	}
#endif
}

/**
 * @brief AddListenerForAllEvents - Register one listener for all events
 */ 
void AampEventManager::AddListenerForAllEvents(EventListener* eventListener)
{
	if(eventListener != NULL)
	{
		AddEventListener(AAMP_EVENT_ALL_EVENTS,eventListener);
	}
	else
	{
		AAMPLOG_ERR("Null eventlistener. Failed to register");
	}
}

/**
 * @brief RemoveListenerForAllEvents - Remove listener for all events
 */
void AampEventManager::RemoveListenerForAllEvents(EventListener* eventListener)
{
	if(eventListener != NULL)
	{
		RemoveEventListener(AAMP_EVENT_ALL_EVENTS,eventListener);
	}
	else
	{
		AAMPLOG_ERR("Null eventlistener. Failed to deregister");
	}
}

/**
 * @brief AddEventListener - Register  listener for one eventtype
 */ 
void AampEventManager::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	if ((eventListener != NULL) && (eventType >= AAMP_EVENT_ALL_EVENTS) && (eventType < AAMP_MAX_NUM_EVENTS))
	{
		ListenerData* pListener = new ListenerData;
		if (pListener)
		{
			AAMPLOG_INFO("EventType:%d, Listener %p new %p", eventType, eventListener, pListener);
			std::lock_guard<std::mutex> guard(mMutexVar);
			pListener->eventListener = eventListener;
			pListener->pNext = mEventListeners[eventType];
			mEventListeners[eventType] = pListener;
		}
	}
	else
	{
		AAMPLOG_ERR("EventType %d registered out of range",eventType);
	}
}

/**
 * @brief RemoveEventListener - Remove one listener registration for one event
 */
void AampEventManager::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
	// listener instance is cleared here , but created outside
	if ((eventListener != NULL) && (eventType >= AAMP_EVENT_ALL_EVENTS) && (eventType < AAMP_MAX_NUM_EVENTS))
	{
		std::lock_guard<std::mutex> guard(mMutexVar);
		ListenerData** ppLast = &mEventListeners[eventType];
		while (*ppLast != NULL)
		{
			ListenerData* pListener = *ppLast;
			if (pListener->eventListener == eventListener)
			{
				*ppLast = pListener->pNext;
				AAMPLOG_INFO("Eventtype:%d %p delete %p", eventType, eventListener, pListener);
				SAFE_DELETE(pListener);
				return;
			}
			ppLast = &(pListener->pNext);
		}
	}
}

/**
 * @brief IsSpecificEventListenerAvailable - Check if this particular listener present for this event
 */ 
bool AampEventManager::IsSpecificEventListenerAvailable(AAMPEventType eventType)
{	
	bool retVal=false;
	std::lock_guard<std::mutex> guard(mMutexVar);
	if(eventType > AAMP_EVENT_ALL_EVENTS &&  eventType < AAMP_MAX_NUM_EVENTS && mEventListeners[eventType])
	{
		retVal = true;
	}
	return retVal;
}

/**
 * @brief IsEventListenerAvailable - Check if any listeners present for this event
 */ 
bool AampEventManager::IsEventListenerAvailable(AAMPEventType eventType)
{
	bool retVal=false;
	std::lock_guard<std::mutex> guard(mMutexVar);
	if(eventType >= AAMP_EVENT_TUNED &&  eventType < AAMP_MAX_NUM_EVENTS && (mEventListeners[AAMP_EVENT_ALL_EVENTS] || mEventListeners[eventType]))
	{
		retVal = true;
	}
	return retVal;
}

/**
 * @brief SetFakeTuneFlag - Some events are restricted for FakeTune
 */ 
void AampEventManager::SetFakeTuneFlag(bool isFakeTuneSetting)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	mIsFakeTune = isFakeTuneSetting;
}

/**
 * @brief SetAsyncTuneState - Flag for Async Tune
 */ 
void AampEventManager::SetAsyncTuneState(bool isAsyncTuneSetting)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	mAsyncTuneEnabled = isAsyncTuneSetting;
	if(mAsyncTuneEnabled)
	{
		mEventPriority = AAMP_MAX_EVENT_PRIORITY;
	}
	else
	{
		mEventPriority = G_PRIORITY_DEFAULT_IDLE;
	}
}

/**
 * @brief SetPlayerState - Flag to update player state
 */
void AampEventManager::SetPlayerState(AAMPPlayerState state)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	mPlayerState = state;
}

/**
 * @brief SendEvent - Generic function to send events
 */ 
void AampEventManager::SendEvent(const AAMPEventPtr &eventData, AAMPEventMode eventMode)
{
	// If some event wants to override  to send as Sync ,then override flag to be set
	// This will go as Sync only if SourceID of thread != 0 , else there is assert catch in SendEventSync
	AAMPEventType eventType = eventData->getType();
	if ((eventType < AAMP_EVENT_ALL_EVENTS) || (eventType >= AAMP_MAX_NUM_EVENTS))  //CID:81883 - Resolve OVER_RUN
                return;
	if(mIsFakeTune && !(AAMP_EVENT_STATE_CHANGED == eventType && eSTATE_COMPLETE == std::dynamic_pointer_cast<StateChangedEvent>(eventData)->getState()) && !(AAMP_EVENT_EOS == eventType))
	{
		AAMPLOG_TRACE("Events are disabled for fake tune");
		return;
	}

	if(eventMode < AAMP_EVENT_DEFAULT_MODE || eventMode > 	AAMP_EVENT_ASYNC_MODE)
		eventMode = AAMP_EVENT_DEFAULT_MODE;

	if((mPlayerState != eSTATE_RELEASED) && (mEventListeners[AAMP_EVENT_ALL_EVENTS] || mEventListeners[eventType]))
	{
		guint sId = GetSourceID();
		// if caller is asking for Sync Event , ensure it has proper source Id, else it has to go async event
		if(eventMode==AAMP_EVENT_SYNC_MODE &&  sId != 0)
		{
			SendEventSync(eventData);
		}
		else if(eventMode==AAMP_EVENT_ASYNC_MODE)
		{
			SendEventAsync(eventData);
		}
		else
		{
			//For other events if asyncTune enabled or callee from non-UI thread , then send the event as Async
			if (mAsyncTuneEnabled || sId == 0)
			{
				SendEventAsync(eventData);
			}
			else
			{
				SendEventSync(eventData);
			}
		}
	}
}


// Worker thread for handling Async Events
/**
 * @brief AsyncEvent - Task function for IdleEvent
 */ 
void AampEventManager::AsyncEvent()
{
	AAMPEventPtr eventData=NULL;
	{
		std::lock_guard<std::mutex> guard(mMutexVar);
		// pop out the event to sent in async mode
		if(mEventWorkerDataQue.size())
		{
			eventData = (AAMPEventPtr)mEventWorkerDataQue.front();
			mEventWorkerDataQue.pop();
		}
	}
	// Push the new event in sync mode from the idle task
	if(eventData && (IsEventListenerAvailable(eventData->getType())) && (mPlayerState != eSTATE_RELEASED))
	{
		SendEventSync(eventData);
	}
}

/**
 * @brief SendEventAsync - Function to send events Async
 */ 
void AampEventManager::SendEventAsync(const AAMPEventPtr &eventData)
{
	AAMPEventType eventType = eventData->getType();
	std::unique_lock<std::mutex> lock(mMutexVar);
	// Check if already player in release state , then no need to send any events
	if(mPlayerState != eSTATE_RELEASED)
	{
		AAMPLOG_INFO("Sending event %d to AsyncQ", eventType);
		mEventWorkerDataQue.push(eventData);
		lock.unlock();
		// Every event need a idle task to execute it
		guint callbackID = g_idle_add_full(mEventPriority, EventManagerThreadFunction, this, NULL);
		if(callbackID != 0)
		{
			SetCallbackAsPending(callbackID);
		}
	}
}


/**
 * @brief SendEventSync - Function to send events sync
 */ 
void AampEventManager::SendEventSync(const AAMPEventPtr &eventData)
{
	AAMPEventType eventType = eventData->getType();
	std::unique_lock<std::mutex> lock(mMutexVar);
#ifdef EVENT_DEBUGGING
	long long startTime = NOW_STEADY_TS_MS;
#endif
	// Check if already player in release state , then no need to send any events
	// Its checked again here ,as async events can come to sync mode after playback is stopped 
	if(mPlayerState == eSTATE_RELEASED)
	{
		return;
	}
	
	mEventStats[eventType]++;
	if (eventType != AAMP_EVENT_PROGRESS)
	{
		if (eventType != AAMP_EVENT_STATE_CHANGED)
		{
			AAMPLOG_INFO("(type=%d)(session_id=%s)", eventType, eventData->GetSessionId().c_str());
		}
		else
		{
			AAMPLOG_WARN("(type=%d)(state=%d)(session_id=%s)", eventType, std::dynamic_pointer_cast<StateChangedEvent>(eventData)->getState(),
				eventData->GetSessionId().c_str());
		}
	}

	// Build list of registered event listeners.
	ListenerData* pList = NULL;
	ListenerData* pListener = mEventListeners[eventType];
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
	}
	pListener = mEventListeners[AAMP_EVENT_ALL_EVENTS];  // listeners registered for "all" event types
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
	}
	lock.unlock();

	// After releasing the lock, dispatch each of the registered listeners.
	// This allows event handlers to add/remove listeners for future events.
	while (pList != NULL)
	{
		ListenerData* pCurrent = pList;
		if (pCurrent->eventListener != NULL)
		{
			pCurrent->eventListener->SendEvent(eventData);
		}
		pList = pCurrent->pNext;
		SAFE_DELETE(pCurrent);
	}
#ifdef EVENT_DEBUGGING
	AAMPLOG_WARN("TimeTaken for Event %d SyncEvent [%d]",eventType, (NOW_STEADY_TS_MS - startTime));
#endif

}

/**
 * @brief SetCallbackAsDispatched - Set callbackId as dispatched/done
 */
void AampEventManager::SetCallbackAsDispatched(guint id)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_TRACE("id:%d in mPendingAsyncEvents, erasing it. State:%d", id,itr->second);
		assert (itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		AAMPLOG_TRACE("id:%d not in mPendingAsyncEvents, insert and mark as not pending", id);
		mPendingAsyncEvents[id] = false;
	}
}

/**
 * @brief SetCallbackAsPending - Set callbackId as Pending/to be done
 */ 
void AampEventManager::SetCallbackAsPending(guint id)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_TRACE("id:%d already in mPendingAsyncEvents and completed, erase it State:%d",id,itr->second);
		assert (!itr->second);
		mPendingAsyncEvents.erase(itr);
	}
	else
	{
		mPendingAsyncEvents[id] = true;
		AAMPLOG_TRACE("id:%d in mPendingAsyncEvents, added to list", id);
	}
}
