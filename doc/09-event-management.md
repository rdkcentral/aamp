# Event Management

## Overview

AAMP provides a comprehensive event system for notifying applications of playback state changes, errors, and metadata.

## Architecture

**Files**: `AampEvent.h/cpp`, `AampEventListener.h/cpp`, `AampEventManager.h/cpp`

**Key Classes**:
- `AampEventManager`: Event dispatch
- `EventListener`: Listener interface
- `AAMPEvent`: Event data

## Event Types

### Playback Events
- `eSTATE_CHANGED`: Player state change
- `ePROGRESS`: Playback progress
- `eSPEED_CHANGED`: Playback speed change

### Content Events
- `eMEDIA_METADATA`: Media metadata
- `eBITRATE_CHANGED`: Bitrate change
- `eTIMED_METADATA`: Timed metadata (ads, etc.)

### Error Events
- `eERROR`: Playback error
- `eBUFFERING`: Buffering state

### DRM Events
- `eDRM_METADATA`: DRM metadata
- `eCONTENT_PROTECTION_DATA_UPDATE`: Key rotation

## Event Dispatch

### Synchronous Events

Immediate dispatch to listeners:
```cpp
void AampEventManager::SendEventSync(const AAMPEventPtr &event)
{
    // Immediately call all listeners
    for (auto listener : listeners) {
        listener->EventReceived(event);
    }
}
```

### Asynchronous Events

Queued for later dispatch:
```cpp
void AampEventManager::SendEventAsync(const AAMPEventPtr &event)
{
    // Add to queue
    mEventWorkerDataQue.push(event);
    // Schedule dispatch
    ScheduleAsyncDispatch();
}
```

## Listener Registration

```cpp
// Register for specific event
player->RegisterEvent(eBITRATE_CHANGED, listener);

// Register for all events
player->RegisterEvents(listener);
```

## Summary

The event system provides:
- Comprehensive event coverage
- Flexible listener registration
- Async and sync dispatch
- Thread-safe operation