<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Troubleshooting Guide

## Build Issues

### CMake Not Found

**Error**: `cmake: command not found`

**Solution**:
```bash
# macOS
brew install cmake

# Ubuntu/Debian
sudo apt-get install cmake

# Verify installation
cmake --version
```

### GStreamer Not Found

**Error**: `Package gstreamer-1.0 was not found`

**Solution**:
```bash
# Check if GStreamer is installed
pkg-config --cflags --libs gstreamer-1.0

# Install GStreamer
# macOS
brew install gstreamer

# Ubuntu/Debian
sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

### OpenSSL Not Found

**Error**: `Could not find OpenSSL`

**Solution**:
```bash
# macOS
brew install openssl
export OPENSSL_DIR=$(brew --prefix openssl)

# Ubuntu/Debian
sudo apt-get install libssl-dev

# Verify
pkg-config --cflags --libs openssl
```

### libcurl Not Found

**Error**: `Could not find CURL`

**Solution**:
```bash
# macOS
brew install curl

# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# Verify
pkg-config --cflags --libs libcurl
```

### Build Fails with C++ Version

**Error**: `error: 'optional' is not a member of 'std'`

**Solution**:
Ensure C++17 or later is used:

```bash
cmake -DCMAKE_CXX_STANDARD=17 ..
```

## Runtime Issues

### No Video Output

**Possible Causes**:

1. **Sink Configuration**
   - Ensure the correct sink is configured (GStreamer vs Westeros)
   - Check if sink plugin is available: `gst-inspect-1.0 | grep -i sink`

2. **Pipeline Not Ready**
   - Verify `gstBufferAndPlay` is enabled (default: true)
   - Check if video codec is supported

3. **DRM Issues**
   - Verify DRM license was acquired
   - Check DRM system compatibility
   - See DRM-specific troubleshooting below

**Debug**:
```bash
export GST_DEBUG=3
export AAMP_DEBUG_LOG=1
./aamp_player <url>
```

### Audio/Video Out of Sync

**Possible Causes**:

1. **PDT-based sync disabled**
   ```javascript
   config.hlsAVTrackSyncUsingPDT = true;
   player.initConfig(config);
   ```

2. **Low Latency correction**
   ```javascript
   config.enableLowLatencyCorrection = true;
   player.initConfig(config);
   ```

3. **Discontinuity handling**
   ```javascript
   config.mpdDiscontinuityHandling = true;
   config.useNewAdBreaker = true;
   player.initConfig(config);
   ```

### Buffering Issues

**High Rebuffering Rate**:

1. **Increase buffer size**:
   ```javascript
   config.abrCacheLength = 5;  // Increase from default 3
   config.abrCacheLife = 10000; // Increase from default 5000
   ```

2. **Enable pre-buffering**:
   ```javascript
   config.gstBufferAndPlay = true;
   ```

3. **Disable aggressive ABR**:
   ```javascript
   config.abr = false;  // Test with disabled ABR first
   ```

**Stuck at Live Edge**:
```javascript
config.cdvrLiveOffset = 30;  // Increase offset from default
config.enableLowLatencyCorrection = true;
```

### High CPU Usage

**Possible Causes**:

1. **4K playback on limited device**:
   ```javascript
   config.disable4K = true;
   ```

2. **Excessive codec usage**:
   ```javascript
   config.stereoOnly = true;  // Reduces codec complexity
   config.disableATMOS = true;
   ```

3. **Native CC rendering**:
   ```javascript
   config.nativeCCRendering = false;
   ```

## DRM Issues

### License Acquisition Timeout

**Error**: `License acquisition timeout`

**Solution**:
```javascript
config.licenseAcquisitionTimeout = 15000;  // Increase from default 10000
config.contentProtectionDataUpdateTimeout = 7000;
```

### PlayReady Specific

Check PlayReady SDK version compatibility and license server endpoint.

**Debug**:
```bash
export AAMP_DEBUG_LOG=3
# Check DRM plugin logs
gst-inspect-1.0 msdk
```

### Widevine Specific

Verify Widevine CDM (Content Decryption Module) is installed on the platform.

### Clear Key / AES-128

No special setup required. If failing:

1. Verify encryption key format
2. Check key acquisition endpoint
3. Enable debug logging

## Network Issues

### Timeout on Fragment Download

**Error**: `Fragment download timeout`

**Solution**:
Adjust timeout values based on network conditions:

```javascript
// Create custom timeout (check API for timeout methods)
config.fragmentDownloadTimeout = 15000;  // Adjust as needed
```

### SSL/Certificate Errors

**For Self-Signed Certificates** (development only):
```javascript
config.sslVerifyPeer = false;
```

**For Production**: Fix certificate chain or use proper CA certificate.

### Proxy Issues

Add custom headers for proxy authentication:

```javascript
config.customHeader = "Proxy-Authorization: Basic base64credentials";
```

## Performance Issues

### High Latency in Low Latency Playback

**High Latency**:
```javascript
config.enableLowLatencyDash = true;
config.enableLowLatencyCorrection = true;
config.disableLowLatencyABR = false;  // Enable ABR for LL
config.downloadBufferChunks = 10;  // Reduce from default 20
```

### Fragment Collection Slowness

**Optimize Playlist Refresh**:
```javascript
config.parallelPlaylistDownload = true;  // For tune
config.parallelPlaylistRefresh = true;   // For refresh (default)
config.preFetchIframePlaylist = true;    // For I-frame playback
```

## Test Failures

### Unit Tests Crashing

**Debug**:
```bash
cd build
ctest -V  # Verbose output
./test_executable --gtest_filter="TestName" --gtest_catch_exceptions=0
```

### Flaky Tests

**Causes**:
- Timing dependencies
- Uninitialized test data
- Race conditions

**Fix**:
- Use deterministic test data
- Remove sleep() calls, use waitFor() instead
- Mock time-dependent components

## Platform-Specific Issues

### Ubuntu/Linux

**Missing packages**: See [UbuntuSetup.md](UbuntuSetup.md)

### macOS

**Homebrew dependencies**:
```bash
brew install gstreamer gst-plugins-base openssl curl
```

### RDK

Consult RDK-specific build documentation and environment setup.

## Getting Help

1. **Review Logs**: Enable debug logging
   ```bash
   export AAMP_DEBUG_LOG=3
   export GST_DEBUG=4
   ```

2. **Check Configuration**: Verify all settings via `initConfig()`

3. **Community Support**: File issues with reproduction steps and logs

4. **Documentation**:
   - [Architecture](ARCHITECTURE.md)
   - [Configuration](CONFIGURATION.md)
   - [Testing](TESTING.md)
   - [API Reference](AAMP-UVE-API.md)

## Reporting Bugs

When reporting issues, include:

- Platform and OS version
- AAMP version/commit hash
- Reproduction URL (sanitized)
- Configuration used
- Debug logs (with AAMP_DEBUG_LOG=3)
- GStreamer version: `gst-launch-1.0 --version`
