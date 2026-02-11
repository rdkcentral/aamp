# DRM System

## Overview

AAMP supports multiple DRM (Digital Rights Management) systems for content protection.

## Supported DRM Systems

1. **Widevine**: Google's DRM (used in DASH)
2. **PlayReady**: Microsoft's DRM (used in DASH)
3. **ClearKey**: Simple key-based encryption
4. **Vanilla AES**: AES-128 encryption (HLS)
5. **Adobe Access**: Legacy support

## Architecture

### High-Level Layer

**Files**: `drm/AampDRMLicManager.h/cpp`, `drm/DrmInterface.h/cpp`

**Purpose**: License management and interface abstraction

### Middleware Layer

**Files**: `middleware/drm/`

**Purpose**: Platform-specific DRM implementations

**Key Components**:
- `DrmSession`: DRM session management
- `DrmSessionFactory`: Creates DRM sessions
- `DrmHelper`: DRM-specific helpers (Widevine, PlayReady, etc.)
- `HlsDrmSessionManager`: HLS-specific DRM

## DRM Workflow

### 1. License Acquisition

```cpp
bool AampDRMLicManager::AcquireLicense(
    const DrmInfo& drmInfo)
{
    // Build license request
    // Send to license server
    // Receive license response
    // Extract keys
    // Store in session
}
```

### 2. Key Management

Keys are stored in DRM sessions:
- Session per content/key rotation
- Key caching for performance
- Session cleanup on stop

### 3. Fragment Decryption

```cpp
bool MediaTrack::DecryptFragment(CachedFragment* fragment)
{
    // Get DRM session
    // Get decryption key
    // Decrypt fragment data
    // Update fragment buffer
}
```

## Configuration

Key DRM configuration:
- `licenseServerUrl`: License server URL
- `preferredDrm`: Preferred DRM system
- `setLicenseCaching`: Enable license caching
- `licenseRetryWaitTime`: Retry wait time (ms)

## Summary

The DRM system provides:
- Multi-DRM support
- Secure key management
- Efficient license caching
- Platform integration
