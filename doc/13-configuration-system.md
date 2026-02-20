# Configuration System

## Overview

AAMP uses a hierarchical configuration system with multiple sources and priority levels. Configuration can be set through code defaults, operator settings, stream settings, application settings, and developer configuration files.

## Configuration Priority

Configuration values are resolved in the following order (lowest to highest priority):

1. **Code Defaults**: Hard-coded default values in `AampConfig.cpp`
2. **Operator Settings**: RFC (Remote Framework Configuration) and environment variables
3. **Stream Settings**: Settings embedded in the manifest/playlist
4. **Application Settings**: Runtime API calls (e.g., `SetVideoBitrate()`)
5. **Developer Configuration**: `/opt/aamp.cfg` (text) or `/opt/aampcfg.json` (JSON)

## Configuration Files

### Text Configuration (`/opt/aamp.cfg`)

Simple key-value format:

```
abr=true
initialBitrate=2500000
networkTimeout=10.0
licenseServerUrl=https://license.example.com
```

### JSON Configuration (`/opt/aampcfg.json`)

Structured JSON format:

```json
{
  "abr": true,
  "initialBitrate": 2500000,
  "networkTimeout": 10.0,
  "licenseServerUrl": "https://license.example.com"
}
```

## Configuration Categories

### Boolean Switches

Enable/disable features:

- `abr`: Enable adaptive bitrate
- `fog`: Enable FOG (Fragment Orchestrator)
- `demuxHlsAudioTrack`: Demux audio from HLS TS
- `useNewABR`: Enable new buffer-based ABR
- `asyncTune`: Enable asynchronous tune API

### Integer Values

Numeric configuration:

- `initialBitrate`: Default bitrate (bps)
- `abrCacheLife`: ABR cache lifetime (ms)
- `downloadBuffer`: Fragment cache length
- `progressReportingInterval`: Progress event interval (seconds)

### String Values

Text configuration:

- `licenseServerUrl`: DRM license server URL
- `userAgent`: HTTP user agent string
- `networkProxy`: Network proxy address

### Double Values

Floating-point configuration:

- `networkTimeout`: Download timeout (seconds)
- `manifestTimeout`: Manifest download timeout (seconds)

## Key Configuration Parameters

See [CONFIGURATION.md](../CONFIGURATION.md) for complete list of all configuration parameters.

## Configuration API

### Setting Configuration

```cpp
// Via PlayerInstanceAAMP
player->SetVideoBitrate(5000000);
player->SetNetworkTimeout(15.0);
player->SetInitialBitrate(3000000);

// Via configuration file
// Set in /opt/aamp.cfg or /opt/aampcfg.json
```

### Getting Configuration

Configuration is read through `AampConfig` accessors (e.g. getter methods for each setting). The effective value reflects the current priority resolution. See **Configuration API Reference** below for programmatic access.

## Configuration Architecture

### Type-Safe Configuration Categories

AAMP configuration uses strongly-typed enums for different parameter types:

```cpp
// Boolean Configuration (140+ parameters)
typedef enum AAMPConfigSettingBool {
    eAAMPConfig_EnableABR,                    // Adaptive bitrate logic enable/disable
    eAAMPConfig_Fog,                          // Fragment Orchestrator Gateway enable
    eAAMPConfig_AsyncTune,                    // Asynchronous tune API
    eAAMPConfig_EnableLowLatencyDash,         // Low-latency DASH optimization
    eAAMPConfig_NativeCCRendering,            // Native closed captions
    eAAMPConfig_BoolMaxValue                  // Sentinel for array sizing
} AAMPConfigSettingBool;

// Integer Configuration (80+ parameters)
typedef enum AAMPConfigSettingInt {
    eAAMPConfig_ABRCacheLife,                 // ABR cache lifetime (seconds)
    eAAMPConfig_MaxFragmentCached,            // Fragment cache depth
    eAAMPConfig_BufferHealthMonitorInterval,  // Buffer monitoring frequency
    eAAMPConfig_PreferredDRM,                 // DRM system preference
    eAAMPConfig_MaxDASHDRMSessions,          // DASH DRM session pool size
    eAAMPConfig_IntMaxValue                   // Sentinel for array sizing
} AAMPConfigSettingInt;

// Float Configuration (20+ parameters)
typedef enum AAMPConfigSettingFloat {
    eAAMPConfig_NetworkTimeout,               // Download timeout (seconds)
    eAAMPConfig_ManifestTimeout,             // Manifest download timeout
    eAAMPConfig_LiveOffset,                  // Live edge offset
    eAAMPConfig_PlaybackOffset,              // Initial playback position
    eAAMPConfig_FloatMaxValue                // Sentinel for array sizing
} AAMPConfigSettingFloat;

// String Configuration (40+ parameters)
typedef enum AAMPConfigSettingString {
    eAAMPConfig_LicenseServerUrl,            // DRM license server URL
    eAAMPConfig_UserAgent,                   // HTTP User-Agent header
    eAAMPConfig_PreferredAudioLanguage,      // Audio language preferences
    eAAMPConfig_HarvestPath,                 // Debug harvest storage path
    eAAMPConfig_StringMaxValue               // Sentinel for array sizing
} AAMPConfigSettingString;
```

### Configuration Priority Hierarchy

Configuration values resolve with strict priority ordering (lowest to highest):

```cpp
enum ConfigPriority {
    AAMP_APPLICATION_SETTING = 0,          // Application API calls (highest priority)
    AAMP_JSON_CONFIG_SETTING,              // JSON configuration files
    AAMP_STREAM_SETTING,                   // Manifest/stream-embedded settings
    AAMP_OPERATOR_SETTING,                 // MSO/operator defaults via RFC
    AAMP_DEFAULT_SETTING                   // Compiled-in defaults (lowest priority)
};
```

**Resolution Logic**: Higher priority settings override lower ones. Application settings (via API calls) always take precedence.

## Configuration Sources & Formats

### 1. Text Configuration (`/opt/aamp.cfg`)
Simple key-value format for basic configuration:

```bash
# Boolean settings (true/false)
abr=true
fog=false
asyncTune=true
nativeCCRendering=false

# Integer settings
initialBitrate=2500000
maxFragmentCached=8
abrCacheLife=30

# Float settings
networkTimeout=10.0
manifestTimeout=15.0
liveOffset=30.0

# String settings
licenseServerUrl=https://license.example.com/v1/
userAgent=AAMP-Player/1.0 (RDK)
preferredAudioLanguage=en,es,fr
```

### 2. JSON Configuration (`/opt/aampcfg.json`)
Structured configuration with enhanced readability and validation:

```json
{
  "playback": {
    "abr": true,
    "asyncTune": true,
    "initialBitrate": 2500000,
    "networkTimeout": 10.0,
    "liveOffset": 30.0
  },
  "drm": {
    "licenseServerUrl": "https://license.example.com/v1/",
    "anonymousLicenseRequest": false,
    "licenseRetryWaitTime": 5000
  },
  "debug": {
    "harvestPath": "/tmp/aamp-harvest/",
    "infoLogging": true,
    "gstLogging": false,
    "curlLogging": false
  },
  "streaming": {
    "fog": false,
    "enableLowLatencyDash": true,
    "maxFragmentCached": 8,
    "bufferHealthMonitorInterval": 1000
  }
}
```

### 3. Runtime API Configuration
Programmatic configuration via PlayerInstanceAAMP methods:

```cpp
// Direct API calls (highest priority)
player->SetVideoBitrate(5000000);
player->SetNetworkTimeout(15.0);
player->SetInitialBitrate(3000000);
player->SetLicenseServerURL("https://drm.example.com/", eDRM_WideVine);

// Advanced configuration via AampConfig
AampConfig &config = player->mConfig;
config.SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_EnableABR, true);
config.SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AsyncTune, true);
config.SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_LiveOffset, 20.0);
```

## Key Configuration Categories

### Core Playback Parameters

#### Adaptive Bitrate (ABR) Configuration
```cpp
// ABR Enable/Disable & Strategy
eAAMPConfig_EnableABR                    // Master ABR switch
eAAMPConfig_UseNewABR                    // Buffer-based ABR algorithm
eAAMPConfig_ABRCacheLife                 // Bandwidth measurement window (30s default)
eAAMPConfig_ABRThresholdSize             // Profile switching threshold (bytes)
eAAMPConfig_ABRSkipDuration             // Startup ABR skip period (seconds)

// Profile Selection & Constraints
eAAMPConfig_DefaultBitrate              // Initial bitrate selection (2.5Mbps default)
eAAMPConfig_DefaultBitrate4K            // 4K content initial bitrate (8Mbps default)
eAAMPConfig_MinBitrate                  // Minimum allowable profile bitrate
eAAMPConfig_MaxBitrate                  // Maximum allowable profile bitrate
eAAMPConfig_PersistentBitRateOverSeek   // Maintain profile during seek operations
```

#### Network & Download Configuration
```cpp
// Timeout Configuration
eAAMPConfig_NetworkTimeout              // Fragment download timeout (10s default)
eAAMPConfig_ManifestTimeout            // Manifest download timeout (15s default)
eAAMPConfig_PlaylistTimeout            // HLS playlist timeout (10s default)

// Connection Management
eAAMPConfig_Curl_ConnectTimeout        // TCP connection timeout (5s default)
eAAMPConfig_CurlStallTimeout          // Download stall detection (30s default)
eAAMPConfig_EnableSharedSSLSession     // SSL session reuse optimization
eAAMPConfig_MaxCurlSockStore          // Curl socket pool size (16 default)

// Retry & Error Handling
eAAMPConfig_RampDownLimit             // Fragment failure retry count
eAAMPConfig_InitRampDownLimit         // Initial manifest retry count
eAAMPConfig_Http5XXRetryWaitInterval  // 5xx error retry delay (ms)
```

### DRM & Security Configuration
```cpp
// Supported DRM Systems
eAAMPConfig_PreferredDRM              // DRM preference: PlayReady/Widevine/ClearKey
eAAMPConfig_LicenseServerUrl          // Universal license server URL
eAAMPConfig_PRLicenseServerUrl        // PlayReady-specific license URL
eAAMPConfig_WVLicenseServerUrl        // Widevine-specific license URL
eAAMPConfig_CKLicenseServerUrl        // ClearKey license URL (testing)

// License Management
eAAMPConfig_AnonymousLicenseRequest   // License acquisition without auth token
eAAMPConfig_LicenseRetryWaitTime     // License failure retry interval (5s default)
eAAMPConfig_DrmNetworkTimeout        // DRM request timeout (30s default)
eAAMPConfig_SetLicenseCaching        // License response caching
eAAMPConfig_MaxDASHDRMSessions       // DASH DRM session pool (4 default)

// Security Features
eAAMPConfig_SslVerifyPeer            // SSL certificate verification
eAAMPConfig_TLSVersion              // Minimum TLS version (1.2 default)
```

### Buffer Management Configuration
```cpp
// Fragment Caching Strategy
eAAMPConfig_MaxFragmentCached         // Per-track fragment cache depth (8 default)
eAAMPConfig_InitialBuffer            // Startup buffer target (seconds)
eAAMPConfig_PlaybackBuffer           // Steady-state buffer target (seconds)
eAAMPConfig_PrePlayBufferCount       // Segments before playback start (3 default)

// Buffer Health Monitoring
eAAMPConfig_BufferHealthMonitorDelay     // Monitor startup delay (5s default)
eAAMPConfig_BufferHealthMonitorInterval  // Monitor check frequency (1s default)
eAAMPConfig_MinABRNWBufferRampDown      // ABR ramp-down buffer threshold
eAAMPConfig_MaxABRNWBufferRampUp        // ABR ramp-up buffer threshold

// GStreamer Buffer Configuration
eAAMPConfig_GstVideoBufBytes         // GStreamer video buffer size (bytes)
eAAMPConfig_GstAudioBufBytes         // GStreamer audio buffer size (bytes)
```

### Live Streaming & Low-Latency Configuration
```cpp
// Live Edge Management
eAAMPConfig_LiveOffset               // General live edge offset (30s default)
eAAMPConfig_LiveOffset4K             // 4K-specific live offset (40s default)
eAAMPConfig_CDVRLiveOffset           // Cloud DVR live offset (60s default)

// Low-Latency DASH (LLD)
eAAMPConfig_EnableLowLatencyDash     // LLD feature enable
eAAMPConfig_LLMinLatency            // Minimum latency target (ms)
eAAMPConfig_LLTargetLatency         // Target latency (3s default)
eAAMPConfig_LLMaxLatency           // Maximum acceptable latency (ms)
eAAMPConfig_EnableLowLatencyCorrection // Rate correction enable

// Live Corrections & Sync
eAAMPConfig_EnableLiveLatencyCorrection  // Drift correction via rate adjustment
eAAMPConfig_PlaybackRate               // Normal playback rate (1.0 default)
eAAMPConfig_UTCSyncOnStartup           // UTC time sync at startup
```

### Debug & Logging Configuration
```cpp
// Logging Levels & Components
eAAMPConfig_InfoLogging              // Info-level log output
eAAMPConfig_DebugLogging             // Debug-level log output
eAAMPConfig_TraceLogging            // Trace-level log output
eAAMPConfig_GSTLogging              // GStreamer debug logging
eAAMPConfig_CurlLogging             // HTTP transfer logging
eAAMPConfig_MetadataLogging         // Timed metadata logging

// Harvest & Debug Features
eAAMPConfig_HarvestPath             // Debug file storage path
eAAMPConfig_HarvestCountLimit       // Max harvested files
eAAMPConfig_StreamLogging           // Manifest content logging
eAAMPConfig_ProgressLogging         // Playback progress logging
```

## Configuration API Reference

### Efficient Configuration Macros
AAMP provides optimized macros for configuration access:

```cpp
// Configuration Value Access (with internal caching)
#define GETCONFIGVALUE(key) (aamp->mConfig->GetConfigValue(key))
#define SETCONFIGVALUE(owner,key,value) (aamp->mConfig->SetConfigValue(owner, key, value))
#define ISCONFIGSET(key) (aamp->mConfig->IsConfigSet(key))
#define GETCONFIGOWNER(key) (aamp->mConfig->GetConfigOwner(key))

// Usage Examples
if (ISCONFIGSET(eAAMPConfig_EnableABR)) {
    bool abrEnabled = GETCONFIGVALUE(eAAMPConfig_EnableABR);
    if (abrEnabled) {
        int cacheLife = GETCONFIGVALUE(eAAMPConfig_ABRCacheLife);
        // Use ABR with specified cache lifetime
    }
}

// Runtime Configuration Updates
SETCONFIGVALUE(AAMP_APPLICATION_SETTING, eAAMPConfig_NetworkTimeout, 20.0);
SETCONFIGVALUE(AAMP_APPLICATION_SETTING, eAAMPConfig_EnableLowLatencyDash, true);
```

### Type-Safe Configuration Access
```cpp
class AampConfig {
public:
    // Generic value access with type safety
    template<typename T>
    T GetConfigValue(AAMPConfigSettings configParam);

    void SetConfigValue(ConfigPriority owner, AAMPConfigSettings configParam, bool value);
    void SetConfigValue(ConfigPriority owner, AAMPConfigSettings configParam, int value);
    void SetConfigValue(ConfigPriority owner, AAMPConfigSettings configParam, double value);
    void SetConfigValue(ConfigPriority owner, AAMPConfigSettings configParam, std::string value);

    // Configuration state queries
    bool IsConfigSet(AAMPConfigSettings configParam);
    ConfigPriority GetConfigOwner(AAMPConfigSettings configParam);
    void ResetConfiguration();  // Reset to defaults
};
```

## Advanced Configuration Features

### Dynamic Configuration Updates
Configuration changes apply instantly without requiring restart:

```cpp
// Runtime ABR strategy change
player->mConfig.SetConfigValue(AAMP_APPLICATION_SETTING,
                              eAAMPConfig_UseNewABR, true);

// Live offset adjustment during playback
player->mConfig.SetConfigValue(AAMP_APPLICATION_SETTING,
                              eAAMPConfig_LiveOffset, 15.0);

// DRM server failover
player->SetLicenseServerURL("https://backup-drm.example.com/", eDRM_WideVine);
```

### Configuration Validation & Error Handling
```cpp
// Range validation for numeric parameters
bool ValidateConfiguration() {
    double timeout = GETCONFIGVALUE(eAAMPConfig_NetworkTimeout);
    if (timeout < 1.0 || timeout > 300.0) {
        AAMPLOG_ERR("Invalid network timeout: %f (must be 1.0-300.0)", timeout);
        return false;
    }

    int cacheDepth = GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached);
    if (cacheDepth < 2 || cacheDepth > 32) {
        AAMPLOG_ERR("Invalid cache depth: %d (must be 2-32)", cacheDepth);
        return false;
    }

    return true;
}
```

## Best Practices & Recommendations

### Performance Optimization Settings
```json
{
  "performance": {
    "enableSharedSSLSession": true,     // Reduce SSL handshake overhead
    "maxCurlSockStore": 16,            // Socket connection pooling
    "enableCurlStore": true,           // Curl handle reuse
    "dashParallelFragDownload": true   // Concurrent DASH downloads
  },
  "buffer_optimization": {
    "maxFragmentCached": 8,            // Balance memory vs resilience
    "prePlayBufferCount": 3,           // Fast startup
    "abrCacheLife": 30,               // Stable ABR decisions
    "bufferHealthMonitorInterval": 1000 // Responsive monitoring
  }
}
```

### Low-Latency Streaming Configuration
```json
{
  "low_latency": {
    "enableLowLatencyDash": true,
    "llTargetLatency": 3000,          // 3s target latency (ms)
    "llMinLatency": 1000,             // 1s minimum latency
    "enableLowLatencyCorrection": true,
    "liveOffset": 6.0,                // Reduced live offset
    "networkTimeout": 5.0,            // Aggressive timeouts
    "manifestTimeout": 3.0
  }
}
```

### Development & Debug Configuration
```json
{
  "debug": {
    "infoLogging": true,
    "gstLogging": true,              // GStreamer pipeline debug
    "curlLogging": true,             // HTTP transaction logging
    "streamLogging": true,           // Manifest content capture
    "harvestPath": "/tmp/aamp-debug/",
    "harvestCountLimit": 100         // Limit debug file accumulation
  }
}
```

## Configuration System Implementation Summary

The AAMP configuration system provides:

- **Type Safety**: Strongly-typed enums prevent configuration errors
- **Priority Management**: Hierarchical override system ensures correct precedence
- **Runtime Updates**: Dynamic reconfiguration without service interruption
- **Comprehensive Coverage**: 200+ parameters covering all player subsystems
- **Performance**: Optimized access patterns with caching and batching
- **Debugging Support**: Extensive logging and harvest capabilities
- **Validation**: Range checking and format validation for reliability

This sophisticated configuration architecture enables fine-grained control over AAMP behavior while maintaining simplicity for common use cases.
BitsPerSecond bitrate = player->GetVideoBitrate();
double timeout = player->GetNetworkTimeout();
```

## Runtime Configuration Changes

Most configuration can be changed at runtime, but some require retune:

- **No Retune Required**: Bitrate, volume, subtitle language
- **Requires Retune**: DRM settings, protocol-specific settings

## Configuration Implementation

### AampConfig Class

**File**: `AampConfig.h/cpp`

**Key Methods**:
- `Initialize()`: Initialize configuration system
- `ReadAampCfgTxtFile()`: Read text config file
- `ReadAampCfgJsonFile()`: Read JSON config file
- `ReadAampCfgFromEnv()`: Read environment variables
- `ReadOperatorConfiguration()`: Read RFC/operator config
- `SetConfigValue()`: Set configuration value
- `GetConfigValue()`: Get configuration value
- `IsConfigSet()`: Check if boolean config is set

### Configuration Storage

Configuration values are stored in arrays indexed by enum:

```cpp
enum AAMPConfigSettings {
    eAAMPConfig_EnableABR,
    eAAMPConfig_Fog,
    // ... more configs
};

class AampConfig {
    bool mBoolConfigs[AAMP_MAX_BOOL_CONFIGS];
    int mIntConfigs[AAMP_MAX_INT_CONFIGS];
    // ... more arrays
};
```

## Environment Variables

Configuration can be set via environment variables:

```bash
export AAMP_INITIAL_BITRATE=3000000
export AAMP_NETWORK_TIMEOUT=15.0
export AAMP_ENABLE_ABR=true
```

## Operator Configuration (RFC)

RDK platforms support RFC (Remote Framework Configuration) for operator-specific settings. AAMP reads RFC values and applies them with appropriate priority.

## Configuration Validation

Configuration values are validated when set:

- **Range Checks**: Bitrates, timeouts within valid ranges
- **Type Checks**: Ensure correct data types
- **Dependency Checks**: Some configs depend on others

## Summary

The AAMP configuration system provides:

1. **Multiple Sources**: Code, files, environment, RFC, API
2. **Priority System**: Clear precedence for conflicting values
3. **Runtime Changes**: Most configs can change during playback
4. **Type Safety**: Strongly-typed configuration values
5. **Validation**: Input validation and range checking

This flexible system allows AAMP to be configured for different platforms, operators, and use cases.
