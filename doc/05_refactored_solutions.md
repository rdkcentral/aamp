# Refactored Solutions & Optimizations

Proposed refactoring solutions with code examples

[← Back to Index](README.md) | [← Previous: Design Analysis](04_current_design_analysis.md) | [Next: Patch File →](06_patch_file.md)

## 1. Memory Management Refactoring

### 1.1 Smart Pointer Migration

**Problem:**

Raw pointer usage in `priv_aamp.cpp` lines 1302-1336

**Current Code:**

```cpp
// priv_aamp.cpp - Current problematic code
mAampCacheHandler = new AampCacheHandler(mPlayerId);
mEventManager = new AampEventManager(mPlayerId);
mCMCDCollector = new AampCMCDCollector();
mDRMLicenseManager = new AampDRMLicenseManager(maxDrmSession, this);

// In destructor:
delete mAampCacheHandler;
delete mEventManager;
delete mCMCDCollector;
delete mDRMLicenseManager;
```

**Refactored Solution:**

```cpp
// priv_aamp.h - Header changes
class PrivateInstanceAAMP {
private:
    std::unique_ptr<AampCacheHandler> mAampCacheHandler;
    std::unique_ptr<AampEventManager> mEventManager;
    std::unique_ptr<AampCMCDCollector> mCMCDCollector;
    std::unique_ptr<AampDRMLicenseManager> mDRMLicenseManager;
    // ... other members
};

// priv_aamp.cpp - Implementation
PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig* config)
    : mConfig(config)
    , mAampCacheHandler(std::make_unique<AampCacheHandler>(mPlayerId))
    , mEventManager(std::make_unique<AampEventManager>(mPlayerId))
    , mCMCDCollector(std::make_unique<AampCMCDCollector>())
    , mDRMLicenseManager(std::make_unique<AampDRMLicenseManager>(
          maxDrmSession, this))
{
    // No manual cleanup needed - automatic via destructor
}

// Destructor becomes simpler - no explicit deletes needed
PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
    // Smart pointers automatically clean up
    // Just need to stop threads, close connections, etc.
}
```

**Benefits:**

- Automatic memory management
- Exception-safe
- No memory leaks
- Clearer ownership semantics

### 1.2 RAII for CURL Handles

**Problem:**

C-style resource management for curl handles

**Refactored Solution:**

```cpp
// New file: AampCurlHandle.h
class AampCurlHandle {
public:
    AampCurlHandle() : mCurl(curl_easy_init()) {
        if (!mCurl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }
    
    ~AampCurlHandle() {
        if (mCurl) {
            curl_easy_cleanup(mCurl);
        }
    }
    
    // Non-copyable
    AampCurlHandle(const AampCurlHandle&) = delete;
    AampCurlHandle& operator=(const AampCurlHandle&) = delete;
    
    // Movable
    AampCurlHandle(AampCurlHandle&& other) noexcept 
        : mCurl(other.mCurl) {
        other.mCurl = nullptr;
    }
    
    AampCurlHandle& operator=(AampCurlHandle&& other) noexcept {
        if (this != &other) {
            if (mCurl) curl_easy_cleanup(mCurl);
            mCurl = other.mCurl;
            other.mCurl = nullptr;
        }
        return *this;
    }
    
    CURL* get() const { return mCurl; }
    CURL* operator->() const { return mCurl; }
    
private:
    CURL* mCurl;
};

// Usage in priv_aamp.cpp
class PrivateInstanceAAMP {
private:
    std::vector<std::unique_ptr<AampCurlHandle>> mCurlHandles;
};
```

## 2. Thread Safety Improvements

### 2.1 Eliminating Recursive Mutexes

**Problem:**

Use of recursive mutexes indicates design issues

**Refactored Solution:**

```cpp
// Instead of recursive mutex, use separate methods for internal/external calls
class PrivateInstanceAAMP {
private:
    std::mutex mLock;  // Regular mutex instead of recursive
    
    // Internal method (assumes lock already held)
    void TuneInternalLocked(const char* url, bool autoPlay);
    
public:
    // Public method (acquires lock)
    void Tune(const char* url, bool autoPlay) {
        std::lock_guard<std::mutex> lock(mLock);
        TuneInternalLocked(url, autoPlay);
    }
};

// Or use a lock guard helper
class LockGuardHelper {
public:
    explicit LockGuardHelper(std::mutex& mtx) : mLock(mtx) {}
    void ensureLocked() {
        if (!mLock.owns_lock()) {
            mLock.lock();
        }
    }
private:
    std::unique_lock<std::mutex> mLock;
};
```

### 2.2 Lock Ordering Strategy

**Refactored Solution:**

```cpp
// Define lock ordering constants
namespace LockOrder {
    constexpr int CONFIG = 1;
    constexpr int STREAM = 2;
    constexpr int CACHE = 3;
    constexpr int EVENT = 4;
}

// Use std::lock for multiple locks
void SomeMethod() {
    std::lock(mConfigLock, mStreamLock, mCacheLock);
    std::lock_guard<std::mutex> lock1(mConfigLock, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(mStreamLock, std::adopt_lock);
    std::lock_guard<std::mutex> lock3(mCacheLock, std::adopt_lock);
    
    // Critical section
}
```

## 3. Class Decomposition

### 3.1 Extracting Configuration Manager

**Refactored Solution:**

```cpp
// New file: ConfigManager.h
class ConfigManager {
public:
    explicit ConfigManager(AampConfig* config) : mConfig(config) {}
    
    void LoadTuneSettings(const std::string& manifestUrl);
    void ApplyChannelOverrides(const std::string& url);
    void ResetForNewTune();
    
private:
    AampConfig* mConfig;
};

// Usage in PrivateInstanceAAMP
class PrivateInstanceAAMP {
private:
    std::unique_ptr<ConfigManager> mConfigManager;
    
public:
    void Tune(const char* url, ...) {
        mConfigManager->LoadTuneSettings(url);
        // ... rest of tune logic
    }
};
```

### 3.2 Stream Factory Pattern

**Refactored Solution:**

```cpp
// New file: StreamFactory.h
class StreamFactory {
public:
    static std::unique_ptr<StreamAbstractionAAMP> Create(
        PrivateInstanceAAMP* aamp,
        const std::string& url,
        double seekPos,
        float rate) {
        
        MediaFormatType format = GetMediaFormatType(url);
        
        switch (format) {
            case eMEDIAFORMAT_HLS:
                return std::make_unique<StreamAbstractionAAMP_HLS>(
                    aamp, seekPos, rate);
            case eMEDIAFORMAT_DASH:
                return std::make_unique<StreamAbstractionAAMP_MPD>(
                    aamp, seekPos, rate);
            case eMEDIAFORMAT_PROGRESSIVE:
                return std::make_unique<StreamAbstractionAAMP_PROGRESSIVE>(
                    aamp, seekPos, rate);
            default:
                throw std::runtime_error("Unsupported media format");
        }
    }
};

// Usage
void PrivateInstanceAAMP::Tune(...) {
    mpStreamAbstractionAAMP = StreamFactory::Create(
        this, url, seekPos, rate);
}
```

## 4. Method Decomposition

### 4.1 Breaking Down Tune() Method

**Current Problem:**

`Tune()` method is 350+ lines

**Refactored Solution:**

```cpp
// priv_aamp.cpp - Refactored Tune method
void PrivateInstanceAAMP::Tune(
    const char* mainManifestUrl,
    bool autoPlay,
    const char* contentType,
    bool bFirstAttempt,
    bool bFinalAttempt,
    const char* pTraceID,
    bool audioDecoderStreamSync,
    const char* refreshManifestUrl,
    int mpdStitchingMode,
    std::string sid,
    const char* manifestData) {
    
    // Step 1: Initialize tune session
    InitializeTuneSession(mainManifestUrl, sid, manifestData);
    
    // Step 2: Apply configuration
    mConfigManager->LoadTuneSettings(mainManifestUrl);
    
    // Step 3: Determine media format
    mMediaFormat = GetMediaFormatType(mainManifestUrl);
    
    // Step 4: Create stream abstraction
    CreateStreamAbstraction(mainManifestUrl, seek_pos_seconds, rate);
    
    // Step 5: Initialize playback
    if (autoPlay) {
        ActivatePlayer();
    }
    
    // Step 6: Start downloads
    ResumeDownloads();
}

private:
void PrivateInstanceAAMP::InitializeTuneSession(
    const std::string& url,
    const std::string& sid,
    const char* manifestData) {
    
    mEventManager->SetPlayerState(eSTATE_IDLE);
    SetSessionId(std::move(sid));
    
    if (manifestData) {
        mProvidedManifestFile = manifestData;
    }
    
    // Reset state variables
    mCurrentAudioTrackIndex = -1;
    mCurrentTextTrackIndex = -1;
    mManifestRefreshCount = 0;
}

void PrivateInstanceAAMP::CreateStreamAbstraction(
    const std::string& url,
    double seekPos,
    float rate) {
    
    mpStreamAbstractionAAMP = StreamFactory::Create(
        this, url, seekPos, rate);
    
    if (!mpStreamAbstractionAAMP) {
        throw std::runtime_error("Failed to create stream abstraction");
    }
}
```

## 5. Parameter Object Pattern

### 5.1 Tune Parameters

**Refactored Solution:**

```cpp
// New file: TuneParameters.h
struct TuneParameters {
    std::string manifestUrl;
    bool autoPlay = true;
    std::string contentType;
    bool firstAttempt = true;
    bool finalAttempt = false;
    std::string traceUUID;
    bool audioDecoderStreamSync = true;
    std::string refreshManifestUrl;
    int mpdStitchingMode = 0;
    std::string sessionId;
    std::string manifestData;
    
    // Builder pattern for easy construction
    class Builder {
    public:
        Builder& setUrl(const std::string& url) {
            params.manifestUrl = url;
            return *this;
        }
        
        Builder& setAutoPlay(bool play) {
            params.autoPlay = play;
            return *this;
        }
        
        // ... other setters
        
        TuneParameters build() const {
            return params;
        }
        
    private:
        TuneParameters params;
    };
};

// Usage
void PlayerInstanceAAMP::Tune(const TuneParameters& params) {
    // Much cleaner API
    aamp->Tune(params);
}

// Or with builder:
player->Tune(TuneParameters::Builder()
    .setUrl("http://example.com/manifest.mpd")
    .setAutoPlay(true)
    .build());
```

## 6. Error Handling Improvements

### 6.1 Custom Exception Types

**Refactored Solution:**

```cpp
// New file: AampExceptions.h
namespace aamp {

class AampException : public std::runtime_error {
public:
    explicit AampException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class NetworkException : public AampException {
public:
    explicit NetworkException(const std::string& msg, int errorCode)
        : AampException(msg), mErrorCode(errorCode) {}
    int getErrorCode() const { return mErrorCode; }
private:
    int mErrorCode;
};

class DRMException : public AampException {
public:
    explicit DRMException(const std::string& msg, DRMSystems system)
        : AampException(msg), mDRMSystem(system) {}
    DRMSystems getDRMSystem() const { return mDRMSystem; }
private:
    DRMSystems mDRMSystem;
};

} // namespace aamp

// Usage
void PrivateInstanceAAMP::DownloadManifest(const std::string& url) {
    try {
        // Download logic
    } catch (const NetworkException& e) {
        AAMPLOG_ERR("Network error: %s (code: %d)", 
                   e.what(), e.getErrorCode());
        throw;
    }
}
```

## 7. Performance Optimizations

### 7.1 Move Semantics

**Refactored Solution:**

```cpp
// Use move semantics for large objects
void ProcessFragment(std::vector<uint8_t>&& data) {
    // Takes ownership, no copy
    mFragmentBuffer = std::move(data);
}

// Use const references for read-only access
void ProcessFragment(const std::vector<uint8_t>& data) {
    // Read-only access, no copy
    size_t size = data.size();
}
```

### 7.2 Lock-Free Data Structures

**Refactored Solution:**

```cpp
// Use atomic for simple state flags
class PrivateInstanceAAMP {
private:
    std::atomic<bool> mDownloadsEnabled{false};
    std::atomic<AAMPPlayerState> mState{eSTATE_IDLE};
    
public:
    bool DownloadsAreEnabled() const {
        return mDownloadsEnabled.load(std::memory_order_acquire);
    }
    
    void SetDownloadsEnabled(bool enabled) {
        mDownloadsEnabled.store(enabled, std::memory_order_release);
    }
};
```

## 8. Summary of Refactoring Benefits

| Area | Improvement | Impact |
|------|-------------|--------|
| Memory Management | Smart pointers | Eliminates memory leaks, exception-safe |
| Thread Safety | Eliminate recursive mutexes | Reduces deadlock risk, better performance |
| Code Organization | Class decomposition | Easier to test, maintain, and understand |
| Method Complexity | Break down large methods | Improved readability and testability |
| Error Handling | Exception-based | Consistent error propagation |
| Performance | Move semantics, lock-free | Reduced overhead, better concurrency |

---

[← Back to Index](README.md) | [← Previous: Design Analysis](04_current_design_analysis.md) | [Next: Patch File →](06_patch_file.md)

