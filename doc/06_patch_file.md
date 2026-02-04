# C++11 Refactored Code Patch

Actual refactored code following C++11 standards and best practices

[← Back to Index](README.md) | [← Previous: Refactored Solutions](05_refactored_solutions.md)

## 1. Patch File Instructions

**To apply this patch:**

1. Review each change carefully
2. Test in a development environment first
3. Apply changes incrementally
4. Run comprehensive tests after each change
5. Ensure backward compatibility

## 2. Memory Management Refactoring

### 2.1 priv_aamp.h - Smart Pointer Migration

```diff
--- a/priv_aamp.h
+++ b/priv_aamp.h
@@ -XXX,XXX +XXX,XXX @@
 class PrivateInstanceAAMP : public DrmCallbacks, public std::enable_shared_from_this<PrivateInstanceAAMP>
 {
 private:
-    AampCacheHandler* mAampCacheHandler;
-    AampEventManager* mEventManager;
-    AampCMCDCollector* mCMCDCollector;
-    AampDRMLicenseManager* mDRMLicenseManager;
+    std::unique_ptr<AampCacheHandler> mAampCacheHandler;
+    std::unique_ptr<AampEventManager> mEventManager;
+    std::unique_ptr<AampCMCDCollector> mCMCDCollector;
+    std::unique_ptr<AampDRMLicenseManager> mDRMLicenseManager;
     
     // ... other members
 };
```

### 2.2 priv_aamp.cpp - Constructor Changes

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
 PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig* config)
     : mConfig(config)
     , mPlayerId(GetNextPlayerId())
-    , mAampCacheHandler(nullptr)
-    , mEventManager(nullptr)
-    , mCMCDCollector(nullptr)
-    , mDRMLicenseManager(nullptr)
+    , mAampCacheHandler(std::make_unique<AampCacheHandler>(mPlayerId))
+    , mEventManager(std::make_unique<AampEventManager>(mPlayerId))
+    , mCMCDCollector(std::make_unique<AampCMCDCollector>())
+    , mDRMLicenseManager(std::make_unique<AampDRMLicenseManager>(
+          GETCONFIGVALUE_PRIV(eAAMPConfig_DashMaxDrmSessions), this))
 {
-    mAampCacheHandler = new AampCacheHandler(mPlayerId);
-    mEventManager = new AampEventManager(mPlayerId);
-    mCMCDCollector = new AampCMCDCollector();
-    mDRMLicenseManager = new AampDRMLicenseManager(
-        GETCONFIGVALUE_PRIV(eAAMPConfig_DashMaxDrmSessions), this);
-    
     // ... rest of constructor
 }
```

### 2.3 priv_aamp.cpp - Destructor Changes

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
 PrivateInstanceAAMP::~PrivateInstanceAAMP()
 {
     // Stop all operations
     Stop(false);
     
-    // Manual cleanup no longer needed - smart pointers handle it
-    SAFE_DELETE(mAampCacheHandler);
-    SAFE_DELETE(mEventManager);
-    SAFE_DELETE(mCMCDCollector);
-    SAFE_DELETE(mDRMLicenseManager);
-    
     // ... other cleanup
 }
```

### 2.4 priv_aamp.cpp - Usage Updates

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
 void PrivateInstanceAAMP::SomeMethod()
 {
-    if (mAampCacheHandler) {
-        mAampCacheHandler->DoSomething();
+    if (mAampCacheHandler) {
+        mAampCacheHandler->DoSomething();
     }
     
-    mEventManager->SendEvent(event);
+    mEventManager->SendEvent(event);
     
     // Smart pointers provide automatic null checking and cleanup
 }
```

## 3. Thread Safety Improvements

### 3.1 priv_aamp.h - Mutex Type Changes

```diff
--- a/priv_aamp.h
+++ b/priv_aamp.h
@@ -XXX,XXX +XXX,XXX @@
 class PrivateInstanceAAMP {
 private:
-    mutable std::recursive_mutex mLock;
-    mutable std::recursive_mutex mStreamLock;
-    mutable std::recursive_mutex mFragmentCachingLock;
+    mutable std::mutex mLock;
+    mutable std::mutex mStreamLock;
+    mutable std::mutex mFragmentCachingLock;
     
     std::mutex mPausePositionMonitorMutex;
     std::mutex mDiscoCompleteLock;
     std::mutex mAdEventQMtx;
 };
```

### 3.2 priv_aamp.cpp - Lock Scope Reduction

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
 void PrivateInstanceAAMP::Tune(const char* url, ...)
 {
-    std::unique_lock<std::recursive_mutex> lock(mLock);
-    
-    // 300+ lines of code in critical section
-    // ... all tune logic ...
+    // Break into smaller methods with fine-grained locking
+    InitializeTuneSession(url, sid, manifestData);
+    
+    {
+        std::lock_guard<std::mutex> lock(mLock);
+        mConfigManager->LoadTuneSettings(url);
+    }
+    
+    {
+        std::lock_guard<std::mutex> lock(mStreamLock);
+        CreateStreamAbstraction(url, seekPos, rate);
+    }
+    
+    // ... rest of tune logic with minimal locking
 }
```

## 4. Class Decomposition

### 4.1 New File: ConfigManager.h

```cpp
// New file: ConfigManager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "AampConfig.h"
#include <string>
#include <memory>

class ConfigManager {
public:
    explicit ConfigManager(AampConfig* config);
    
    void LoadTuneSettings(const std::string& manifestUrl);
    void ApplyChannelOverrides(const std::string& url);
    void ResetForNewTune();
    void UpdatePreferredLanguages();
    
private:
    AampConfig* mConfig;
    
    void LoadAudioPreferences();
    void LoadSubtitlePreferences();
    void ApplyTimeoutSettings();
};

#endif // CONFIG_MANAGER_H
```

### 4.2 New File: ConfigManager.cpp

```cpp
// New file: ConfigManager.cpp
#include "ConfigManager.h"
#include "priv_aamp.h"

ConfigManager::ConfigManager(AampConfig* config)
    : mConfig(config)
{
}

void ConfigManager::LoadTuneSettings(const std::string& manifestUrl)
{
    // Extract configuration loading logic from Tune()
    seek_pos_seconds = GETCONFIGVALUE_PRIV(eAAMPConfig_PlaybackOffset);
    preferredRenditionString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioRendition);
    preferredCodecString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioCodec);
    preferredLanguagesString = GETCONFIGVALUE_PRIV(eAAMPConfig_PreferredAudioLanguage);
    // ... more config loading
}

void ConfigManager::ApplyChannelOverrides(const std::string& url)
{
    const char* remapUrl = mConfig->GetChannelOverride(url.c_str());
    if (remapUrl) {
        // Apply override
    }
}
```

### 4.3 New File: StreamFactory.h

```cpp
// New file: StreamFactory.h
#ifndef STREAM_FACTORY_H
#define STREAM_FACTORY_H

#include "StreamAbstractionAAMP.h"
#include "priv_aamp.h"
#include <memory>
#include <string>

class StreamFactory {
public:
    static std::unique_ptr<StreamAbstractionAAMP> Create(
        PrivateInstanceAAMP* aamp,
        const std::string& url,
        double seekPos,
        float rate,
        id3_callback_t id3Handler = nullptr);
        
private:
    static MediaFormatType GetMediaFormatType(const std::string& url);
};

#endif // STREAM_FACTORY_H
```

### 4.4 New File: StreamFactory.cpp

```cpp
// New file: StreamFactory.cpp
#include "StreamFactory.h"
#include "fragmentcollector_hls.h"
#include "fragmentcollector_mpd.h"
#include "fragmentcollector_progressive.h"
#include "AampUtils.h"

std::unique_ptr<StreamAbstractionAAMP> StreamFactory::Create(
    PrivateInstanceAAMP* aamp,
    const std::string& url,
    double seekPos,
    float rate,
    id3_callback_t id3Handler)
{
    MediaFormatType format = GetMediaFormatType(url);
    
    switch (format) {
        case eMEDIAFORMAT_HLS:
            return std::make_unique<StreamAbstractionAAMP_HLS>(
                aamp, seekPos, rate);
                
        case eMEDIAFORMAT_DASH:
            return std::make_unique<StreamAbstractionAAMP_MPD>(
                aamp, seekPos, rate, id3Handler);
                
        case eMEDIAFORMAT_PROGRESSIVE:
            return std::make_unique<StreamAbstractionAAMP_PROGRESSIVE>(
                aamp, seekPos, rate);
                
        default:
            AAMPLOG_ERR("Unsupported media format: %d", format);
            return nullptr;
    }
}

MediaFormatType StreamFactory::GetMediaFormatType(const std::string& url)
{
    return aamp_utils::GetMediaFormatType(url);
}
```

### 4.5 priv_aamp.cpp - Use StreamFactory

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
 #include "priv_aamp.h"
+#include "StreamFactory.h"
+#include "ConfigManager.h"
 
 PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig* config)
     : mConfig(config)
+    , mConfigManager(std::make_unique<ConfigManager>(config))
 {
     // ... initialization
 }
 
 void PrivateInstanceAAMP::Tune(const char* url, ...)
 {
-    // 350+ lines of code
-    // ... determine format ...
-    if (mMediaFormat == eMEDIAFORMAT_DASH) {
-        mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(...);
-    } else if (mMediaFormat == eMEDIAFORMAT_HLS) {
-        mpStreamAbstractionAAMP = new StreamAbstractionAAMP_HLS(...);
-    } else if (mMediaFormat == eMEDIAFORMAT_PROGRESSIVE) {
-        mpStreamAbstractionAAMP = new StreamAbstractionAAMP_PROGRESSIVE(...);
-    }
+    // Simplified tune logic
+    InitializeTuneSession(url, sid, manifestData);
+    mConfigManager->LoadTuneSettings(url);
+    
+    mMediaFormat = GetMediaFormatType(url);
+    mpStreamAbstractionAAMP = StreamFactory::Create(
+        this, url, seek_pos_seconds, rate, nullptr);
+    
+    if (!mpStreamAbstractionAAMP) {
+        AAMPLOG_ERR("Failed to create stream abstraction");
+        return;
+    }
+    
+    // ... rest of tune logic
 }
```

## 5. Method Decomposition

### 5.1 priv_aamp.cpp - Extract Tune Helper Methods

```diff
--- a/priv_aamp.cpp
+++ b/priv_aamp.cpp
@@ -XXX,XXX +XXX,XXX @@
+void PrivateInstanceAAMP::InitializeTuneSession(
+    const std::string& url,
+    const std::string& sid,
+    const char* manifestData)
+{
+    mEventManager->SetPlayerState(eSTATE_IDLE);
+    SetSessionId(std::move(sid));
+    
+    if (manifestData) {
+        mProvidedManifestFile = manifestData;
+    }
+    
+    // Reset state
+    mCurrentAudioTrackIndex = -1;
+    mCurrentTextTrackIndex = -1;
+    mManifestRefreshCount = 0;
+    mProgramDateTime = 0;
+    mMPDPeriodsInfo.clear();
+}
+
+void PrivateInstanceAAMP::CreateStreamAbstraction(
+    const std::string& url,
+    double seekPos,
+    float rate)
+{
+    mpStreamAbstractionAAMP = StreamFactory::Create(
+        this, url, seekPos, rate);
+    
+    if (!mpStreamAbstractionAAMP) {
+        throw std::runtime_error("Failed to create stream abstraction");
+    }
+}
+
 void PrivateInstanceAAMP::Tune(const char* url, ...)
 {
-    // 350+ lines of initialization code
+    InitializeTuneSession(url, sid, manifestData);
+    mConfigManager->LoadTuneSettings(url);
+    CreateStreamAbstraction(url, seek_pos_seconds, rate);
+    // ... simplified tune logic
 }
```

## 6. Error Handling Improvements

### 6.1 New File: AampExceptions.h

```cpp
// New file: AampExceptions.h
#ifndef AAMP_EXCEPTIONS_H
#define AAMP_EXCEPTIONS_H

#include <stdexcept>
#include <string>
#include "DrmSystems.h"

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

#endif // AAMP_EXCEPTIONS_H
```

## 7. CMakeLists.txt Updates

### 7.1 Add New Source Files

```diff
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -XXX,XXX +XXX,XXX @@
 set(AAMP_SOURCES
     main_aamp.cpp
     priv_aamp.cpp
+    ConfigManager.cpp
+    StreamFactory.cpp
     # ... existing sources
 )
 
 set(AAMP_HEADERS
     main_aamp.h
     priv_aamp.h
+    ConfigManager.h
+    StreamFactory.h
+    AampExceptions.h
     # ... existing headers
 )
```

## 8. Testing Considerations

**After applying these patches, ensure:**

1. All unit tests pass
2. Integration tests verify functionality
3. Memory leak detection (valgrind, sanitizers)
4. Thread safety verification
5. Performance benchmarks maintained
6. Backward compatibility with existing applications

## 9. Migration Checklist

| Step | Description | Status |
|------|-------------|--------|
| 1 | Create new header files (ConfigManager.h, StreamFactory.h, etc.) | □ |
| 2 | Implement new classes | □ |
| 3 | Update priv_aamp.h to use smart pointers | □ |
| 4 | Update priv_aamp.cpp constructor/destructor | □ |
| 5 | Refactor Tune() method | □ |
| 6 | Update all usages of raw pointers | □ |
| 7 | Replace recursive mutexes with regular mutexes | □ |
| 8 | Update CMakeLists.txt | □ |
| 9 | Run comprehensive tests | □ |
| 10 | Code review and documentation update | □ |

---

[← Back to Index](README.md) | [← Previous: Refactored Solutions](05_refactored_solutions.md)

