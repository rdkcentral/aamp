# AAMP Documentation

This directory contains comprehensive documentation for the AAMP (Advanced Adaptive Media Player) codebase, including architecture analysis, design documentation, and refactoring proposals.

## Documentation Structure

### Core Documentation

1. **[index.html](index.html)** - Main entry point with navigation to all documents (HTML version)
1. **[README.md](README.md)** - This file - Main entry point with navigation to all documents (Markdown version)

2. **[01_architecture_overview.md](01_architecture_overview.md)**
   - High-level system architecture
   - Component relationships
   - Data flow architecture
   - Threading model
   - Configuration system
   - Event system

3. **[02_code_organization.md](02_code_organization.md)**
   - Repository structure
   - Folder organization
   - Main entry points
   - Execution flows (Tune API, Fragment Download, ABR)
   - Class relationships
   - Module interactions
   - Threading model

4. **[03_apis_classes.md](03_apis_classes.md)**
   - Public API documentation (PlayerInstanceAAMP)
   - Core classes (PrivateInstanceAAMP, StreamAbstraction, etc.)
   - Event system
   - Configuration system
   - DRM system
   - Utility classes
   - Code examples

### Design Analysis & Refactoring

5. **[04_current_design_analysis.md](04_current_design_analysis.md)**
   - Code organization issues
   - Memory management problems
   - Thread safety issues
   - Design pattern violations
   - Code quality issues
   - Performance issues
   - C++11 compliance issues
   - Specific code issues with line numbers

6. **[05_refactored_solutions.md](05_refactored_solutions.md)**
   - Memory management refactoring (smart pointers)
   - Thread safety improvements
   - Class decomposition
   - Method decomposition
   - Parameter object pattern
   - Error handling improvements
   - Performance optimizations

7. **[06_patch_file.md](06_patch_file.md)**
   - Actual C++11 refactored code patches
   - Memory management changes
   - Thread safety improvements
   - New class implementations
   - CMakeLists.txt updates
   - Migration checklist

8. **[AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md)**
   - Comprehensive high-level design overview
   - RDK-E integration details and usage patterns
   - Component interactions and data flows
   - Deployment scenarios and performance optimizations
   - Security considerations and monitoring

### Architecture Components

9. **[07_middleware_architecture.md](07_middleware_architecture.md)**
   - Middleware layer architecture and codeflow
   - GStreamer integration and SOC-specific implementations
   - Platform detection and hardware acceleration

10. **[08_closedcaptions_architecture.md](08_closedcaptions_architecture.md)**
    - Closed captions rendering and management
    - CC decoder integration and style handling

11. **[09_drm_architecture.md](09_drm_architecture.md)**
    - DRM system architecture and implementation
    - License acquisition and content decryption
    - Multiple DRM system support (Widevine, PlayReady, etc.)

12. **[10_osx_build_support.md](10_osx_build_support.md)**
    - macOS build support and patches
    - Platform-specific configurations

13. **[11_subtec_architecture.md](11_subtec_architecture.md)**
    - Subtec subtitle renderer implementation
    - WebVTT and TTML support

14. **[12_subtitle_architecture.md](12_subtitle_architecture.md)**
    - Subtitle parsing base interface
    - Subtitle data structures and APIs

15. **[13_contentsecuritymanager_architecture.md](13_contentsecuritymanager_architecture.md)**
    - Content security manager implementation
    - DRM license acquisition and watermarking

16. **[14_playerisobmff_architecture.md](14_playerisobmff_architecture.md)**
    - ISO BMFF container parsing
    - DASH and HLS container support

17. **[15_playerjsonobject_architecture.md](15_playerjsonobject_architecture.md)**
    - JSON object wrapper and utilities
    - cJSON integration

18. **[16_playerlogmanager_architecture.md](16_playerlogmanager_architecture.md)**
    - Centralized logging system
    - Log level management and output routing

19. **[17_aampabr_architecture.md](17_aampabr_architecture.md)**
    - Adaptive bitrate management
    - Network bandwidth detection and profile switching

20. **[18_aampmetrics_architecture.md](18_aampmetrics_architecture.md)**
    - Session metrics and statistics
    - JSON-based reporting

21. **[SUMMARY.md](SUMMARY.md)**
   - Quick reference guide
   - Key architecture components
   - Main execution flows
   - Design problems summary
   - Refactoring solutions overview
   - Implementation priority

## How to Use This Documentation

### For New Developers
1. Start with [01_architecture_overview.md](01_architecture_overview.md) to understand the system
2. Read [02_code_organization.md](02_code_organization.md) to understand code structure
3. Review [03_apis_classes.md](03_apis_classes.md) for API usage

### For Code Reviewers
1. Review [04_current_design_analysis.md](04_current_design_analysis.md) for identified issues
2. Check [05_refactored_solutions.md](05_refactored_solutions.md) for proposed fixes
3. Verify [06_patch_file.md](06_patch_file.md) for actual code changes

### For RDK-E Developers
1. Read [AAMP_High_Level_Design_and_RDK-E_Usage.md](AAMP_High_Level_Design_and_RDK-E_Usage.md) for RDK-E integration
2. Review [07_middleware_architecture.md](07_middleware_architecture.md) for middleware details
3. Check [09_drm_architecture.md](09_drm_architecture.md) for DRM integration

### For Refactoring Implementation
1. Follow the migration checklist in [06_patch_file.md](06_patch_file.md)
2. Reference [05_refactored_solutions.md](05_refactored_solutions.md) for design patterns
3. Use [SUMMARY.md](SUMMARY.md) as a quick reference

## Key Findings

### Critical Issues
- **Memory Management**: Raw pointer usage throughout codebase
- **Thread Safety**: Recursive mutexes indicate design problems
- **Code Organization**: Monolithic files (7000+ lines)
- **Tight Coupling**: Direct dependencies between components

### Proposed Solutions
- **Smart Pointers**: Replace raw pointers with `std::unique_ptr`/`std::shared_ptr`
- **Class Decomposition**: Extract managers (ConfigManager, StreamFactory)
- **Eliminate Recursive Mutexes**: Restructure to use regular mutexes
- **Method Decomposition**: Break large methods into smaller ones

## Implementation Timeline

- **Phase 1 (4-6 weeks)**: Memory management, basic thread safety
- **Phase 2 (6-8 weeks)**: Method decomposition, error handling
- **Phase 3 (4-6 weeks)**: Interface abstractions, optimizations

## Notes

- All diagrams use Mermaid.js and render in modern browsers
- Code examples follow C++11 standards
- Line numbers refer to the analyzed codebase version
- Patches should be applied incrementally with testing

## Contributing

When updating this documentation:
1. Keep diagrams in Mermaid format for easy editing
2. Include line numbers for code references
3. Update the summary document when adding new sections
4. Maintain consistency in code examples

