# Important APIs and Classes

Public APIs, key classes, and usage examples

[← Back to Index](README.md) | [← Previous: Code Organization](02_code_organization.md) | [Next: Design Analysis →](04_current_design_analysis.md)

## 1. Public API - PlayerInstanceAAMP

### 1.1 Construction

```cpp
// Create a player instance
PlayerInstanceAAMP* player = new PlayerInstanceAAMP();

// With custom stream sink
StreamSink* customSink = new CustomStreamSink();
PlayerInstanceAAMP* player = new PlayerInstanceAAMP(customSink);

// With frame export callback
auto exportFrames = [](const unsigned char* data, int width, int height, int format) {
    // Process video frames
};
PlayerInstanceAAMP* player = new PlayerInstanceAAMP(nullptr, exportFrames);
```

### 1.2 Tune API

```cpp
// Basic tune
player->Tune("http://example.com/manifest.mpd");

// Tune with options
player->Tune(
    "http://example.com/manifest.mpd",  // URL
    true,                                // autoPlay
    "application/dash+xml",              // contentType
    true,                                // firstAttempt
    false,                               // finalAttempt
    "trace-uuid-123",                    // traceUUID
    true,                                // audioDecoderStreamSync
    nullptr,                             // refreshManifestUrl
    0,                                   // mpdStitchingMode
    "session-id",                        // sessionId
    nullptr                              // manifestData
);
```

### 1.3 Playback Control

```cpp
// Stop playback
player->Stop();

// Seek to position (seconds)
player->Seek(30.0);  // Seek to 30 seconds

// Seek to live
player->SeekToLive();

// Set playback rate
player->SetRate(2.0);  // 2x speed

// Pause/Resume
player->SetRate(0.0);  // Pause
player->SetRate(1.0);  // Resume

// Set trick play speed
player->SetPlaybackSpeed(8.0);  // 8x fast forward
```

### 1.4 Audio/Subtitle Control

```cpp
// Set audio language
player->SetLanguage("eng");

// Set audio track by index
player->SetAudioTrack(2);

// Set subtitle track
player->SetTextTrack(1);

// Enable/disable subtitles
player->SetCCStatus(true);

// Set subtitle language
player->SetPreferredSubtitleLanguage("eng");
```

### 1.5 Event Registration

```cpp
// Create event listener
class MyEventListener : public EventListener {
public:
    void EventReceived(const AAMPEventPtr& event) override {
        switch(event->getType()) {
            case AAMP_EVENT_STATE_CHANGED:
                // Handle state change
                break;
            case AAMP_EVENT_PROGRESS:
                // Handle progress
                break;
            // ... other events
        }
    }
};

// Register listener
MyEventListener* listener = new MyEventListener();
player->RegisterEvents(listener);

// Register for specific event
player->RegisterEvent(AAMP_EVENT_BITRATE_CHANGED, listener);
```

### 1.6 Configuration

```cpp
// Set initial bitrate
player->SetInitialBitrate(5000000);  // 5 Mbps

// Set network timeout
player->SetNetworkTimeout(10.0);  // 10 seconds

// Set license server URL
player->SetLicenseServerURL("https://license.example.com", eDRM_WIDEVINE);

// Add custom HTTP headers
std::map<std::string, std::string> headers;
headers["X-Custom-Header"] = "value";
player->AddPageHeaders(headers);

// Set preferred DRM
player->SetPreferredDRM(eDRM_WIDEVINE);
```

## 2. Core Classes

### 2.1 PrivateInstanceAAMP

**Location:** `priv_aamp.h/cpp`

**Purpose:** Core player implementation

```cpp
// Key methods (internal, but important to understand)
class PrivateInstanceAAMP {
public:
    void Tune(const char* url, bool autoPlay, ...);
    void Stop();
    void Seek(double position);
    void SetRate(float rate);
    bool GetFile(const std::string& url, AampMediaType type, ...);
    void SendEvent(const AAMPEventPtr& event);
    
private:
    AampConfig* mConfig;
    StreamAbstractionAAMP* mpStreamAbstractionAAMP;
    AampEventManager* mEventManager;
    AampDRMLicenseManager* mDRMLicenseManager;
    // ... many more members
};
```

### 2.2 StreamAbstractionAAMP

**Location:** `StreamAbstractionAAMP.h`

**Purpose:** Abstract interface for streaming protocols

```cpp
class StreamAbstractionAAMP {
public:
    virtual AAMPStatusType Init(TuneType tuneType) = 0;
    virtual void Start() = 0;
    virtual void Stop(bool clearChannelData) = 0;
    virtual MediaTrack* GetMediaTrack(TrackType type) = 0;
    virtual double GetStreamPosition() = 0;
    virtual std::vector<BitsPerSecond> GetVideoBitrates() = 0;
    virtual std::vector<BitsPerSecond> GetAudioBitrates() = 0;
    // ... more methods
};

// Implementations:
// - StreamAbstractionAAMP_HLS
// - StreamAbstractionAAMP_MPD
// - StreamAbstractionAAMP_Progressive
```

### 2.3 MediaTrack

**Location:** `StreamAbstractionAAMP.h`

**Purpose:** Represents a media track (video/audio/subtitle)

```cpp
class MediaTrack {
public:
    MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name);
    virtual ~MediaTrack();
    
    void StartInjectLoop();
    void StopInjectLoop();
    void PlaylistDownloader();
    void ProcessPlaylist(const std::string& playlist, int http_error);
    // ... more methods
    
protected:
    TrackType mType;
    PrivateInstanceAAMP* aamp;
    std::string mName;
    // ... more members
};
```

### 2.4 AAMPGstPlayer

**Location:** `aampgstplayer.h/cpp`

**Purpose:** GStreamer pipeline management

```cpp
class AAMPGstPlayer : public StreamSink {
public:
    AAMPGstPlayer(PrivateInstanceAAMP* aamp, 
                  id3_callback_t id3Handler,
                  std::function<void(const unsigned char*, int, int, int)> exportFrames);
    
    void Configure(StreamOutputFormat format, ...);
    bool SendCopy(AampMediaType type, const void* ptr, size_t len, ...);
    bool SendTransfer(AampMediaType type, void* ptr, size_t len, ...);
    void Stop(bool keepLastFrame);
    void Flush(double position, int rate, bool shouldTearDown);
    bool Pause(bool pause, bool forceStopGstreamerPreBuffering);
    long long GetPositionMilliseconds();
    // ... more methods
};
```

## 3. Event System

### 3.1 Event Types

```cpp
enum AAMPEventType {
    AAMP_EVENT_STATE_CHANGED,        // Player state change
    AAMP_EVENT_PROGRESS,              // Playback progress
    AAMP_EVENT_SPEED_CHANGED,         // Playback speed change
    AAMP_EVENT_BITRATE_CHANGED,      // Bitrate change
    AAMP_EVENT_MEDIA_METADATA,       // Media metadata
    AAMP_EVENT_TIMED_METADATA,        // Timed metadata (ads, etc.)
    AAMP_EVENT_TUNE_FAILED,          // Tune failure
    AAMP_EVENT_TUNED,                // Tune success
    AAMP_EVENT_SEEKED,                // Seek complete
    AAMP_EVENT_BUFFERING_CHANGED,     // Buffering state
    AAMP_EVENT_VIDEO_BITRATE_CHANGED, // Video bitrate change
    AAMP_EVENT_AUDIO_BITRATE_CHANGED, // Audio bitrate change
    AAMP_EVENT_PLAYLIST_INDEXED,      // Playlist indexed
    AAMP_EVENT_CC_HANDLE_RECEIVED,    // Closed caption handle
    AAMP_EVENT_BULK_TIMED_METADATA,   // Bulk timed metadata
    AAMP_EVENT_ANOMALY_REPORT,        // Anomaly report
    // ... more events
};
```

### 3.2 Event Listener Interface

```cpp
class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void EventReceived(const AAMPEventPtr& event) = 0;
};

// Example implementation
class MyEventListener : public EventListener {
public:
    void EventReceived(const AAMPEventPtr& event) override {
        AAMPEventType type = event->getType();
        
        switch(type) {
            case AAMP_EVENT_STATE_CHANGED: {
                StateChangedEventPtr stateEvent = 
                    std::dynamic_pointer_cast<StateChangedEvent>(event);
                AAMPPlayerState state = stateEvent->getState();
                // Handle state change
                break;
            }
            case AAMP_EVENT_PROGRESS: {
                ProgressEventPtr progressEvent = 
                    std::dynamic_pointer_cast<ProgressEvent>(event);
                double position = progressEvent->getPosition();
                double duration = progressEvent->getDuration();
                // Handle progress
                break;
            }
            // ... handle other events
        }
    }
};
```

## 4. Configuration System

### 4.1 AampConfig

**Location:** `AampConfig.h/cpp`

```cpp
class AampConfig {
public:
    void Initialize();
    void ReadAampCfgTxtFile();
    void ReadAampCfgJsonFile();
    void ReadAampCfgFromEnv();
    void ReadOperatorConfiguration();
    
    // Configuration access
    bool IsConfigSet(AAMPConfigSettings setting);
    long GetConfigValue(AAMPConfigSettings setting);
    void SetConfigValue(AAMPConfigOwner owner, 
                       AAMPConfigSettings setting, 
                       long value);
    
    // Configuration priority (lowest to highest):
    // 1. Default settings
    // 2. Operator configuration (RFC/ENV)
    // 3. Stream settings
    // 4. Application settings
    // 5. Dev configuration (/opt/aamp.cfg)
};
```

### 4.2 Common Configuration Settings

| Setting | Type | Description |
|---------|------|-------------|
| abr | bool | Enable/disable adaptive bitrate |
| networkTimeout | double | Network timeout in seconds |
| initialBitrate | long | Initial bitrate in bps |
| licenseServerUrl | string | DRM license server URL |
| preferredDrm | int | Preferred DRM system |

## 5. DRM System

### 5.1 AampDRMLicManager

```cpp
class AampDRMLicenseManager {
public:
    AampDRMLicenseManager(int maxSessions, PrivateInstanceAAMP* aamp);
    
    // License acquisition
    bool AcquireLicense(const std::string& url,
                       const std::vector<uint8_t>& initData,
                       DRMSystems drmSystem,
                       AampMediaType mediaType);
    
    // Key extraction
    bool ExtractKeyId(const std::vector<uint8_t>& initData,
                     std::vector<uint8_t>& keyId);
    
    // Session management
    void ReleaseSession(DRMSystems drmSystem);
    void ReleaseAllSessions();
};
```

## 6. Utility Classes

### 6.1 AampUtils

```cpp
namespace aamp_utils {
    // URL resolution
    void aamp_ResolveURL(std::string& resolvedUrl,
                         const std::string& baseUrl,
                         const std::string& relativeUrl);
    
    // Time utilities
    long long aamp_GetCurrentTimeMS();
    double aamp_GetCurrentTimeSeconds();
    
    // Media format detection
    MediaFormatType GetMediaFormatType(const std::string& url);
    
    // String utilities
    std::string aamp_ToLower(const std::string& str);
    std::vector<std::string> aamp_Split(const std::string& str, char delimiter);
}
```

---

[← Back to Index](README.md) | [← Previous: Code Organization](02_code_organization.md) | [Next: Design Analysis →](04_current_design_analysis.md)

