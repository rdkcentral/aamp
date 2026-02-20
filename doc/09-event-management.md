# Event Management System

## Overview

AAMP implements a comprehensive, thread-safe event system designed for real-time notification of playback state changes, errors, metadata, and system events. The architecture supports both synchronous and asynchronous event dispatch with flexible listener registration and comprehensive event telemetry.

**Core Implementation**: `AampEventManager.h/cpp`, `AampEvent.h/cpp`, `AampEventListener.h/cpp`

## Architecture & Design

### Event System Components

#### AampEventManager
**Purpose**: Centralized event dispatch and listener management with thread-safe operation

```cpp
class AampEventManager {
private:
    // Listener Management
    ListenerData* mEventListeners[AAMP_MAX_NUM_EVENTS];    // Per-event type listener arrays
    int mEventStats[AAMP_MAX_NUM_EVENTS];                  // Event dispatch statistics

    // Async Event Processing
    typedef std::queue<AAMPEventPtr> EventWorkerDataQ;
    EventWorkerDataQ mEventWorkerDataQue;                  // Event queue for async dispatch
    typedef std::map<guint, bool> AsyncEventList;
    AsyncEventList mPendingAsyncEvents;                    // Pending async event tracking

    // Thread Safety & State
    std::mutex mMutexVar;                                  // Event queue synchronization
    AAMPPlayerState mPlayerState;                          // Player state tracking
    int mPlayerId;                                         // Player instance identifier
    int mEventPriority;                                    // GLib event priority

    // Configuration Flags
    bool mIsFakeTune;                                     // Fake tune mode flag
    bool mAsyncTuneEnabled;                               // Async tune capability
};
```

#### ListenerData Structure
**Purpose**: Linked-list based listener registration with efficient traversal

```cpp
struct ListenerData {
    std::shared_ptr<EventListener> eventListener;         // Event handler reference
    ListenerData* pNext;                                  // Linked list pointer
};
```

#### AAMPEvent Base Class
**Purpose**: Type-safe event encapsulation with polymorphic event data

```cpp
class AAMPEvent {
public:
    AAMPEventType type;           // Event type identifier
    int playerId;                 // Source player ID

    // Type-specific event data (polymorphic)
    virtual ~AAMPEvent() = default;
};

typedef std::shared_ptr<AAMPEvent> AAMPEventPtr;   // Event smart pointer type
```

## Comprehensive Event Types (47 Event Categories)

### Core Playback Events
```cpp
typedef enum AAMPEventType {
    // Lifecycle Events
    AAMP_EVENT_TUNED                   = 1,   // Successful tune completion
    AAMP_EVENT_TUNE_FAILED            = 2,   // Tune operation failure
    AAMP_EVENT_EOS                    = 4,   // End of stream reached
    AAMP_EVENT_STATE_CHANGED          = 14,  // Player state transitions

    // Playback Control Events
    AAMP_EVENT_SPEED_CHANGED          = 3,   // Playback rate modification
    AAMP_EVENT_SEEKED                 = 16,  // Seek operation completion
    AAMP_EVENT_PROGRESS               = 6,   // Periodic playback progress
    AAMP_EVENT_DURATION_CHANGED       = 19,  // Content duration updates

    // Quality & Performance Events
    AAMP_EVENT_BITRATE_CHANGED        = 11,  // ABR profile switching
    AAMP_EVENT_SPEEDS_CHANGED         = 15,  // Available playback speeds update
    AAMP_EVENT_BUFFERING_CHANGED      = 18,  // Buffering state transitions
    AAMP_EVENT_ENTERING_LIVE          = 10,  // Live edge reached notification
} AAMPEventType;
```

### Content & Metadata Events
```cpp
// Metadata Events
AAMP_EVENT_MEDIA_METADATA          = 9,   // Asset metadata (duration, codecs, etc.)
AAMP_EVENT_TIMED_METADATA          = 12,  // HLS timed metadata (EXT-X-DATERANGE, ID3)
AAMP_EVENT_BULK_TIMED_METADATA     = 13,  // Batch timed metadata delivery
AAMP_EVENT_ID3_METADATA            = 36,  // ID3 tags from audio streams

// Track Information Events
AAMP_EVENT_AUDIO_TRACKS_CHANGED    = 20,  // Available audio tracks update
AAMP_EVENT_TEXT_TRACKS_CHANGED     = 21,  // Available subtitle tracks update
AAMP_EVENT_WEBVTT_CUE_DATA        = 27,  // WebVTT subtitle cue delivery

// Manifest & Playlist Events
AAMP_EVENT_PLAYLIST_INDEXED        = 5,   // HLS/DASH manifest parsing completion
AAMP_EVENT_MANIFEST_REFRESH_NOTIFY = 43,  // Live manifest update notification
AAMP_EVENT_NEED_MANIFEST_DATA      = 45,  // Dynamic manifest data request
```

### DRM & Security Events
```cpp
// DRM Lifecycle Events
AAMP_EVENT_DRM_METADATA            = 25,  // DRM system information and status
AAMP_EVENT_DRM_MESSAGE             = 37,  // DRM system messages and errors
AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE = 42, // Key rotation and license updates

// Security Events
AAMP_EVENT_WATERMARK_SESSION_UPDATE = 41, // Watermarking session state
AAMP_EVENT_BLOCKED                 = 38,  // ATSC content blocking event
```

### Advertisement Events
```cpp
// Ad Lifecycle Events
AAMP_EVENT_AD_BREAKS_CHANGED       = 22,  // Ad break schedule updates
AAMP_EVENT_AD_RESOLVED             = 28,  // Ad fulfillment status
AAMP_EVENT_AD_STARTED              = 23,  // Ad playback initiation
AAMP_EVENT_AD_COMPLETED            = 24,  // Ad playbook completion

// Ad Break Management
AAMP_EVENT_AD_RESERVATION_START    = 29,  // Ad reservation period start
AAMP_EVENT_AD_RESERVATION_END      = 30,  // Ad reservation period end
AAMP_EVENT_AD_PLACEMENT_START      = 31,  // Individual ad placement start
AAMP_EVENT_AD_PLACEMENT_END        = 32,  // Individual ad placement end
AAMP_EVENT_AD_PLACEMENT_ERROR      = 33,  // Ad placement failure
AAMP_EVENT_AD_PLACEMENT_PROGRESS   = 34,  // Ad playback progress reporting
```

### System & Diagnostic Events
```cpp
// Performance & Monitoring
AAMP_EVENT_REPORT_ANOMALY          = 26,  // Performance anomaly detection
AAMP_EVENT_REPORT_METRICS_DATA     = 35,  // Comprehensive telemetry data
AAMP_EVENT_TUNE_PROFILING          = 17,  // Tune performance micro-events
AAMP_EVENT_TUNE_TIME_METRICS       = 44,  // Tune timing breakdown
AAMP_EVENT_MONITORAV_STATUS        = 46,  // A/V sync monitoring status

// Network & Protocol Events
AAMP_EVENT_HTTP_RESPONSE_HEADER    = 40,  // HTTP response header data
AAMP_EVENT_CONTENT_GAP             = 39,  // Content discontinuity detection

// System Integration
AAMP_EVENT_CC_HANDLE_RECEIVED      = 7,   // Video decoder handle availability
AAMP_EVENT_JS_EVENT                = 8,   // JavaScript binding events
```

## Event Dispatch Mechanisms

AAMP supports both synchronous and asynchronous event dispatch, each optimized for different use cases:

### Synchronous Event Dispatch

**Use Case**: Critical events requiring immediate processing (state changes, errors)

Synchronous dispatch executes event listeners immediately in the calling thread, ensuring immediate notification for time-sensitive events:

```cpp
void AampEventManager::SendEventSync(const AAMPEventPtr &eventData) {
    if (!eventData) return;

    // Direct dispatch to all registered listeners
    ListenerData* listenerData = mEventListeners[eventData->type];
    while (listenerData) {
        try {
            // Immediate callback execution
            listenerData->eventListener->EventReceived(eventData);
        }
        catch (const std::exception& e) {
            AAMPLOG_ERR("Exception in event listener: %s", e.what());
            // Continue to next listener despite failure
        }
        listenerData = listenerData->pNext;
    }

    // Update statistics
    mEventStats[eventData->type]++;
}
```

**Synchronous Dispatch Characteristics**:
- **Immediate Execution**: Event listeners are invoked immediately in the calling thread, providing instant notification. This ensures critical events (state changes, errors) are processed immediately without delay.

- **Blocking Behavior**: Synchronous dispatch blocks the calling thread until all listeners complete execution. This ensures event processing completes before caller continues, providing predictable execution order.

- **Exception Isolation**: Listener exceptions are caught and logged, preventing one listener's failure from affecting other listeners. Exception isolation ensures robust event handling even when listeners contain bugs.

- **Use Cases**: Synchronous dispatch is used for:
  - **State Change Events**: `AAMP_EVENT_STATE_CHANGED` requires immediate notification for UI updates
  - **Error Events**: `AAMP_EVENT_TUNE_FAILED`, `AAMP_EVENT_ERROR` require immediate handling for error recovery
  - **Critical Metadata**: Events requiring immediate application response

**Performance Considerations**: Synchronous dispatch has minimal overhead (no queuing, no thread switching) but blocks the calling thread. Long-running listeners can delay caller execution, so listeners should perform minimal work or delegate to background threads.

### Asynchronous Event Dispatch

**Use Case**: High-frequency events, metadata, progress updates

Asynchronous dispatch queues events for background processing, preventing event handling from blocking player operations:

```cpp
void AampEventManager::SendEventAsync(const AAMPEventPtr &eventData) {
    if (!eventData) return;

    {
        std::lock_guard<std::mutex> lock(mMutexVar);
        // Queue event for async processing
        mEventWorkerDataQue.push(eventData);
    }

    // Schedule GLib async callback
    guint sourceId = g_idle_add_full(
        mEventPriority,                    // Event priority level
        EventManagerThreadFunction,       // Callback function
        this,                             // User data
        NULL                              // Destructor
    );

    // Track pending async events
    SetCallbackAsPending(sourceId);
}

// Async event processing thread function
void AampEventManager::AsyncEvent() {
    AAMPEventPtr eventPtr = nullptr;

    {
        std::lock_guard<std::mutex> lock(mMutexVar);
        if (!mEventWorkerDataQue.empty()) {
            eventPtr = mEventWorkerDataQue.front();
            mEventWorkerDataQue.pop();
        }
    }

    if (eventPtr) {
        // Dispatch to listeners in async context
        SendEventSync(eventPtr);
    }
}
```

**Asynchronous Dispatch Characteristics**:
- **Event Queuing**: Events are queued in `mEventWorkerDataQue` for background processing. Queue operations are mutex-protected to ensure thread-safe access from multiple threads. Queue provides FIFO ordering, ensuring events are processed in generation order.

- **GLib Integration**: GLib's idle callback mechanism (`g_idle_add_full()`) schedules event processing in the main event loop. GLib integration enables event processing in application's main thread context, ensuring listener execution occurs in appropriate thread (important for UI updates in JavaScript/WebKit environments).

- **Non-Blocking**: Async dispatch returns immediately after queuing, allowing caller to continue execution without waiting for listener completion. Non-blocking behavior prevents event handling from delaying player operations, maintaining playback performance.

- **Event Priority**: `mEventPriority` controls event processing priority in GLib event loop. Higher priority events are processed before lower priority events, enabling priority-based event handling for critical vs. non-critical events.

- **Use Cases**: Asynchronous dispatch is used for:
  - **Progress Events**: `AAMP_EVENT_PROGRESS` occurs frequently (every second) and doesn't require immediate processing
  - **Metadata Events**: `AAMP_EVENT_TIMED_METADATA`, `AAMP_EVENT_ID3_METADATA` provide supplementary information
  - **Telemetry Events**: `AAMP_EVENT_REPORT_METRICS_DATA` provides performance data without blocking playback

**Performance Benefits**: Asynchronous dispatch prevents high-frequency events from impacting playback performance. Event queuing absorbs event bursts, and background processing distributes listener execution overhead over time, maintaining smooth playback.

## Event Registration & Management

### Flexible Listener Registration
```cpp
class PlayerInstanceAAMP {
public:
    // Type-specific registration
    void RegisterEvent(AAMPEventType eventType, EventListener* listener) {
        mEventManager->AddEventListener(eventType, listener);
    }

    // All-events registration
    void RegisterEvents(EventListener* listener) {
        for (int i = 1; i < AAMP_MAX_NUM_EVENTS; i++) {
            mEventManager->AddEventListener((AAMPEventType)i, listener);
        }
    }

    // Modern smart pointer API
    void AddEventListener(AAMPEventType eventType,
                         std::shared_ptr<EventListener> eventListener) {
        mEventManager->AddListener(eventType, eventListener);
    }
};
```

### Listener Management Implementation
```cpp
void AampEventManager::AddEventListener(AAMPEventType eventType, EventListener* eventListener) {
    if (eventType >= AAMP_MAX_NUM_EVENTS || !eventListener) return;

    // Create new listener node
    ListenerData* newListener = new ListenerData();
    newListener->eventListener = std::shared_ptr<EventListener>(eventListener);

    // Insert at head of listener list (O(1) insertion)
    newListener->pNext = mEventListeners[eventType];
    mEventListeners[eventType] = newListener;

    AAMPLOG_INFO("Added event listener for type %d", eventType);
}

void AampEventManager::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener) {
    if (eventType >= AAMP_MAX_NUM_EVENTS || !eventListener) return;

    ListenerData** current = &mEventListeners[eventType];
    while (*current) {
        if ((*current)->eventListener.get() == eventListener) {
            ListenerData* toRemove = *current;
            *current = (*current)->pNext;
            delete toRemove;
            return;
        }
        current = &((*current)->pNext);
    }
}
```

## Summary

AAMP's event system provides a robust, scalable foundation for real-time media player communication with:

- **47 Event Types**: Comprehensive coverage of playback, content, DRM, and system events
- **Dual Dispatch**: Synchronous for critical events, asynchronous for high-frequency updates
- **Thread Safety**: Mutex-protected queue operations and listener management
- **Performance**: Optimized for minimal latency and memory efficiency
- **Flexibility**: Type-specific and bulk listener registration options
- **Reliability**: Exception isolation and automatic resource cleanup
- **Telemetry**: Built-in event statistics and monitoring capabilities

This architecture enables responsive user interfaces, robust error handling, and comprehensive playback monitoring while maintaining optimal performance characteristics.