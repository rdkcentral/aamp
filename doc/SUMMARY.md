# AAMP Documentation Summary

This document provides a quick reference guide to the AAMP architecture and documentation structure.

## Documentation Structure

1. **01_architecture_overview.html** - High-level system architecture
2. **02_code_organization.html** - Code organization and folder structure (to be created)
3. **03_apis_classes.html** - Important APIs and classes (to be created)
4. **04_diagrams_flowcharts.html** - Flow charts and diagrams (to be created)
5. **05_implementation_strategy.html** - Implementation strategy (to be created)
4. **04_current_design_analysis.html** - Current design problems and issues
5. **05_refactored_solutions.html** - Proposed refactoring solutions
6. **06_patch_file.html** - C++11 patch file

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

## Key Design Problems Identified

1. **Monolithic Files** - Files with 7000+ lines
2. **Raw Pointers** - Manual memory management
3. **Recursive Mutexes** - Indicates design issues
4. **God Object** - PrivateInstanceAAMP has too many responsibilities
5. **Tight Coupling** - Direct dependencies between components
6. **Long Methods** - Methods with 300+ lines
7. **Mixed C/C++** - Legacy C-style code

## Refactoring Solutions

1. **Smart Pointers** - Replace raw pointers with `std::unique_ptr`/`std::shared_ptr`
2. **Class Decomposition** - Extract managers (ConfigManager, StreamFactory, etc.)
3. **Method Decomposition** - Break large methods into smaller ones
4. **Eliminate Recursive Mutexes** - Restructure to use regular mutexes
5. **Interface-Based Design** - Program to interfaces
6. **RAII** - Resource management through constructors/destructors
7. **Exception Handling** - Standardize error handling

## Implementation Priority

### Phase 1: Critical (4-6 weeks)
- Memory management (smart pointers)
- Thread safety improvements
- Basic class decomposition

### Phase 2: Important (6-8 weeks)
- Method decomposition
- Error handling standardization
- Performance optimizations

### Phase 3: Enhancement (4-6 weeks)
- Interface abstractions
- Advanced optimizations
- Documentation updates

## Testing Strategy

1. **Unit Tests** - Test individual components in isolation
2. **Integration Tests** - Test component interactions
3. **Regression Tests** - Ensure existing functionality works
4. **Performance Tests** - Verify performance improvements

## Migration Strategy

1. Create new refactored classes alongside existing code
2. Gradually migrate functionality
3. Maintain backward compatibility
4. Comprehensive testing at each step
5. Remove old code once migration complete

