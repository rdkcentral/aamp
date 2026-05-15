---
description: Aamp overview
applyTo:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cxx"
  - "**/*.hxx"
  - "**/*.c"
---
# AAMP Codebase Guide for AI Agents

This document provides a guide for AI coding agents to understand the architecture, conventions, and key components of the AAMP codebase. It describes the "as-is" state of the project, including areas with legacy C-style code that are targets for future refactoring into modern C++.

## 1. Project Overview

AAMP (Advanced Adaptive Media Player) is a high-performance, embedded systems-focused video player developed by RDK Management. It is designed for reliability, efficiency, and speed, primarily for use in set-top boxes and other embedded devices. The player supports various streaming protocols, including HLS, MPEG-DASH, and progressive download, with a flexible architecture for handling different media formats and DRM technologies.

## 2. Core Technologies

-   **Language:** The existing AAMP codebase is predominantly C++11, but new code must target C++17. While the goal is modern C++, the codebase contains significant legacy C-style code (e.g., `memcpy`, raw pointers). Refactoring this to use modern C++ features (smart pointers, STL containers, RAII) is an ongoing task.
-   **Media Framework:** GStreamer is used for the underlying media pipeline, including demuxing, decoding, and rendering. AAMP interacts with GStreamer to manage the flow of media data.
-   **Build System:** CMake is the primary build system. All source files, dependencies, and build targets are defined in `CMakeLists.txt` files.
-   **Testing:** Google Test & Google Mock are the frameworks used for unit and functional testing.
-   **Networking:** cURL is used for the low-level downloading of manifests and media segments.

## 3. Key Architectural Pillars

The AAMP architecture is multi-threaded, event-driven, and built on several key pillars that are crucial to understand before making changes.

### 3.1. Streaming Protocol Abstraction

AAMP abstracts the specifics of different streaming protocols (HLS, DASH, Progressive) behind a common interface to keep the core player logic protocol-agnostic.

-   **`StreamAbstractionAAMP`**: This is the abstract base class that defines the common interface for all streaming protocol handlers.
-   **Implementations**:
    -   `FragmentCollector_HLS` (`fragmentcollector_hls.cpp`): Handles HLS (HTTP Live Streaming) playlist parsing, variant selection, and segment fetching logic.
    -   `FragmentCollector_MPD` (`fragmentcollector_mpd.cpp`): Handles MPEG-DASH manifest parsing, period/adaptation set processing, and segment fetching.
    -   `FragmentCollector_Progressive` (`fragmentcollector_progressive.cpp`): Handles simple progressive (MP4) downloads.
-   **Responsibility**: These classes are responsible for all protocol-specific logic, including parsing manifests, managing bitrate adaptation (ABR), and queuing fragment download requests.

### 3.2. Time Shift Buffer (TSB) / DVR

The TSB provides DVR-like functionality for live streams, allowing users to pause, rewind, and resume. This is one of the most complex and critical subsystems in AAMP.

-   **Core Components**:
    -   `AampTsbDataManager`: Manages the in-memory cache of media fragments. It uses a **doubly linked-list of `TsbFragmentData` objects** to store fragment information and data. This linked-list structure is fundamental to its operation, enabling efficient traversal and management of the buffer while avoiding the race conditions and complexities of time-based lookups.
    -   `AampTsbReader`: Provides a read interface into the TSB for the playback pipeline. It traverses the linked list in `AampTsbDataManager` to find and retrieve the next required fragment, correctly handling discontinuities and seek requests within the buffered content.
    -   `AampTsbMetaDataManager`: Manages time-associated metadata within the TSB, such as SCTE-35 ad markers, synchronizing them with the media fragments.

### 3.3. Asynchronous Task Management

AAMP is heavily multi-threaded. To manage asynchronous operations without blocking the main player loop, it uses dedicated schedulers and workers.

-   **`AampScheduler`**: A general-purpose scheduler that runs tasks on its own thread. It maintains a `std::deque` of `AsyncTaskObj` objects and executes them sequentially. This is used for operations that need to be deferred or run off the main thread but do not require a dedicated long-term worker.
-   **`AampTrackWorker`**: A dedicated worker thread for processing jobs related to a specific media track (e.g., video, audio, subtitles). This ensures that operations for a single track are serialized and handled efficiently. It uses a producer-consumer model with a single job slot and a condition variable.

### 3.4. Event-Driven Architecture

Components in AAMP are decoupled and communicate using a publisher-subscriber event system.

-   **`AampEventManager`**: A central singleton hub for dispatching events to registered listeners.
-   **`AampEventListener`**: An interface that components can implement to listen for specific events (e.g., playback state changes, errors, bitrate shifts, ad events).
-   **`AampEvent`**: Represents a dispatched event, containing its type and any associated data.

### 3.5. Configuration and Logging

-   **`AampConfig` (`AampConfig.cpp`)**: Provides a centralized class to manage player configuration settings. These settings can be loaded from a configuration file (`aamp.cfg`) or set programmatically.
-   **`aamplogging.cpp` / `AampLogManager.h`**: Implements the logging framework for the player, allowing for configurable log levels and directing output, which is essential for debugging on embedded devices.

## 4. Build System

-   **`CMakeLists.txt`**: The root file defines the project structure, targets, dependencies, and build options. It's the source of truth for how the application is built.
-   **`install-aamp.sh`**: A key shell script that automates the process of installing dependencies (like `libdash`) and building the AAMP player. It is a valuable reference for understanding the complete build and environment setup.

## 5. Testing Strategy

AAMP maintains a comprehensive testing suite located in the `test/` directory. Adhering to the testing strategy is critical for maintaining code quality.

-   **Framework**: Google Test and Google Mock are used for all unit tests.
-   **Location**: Unit tests are located under `test/utests/tests/`. Each class or component typically has its own subdirectory (e.g., `test/utests/tests/AampTsbReaderTests/`).
-   **Mocks and Fakes**: The codebase makes extensive use of mocks and fakes to achieve component isolation.
    -   `test/utests/mocks/`: Contains mock objects generated using Google Mock (`MOCK_METHOD`). These are used to verify interactions between the class under test and its dependencies.
    -   `test/utests/fakes/`: Contains fake implementations of classes. These provide a more lightweight, functional stub than a full mock and are used when the dependency's behavior is simple to simulate or when a mock is overly complex. This is a very common and important pattern in the codebase.

## 6. AAMP File Naming Conventions

- Class files should use an `Aamp` prefix where applicable (e.g., `AampConfig.cpp`, `AampScheduler.h`).
- File names must match the primary class implementation they contain.
- Use CamelCase for file names.
- Avoid underscores in new AAMP class file names.

## 7. Include Guard Convention

Use `#pragma once` as the preferred include guard for new header files. If traditional include guards are required, follow the pattern:

```cpp
#ifndef AAMP_CLASSNAME_H
#define AAMP_CLASSNAME_H

// ... header content ...

#endif /* AAMP_CLASSNAME_H */
```

## 8. AAMP Logging Guidance

Prefer AAMP logging macros over `logprintf` for all new code. The supported macros and their intended usage:

| Macro | Level | Usage |
|-------|-------|-------|
| `AAMPLOG_TRACE` | Trace | Development and triage-level detail; verbose output for deep debugging. |
| `AAMPLOG_INFO` | Info | Informative and debug messages; especially useful during tune operations. |
| `AAMPLOG_WARN` | Warn | Recoverable warnings or important runtime conditions worth noting. |
| `AAMPLOG_ERR` | Error | Severe or unexpected conditions that warrant investigation. |

### Logging Best Practices
- Use the appropriate log level for the message severity.
- Include relevant context in log messages (function name, identifiers, values).
- Avoid excessive logging in hot paths (e.g., per-fragment processing); prefer `AAMPLOG_TRACE` for these.
- Use correct printf format specifiers (see `cpp.instructions.md` for reference).
