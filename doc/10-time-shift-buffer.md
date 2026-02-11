# Time Shift Buffer (TSB)

## Overview

TSB enables time-shifted playback, allowing users to pause, rewind, and catch up on live content.

## Architecture

**Files**: `AampTSBSessionManager.h/cpp`, `AampTsbDataManager.h/cpp`, `AampTsbReader.h/cpp`, `tsb/`

**Key Classes**:
- `AampTSBSessionManager`: TSB session management
- `AampTsbDataManager`: Data storage management
- `AampTsbReader`: TSB reading interface

## Components

### TSB Storage

Local filesystem storage for fragments:
- Organized by media type (video/audio/subtitle)
- Indexed by time
- Automatic culling when full

### TSB API

Interface to TSB storage (`tsb/api/TsbApi.h`):
- Write fragments to storage
- Read fragments from storage
- Query available time range

## Workflow

### Writing to TSB

```cpp
void AampTSBSessionManager::EnqueueWrite(
    std::string url,
    std::shared_ptr<CachedFragment> fragment,
    std::string periodId)
{
    // Queue fragment for writing
    // Background thread writes to storage
}
```

### Reading from TSB

```cpp
bool AampTsbReader::ReadFragment(
    double position,
    CachedFragment* fragment)
{
    // Find fragment in TSB
    // Read from storage
    // Return fragment data
}
```

## Configuration

Key TSB configuration:
- `tsbLength`: TSB length (seconds)
- `tsbLocation`: Storage location
- `tsbMinFreePercentage`: Minimum free disk space
- `tsbMaxDiskStorage`: Maximum disk storage

## Summary

TSB provides:
- Time-shifted playback
- Catch-up viewing
- Local storage
- Seamless integration
