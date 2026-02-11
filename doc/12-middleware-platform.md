# Middleware & Platform Integration

## Overview

The middleware layer provides platform-specific integrations for RDK and other platforms.

## Architecture

**Location**: `middleware/`

**Key Components**:
- DRM implementations
- Closed caption management
- External service integration
- GStreamer plugins
- Platform utilities

## DRM Middleware

**Location**: `middleware/drm/`

Platform-specific DRM implementations:
- **OCDM**: Open CDM interface
- **SecClient/SecManager**: RDK security clients
- **Helper Classes**: DRM-specific helpers

## Closed Captions

**Location**: `middleware/closedcaptions/`

Closed caption rendering:
- **Subtec**: Hardware subtitle rendering
- **Rialto**: Rialto-based rendering
- **Native**: Software rendering

## External Services

**Location**: `middleware/externals/`

Platform service integration:
- **Thunder/RPC**: Remote procedure calls
- **RFC**: Remote Framework Configuration
- **Content Security Manager**: Content protection

## GStreamer Plugins

**Location**: `middleware/gst-plugins/`

Custom GStreamer plugins:
- **DRM Plugins**: DRM-specific elements
- **Subtec Plugin**: Subtitle rendering

## Platform Utilities

**Location**: `middleware/`

Platform-specific utilities:
- **SocUtils**: SoC-specific functions
- **PlayerUtils**: Player utilities
- **LogManager**: Logging system

## Summary

Middleware provides:
- Platform abstraction
- DRM integration
- Service integration
- Custom plugins
