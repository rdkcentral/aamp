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

# AAMP Architecture

## Overview

This document provides an architectural overview of the Advanced Adaptive Media Player (AAMP). For comprehensive architectural details, design patterns, and module descriptions, refer to [.github/instructions/aamp.instructions.md](.github/instructions/aamp.instructions.md).

## Core Components

### Native Engine (C++)
- **Foundation**: Built on GStreamer for low-level media handling
- **Optimization**: Designed for performance, memory efficiency, and embedded constraints
- **Primary Use Case**: Core playback engine for RDK-based devices

### JavaScript Bindings (UVE)
- **Interface**: Universal Video Engine (UVE) API provides JavaScript access to AAMP
- **Implementation**: WebKit Injectedbundle on RDK platforms
- **Compatibility**: Also available as API wrapper for non-RDK browsers

### Key Architectural Principles

**SOLID Design**
- Single Responsibility: Each module has one clear purpose
- Open/Closed: Extend via interfaces, not by modifying core logic
- Liskov Substitution: Subtypes are interchangeable
- Interface Segregation: Small, specific interfaces
- Dependency Inversion: Depend on abstractions

**Modern C++ Patterns**
- RAII (Resource Acquisition Is Initialization) for all resources
- Smart pointers (`unique_ptr`, `shared_ptr`) instead of raw pointers
- Composition over inheritance
- Low cyclomatic complexity

**Video Streaming Focus**
- Low latency playback
- Correct buffering behavior
- Real-time performance guarantees
- Minimal memory footprint

## Module Organization

- **Fragment Collectors**: HLS (`fragmentcollector_hls.cpp`), DASH (`fragmentcollector_mpd.cpp`), Progressive MP4
- **DRM Support**: Clear Key, AES-128, PlayReady, Widevine
- **Buffering & ABR**: Adaptive bitrate management and buffer control
- **Event Management**: Event dispatching and listener coordination
- **Diagnostics**: Profiling, telemetry, and logging

## Threading & Concurrency

AAMP uses a scheduler-based approach with worker threads for asynchronous operations:
- Fragment downloading happens on worker threads
- Main playback synchronization on GStreamer thread
- Thread-safe event handling through event manager

## Configuration System

Configuration has priority order (lowest to highest):
1. AAMP defaults in code
2. Operator settings (RFC/ENV variables)
3. Stream-provided settings
4. Application settings
5. Developer configuration (`/opt/aamp.cfg`, `/opt/aampcfg.json`)

See [CONFIGURATION.md](CONFIGURATION.md) for detailed options.

## Further Reading

- [AAMP Instructions](.github/instructions/aamp.instructions.md) - Deep architectural details
- [C++ Guidelines](.github/instructions/cpp.instructions.md) - Coding standards
- [Testing Strategy](TESTING.md) - L1 unit tests and test infrastructure
