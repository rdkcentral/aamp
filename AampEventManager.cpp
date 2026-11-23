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
					mPlayerState(eSTATE_IDLE),mEventWorkerDataQue(),mPendingAsyncEvents(),mPlayerId(playerId),
					mEventProfilingEnabled(false),mEventProfilingThresholdMs(0),mIsFirstProgressSent(false)
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
	
	AAMPLOG_WARN("vk::FlushPendingEvents - START, queueSize=%zu, pendingCallbacks=%zu", 
		mEventWorkerDataQue.size(), mPendingAsyncEvents.size());
	
	int queuedEvents = 0;
	while(!mEventWorkerDataQue.empty())
	{
		// Remove each AampEventPtr from the queue, not deleting the shared_ptr
		mEventWorkerDataQue.pop();
		queuedEvents++;
	}
	
	if (queuedEvents > 0)
	{
		AAMPLOG_WARN("vk::FlushPendingEvents - CLEARED %d queued events from mEventWorkerDataQue", queuedEvents);
	}

	if (mPendingAsyncEvents.size() > 0)
	{
		AAMPLOG_WARN("vk::FlushPendingEvents - CLEARING %zu pending GLib callbacks", mPendingAsyncEvents.size());
		AAMPLOG_WARN("mPendingAsyncEvents.size - %zu", mPendingAsyncEvents.size());
		
		int callbacksRemoved = 0;
		for (AsyncEventListIter it = mPendingAsyncEvents.begin(); it != mPendingAsyncEvents.end(); it++)
		{
			if ((it->first != 0) && (it->second))
			{
				AAMPLOG_WARN("vk::FlushPendingEvents - Removing GLib callback id=%u (pending=%d)", it->first, it->second);
				AAMPLOG_WARN("remove id - %d", (int) it->first);
				g_source_remove(it->first);
				callbacksRemoved++;
			}
		}
		AAMPLOG_WARN("vk::FlushPendingEvents - Removed %d pending GLib callbacks via g_source_remove", callbacksRemoved);
		mPendingAsyncEvents.clear();
		AAMPLOG_WARN("vk::FlushPendingEvents - Cleared mPendingAsyncEvents map");
	}

	if (mEventProfilingEnabled)
	{
		for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
		{
			if (mEventStats[i] > 0)
				AAMPLOG_WARN("[EventProfiling] EventType[%d] total dispatches: %d", i, mEventStats[i]);
		}
	}
	for (int i = 0; i < AAMP_MAX_NUM_EVENTS; i++)
		mEventStats[i] = 0;

	mIsFirstProgressSent = false;
	AAMPLOG_WARN("vk::FlushPendingEvents - END, all events flushed");
}

/**
 * @brief AddListenerForAllEvents - Register one listener for all events
 */ 
void AampEventManager::AddListenerForAllEvents(EventListener* eventListener)
{
	if(eventListener != NULL)
	{
		std::shared_ptr<EventListener> sharedListener(eventListener, [](EventListener* ptr) {
			// No-op deleter to avoid accidental deletion
		});
		AddEventListener(AAMP_EVENT_ALL_EVENTS,sharedListener);
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
		std::shared_ptr<EventListener> sharedListener(eventListener, [](EventListener* ptr) {
		// No-op deleter to avoid accidental deletion
		});
		RemoveEventListener(AAMP_EVENT_ALL_EVENTS,sharedListener);
	}
	else
	{
		AAMPLOG_ERR("Null eventlistener. Failed to deregister");
	}
}

/**
 * @brief AddEventListener - Register  listener for one eventtype
 */
void AampEventManager::AddEventListener(AAMPEventType eventType, std::shared_ptr<EventListener>& eventListener)
{
	if ((eventListener != NULL) && (eventType >= AAMP_EVENT_ALL_EVENTS) && (eventType < AAMP_MAX_NUM_EVENTS))
	{
		ListenerData* pListener = new ListenerData;
		if (pListener)
		{
			AAMPLOG_INFO("EventType:%d, Listener %p new %p", eventType, eventListener.get(), pListener);
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
void AampEventManager::RemoveEventListener(AAMPEventType eventType, std::shared_ptr<EventListener>& eventListener)
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
				AAMPLOG_INFO("Eventtype:%d %p delete %p use_count = %d", eventType, eventListener.get(), pListener, (int)eventListener.use_count());
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
	AAMPLOG_WARN("vk::SetPlayerState - EventManager playerState changing to %d (released=%d)", state, (state == eSTATE_RELEASED) ? 1 : 0);
	if (mPlayerState != state)
	{
		AAMPLOG_WARN("vk::SetPlayerState - EventManager playerState=%d → %d, pendingQueue=%zu, pendingCallbacks=%zu", 
			mPlayerState, state, mEventWorkerDataQue.size(), mPendingAsyncEvents.size());
	}
	mPlayerState = state;
}

/**
 * @brief SetEventProfilingConfig - Configure event round-trip time profiling at runtime
 */
void AampEventManager::SetEventProfilingConfig(bool enabled, int thresholdMs)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	mEventProfilingEnabled     = enabled;
	mEventProfilingThresholdMs = thresholdMs;
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
	
	AAMPLOG_WARN("vk::SendEvent - eventType=%d, eventMode=%d, mPlayerState=%d", eventType, eventMode, mPlayerState);
	
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
		AAMPLOG_WARN("vk::SendEvent - has listeners, sId=%u, proceeding to send", sId);
		// if caller is asking for Sync Event , ensure it has proper source Id, else it has to go async event
		if(eventMode==AAMP_EVENT_SYNC_MODE &&  sId != 0)
		{
			AAMPLOG_WARN("vk::SendEvent - sending eventType=%d as SYNC", eventType);
			SendEventSync(eventData);
		}
		else if(eventMode==AAMP_EVENT_ASYNC_MODE)
		{
			AAMPLOG_WARN("vk::SendEvent - sending eventType=%d as ASYNC", eventType);
			SendEventAsync(eventData);
		}
		else
		{
			//For other events if asyncTune enabled or callee from non-UI thread , then send the event as Async
			if (mAsyncTuneEnabled || sId == 0)
			{
				AAMPLOG_WARN("vk::SendEvent - sending eventType=%d as ASYNC (default mode)", eventType);
				SendEventAsync(eventData);
			}
			else
			{
				AAMPLOG_WARN("vk::SendEvent - sending eventType=%d as SYNC (default mode)", eventType);
				SendEventSync(eventData);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("vk::SendEvent - eventType=%d NOT sent, mPlayerState=%d (released=%d), hasListeners=%d", 
			eventType, mPlayerState, (mPlayerState == eSTATE_RELEASED), 
			(mEventListeners[AAMP_EVENT_ALL_EVENTS] || mEventListeners[eventType]) ? 1 : 0);
	}
}


// Worker thread for handling Async Events
/**
 * @brief AsyncEvent - Task function for IdleEvent
 */ 
void AampEventManager::AsyncEvent()
{
	AAMPEventPtr eventData    = nullptr;
	long long    enqueueTimeMs = 0;
	{
		std::lock_guard<std::mutex> guard(mMutexVar);
		// pop out the event to sent in async mode
		if(!mEventWorkerDataQue.empty())
		{
			AsyncEventEntry& entry = mEventWorkerDataQue.front();
			eventData      = std::move(entry.event);
			enqueueTimeMs  = entry.enqueueTimeMs;
			AAMPEventType eventType = eventData->getType();
			AAMPLOG_WARN("vk::AsyncEvent - popping eventType=%d from queue, queue size before pop: %zu", eventType, mEventWorkerDataQue.size());
			mEventWorkerDataQue.pop();
			AAMPLOG_WARN("vk::AsyncEvent - eventType=%d popped, queue size after pop: %zu", eventType, mEventWorkerDataQue.size());
		}
		else
		{
			AAMPLOG_WARN("vk::AsyncEvent - queue is empty!");
		}
	}
	// Push the new event in sync mode from the idle task.
	// enqueueTimeMs is forwarded so SendEventSync can compute the full async round-trip
	// (queue dwell time + sync delivery time) for profiling.
	if(eventData)
	{
		AAMPEventType eventType = eventData->getType();
		bool hasListeners = IsEventListenerAvailable(eventData->getType());
		bool isNotReleased = (mPlayerState != eSTATE_RELEASED);
		AAMPLOG_WARN("vk::AsyncEvent - eventType=%d, hasListeners=%d, playerNotReleased=%d, mPlayerState=%d", 
			eventType, hasListeners ? 1 : 0, isNotReleased ? 1 : 0, mPlayerState);
		
		if(hasListeners && isNotReleased)
		{
			AAMPLOG_WARN("vk::AsyncEvent - dispatching eventType=%d via SendEventSync", eventType);
			SendEventSync(eventData, enqueueTimeMs);
		}
		else
		{
			AAMPLOG_WARN("vk::AsyncEvent - eventType=%d NOT dispatched (hasListeners=%d, playerNotReleased=%d)", 
				eventType, hasListeners ? 1 : 0, isNotReleased ? 1 : 0);
		}
	}
	else
	{
		AAMPLOG_WARN("vk::AsyncEvent - eventData is null!");
	}
}

/**
 * @brief SendEventAsync - Function to send events Async
 */ 
void AampEventManager::SendEventAsync(const AAMPEventPtr &eventData)
{
	AAMPEventType eventType = eventData->getType();
	std::unique_lock<std::mutex> lock(mMutexVar);
	
	AAMPLOG_WARN("vk::SendEventAsync - eventType=%d, mPlayerState=%d", eventType, mPlayerState);
	
	// Check if already player in release state , then no need to send any events
	if(mPlayerState != eSTATE_RELEASED)
	{
		AAMPLOG_INFO("Sending event %d to AsyncQ", eventType);
		AAMPLOG_WARN("vk::SendEventAsync - queuing eventType=%d, queue size before push: %zu", eventType, mEventWorkerDataQue.size());
		
		AsyncEventEntry entry;
		entry.event         = eventData;
		entry.enqueueTimeMs = mEventProfilingEnabled ? NOW_STEADY_TS_MS : 0;
		mEventWorkerDataQue.push(std::move(entry));
		
		AAMPLOG_WARN("vk::SendEventAsync - eventType=%d queued, queue size after push: %zu", eventType, mEventWorkerDataQue.size());
		
		lock.unlock();
		// Every event need a idle task to execute it
		guint callbackID = g_idle_add_full(mEventPriority, EventManagerThreadFunction, this, NULL);
		AAMPLOG_WARN("vk::SendEventAsync - g_idle_add_full returned callbackID=%u for eventType=%d", callbackID, eventType);
		
		if(callbackID != 0)
		{
			SetCallbackAsPending(callbackID);
			AAMPLOG_WARN("vk::SendEventAsync - callbackID=%u set as pending for eventType=%d", callbackID, eventType);
		}
		else
		{
			AAMPLOG_WARN("vk::SendEventAsync - ERROR: g_idle_add_full returned 0 for eventType=%d", eventType);
		}
	}
	else
	{
		AAMPLOG_WARN("vk::SendEventAsync - eventType=%d NOT queued, player in RELEASED state", eventType);
	}
}


/**
 * @brief SendEventSync - Function to send events sync
 */ 
void AampEventManager::SendEventSync(const AAMPEventPtr &eventData, long long enqueueTimeMs)
{
	AAMPEventType eventType = eventData->getType();
	std::unique_lock<std::mutex> lock(mMutexVar);
	
	AAMPLOG_WARN("vk::SendEventSync - eventType=%d, enqueueTimeMs=%lld, mPlayerState=%d", eventType, enqueueTimeMs, mPlayerState);
	
	// Snapshot profiling config under the lock. mEventProfilingEnabled and
	// mEventProfilingThresholdMs are written by SetEventProfilingConfig (also under
	// mMutexVar). Reading them after lock.unlock() without a snapshot would be a data
	// race under the C++ memory model (concurrent unsynchronised read + write).
	const bool profilingEnabled   = mEventProfilingEnabled;
	const int  profilingThreshold = mEventProfilingThresholdMs;
	// Capture dispatch start time while still holding the lock so that the dwell-time
	// boundary (enqueueTimeMs → dispatchStartMs) and the delivery-time boundary
	// (dispatchStartMs → end) are both taken on a consistent clock epoch.
	long long dispatchStartMs = profilingEnabled ? NOW_STEADY_TS_MS : 0;
	// Check if already player in release state , then no need to send any events
	// Its checked again here ,as async events can come to sync mode after playback is stopped 
	if(mPlayerState == eSTATE_RELEASED)
	{
		AAMPLOG_WARN("vk::SendEventSync - eventType=%d NOT sent, player in RELEASED state", eventType);
		return;
	}
	
	mEventStats[eventType]++;
	if (eventType != AAMP_EVENT_PROGRESS)
	{
		if (eventType != AAMP_EVENT_STATE_CHANGED)
		{
			AAMPLOG_WARN("vk::SendEventSync - dispatching eventType=%d (session_id=%s)", eventType, eventData->GetSessionId().c_str());
			AAMPLOG_INFO("(type=%d)(session_id=%s)", eventType, eventData->GetSessionId().c_str());
		}
		else
		{
			AAMPLOG_WARN("vk::SendEventSync - dispatching STATE_CHANGED eventType=%d (session_id=%s)", eventType, eventData->GetSessionId().c_str());
			AAMPLOG_MIL("(type=%d)(state=%d)(session_id=%s)", eventType, std::dynamic_pointer_cast<StateChangedEvent>(eventData)->getState(),
				eventData->GetSessionId().c_str());
		}
	}
	else if (!mIsFirstProgressSent)
	{
		ProgressEventPtr progressEvt = std::dynamic_pointer_cast<ProgressEvent>(eventData);
		AAMPLOG_MIL("First progress event sent(session_id=%s) position:%0.3lfms, live-latency:%0.3lfms",
			eventData->GetSessionId().c_str(), progressEvt->getPosition(), progressEvt->getLiveLatency());
		mIsFirstProgressSent = true;
	}
	// Build list of registered event listeners.
	ListenerData* pList = NULL;
	ListenerData* pListener = mEventListeners[eventType];
	int listenerCount = 0;
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
		listenerCount++;
	}
	pListener = mEventListeners[AAMP_EVENT_ALL_EVENTS];  // listeners registered for "all" event types
	int allEventListenerCount = 0;
	while (pListener != NULL)
	{
		ListenerData* pNew = new ListenerData;
		pNew->eventListener = pListener->eventListener;
		pNew->pNext = pList;
		pList = pNew;
		pListener = pListener->pNext;
		allEventListenerCount++;
	}
	
	AAMPLOG_WARN("vk::SendEventSync - eventType=%d, typeListeners=%d, allEventListeners=%d, totalListeners=%d", 
		eventType, listenerCount, allEventListenerCount, listenerCount + allEventListenerCount);
	
	lock.unlock();

	// After releasing the lock, dispatch each of the registered listeners.
	// This allows event handlers to add/remove listeners for future events.
	int dispatchCount = 0;
	while (pList != NULL)
	{
		ListenerData* pCurrent = pList;
		if (pCurrent->eventListener != NULL)
		{
			AAMPLOG_WARN("vk::SendEventSync - dispatching eventType=%d to listener #%d", eventType, dispatchCount + 1);
			pCurrent->eventListener->SendEvent(eventData);
			dispatchCount++;
		}
		pList = pCurrent->pNext;
		SAFE_DELETE(pCurrent);
	}
	
	AAMPLOG_WARN("vk::SendEventSync - eventType=%d dispatched to %d listeners", eventType, dispatchCount);

	if (profilingEnabled && dispatchStartMs > 0)
	{
		long long deliveryMs = NOW_STEADY_TS_MS - dispatchStartMs;
		long long dwellMs = 0;
		if (enqueueTimeMs > 0)
		{
			// Async path: Calculate queue dwell time
			dwellMs = dispatchStartMs - enqueueTimeMs;
		}
		long long totalMs = dwellMs + deliveryMs;
		if (totalMs >= profilingThreshold)
		{
			AAMPLOG_WARN("[EventProfiling] Event %d total: %lld ms (queue dwell: %lld ms + delivery: %lld ms), threshold: %d ms",
						eventType, totalMs, dwellMs, deliveryMs, profilingThreshold);
		}
	}
}

/**
 * @brief SetCallbackAsDispatched - Set callbackId as dispatched/done
 */
void AampEventManager::SetCallbackAsDispatched(guint id)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	AAMPLOG_WARN("vk::SetCallbackAsDispatched - id=%u, pendingCount=%zu", id, mPendingAsyncEvents.size());
	
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_WARN("vk::SetCallbackAsDispatched - id=%u FOUND in pending list, State=%d, erasing", id, itr->second);
		AAMPLOG_TRACE("id:%d in mPendingAsyncEvents, erasing it. State:%d", id,itr->second);
		assert (itr->second);
		mPendingAsyncEvents.erase(itr);
		AAMPLOG_WARN("vk::SetCallbackAsDispatched - id=%u erased, pendingCount now=%zu", id, mPendingAsyncEvents.size());
	}
	else
	{
		AAMPLOG_WARN("vk::SetCallbackAsDispatched - id=%u NOT in pending list, adding with State=false", id);
		AAMPLOG_TRACE("id:%d not in mPendingAsyncEvents, insert and mark as not pending", id);
		mPendingAsyncEvents[id] = false;
		AAMPLOG_WARN("vk::SetCallbackAsDispatched - id=%u added, pendingCount now=%zu", id, mPendingAsyncEvents.size());
	}
}

/**
 * @brief SetCallbackAsPending - Set callbackId as Pending/to be done
 */ 
void AampEventManager::SetCallbackAsPending(guint id)
{
	std::lock_guard<std::mutex> guard(mMutexVar);
	AAMPLOG_WARN("vk::SetCallbackAsPending - id=%u, pendingCount=%zu", id, mPendingAsyncEvents.size());
	
	AsyncEventListIter  itr = mPendingAsyncEvents.find(id);
	if(itr != mPendingAsyncEvents.end())
	{
		AAMPLOG_WARN("vk::SetCallbackAsPending - id=%u ALREADY in pending list with State=%d, erasing", id, itr->second);
		AAMPLOG_TRACE("id:%d already in mPendingAsyncEvents and completed, erase it State:%d",id,itr->second);
		assert (!itr->second);
		mPendingAsyncEvents.erase(itr);
		AAMPLOG_WARN("vk::SetCallbackAsPending - id=%u erased, pendingCount now=%zu", id, mPendingAsyncEvents.size());
	}
	else
	{
		AAMPLOG_WARN("vk::SetCallbackAsPending - id=%u NOT in list, adding with State=true", id);
		mPendingAsyncEvents[id] = true;
		AAMPLOG_TRACE("id:%d in mPendingAsyncEvents, added to list", id);
		AAMPLOG_WARN("vk::SetCallbackAsPending - id=%u added to pending, pendingCount now=%zu", id, mPendingAsyncEvents.size());
	}
}
