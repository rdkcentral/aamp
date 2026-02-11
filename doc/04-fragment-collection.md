# Fragment Collection System

## Table of Contents
1. [Overview](#overview)
2. [HLS Fragment Collector](#hls-fragment-collector)
3. [DASH Fragment Collector](#dash-fragment-collector)
4. [Progressive Fragment Collector](#progressive-fragment-collector)
5. [Common Base Functionality](#common-base-functionality)
6. [Fragment Lifecycle](#fragment-lifecycle)

## Overview

The Fragment Collection System is responsible for downloading, parsing, and managing media fragments for adaptive streaming. AAMP supports three protocols:

1. **HLS (HTTP Live Streaming)**: Apple's adaptive streaming protocol
2. **DASH (Dynamic Adaptive Streaming over HTTP)**: MPEG-DASH standard
3. **Progressive MP4**: Simple progressive download

All fragment collectors inherit from `StreamAbstractionAAMP`, which provides common functionality for fragment caching, injection, and track management.

## HLS Fragment Collector

### Architecture

**Files**: `fragmentcollector_hls.h/cpp`

**Class**: `StreamAbstractionAAMP_HLS`

### Key Components

#### TrackState
Represents a media track (video, audio, subtitle) in HLS:

```cpp
class TrackState : public MediaTrack
{
    // HLS-specific track state
    std::vector<IndexNode> indexNodeList;  // Fragment index
    std::vector<KeyTagStruct> keyTagList;  // DRM keys
    // ... track-specific data
};
```

#### HlsStreamInfo
Stream information from `#EXT-X-STREAM-INF`:

```cpp
struct HlsStreamInfo : public StreamInfo
{
    long program_id;
    std::string audio;
    std::string uri;
    BitsPerSecond averageBandwidth;
    StreamOutputFormat audioFormat;
};
```

#### MediaInfo
Media information from `#EXT-X-MEDIA`:

```cpp
struct MediaInfo
{
    AampMediaType type;
    std::string group_id;
    std::string name;
    std::string language;
    std::string uri;
    StreamOutputFormat audioFormat;
    bool isCC;  // Closed captions
};
```

#### IndexNode
Fragment index entry:

```cpp
struct IndexNode
{
    AampTime completionTimeSecondsFromStart;
    long long mediaSequenceNumber;
    lstring pFragmentInfo;  // Fragment URL
    int drmMetadataIdx;     // DRM key index
    lstring initFragmentPtr; // Init fragment (for fMP4)
};
```

### HLS Playlist Parsing

#### Master Playlist
Parses `#EXT-X-STREAM-INF` tags to identify available streams:

```cpp
void StreamAbstractionAAMP_HLS::ProcessMasterPlaylist(
    AampGrowableBuffer& playlistBuffer)
{
    // Parse #EXT-X-STREAM-INF
    // Extract stream info (bandwidth, resolution, codecs)
    // Build stream list
}
```

#### Media Playlist
Parses variant playlists to extract fragment URLs:

```cpp
void TrackState::ProcessPlaylist(
    AampGrowableBuffer& newPlaylist, int http_error)
{
    // Parse #EXTINF tags
    // Extract fragment URLs
    // Update indexNodeList
    // Handle #EXT-X-KEY for DRM
    // Handle #EXT-X-DISCONTINUITY
}
```

### Fragment URL Generation

HLS fragments are typically relative URLs that need to be resolved:

```cpp
std::string TrackState::GetFragmentUrl(int index)
{
    // Resolve relative URL against playlist base URL
    // Handle #EXT-X-BYTERANGE if present
    return resolvedUrl;
}
```

### Playlist Refresh

For live streams, playlists are periodically refreshed:

```cpp
void TrackState::PlaylistDownloader()
{
    while (!abortPlaylistDownloader)
    {
        // Download playlist
        // Parse updates
        // Add new fragments to indexNodeList
        // Wait for next refresh interval
    }
}
```

### Track Synchronization

HLS requires synchronization between audio and video tracks:

```cpp
void StreamAbstractionAAMP_HLS::SyncTracks()
{
    // Use sequence numbers or PDT (Program Date Time)
    // Align audio and video fragment indices
    // Handle discontinuities
}
```

### DRM Key Management

HLS uses `#EXT-X-KEY` tags for encryption:

```cpp
void TrackState::ProcessKeyTag(const std::string& keyTag)
{
    // Extract key URI, IV, method
    // Store in keyTagList
    // Associate with fragments
}
```

## DASH Fragment Collector

### Architecture

**Files**: `fragmentcollector_mpd.h/cpp`

**Class**: `StreamAbstractionAAMP_MPD`

### Key Components

#### MediaStreamContext
Represents a DASH stream (adaptation set + representation):

```cpp
class MediaStreamContext : public MediaTrack
{
    // DASH-specific context
    IPeriod* period;
    IAdaptationSet* adaptationSet;
    IRepresentation* representation;
    FragmentDescriptor fragmentDescriptor;
    // ... stream-specific data
};
```

#### FragmentDescriptor
DASH fragment information:

```cpp
struct FragmentDescriptor
{
    uint64_t Number;      // Segment number
    uint64_t Time;        // Presentation time
    uint32_t TimeScale;   // Time scale
    BitsPerSecond Bandwidth;
    // ... fragment metadata
};
```

### MPD Parsing

Uses libdash library for MPD parsing:

```cpp
bool StreamAbstractionAAMP_MPD::Init(TuneType tuneType)
{
    // Download MPD
    IMPD* mpd = dashManager->Open(url);

    // Parse periods
    for (auto period : mpd->GetPeriods())
    {
        // Parse adaptation sets
        for (auto adaptSet : period->GetAdaptationSets())
        {
            // Parse representations
            // Build profile list
        }
    }
}
```

### Period Management

DASH content is organized into periods:

```cpp
void StreamAbstractionAAMP_MPD::ProcessPeriod(IPeriod* period)
{
    // Extract period start time
    // Process adaptation sets
    // Handle period transitions
    // Manage period-specific ABR
}
```

### Segment URL Generation

DASH segments can be generated using templates or explicit URLs:

```cpp
std::string MediaStreamContext::GetSegmentUrl(
    uint64_t segmentNumber)
{
    ISegmentURL* segmentUrl = segmentTemplate->GetMediaURL(
        representation, segmentNumber);
    return segmentUrl->GetUrl();
}
```

### Low-Latency DASH

Special handling for low-latency DASH (LL-DASH):

```cpp
void StreamAbstractionAAMP_MPD::ProcessLLDash()
{
    // Use chunked transfer encoding
    // Process partial segments
    // Handle availability time offset
    // Manage chunk timing
}
```