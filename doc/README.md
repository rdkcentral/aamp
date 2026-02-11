# AAMP Documentation

This directory contains comprehensive documentation for the AAMP (Advanced Adaptive Media Player) codebase, including architecture analysis, design documentation, and implementation guides.

## Documentation Structure

### Getting Started

1. **[README.md](README.md)** - This file - Main entry point with navigation to all documents
2. **[SUMMARY.md](SUMMARY.md)** - Quick reference guide to the documentation structure

### Core Architecture Documentation

3. **[01-architecture-overview.md](01-architecture-overview.md)**
   - High-level system architecture
   - Component relationships
   - Role in RDK infrastructure
   - Design principles
   - Data flow architecture

4. **[02-code-organization.md](02-code-organization.md)**
   - Repository structure
   - Folder organization
   - File-by-file breakdown
   - Dependencies and relationships
   - Main entry points

5. **[03-core-classes-interfaces.md](03-core-classes-interfaces.md)**
   - Public API documentation (PlayerInstanceAAMP)
   - Core classes (PrivateInstanceAAMP, StreamAbstraction, etc.)
   - Class relationships and responsibilities
   - Interface definitions
   - Code examples

### System Components

6. **[04-fragment-collection.md](04-fragment-collection.md)**
   - Fragment collection system architecture
   - HLS fragment collector implementation
   - DASH fragment collector implementation
   - Progressive fragment collector
   - Fragment lifecycle management

7. **[05-adaptive-bitrate.md](05-adaptive-bitrate.md)**
   - ABR (Adaptive Bitrate) system architecture
   - Network bandwidth estimation
   - Profile management and selection
   - Quality switching algorithms

8. **[06-buffer-management.md](06-buffer-management.md)**
   - Buffer management strategies
   - Time-based and byte-based buffering
   - Buffer level monitoring
   - Buffer optimization techniques

9. **[07-drm-system.md](07-drm-system.md)**
   - DRM system architecture and implementation
   - License acquisition and management
   - Multiple DRM system support (Widevine, PlayReady, ClearKey, etc.)
   - Content decryption flow

10. **[08-downloader-network.md](08-downloader-network.md)**
    - HTTP/HTTPS download management
    - Network layer architecture
    - Connection pooling and reuse
    - Download optimization

11. **[09-event-management.md](09-event-management.md)**
    - Event system architecture
    - Event types and categories
    - Event listener registration
    - Event dispatching mechanism

12. **[10-time-shift-buffer.md](10-time-shift-buffer.md)**
    - Time Shift Buffer (TSB) architecture
    - Local and remote TSB support
    - TSB API and usage
    - Buffer management for time-shifted playback

### Integration & Platform

13. **[11-gstreamer-integration.md](11-gstreamer-integration.md)**
    - GStreamer pipeline integration
    - Media injection interface
    - Pipeline configuration
    - Low-level media processing

14. **[12-middleware-platform.md](12-middleware-platform.md)**
    - Middleware layer architecture
    - Platform-specific implementations
    - Platform detection and hardware acceleration
    - SOC-specific optimizations

15. **[13-configuration-system.md](13-configuration-system.md)**
    - Configuration management system
    - Configuration options and defaults
    - Runtime configuration changes
    - Configuration validation

16. **[14-build-system.md](14-build-system.md)**
    - Build system architecture
    - CMake configuration
    - Platform-specific build options
    - Dependency management

### Execution & Development

17. **[15-workflows-execution.md](15-workflows-execution.md)**
    - Player lifecycle overview
    - Initialization sequence
    - Tune workflow
    - Playback flow
    - Seek workflow
    - Error handling

18. **[16-javascript-bindings.md](16-javascript-bindings.md)**
    - JavaScript/WebKit bindings
    - UVE (Universal Video Engine) API
    - Binding implementation
    - Web application integration

19. **[17-testing-quality.md](17-testing-quality.md)**
    - Testing infrastructure
    - Quality assurance processes
    - Test coverage and strategies
    - Performance testing

### Guides

20. **[18-beginners-guide.md](18-beginners-guide.md)**
    - Introduction for new developers
    - Key concepts and terminology
    - Recommended learning path
    - Common tasks and examples

21. **[19-expert-deep-dive.md](19-expert-deep-dive.md)**
    - Advanced topics for experienced developers
    - Low-latency optimizations
    - Performance tuning
    - Memory optimization techniques

### High-Level Design

22. **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)**
    - Comprehensive high-level design overview
    - RDK-E integration details and usage patterns
    - Component interactions and data flows
    - Deployment scenarios and performance optimizations
    - Security considerations and monitoring

## How to Use This Documentation

### For New Developers
1. Start with **[18-beginners-guide.md](18-beginners-guide.md)** for an introduction
2. Read **[01-architecture-overview.md](01-architecture-overview.md)** to understand the system
3. Review **[02-code-organization.md](02-code-organization.md)** to understand code structure
4. Explore **[03-core-classes-interfaces.md](03-core-classes-interfaces.md)** for API usage

### For System Architects
1. Review **[01-architecture-overview.md](01-architecture-overview.md)** for high-level design
2. Read **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)** for RDK-E integration
3. Study **[15-workflows-execution.md](15-workflows-execution.md)** for execution flows
4. Check **[12-middleware-platform.md](12-middleware-platform.md)** for platform integration

### For Component Developers
1. Review component-specific documentation (e.g., **[04-fragment-collection.md](04-fragment-collection.md)**, **[05-adaptive-bitrate.md](05-adaptive-bitrate.md)**)
2. Understand **[11-gstreamer-integration.md](11-gstreamer-integration.md)** for media pipeline
3. Reference **[09-event-management.md](09-event-management.md)** for event handling
4. Check **[13-configuration-system.md](13-configuration-system.md)** for configuration

### For RDK-E Developers
1. Read **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)** for RDK-E integration
2. Review **[12-middleware-platform.md](12-middleware-platform.md)** for middleware details
3. Check **[07-drm-system.md](07-drm-system.md)** for DRM integration
4. Study **[16-javascript-bindings.md](16-javascript-bindings.md)** for WebKit integration

### For Advanced Developers
1. Review **[19-expert-deep-dive.md](19-expert-deep-dive.md)** for advanced topics
2. Study **[15-workflows-execution.md](15-workflows-execution.md)** for detailed execution flows
3. Reference component-specific deep dives as needed

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

## Documentation Notes

- All diagrams use Mermaid.js and render in modern browsers
- Code examples follow C++11 standards
- File paths and line numbers refer to the current codebase version
- Documentation is maintained alongside code changes

## Contributing

When updating this documentation:
1. Keep diagrams in Mermaid format for easy editing
2. Include file paths and line numbers for code references
3. Update the summary document when adding new sections
4. Maintain consistency in code examples and formatting
5. Follow the existing documentation structure and naming conventions
