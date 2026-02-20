# Middleware & Platform Integration

## Overview

The middleware layer provides platform-specific integrations that abstract platform differences and enable AAMP to leverage platform capabilities for optimal performance and functionality. The middleware acts as a bridge between AAMP's portable player core and platform-specific services, libraries, and hardware features.

The middleware architecture enables AAMP to run on diverse platforms (RDK set-top boxes, Linux systems, macOS simulators) while taking advantage of platform-specific optimizations (hardware decoders, secure DRM implementations, platform rendering systems). This separation of concerns allows the player core to remain portable while platform-specific code is isolated in the middleware layer, facilitating maintenance and platform porting.

The middleware layer handles platform detection, service integration, hardware abstraction, and custom plugin integration, providing a unified interface to the player core while implementing platform-specific details underneath.

## Architecture

**Location**: `middleware/`

The middleware directory contains platform-specific implementations organized by functionality:

**Key Components**:
- **DRM Implementations** (`middleware/drm/`): Platform-specific DRM system integrations (OCDM, SecClient, SecManager) that interface with platform security frameworks and hardware security modules.
- **Closed Caption Management** (`middleware/closedcaptions/`): Platform-specific subtitle rendering implementations (Subtec hardware rendering, Rialto-based rendering, native software rendering) that leverage platform capabilities for optimal subtitle display.
- **External Service Integration** (`middleware/externals/`): Platform service integrations (Thunder/RPC, RFC configuration, Content Security Manager) that connect AAMP with platform frameworks and services.
- **GStreamer Plugins** (`middleware/gst-plugins/`): Custom GStreamer elements and plugins that provide platform-specific media processing, DRM decryption, and rendering capabilities.
- **Platform Utilities** (`middleware/`): Platform-specific utility functions (SoC detection, player utilities, logging systems) that abstract platform differences and provide common interfaces.

## DRM Middleware

**Location**: `middleware/drm/`

The DRM middleware layer provides platform-specific DRM implementations that interface with platform security frameworks:

- **OCDM (Open CDM) Interface**: Open CDM is a standardized interface for Content Decryption Modules (CDM) that provides a common API across platforms. The OCDM implementation (`middleware/drm/ocdm/`) wraps platform-specific CDM implementations (Widevine CDM, PlayReady CDM) and provides a unified interface to AAMP's DRM system. OCDM handles session creation, license processing, key management, and content decryption through platform CDM libraries.

- **SecClient/SecManager**: RDK-specific security clients (`middleware/drm/secclient/`, `middleware/drm/secmanager/`) that interface with RDK's security framework. SecClient provides direct DRM operations for RDK platforms, while SecManager provides higher-level security management including license acquisition, key rotation, and secure content path management. These implementations leverage RDK's hardware security modules (HSM) and trusted execution environments (TEE) for secure key storage and decryption.

- **Helper Classes**: DRM-specific helper classes (`middleware/drm/helper/`) provide DRM system-specific logic:
  - **WidevineHelper**: Handles Widevine-specific operations (PSSH parsing, license request generation, key extraction) and interfaces with Widevine CDM.
  - **PlayReadyHelper**: Handles PlayReady-specific operations (PSSH parsing, license challenge/response, key management) and interfaces with PlayReady CDM.
  - **ClearKeyHelper**: Implements W3C EME Clear Key for testing and development scenarios.
  - **HlsDrmBase**: Base class for HLS-specific DRM handling, with implementations for different DRM systems used in HLS streams.

**Platform Selection**: The build system selects appropriate DRM middleware implementations based on platform detection. RDK platforms use SecClient/SecManager, while other platforms may use OCDM or direct DRM library integration. The selection is made at build time via CMake configuration.

## Closed Captions

**Location**: `middleware/closedcaptions/`

The closed caption middleware provides platform-specific subtitle rendering implementations:

- **Subtec Hardware Rendering**: Subtec (`middleware/subtec/`) is a hardware-accelerated subtitle rendering system available on certain RDK platforms. Subtec provides dedicated hardware for subtitle overlay, enabling efficient subtitle rendering without CPU overhead. The Subtec integration (`gstSubtecEnabled` configuration) uses GStreamer Subtec plugins to render subtitles directly in hardware, providing smooth subtitle display even during high-bitrate video playback.

- **Rialto-Based Rendering**: Rialto (`middleware/closedcaptions/rialto/`) provides a platform abstraction layer for subtitle rendering on Rialto-enabled platforms. Rialto handles subtitle positioning, styling, and synchronization, integrating with platform compositors for optimal subtitle display. Rialto rendering supports advanced subtitle features (positioning, styling, animations) and provides consistent subtitle appearance across different content sources.

- **Native Software Rendering**: Native rendering (`middleware/closedcaptions/native/`) provides software-based subtitle rendering for platforms without hardware subtitle support. Native rendering uses GStreamer text rendering plugins (`textoverlay`, `subtitleoverlay`) to render subtitles as video overlays. While less efficient than hardware rendering, native rendering provides portability and supports all subtitle formats (WebVTT, TTML, CEA-608/708).

**Subtitle Format Support**: The middleware handles multiple subtitle formats:
- **WebVTT**: Web Video Text Tracks format, parsed by `subtitle/webvttParser.cpp` and rendered via platform-specific renderers.
- **TTML**: Timed Text Markup Language, used in DASH streams, parsed and converted to platform subtitle formats.
- **CEA-608/708**: Closed caption formats used in broadcast streams, extracted from video streams and rendered via platform caption systems.

## External Services

**Location**: `middleware/externals/`

The external services middleware integrates AAMP with platform frameworks and services:

- **Thunder/RPC Integration**: Thunder (`middleware/externals/thunder/`) is RDK's remote procedure call (RPC) framework that enables inter-process communication and service discovery. AAMP integrates with Thunder to access platform services (configuration, telemetry, device management) and expose AAMP functionality to other platform components. Thunder integration enables AAMP to participate in platform service ecosystems and provides standardized interfaces for platform interactions.

- **RFC (Remote Framework Configuration)**: RFC integration (`middleware/externals/rfc/`) allows AAMP to receive configuration updates from platform configuration systems. RFC provides dynamic configuration management, enabling operators to update AAMP settings without code changes or device restarts. RFC integration reads configuration parameters from platform configuration stores and applies them to AAMP's configuration system, following the configuration priority hierarchy (RFC settings override defaults but are overridden by application settings).

- **Content Security Manager**: Content Security Manager (`middleware/externals/contentsecuritymanager/`) provides platform-level content protection services. The manager coordinates with platform security frameworks to enforce content protection policies, manage secure content paths, and provide content security event reporting. Integration with Content Security Manager enables AAMP to participate in platform-wide content protection strategies and leverage platform security infrastructure.

**Service Discovery**: The middleware implements service discovery mechanisms to locate and connect to platform services. Service discovery uses platform-specific mechanisms (Thunder service registry, systemd service discovery, etc.) to find available services and establish connections. Failed service connections are handled gracefully, with AAMP falling back to default behavior when services are unavailable.

## GStreamer Plugins

**Location**: `middleware/gst-plugins/`

Custom GStreamer plugins extend GStreamer's capabilities with platform-specific functionality:

- **DRM Plugins**: DRM-specific GStreamer elements (`middleware/gst-plugins/drm/`) provide content decryption within the GStreamer pipeline. DRM plugins integrate with platform DRM middleware (OCDM, SecClient) to decrypt encrypted media streams during pipeline processing. Plugins handle key management, decryption coordination, and secure content path enforcement, ensuring decrypted content remains protected until rendering.

- **Subtec Plugin**: Subtec GStreamer plugin (`middleware/gst-plugins/subtec/`) provides hardware-accelerated subtitle rendering within GStreamer pipelines. The plugin interfaces with Subtec hardware to render subtitles efficiently, handling subtitle positioning, styling, and synchronization. Subtec plugin integration is controlled by `gstSubtecEnabled` configuration, enabling/disabling hardware subtitle rendering based on platform capabilities.

**Plugin Registration**: Custom plugins are registered with GStreamer during AAMP initialization. Plugin registration occurs via `gst_plugin_register_static()` or dynamic plugin loading, making plugins available to GStreamer pipeline construction. Platform detection determines which plugins are available and registered, ensuring plugins are only used on supported platforms.

## Platform Utilities

**Location**: `middleware/`

Platform-specific utility functions provide platform abstraction and common functionality:

- **SocUtils**: SoC (System on Chip) detection and utility functions (`middleware/socutils/`) identify the target platform's SoC (Amlogic, Broadcom, Realtek, etc.) and provide SoC-specific optimizations. SoC detection enables platform-specific code paths, hardware decoder selection, and performance optimizations tailored to each SoC's capabilities. SoC utilities abstract SoC differences, providing common interfaces while implementing SoC-specific details.

- **PlayerUtils**: Player utility functions (`middleware/playerutils/`) provide platform-specific player operations (display management, audio routing, power management). Utilities abstract platform differences in display handling, audio output configuration, and system integration, enabling portable player code while leveraging platform capabilities.

- **LogManager**: Platform logging system (`middleware/playerLogManager/`) provides unified logging interfaces that integrate with platform logging frameworks (syslog, journald, platform-specific logging). LogManager handles log level filtering, log routing, and log formatting, ensuring consistent logging behavior across platforms while supporting platform-specific logging requirements.

**Platform Detection**: Platform utilities implement platform detection logic that identifies the target platform at runtime or build time. Platform detection uses system calls, file system inspection, or build-time configuration to determine platform characteristics. Detected platform information guides utility behavior, enabling platform-specific optimizations and feature selection.

## Summary

The middleware layer provides essential platform integration capabilities:

- **Platform Abstraction**: Middleware abstracts platform differences, enabling portable player core code while leveraging platform-specific capabilities. Platform detection and abstraction layers provide common interfaces that hide platform implementation details.

- **DRM Integration**: Platform-specific DRM implementations enable secure content playback by interfacing with platform security frameworks and hardware security modules. DRM middleware provides secure key management, license processing, and content decryption while maintaining platform security requirements.

- **Service Integration**: Integration with platform services (Thunder/RPC, RFC, Content Security Manager) enables AAMP to participate in platform ecosystems and access platform capabilities. Service integration provides dynamic configuration, telemetry reporting, and platform-wide content protection coordination.

- **Custom Plugins**: GStreamer plugins extend pipeline capabilities with platform-specific functionality (DRM decryption, hardware subtitle rendering). Plugin integration enables optimal performance by leveraging platform hardware and specialized processing capabilities.
