# 07 - Event Manager & Event System

## Module: AampEventManager + AampEvent

**Source Files Read:**
- AampEvent.h (lines 1-500, complete — 48 event types, AAMPTuneFailure enum, AAMPPlayerState enum, AAMPEvent union struct, event data structures)
- AampEventManager.h (complete — AampEventManager class, ListenerData struct, sync/async dispatch)
- AampEventManager.cpp (complete — all method implementations)

**Confidence: 100%**

---

## Diagram 1: Event Registration & Dispatch (Sync Mode)

`mermaid
sequenceDiagram
    participant App as Application/JS Layer
    participant AAMP as PrivateInstanceAAMP
    participant EM as AampEventManager
    participant LL as ListenerData (linked list)
    participant EL as EventListener

    Note over App,EL: Event Listener Registration
    App->>EM: AddEventListener(eventType, eventListener)
    EM->>EM: Validate eventType (0..AAMP_MAX_NUM_EVENTS)
    EM->>EM: Lock mMutexVar
    EM->>LL: Create new ListenerData
    EM->>LL: pListener->eventListener = eventListener
    EM->>LL: pListener->pNext = mEventListeners[eventType]
    EM->>EM: mEventListeners[eventType] = pListener
    EM->>EM: Unlock mMutexVar

    Note over App,EL: Sync Event Dispatch
    AAMP->>EM: SendEvent(eventData, AAMP_EVENT_SYNC_MODE)
    EM->>EM: Check mIsFakeTune (skip if fake tune)
    EM->>EM: Check mPlayerState != eSTATE_RELEASED
    EM->>EM: GetSourceID() — verify non-zero for sync
    EM->>EM: SendEventSync(eventData)
    EM->>EM: Lock mMutexVar
    EM->>EM: mEventStats[eventType]++
    EM->>LL: Copy listeners for eventType into temp list
    EM->>LL: Copy listeners for AAMP_EVENT_ALL_EVENTS into temp list
    EM->>EM: Unlock mMutexVar
    loop For each listener in temp list
        EM->>EL: pCurrent->eventListener->SendEvent(eventData)
    end
    EM->>EM: Delete temp list nodes
`

## Diagram 2: Async Event Dispatch

`mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant EM as AampEventManager
    participant Q as mEventWorkerDataQue
    participant GLib as GLib Main Loop
    participant EL as EventListener

    Note over AAMP,EL: Async Event Dispatch
    AAMP->>EM: SendEvent(eventData, AAMP_EVENT_DEFAULT_MODE)
    EM->>EM: Check mAsyncTuneEnabled || GetSourceID()==0
    alt Async path
        EM->>EM: SendEventAsync(eventData)
        EM->>EM: Lock mMutexVar
        EM->>EM: Check mPlayerState != eSTATE_RELEASED
        EM->>Q: mEventWorkerDataQue.push(eventData)
        EM->>EM: Unlock mMutexVar
        EM->>GLib: g_idle_add_full(mEventPriority, EventManagerThreadFunction, this)
        GLib-->>EM: Returns callbackID
        EM->>EM: SetCallbackAsPending(callbackID)

        Note over GLib,EL: GLib Idle Callback Fires
        GLib->>EM: EventManagerThreadFunction(this)
        EM->>EM: SetCallbackAsDispatched(callbackId)
        EM->>EM: AsyncEvent()
        EM->>EM: Lock mMutexVar
        EM->>Q: eventData = mEventWorkerDataQue.front(); pop()
        EM->>EM: Unlock mMutexVar
        EM->>EM: Check IsEventListenerAvailable && state != RELEASED
        EM->>EM: SendEventSync(eventData)
        EM->>EL: Dispatch to all registered listeners
    end
`

## Diagram 3: Event Lifecycle — Registration to Teardown

`mermaid
sequenceDiagram
    participant App as Application
    participant EM as AampEventManager
    participant LL as ListenerData[]

    Note over App,LL: Initialization
    App->>EM: new AampEventManager(playerId)
    EM->>EM: mPlayerState = eSTATE_IDLE
    EM->>EM: Initialize mEventListeners[0..MAX] = NULL
    EM->>EM: Initialize mEventStats[0..MAX] = 0

    Note over App,LL: Register Listeners
    App->>EM: AddListenerForAllEvents(eventListener)
    EM->>EM: Wrap raw ptr in shared_ptr (no-op deleter)
    EM->>EM: AddEventListener(AAMP_EVENT_ALL_EVENTS, sharedListener)

    App->>EM: AddEventListener(AAMP_EVENT_PROGRESS, listener)
    App->>EM: AddEventListener(AAMP_EVENT_STATE_CHANGED, listener)

    Note over App,LL: Playback Active — Events Flow
    EM->>EM: SendEvent(ProgressEvent) → sync or async
    EM->>EM: SendEvent(StateChangedEvent) → logged with state value
    EM->>EM: SendEvent(BitrateChangedEvent)

    Note over App,LL: Teardown
    App->>EM: SetPlayerState(eSTATE_RELEASED)
    Note right of EM: All subsequent SendEvent calls are blocked
    App->>EM: FlushPendingEvents()
    EM->>EM: Lock mMutexVar
    EM->>EM: Clear mEventWorkerDataQue (pop all)
    EM->>EM: g_source_remove() for all mPendingAsyncEvents
    EM->>EM: Clear mPendingAsyncEvents
    EM->>EM: Reset mEventStats
    EM->>EM: Unlock mMutexVar

    App->>EM: ~AampEventManager()
    EM->>EM: FlushPendingEvents()
    EM->>EM: Lock mMutexVar
    loop For each eventType 0..MAX
        EM->>LL: Delete all ListenerData nodes in chain
    end
    EM->>EM: Unlock mMutexVar
`

## Diagram 4: Event Mode Decision Logic

`mermaid
sequenceDiagram
    participant Caller as Any AAMP Component
    participant EM as AampEventManager

    Caller->>EM: SendEvent(eventData, eventMode)
    EM->>EM: Validate eventType in range
    
    alt FakeTune enabled
        EM->>EM: Skip (unless STATE_CHANGED→COMPLETE or EOS)
    end

    alt eventMode == SYNC && sourceId != 0
        EM->>EM: SendEventSync(eventData)
    else eventMode == ASYNC
        EM->>EM: SendEventAsync(eventData)
    else eventMode == DEFAULT
        alt mAsyncTuneEnabled || sourceId == 0
            EM->>EM: SendEventAsync(eventData)
        else UI thread (sourceId != 0)
            EM->>EM: SendEventSync(eventData)
        end
    end
`

## Key Event Types (48 total from AAMPEventType enum)

| Event | Value | Description |
|-------|-------|-------------|
| AAMP_EVENT_TUNED | 1 | Tune success |
| AAMP_EVENT_TUNE_FAILED | 2 | Tune failure |
| AAMP_EVENT_SPEED_CHANGED | 3 | Speed changed |
| AAMP_EVENT_EOS | 4 | End of stream |
| AAMP_EVENT_PROGRESS | 6 | Playback progress (configurable interval) |
| AAMP_EVENT_MEDIA_METADATA | 9 | Asset metadata |
| AAMP_EVENT_BITRATE_CHANGED | 11 | ABR bitrate switch |
| AAMP_EVENT_STATE_CHANGED | 14 | Player state transition |
| AAMP_EVENT_BUFFERING_CHANGED | 18 | Buffering start/end |
| AAMP_EVENT_DRM_METADATA | 25 | DRM metadata info |
| AAMP_EVENT_ID3_METADATA | 36 | ID3 metadata from audio |
| AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE | 42 | Dynamic key rotation |

## Player State Machine (AAMPPlayerState)

| State | Value | Description |
|-------|-------|-------------|
| eSTATE_IDLE | 0 | Initial state |
| eSTATE_INITIALIZING | 1 | Tune started |
| eSTATE_INITIALIZED | 2 | Config complete |
| eSTATE_PREPARING | 3 | Acquiring manifests |
| eSTATE_PREPARED | 4 | Manifests parsed |
| eSTATE_BUFFERING | 5 | Pipeline dry |
| eSTATE_PAUSED | 6 | Pipeline paused |
| eSTATE_SEEKING | 7 | Seek in progress |
| eSTATE_PLAYING | 8 | Normal playback |
| eSTATE_STOPPING | 9 | Stop in progress |
| eSTATE_STOPPED | 10 | Stop complete |
| eSTATE_COMPLETE | 11 | VOD end |
| eSTATE_ERROR | 12 | Fatal error |
| eSTATE_RELEASED | 13 | Resources released |
| eSTATE_BLOCKED | 14 | Parental control |
