# Time Shift Buffer (TSB)

## Overview

The Time Shift Buffer (TSB) system enables time-shifted playback functionality, allowing users to pause live content, rewind to previously viewed segments, and catch up on live streams by accessing buffered content. TSB provides local filesystem-based storage for media fragments, creating a time window of accessible content that extends backward from the current live playback position.

TSB operates by continuously writing downloaded fragments to persistent storage while playback progresses, maintaining a rolling buffer of recent content. When users seek backward or pause, the system reads fragments from TSB storage instead of re-downloading from the CDN, providing instant seek response and reducing network bandwidth usage. The system manages storage capacity through automatic culling of oldest fragments when storage limits are reached, ensuring efficient disk space utilization.

The TSB architecture separates session management (tracking playback state and TSB operations), data management (organizing fragments by media type and time), and storage operations (filesystem I/O via TSB API), providing a modular design that supports both local TSB (on-device storage) and remote TSB (network-attached storage) scenarios.

## Architecture

**Files**: `AampTSBSessionManager.h/cpp`, `AampTsbDataManager.h/cpp`, `AampTsbReader.h/cpp`, `tsb/api/TsbApi.h`, `tsb/src/TsbStore.cpp`

**Key Classes**:
- **`AampTSBSessionManager`**: Central TSB session management that coordinates TSB operations across all media tracks. Manages TSB store initialization, write queue processing, read coordination, and session lifecycle. Maintains references to data managers and readers for each media type (video, audio, subtitle).
- **`AampTsbDataManager`**: Per-media-type data management that tracks fragment metadata, storage locations, and time indexing. Each media track (video, audio, subtitle) has its own data manager instance that maintains fragment-to-URL mappings and time range information for efficient fragment lookup.
- **`AampTsbReader`**: Reading interface that retrieves fragments from TSB storage based on playback position. Readers query data managers to locate fragments for requested time positions and coordinate with TSB store API to read fragment data from filesystem.

## Components

### TSB Storage

The TSB storage system provides persistent filesystem storage for media fragments with intelligent organization and capacity management:

- **Organized by Media Type**: Fragments are stored separately for each media type (video, audio, subtitle) to enable independent time-shifted access per track. Each media type maintains its own directory structure and indexing, allowing efficient per-track fragment management and enabling scenarios where only specific tracks need time-shift access.

- **Indexed by Time**: Fragments are indexed by their playback time position, enabling efficient lookup when users seek to specific time positions. The indexing system maps time positions to fragment URLs and storage locations, allowing `AampTsbReader` to quickly locate fragments for requested playback positions without scanning all stored fragments.

- **Automatic Culling When Full**: When TSB storage reaches capacity limits (`tsbMaxDiskStorage` or `tsbMinFreePercentage` thresholds), the system automatically culls oldest fragments to make space for new content. Culling removes fragments beyond the configured TSB length (`tsbLength`), maintaining a rolling buffer window. The culling process runs asynchronously to avoid blocking fragment writes, ensuring continuous TSB operation even during storage management operations.

**Storage Configuration**: TSB storage is configured via `TSB::Store::Config` structure containing `location` (filesystem path), `minFreePercentage` (minimum free disk space to maintain), and `maxCapacity` (maximum storage allocation in MiB). The storage system performs automatic cleanup of stale files on initialization and provides thread-safe read/write operations for concurrent access.

### TSB API

The TSB API (`tsb/api/TsbApi.h`) provides a high-level interface to TSB storage operations:

- **Write Fragments to Storage**: The `TSB::Store::Write()` method writes fragment data buffers to persistent storage, keyed by fragment URL. Writes are synchronous and return status codes (`Status::OK`, `Status::NO_SPACE`, `Status::ALREADY_EXISTS`, `Status::FAILED`) indicating operation results. The API handles filesystem operations internally, organizing fragments by URL-derived paths and managing storage capacity.

- **Read Fragments from Storage**: The `TSB::Store::Read()` method retrieves fragment data from storage based on URL. Clients must allocate buffers of appropriate size (obtained via `GetSize()` method) before reading. Reads are synchronous and return fragment data along with status codes. The API handles filesystem I/O and provides efficient fragment retrieval.

- **Query Available Time Range**: The TSB system maintains metadata about available time ranges through `AampTsbDataManager`, which tracks the earliest and latest fragment times stored. This enables applications to determine seekable ranges and display time-shift availability to users. The time range information is updated as fragments are written and culled.

**Thread Safety**: The TSB API is thread-safe, allowing concurrent writes and reads from different threads. The underlying storage implementation uses internal synchronization to ensure data consistency during concurrent operations.

## Workflow

### Writing to TSB

The TSB write workflow begins when fragments are downloaded and cached during normal playback:

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

**Write Process**:
- **Fragment Enqueuing**: When fragments are downloaded and cached in `MediaTrack`, the fragment collector calls `EnqueueWrite()` to add fragments to the TSB write queue. Fragments are enqueued with their URL (for storage keying), fragment data (`CachedFragment`), and period ID (for DASH period tracking). The enqueue operation is non-blocking and returns immediately, allowing playback to continue without waiting for storage I/O.

- **Background Write Thread**: A dedicated background thread (`ProcessWriteQueue()`) processes the write queue asynchronously. The thread wakes up when fragments are enqueued and writes them to TSB storage via `TSB::Store::Write()`. This asynchronous design prevents storage I/O from blocking fragment downloads or playback operations.

- **Storage Operations**: The write thread calls `TSB::Store::Write()` with fragment URL, data buffer, and size. The TSB store API handles filesystem operations, organizing fragments by URL-derived paths and managing storage capacity. If storage is full (`Status::NO_SPACE`), the system triggers culling of oldest fragments before retrying the write.

- **Metadata Updates**: After successful writes, `AampTsbDataManager` updates fragment metadata, adding the fragment URL and time position to its indexing structures. This enables efficient fragment lookup during read operations.

**Write Queue Management**: The write queue uses thread-safe data structures (mutex-protected queues) to coordinate between enqueue operations (from download threads) and dequeue operations (from write thread). The queue prevents unbounded growth by limiting queue size or triggering backpressure when storage is full.

### Reading from TSB

The TSB read workflow occurs when users seek backward or pause playback:

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

**Read Process**:
- **Position Lookup**: When a seek operation requests a specific playback position, `AampTsbReader::ReadFragment()` queries `AampTsbDataManager` to find the fragment URL corresponding to the requested time position. The data manager uses its time-indexed metadata to locate the appropriate fragment, handling cases where the exact position falls within a fragment's time range.

- **Fragment Retrieval**: Once the fragment URL is identified, the reader calls `TSB::Store::Read()` with the URL to retrieve fragment data from storage. The read operation is synchronous and returns fragment data along with status codes. If the fragment is not found in TSB (e.g., beyond TSB length or not yet written), the read fails and the system falls back to CDN download.

- **Fragment Population**: Retrieved fragment data is populated into the `CachedFragment` structure, including fragment buffer, metadata (PTS/DTS, duration), and stream information. The fragment is then available for injection into the GStreamer pipeline, providing seamless playback from TSB storage.

- **Cache Integration**: TSB reads integrate with AAMP's fragment cache system. If fragments are available in both TSB and memory cache, the system prefers memory cache for performance. TSB serves as a fallback when memory cache doesn't contain the requested fragments, extending the effective buffer window beyond memory limits.

**Read Optimization**: The TSB read system optimizes for common access patterns. Sequential reads (forward playback from TSB) benefit from filesystem caching and sequential I/O patterns. Random reads (seeking to arbitrary positions) use the time-indexed metadata for efficient fragment location.

## Configuration

The TSB system exposes configuration parameters that control storage behavior and capacity:

- **`tsbLength`**: TSB length in seconds (default: varies by configuration). This determines how far backward users can seek from the current live position. Longer TSB lengths provide more time-shift capability but require more storage capacity. The system automatically culls fragments older than this duration to maintain the rolling buffer window.

- **`tsbLocation`**: Storage location as an absolute filesystem path (default: configured path). This specifies where TSB data is stored on the device. The location must be unique per TSB session and should have sufficient disk space for the configured TSB length and capacity limits. The path may include session identifiers to support multiple concurrent TSB sessions.

- **`tsbMinFreePercentage`**: Minimum free disk space percentage to maintain in the mounted filesystem (default: varies). When disk free space falls below this threshold, TSB operations may be throttled or stopped to prevent filesystem exhaustion. This protects system stability by ensuring adequate free space for other system operations.

- **`tsbMaxDiskStorage`**: Maximum storage capacity to allocate for TSB in mebibytes (MiB = 1024 * 1024 bytes, default: varies). This limits TSB storage usage regardless of available disk space, preventing TSB from consuming excessive storage. When this limit is reached, automatic culling removes oldest fragments to make space for new content.

- **`tsbLogLevel`**: TSB logging level (TRACE, WARN, MIL, ERROR) for debugging and monitoring TSB operations. Higher log levels provide more detailed information about TSB storage operations, fragment writes/reads, and capacity management.

## Summary

The TSB system provides comprehensive time-shifted playback capabilities:

- **Time-Shifted Playback**: Users can pause live content and resume from the paused position, or seek backward to review previously viewed segments. TSB maintains a rolling buffer window extending backward from the current live position, enabling seamless time-shift access without re-downloading content from CDN.

- **Catch-Up Viewing**: TSB enables catch-up viewing scenarios where users can access content that has already aired but is still within the TSB window. This extends beyond simple pause/resume to support full backward navigation through recent content history.

- **Local Storage**: Fragments are stored on local filesystem, providing fast access times compared to CDN downloads. Local storage reduces network bandwidth usage for time-shift operations and enables offline access to recently viewed content (within TSB window).

- **Seamless Integration**: TSB integrates seamlessly with AAMP's playback pipeline, automatically writing fragments during normal playback and reading from TSB when users seek backward. The system falls back to CDN downloads when fragments are not available in TSB, ensuring continuous playback regardless of TSB availability.
