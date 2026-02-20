# DRM System

## Overview

AAMP implements a comprehensive Digital Rights Management (DRM) system that provides content protection for encrypted media streams. The DRM system supports multiple DRM vendors and protocols, enabling playback of protected content across different streaming formats (HLS, DASH) and platforms. The architecture separates high-level license management from platform-specific DRM implementations, providing flexibility to support various DRM systems while maintaining a unified interface for the player core.

The DRM system handles the complete lifecycle of protected content playback: extracting DRM information from manifests, acquiring licenses from license servers, managing DRM sessions, storing decryption keys, and coordinating decryption during fragment processing. The system integrates with platform-specific DRM middleware (OCDM, SecClient, SecManager) to leverage hardware security modules (HSM) and trusted execution environments (TEE) where available.

## Supported DRM Systems

AAMP supports multiple DRM systems, each optimized for specific use cases and content formats:

1. **Widevine**: Google's DRM system, commonly used in DASH streams. Supports multiple security levels (L1 hardware security, L2 software security, L3 software security) and provides robust content protection with hardware-backed key storage where available. Widevine uses CENC (Common Encryption) and requires PSSH (Protection System Specific Header) boxes in media fragments.

2. **PlayReady**: Microsoft's DRM system, frequently used in DASH streams, especially in enterprise and broadcast scenarios. PlayReady supports output protection (HDCP) and provides comprehensive content protection with support for hardware security modules. PlayReady also uses CENC and requires PSSH boxes for key identification.

3. **ClearKey**: W3C EME (Encrypted Media Extensions) Clear Key system, primarily used for testing and development. ClearKey provides simple key-based encryption without complex license server infrastructure, making it suitable for debugging and non-production scenarios. Keys are provided directly in the manifest or license response.

4. **Vanilla AES**: AES-128 encryption used in HLS streams with `#EXT-X-KEY` tags. This is not a full DRM system but provides basic content encryption. AAMP implements native AES-128 decryption for HLS streams, supporting both URI-based key retrieval and inline key specification. The system handles key rotation and IV (Initialization Vector) management for HLS encryption.

5. **Adobe Access**: Legacy DRM system support (deprecated). Maintained for backward compatibility with older content libraries but not actively developed for new deployments.

## Architecture

### High-Level Layer

**Files**: `drm/AampDRMLicManager.h/cpp`, `drm/DrmInterface.h/cpp`

**Purpose**: License management and interface abstraction

The high-level DRM layer provides unified interfaces and license management logic that abstracts platform-specific DRM implementations:

- **`DrmInterface`**: Singleton class (`GetInstance()`) that serves as the primary interface between AAMP player core and DRM middleware. It provides methods for license acquisition (`GetAccessKey()`), DRM session management (`getHlsDrmSession()`), error notification (`NotifyDrmError()`), and curl instance management for license downloads. The interface maintains references to `PrivateInstanceAAMP` for event reporting and configuration access.

- **`AampDRMLicManager`**: Manages license acquisition workflows, including building license requests, communicating with license servers via HTTP/HTTPS, processing license responses, and extracting decryption keys. The license manager handles retry logic, timeout management, and error recovery for license acquisition failures. It supports multiple license server URLs (Widevine, PlayReady, ClearKey) and custom license headers.

The high-level layer coordinates with `PrivateInstanceAAMP` to report DRM events (`AAMP_EVENT_DRM_METADATA`, `AAMP_EVENT_DRM_MESSAGE`) and integrates with the downloader system (`AampCurlDownloader`) for license server communication.

### Middleware Layer

**Files**: `middleware/drm/`

**Purpose**: Platform-specific DRM implementations

The middleware layer contains platform-specific DRM implementations that interface with system DRM services:

**Key Components**:
- **`DrmSession`**: Manages individual DRM sessions for content playback. Each session corresponds to a content stream or key rotation period. Sessions maintain decryption keys, session state, and platform-specific DRM context. Sessions are created via `DrmSessionFactory` and cleaned up when content stops or keys rotate.

- **`DrmSessionFactory`**: Factory class that creates DRM sessions based on DRM system type (Widevine, PlayReady, etc.) and platform capabilities. The factory selects appropriate DRM helpers and initializes sessions with platform-specific parameters. It handles session pooling and reuse for performance optimization.

- **`DrmHelper`**: DRM-specific helper classes (`WidevineHelper`, `PlayReadyHelper`, `ClearKeyHelper`) that implement DRM system-specific logic. Helpers handle PSSH box parsing, license request generation, license response processing, and key extraction. Each helper implements common interfaces while providing DRM-specific optimizations.

- **`HlsDrmSessionManager`**: HLS-specific DRM session management that handles `#EXT-X-KEY` tag processing, key URI resolution, and AES-128 decryption coordination. The manager tracks key rotation events and manages session transitions when keys change during HLS playback.

The middleware layer integrates with platform DRM services (OCDM - Open CDM, SecClient, SecManager) to leverage hardware security where available. Platform-specific implementations are selected at build time based on target platform configuration.

## DRM Workflow

### 1. License Acquisition

The license acquisition process begins when DRM-protected content is detected in the manifest:

```cpp
KeyState AampDRMLicenseManager::acquireLicense(
    int& responseCode,
    const std::shared_ptr<DrmHelper>& drmHelper,
    int sessionSlot,
    int &cdmError,
    AampMediaType streamType,
    void *metaDataPtr,
    bool isLicenseRenewal = false)
{
    // Build license request
    // Send to license server
    // Receive license response
    // Extract keys
    // Store in session
}
```

**License Acquisition Steps**:
- **DRM Information Extraction**: When parsing manifests (HLS `#EXT-X-KEY` tags or DASH PSSH boxes), the system extracts DRM metadata including key IDs, license server URLs, and encryption parameters. For DASH, PSSH (Protection System Specific Header) boxes contain Widevine/PlayReady-specific initialization data.
- **License Request Building**: The `DrmHelper` (WidevineHelper, PlayReadyHelper, etc.) builds a license request containing key IDs, content metadata, and platform-specific challenge data. The request may include custom headers (`customHeaderLicense`), access tokens (`accessToken`), and user agent information.
- **License Server Communication**: The license request is sent to the license server URL (configured via `licenseServerUrl`, `wvLicenseServerUrl`, `prLicenseServerUrl`, or `ckLicenseServerUrl`). Communication uses HTTPS with configurable SSL verification (`sslVerifyPeer`). The system supports proxy configuration (`licenseProxy`) and custom timeout settings (`drmNetworkTimeout`, `drmStallTimeout`, `drmStartTimeout`).
- **License Response Processing**: The license server responds with encrypted keys and usage rules. The `handleLicenseResponse()` method processes the response, extracts keys using platform-specific DRM middleware (OCDM, SecClient), and stores them in the DRM session. The system handles HTTP error codes (4xx, 5xx) and implements retry logic (`licenseRetryWaitTime`).
- **Key Storage**: Extracted keys are stored in the `DrmSession` associated with the content stream. Keys are cached in the session for reuse during playback, reducing license server round-trips. Key caching is controlled by `setLicenseCaching` configuration.

**License Renewal**: For content with expiring licenses, the system implements license renewal via background threads (`mLicenseRenewalThreads`). Renewal occurs before license expiration to ensure continuous playback without interruption.

### 2. Key Management

Keys are managed through DRM sessions that provide secure storage and lifecycle management:

- **Session Per Content/Key Rotation**: Each content stream or key rotation period gets its own `DrmSession`. Sessions are identified by key IDs and maintain decryption keys, session state, and platform-specific DRM context. The system supports multiple concurrent sessions (`dashMaxDrmSessions`, default: 3) for scenarios with multiple streams or key rotations.
- **Key Caching for Performance**: Keys are cached in DRM sessions to avoid repeated license acquisitions for the same content. When a fragment requires decryption, the system first checks if keys are already available in an existing session before requesting a new license. Key caching significantly reduces license server load and improves playback startup time.
- **Session Cleanup on Stop**: When playback stops or content changes, DRM sessions are cleaned up via `clearDrmSession()`. The cleanup process releases platform DRM resources, invalidates cached keys, and frees session slots for reuse. Failed key IDs are tracked separately (`clearFailedKeyIds()`) to prevent retry loops for permanently invalid keys.

**Session Management**: The `DrmSessionManager` maintains a pool of DRM sessions (`mMaxDRMSessions`) and handles session allocation, reuse, and cleanup. Sessions are allocated on-demand when new DRM-protected content is encountered and released when no longer needed.

### 3. Fragment Decryption

Fragment decryption occurs during the fragment processing pipeline before injection into GStreamer:

```cpp
bool MediaTrack::DecryptFragment(CachedFragment* fragment)
{
    // Get DRM session
    // Get decryption key
    // Decrypt fragment data
    // Update fragment buffer
}
```

**Decryption Process**:
- **DRM Session Lookup**: When a fragment is identified as encrypted (via `fragment->encrypted` flag or DRM metadata), the system looks up the appropriate `DrmSession` based on key ID or content identifier. For HLS with `#EXT-X-KEY` tags, the key tag information is matched to existing sessions or triggers new license acquisition.
- **Key Retrieval**: The decryption key is retrieved from the DRM session. If keys are not available (session not created or keys expired), the system triggers license acquisition before proceeding with decryption. Key retrieval uses platform-specific DRM middleware APIs (OCDM `opencdm_session_load()`, SecClient APIs, etc.).
- **Fragment Decryption**: The fragment data buffer (`CachedFragment::fragment`) is decrypted using the retrieved key. Decryption may occur in hardware (via platform DRM middleware) or software (via AAMP's native AES implementation for HLS). For DASH CENC (Common Encryption), decryption uses platform DRM middleware that handles CENC decryption. For HLS AES-128, AAMP implements native decryption using OpenSSL or platform crypto libraries.
- **Buffer Update**: After decryption, the fragment buffer contains decrypted media data ready for injection into GStreamer. The decryption process updates the fragment's encryption status and may modify PTS/DTS timestamps if decryption affects timing information.

**Performance Optimization**: Decryption is performed in-place on the fragment buffer to minimize memory copies. For hardware-accelerated decryption, the platform DRM middleware handles decryption directly in secure memory, providing both performance and security benefits.

## Configuration

The DRM system exposes comprehensive configuration parameters that control license acquisition, key management, and platform integration:

- **`licenseServerUrl`**: Default license server URL for DRM license requests. Used when DRM-specific URLs are not configured. The URL must support HTTPS and provide license responses in the format expected by the DRM system (Widevine, PlayReady, etc.).

- **`preferredDrm`**: Preferred DRM system selection (0=No DRM, 1=Widevine, 2=PlayReady, 3=CONSEC, 4=AdobeAccess, 5=Vanilla AES, 6=ClearKey). When multiple DRM systems are available in the manifest, this setting determines which system to use. Default: PlayReady (2).

- **`setLicenseCaching`**: Enable/disable license caching in DRM sessions (default: true). When enabled, licenses and keys are cached in DRM sessions for reuse, reducing license server round-trips and improving playback performance. Disabling caching forces license acquisition for each playback session, useful for debugging or scenarios requiring fresh licenses.

- **`licenseRetryWaitTime`**: Wait time in milliseconds between license acquisition retry attempts (default: 500ms). When license acquisition fails (network errors, server errors), the system retries after this delay. Exponential backoff may be applied for multiple retry attempts.

- **`wvLicenseServerUrl`**: Widevine-specific license server URL. Overrides `licenseServerUrl` for Widevine DRM content. Allows different license servers for different DRM systems.

- **`prLicenseServerUrl`**: PlayReady-specific license server URL. Overrides `licenseServerUrl` for PlayReady DRM content.

- **`ckLicenseServerUrl`**: ClearKey license server URL for ClearKey DRM testing scenarios.

- **`licenseProxy`**: Proxy server address for license requests. Allows license traffic to route through proxy servers for network configuration or security requirements.

- **`customHeaderLicense`**: Custom HTTP headers to include in license requests. Useful for authentication tokens, content identifiers, or other license server requirements.

- **`drmNetworkTimeout`**: Curl download timeout for DRM license requests in seconds (default: 5s). Controls how long the system waits for license server responses before timing out.

- **`drmStallTimeout`**: Timeout value for detecting curl download stalls for DRM in seconds (default: 0s, disabled). When enabled, detects when license downloads stall and triggers retry or error handling.

- **`drmStartTimeout`**: Timeout value for curl download to start for DRM after connection in seconds (default: 0s, disabled). Detects connection establishment delays.

- **`dashMaxDrmSessions`**: Maximum number of DRM sessions that can be cached by `AampDRMSessionManager` (default: 3). Limits memory usage for DRM session storage while supporting multiple concurrent streams or key rotations.

- **`sendLicenseResponseHeaders`**: Enable/disable sending license response headers as part of DRMMetadata event (default: false). When enabled, license server response headers are included in DRM metadata events for debugging or application processing.

- **`sendUserAgentInLicense`**: Enable/disable sending user agent string in DRM license request headers (default: disabled). Some license servers require user agent information for device identification or analytics.

## Summary

The DRM system provides comprehensive content protection capabilities:

- **Multi-DRM Support**: Unified interface supporting Widevine, PlayReady, ClearKey, Vanilla AES, and legacy DRM systems. The modular architecture allows easy addition of new DRM systems while maintaining consistent player integration.

- **Secure Key Management**: Keys are stored in platform-specific DRM sessions that leverage hardware security modules (HSM) and trusted execution environments (TEE) where available. Key lifecycle management ensures secure storage, rotation, and cleanup.

- **Efficient License Caching**: License and key caching reduces license server load and improves playback performance by avoiding repeated license acquisitions. Session pooling and reuse optimize memory usage while maintaining security.

- **Platform Integration**: Deep integration with platform DRM middleware (OCDM, SecClient, SecManager) enables hardware-accelerated decryption and secure key storage. The abstraction layer allows platform-specific optimizations while maintaining portable player code.
