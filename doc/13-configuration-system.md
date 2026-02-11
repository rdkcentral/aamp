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

```cpp
// Get current value
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
