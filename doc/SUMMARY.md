# AAMP Documentation Summary

This document provides a quick reference guide to the AAMP architecture and documentation structure.

## Documentation Structure

### Getting Started
- **[README.md](README.md)** - Main entry point with navigation to all documents
- **[SUMMARY.md](SUMMARY.md)** - This file - Quick reference guide

### Core Architecture Documentation
1. **[01-architecture-overview.md](01-architecture-overview.md)** - High-level system architecture
2. **[02-code-organization.md](02-code-organization.md)** - Code organization and folder structure
3. **[03-core-classes-interfaces.md](03-core-classes-interfaces.md)** - Important APIs and classes

### System Components
4. **[04-fragment-collection.md](04-fragment-collection.md)** - Fragment collection system
5. **[05-adaptive-bitrate.md](05-adaptive-bitrate.md)** - Adaptive bitrate management
6. **[06-buffer-management.md](06-buffer-management.md)** - Buffer management strategies
7. **[07-drm-system.md](07-drm-system.md)** - DRM system architecture
8. **[08-downloader-network.md](08-downloader-network.md)** - Network and download layer
9. **[09-event-management.md](09-event-management.md)** - Event system architecture
10. **[10-time-shift-buffer.md](10-time-shift-buffer.md)** - Time Shift Buffer (TSB) system

### Integration & Platform
11. **[11-gstreamer-integration.md](11-gstreamer-integration.md)** - GStreamer pipeline integration
12. **[12-middleware-platform.md](12-middleware-platform.md)** - Middleware and platform integration
13. **[13-configuration-system.md](13-configuration-system.md)** - Configuration management
14. **[14-build-system.md](14-build-system.md)** - Build system and compilation

### Execution & Development
15. **[15-workflows-execution.md](15-workflows-execution.md)** - Workflows and execution flows
16. **[16-javascript-bindings.md](16-javascript-bindings.md)** - JavaScript/WebKit bindings
17. **[17-testing-quality.md](17-testing-quality.md)** - Testing and quality assurance

### Guides
18. **[18-beginners-guide.md](18-beginners-guide.md)** - Beginner's guide for new developers
19. **[19-expert-deep-dive.md](19-expert-deep-dive.md)** - Advanced topics for experts

### High-Level Design
- **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)** - Comprehensive RDK-E design and usage

## Key Architecture Components

### Public API Layer
- **PlayerInstanceAAMP** (`main_aamp.h/cpp`) - Public C++ API

### Core Engine
- **PrivateInstanceAAMP** (`priv_aamp.h/cpp`) - Core player implementation
- **AampConfig** - Configuration management
- **AampEventManager** - Event system
- **AampScheduler** - Task scheduling

### Stream Abstraction
- **StreamAbstractionAAMP** - Base class
- **StreamAbstractionAAMP_HLS** - HLS implementation
- **StreamAbstractionAAMP_MPD** - DASH implementation
- **StreamAbstractionAAMP_Progressive** - Progressive download

### Fragment Collection
- **FragmentCollector_HLS** - HLS fragment collection
- **FragmentCollector_MPD** - DASH fragment collection
- **FragmentCollector_Progressive** - Progressive playback

### Media Pipeline
- **AAMPGstPlayer** - GStreamer integration
- **StreamSink** - Media injection interface

### DRM System
- **AampDRMLicManager** - License management
- **DrmInterface** - DRM abstraction

### Network Layer
- **AampCurlDownloader** - HTTP/HTTPS downloads
- **AampCurlStore** - Connection management

## Main Execution Flow

### Tune API Flow
1. Application calls `PlayerInstanceAAMP::Tune()`
2. Validates and processes parameters
3. Calls `PrivateInstanceAAMP::Tune()`
4. Initializes configuration
5. Determines media format (HLS/DASH/Progressive)
6. Creates appropriate StreamAbstraction
7. Downloads and parses manifest
8. Initializes fragment collectors
9. Starts fragment downloads
10. Injects fragments into GStreamer pipeline
11. Begins playback

### Fragment Download Flow
1. Fragment collector requests next fragment
2. Downloader fetches fragment from CDN
3. If encrypted, acquire DRM license
4. Decrypt fragment
5. Process fragment (demux, parse)
6. Inject into GStreamer pipeline
7. GStreamer decodes and renders

## Quick Reference by Topic

### Understanding Architecture
- Start: **[18-beginners-guide.md](18-beginners-guide.md)**
- Overview: **[01-architecture-overview.md](01-architecture-overview.md)**
- Code Structure: **[02-code-organization.md](02-code-organization.md)**
- Classes: **[03-core-classes-interfaces.md](03-core-classes-interfaces.md)**

### Component Details
- Fragments: **[04-fragment-collection.md](04-fragment-collection.md)**
- ABR: **[05-adaptive-bitrate.md](05-adaptive-bitrate.md)**
- Buffering: **[06-buffer-management.md](06-buffer-management.md)**
- DRM: **[07-drm-system.md](07-drm-system.md)**
- Network: **[08-downloader-network.md](08-downloader-network.md)**
- Events: **[09-event-management.md](09-event-management.md)**
- TSB: **[10-time-shift-buffer.md](10-time-shift-buffer.md)**

### Integration
- GStreamer: **[11-gstreamer-integration.md](11-gstreamer-integration.md)**
- Platform: **[12-middleware-platform.md](12-middleware-platform.md)**
- Config: **[13-configuration-system.md](13-configuration-system.md)**
- Build: **[14-build-system.md](14-build-system.md)**

### Development
- Workflows: **[15-workflows-execution.md](15-workflows-execution.md)**
- JS Bindings: **[16-javascript-bindings.md](16-javascript-bindings.md)**
- Testing: **[17-testing-quality.md](17-testing-quality.md)**
- Advanced: **[19-expert-deep-dive.md](19-expert-deep-dive.md)**

### RDK-E Integration
- Design: **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)**

## Documentation Navigation Tips

- **New to AAMP?** → Start with **[18-beginners-guide.md](18-beginners-guide.md)**
- **Need architecture overview?** → Read **[01-architecture-overview.md](01-architecture-overview.md)**
- **Working on a specific component?** → Check the corresponding numbered document (04-17)
- **RDK-E integration?** → See **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)**
- **Advanced topics?** → Review **[19-expert-deep-dive.md](19-expert-deep-dive.md)**
