# Build System

## Overview

AAMP uses CMake, a cross-platform build system generator, for build configuration, dependency management, and compilation. CMake provides a unified build system that generates platform-specific build files (Makefiles for Linux, Xcode projects for macOS, Visual Studio projects for Windows) while maintaining a single source configuration.

The build system handles complex dependency resolution, platform detection, feature flags, and optional component inclusion. It supports multiple build configurations (Debug, Release), platform-specific optimizations, and conditional compilation based on available libraries and platform capabilities. The modular CMake structure allows selective building of components (core player, JavaScript bindings, tests) and integration with RDK build systems.

## Build Configuration

**File**: `CMakeLists.txt` (root and subdirectories)

The CMake configuration is organized hierarchically, with the root `CMakeLists.txt` setting global options and including subdirectory configurations for different components. Each major subsystem (downloader, DRM, fragment collectors) may have its own `CMakeLists.txt` for component-specific build rules.

### Key Dependencies

AAMP requires several external libraries that are detected and linked during the build process:

- **GStreamer** (1.18.0+): Core media pipeline framework providing demuxing, decoding, and rendering capabilities. Required for all AAMP builds. CMake uses `pkg-config` to locate GStreamer installation and required plugins (`gstreamer-1.0`, `gstreamer-app-1.0`, `gstreamer-video-1.0`, `gstreamer-audio-1.0`). Version checking ensures compatibility with AAMP's GStreamer API usage.

- **libcurl**: HTTP/HTTPS client library for manifest downloads, fragment downloads, and license acquisition. Required for network operations. CMake detects libcurl via `find_package(CURL)` or `pkg-config`, checking for required features (SSL support, HTTP/2 support).

- **libdash**: DASH manifest parsing library for MPEG-DASH support. Required for DASH playback functionality. CMake locates libdash installation and links against libdash libraries. DASH support can be conditionally disabled if libdash is unavailable.

- **libxml2**: XML parsing library used for DASH MPD parsing and XML-based configuration files. Required for DASH support and some configuration parsing. CMake detects via `find_package(LibXml2)` or `pkg-config`.

- **OpenSSL**: Cryptographic library for HTTPS support, SSL certificate validation, and native AES decryption (for HLS). Required for secure content delivery. CMake detects via `find_package(OpenSSL)` and checks for required components (SSL, Crypto).

- **cjson**: Lightweight JSON parsing library for JSON configuration file support (`aampcfg.json`). Required for JSON configuration parsing. CMake detects via `find_package(cjson)` or includes bundled cjson source if system library unavailable.

**Optional Dependencies**:
- **JavaScriptCore**: WebKit JavaScript engine for JavaScript bindings (UVE API). Required only when `CMAKE_WPEWEBKIT_JSBINDINGS` is enabled. CMake detects WebKit installation and JavaScriptCore framework.
- **Thunder/RPC**: RDK Thunder framework for platform service integration. Required only for RDK platform builds with Thunder integration enabled.

### Build Options

CMake provides numerous configuration options that control build behavior and feature inclusion:

- **`CMAKE_WPEWEBKIT_JSBINDINGS`**: Enable/disable JavaScript bindings compilation (default: based on platform). When enabled, builds `jsbindings/` components and links against JavaScriptCore. JavaScript bindings are required for WebKit/WebApp integration but not for native C++ applications.

- **`CMAKE_TELEMETRY_2_0_REQUIRED`**: Enable telemetry 2.0 support (default: based on platform). When enabled, includes telemetry event reporting and metrics collection. Telemetry support requires RDK telemetry libraries and is typically enabled for RDK platform builds.

- **`CMAKE_USE_RDK_PLUGINS`**: Use RDK-specific GStreamer plugins (default: based on platform detection). When enabled, links against RDK GStreamer plugins for platform-specific optimizations (hardware decoders, custom sinks). RDK plugins are required for optimal performance on RDK platforms.

- **`UTEST_ENABLED`**: Enable unit test compilation (default: OFF). When enabled, builds test executables in `test/utests/` directory. Tests require Google Test framework (`find_package(GTest)`) and are typically built separately from production code.

- **`CMAKE_BUILD_TYPE`**: Build type selection (Debug, Release, RelWithDebInfo, MinSizeRel). Debug builds include debug symbols and disable optimizations. Release builds enable optimizations and remove debug symbols. RelWithDebInfo provides optimized code with debug symbols for profiling.

- **`CMAKE_INSTALL_PREFIX`**: Installation directory prefix (default: `/usr/local`). Specifies where `make install` installs libraries, headers, and executables. For RDK platforms, typically set to platform-specific installation directories.

**Platform Detection**: CMake automatically detects the target platform (Linux, macOS, RDK) and sets appropriate defaults for platform-specific options. Platform detection uses `CMAKE_SYSTEM_NAME`, `CMAKE_SYSTEM_PROCESSOR`, and custom platform detection logic.

## Build Process

### Configuration

The build configuration phase generates platform-specific build files from CMake configuration:

```bash
mkdir build
cd build
cmake ..
```

**Configuration Steps**:
- **CMake Execution**: Running `cmake` processes `CMakeLists.txt` files, detecting dependencies, checking compiler capabilities, and generating build files (Makefiles, Xcode projects, etc.). CMake outputs dependency detection results, configuration summary, and any warnings or errors.

- **Dependency Detection**: CMake searches for required libraries using `find_package()`, `pkg-config`, or custom detection scripts. Detection results are cached in `CMakeCache.txt` to speed up subsequent configurations. Missing dependencies trigger configuration errors unless marked as optional.

- **Build File Generation**: Based on detected dependencies and configuration options, CMake generates platform-specific build files. On Linux, generates Makefiles. On macOS, can generate Xcode projects (`cmake -G Xcode`). Build files contain compiler flags, include paths, library paths, and link commands.

**Configuration Customization**: Additional CMake options can be specified:
- `-DCMAKE_BUILD_TYPE=Release`: Set build type
- `-DCMAKE_INSTALL_PREFIX=/opt/aamp`: Set installation directory
- `-DCMAKE_WPEWEBKIT_JSBINDINGS=ON`: Enable JavaScript bindings
- `-DUTEST_ENABLED=ON`: Enable unit tests

### Compilation

The compilation phase builds AAMP libraries and executables from source code:

```bash
make
```

**Compilation Process**:
- **Source Compilation**: The build system compiles C++ source files (`.cpp`) into object files (`.o`) using the configured C++ compiler (typically `g++` or `clang++`). Compiler flags include include paths (`-I`), preprocessor definitions (`-D`), and optimization flags (`-O2` for Release builds).

- **Library Creation**: Object files are archived into static libraries (`.a`) or linked into shared libraries (`.so` on Linux, `.dylib` on macOS) using `ar` or linker commands. AAMP typically builds as a shared library (`libaamp.so`) for dynamic linking by applications.

- **Executable Linking**: Test executables and CLI tools (`aampcli`) are linked from object files and libraries to create executables. Linking includes AAMP libraries, dependency libraries (GStreamer, libcurl, etc.), and system libraries.

- **Parallel Compilation**: `make` supports parallel compilation (`make -j4` for 4 parallel jobs) to speed up builds on multi-core systems. CMake-generated Makefiles include proper dependency tracking to ensure correct build order.

**Build Output**: Compiled libraries and executables are placed in build directory subdirectories (`lib/`, `bin/`). Build artifacts can be large (libraries may be several MB) and include debug symbols in Debug builds.

### Installation

The installation phase copies built artifacts to system directories:

```bash
make install
```

**Installation Process**:
- **Library Installation**: Shared libraries (`libaamp.so`) are copied to `$CMAKE_INSTALL_PREFIX/lib/` (typically `/usr/local/lib/`). Libraries are installed with proper permissions and may run `ldconfig` to update dynamic linker cache.

- **Header Installation**: Header files (`.h`, `.hpp`) are copied to `$CMAKE_INSTALL_PREFIX/include/aamp/` for application compilation. Headers are organized in subdirectories matching source structure.

- **Executable Installation**: Executables (`aampcli`) are copied to `$CMAKE_INSTALL_PREFIX/bin/` with execute permissions.

- **Configuration Files**: Example configuration files or documentation may be installed to `$CMAKE_INSTALL_PREFIX/share/aamp/`.

**Installation Permissions**: Installation typically requires root/sudo privileges to write to system directories. For development, `CMAKE_INSTALL_PREFIX` can be set to user-writable directories to avoid privilege requirements.

## Platform-Specific Builds

### Ubuntu/Simulator

Ubuntu builds target development and testing environments:

- **Standard Build**: Uses standard Linux libraries (GStreamer from system packages, libcurl from repositories). No platform-specific optimizations, enabling portable builds for development and CI/CD systems.

- **Development Tools**: Includes debug symbols, logging, and development-friendly configurations. Test harness (`UTEST_ENABLED`) is typically enabled for development builds to support unit testing and debugging.

- **Test Harness**: Builds test executables and test infrastructure for running unit tests and integration tests. Tests can be executed via `ctest` or directly running test executables.

### RDK Platform

RDK platform builds target production set-top boxes and embedded devices:

- **Platform-Specific Libraries**: Links against RDK platform libraries (RDK GStreamer plugins, platform DRM middleware, RDK telemetry). Platform libraries provide hardware acceleration and platform integration.

- **RDK Plugins**: Uses RDK-specific GStreamer plugins (`CMAKE_USE_RDK_PLUGINS`) for hardware-accelerated decoding, platform sinks (Westeros, Rialto), and SOC-specific optimizations. Plugins are provided by RDK platform SDK.

- **Production Optimizations**: Release builds with optimizations (`-O2`, `-O3`) and platform-specific compiler flags. Strips debug symbols to reduce binary size. May include platform-specific security hardening and memory optimizations.

**RDK Integration**: RDK builds integrate with RDK build systems (Yocto, OpenEmbedded) for cross-compilation and platform-specific toolchain configuration. Build outputs are packaged into RDK platform images.

### macOS

macOS builds target development and simulator environments:

- **Xcode Project Generation**: CMake can generate Xcode projects (`cmake -G Xcode`) for macOS development. Xcode projects provide IDE integration, debugging, and macOS-specific build tools.

- **Framework Dependencies**: macOS uses frameworks instead of shared libraries. CMake handles framework linking (`-framework GStreamer`) and framework search paths. Dependencies may be provided via Homebrew or manual installation.

**macOS Considerations**: macOS builds may require code signing for certain features and must handle macOS-specific library paths and framework resolution. Build outputs are `.dylib` shared libraries instead of `.so`.

## Summary

The build system provides comprehensive build infrastructure for AAMP:

- **Cross-Platform Support**: CMake enables builds on Linux, macOS, RDK platforms, and other Unix-like systems. Single source configuration generates platform-specific build files, maintaining portability while leveraging platform capabilities.

- **Flexible Configuration**: Extensive CMake options allow customization of build behavior, feature inclusion, and platform-specific settings. Configuration can be tailored for development (debug symbols, tests) or production (optimizations, minimal features).

- **Dependency Management**: Automatic dependency detection and linking simplifies build setup. CMake handles complex dependency resolution, version checking, and optional dependency handling, reducing manual configuration effort.

- **Platform-Specific Builds**: Platform detection and conditional compilation enable optimal builds for each target platform. RDK builds leverage platform capabilities (hardware acceleration, platform libraries), while Linux/macOS builds provide portable development environments.
