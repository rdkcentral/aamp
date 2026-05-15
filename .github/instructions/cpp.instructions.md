---
description: C++ instructions
applyTo:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cxx"
  - "**/*.hxx"
---

# C++ Copilot Instructions

## C++ Guidelines

- The existing AAMP codebase is predominantly C++11. New code must target C++17. Use C++20+ features only when they are supported by the toolchain, clearly documented, and they provide a meaningful improvement in code quality.
- Always follow the guidelines at [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- Highlight when existing code being studied does not follow the core guidelines and suggest improvements
- Discourage the use of C-style code within C++ (e.g. avoid memcpy(), memcmp() and char* for strings). Emphasise memory safety
- Apply DRY (Don't Repeat Yourself) principles throughout the codebase
- Ensure cyclomatic complexity is minimised both for new code and when refactoring
- Use design patterns where possible
- Use const correctness throughout the codebase
- Prefer auto for type deduction when it improves readability
- Use constexpr for compile-time constants and functions when possible
- Leverage range-based for loops and STL algorithms
- Use explicit constructors to prevent implicit conversions
- Include the RDK copyright header in all new files
- Use Doxygen tags for documentation
- Use universal initialisation in all generated code

This project uses a strict set of C++ coding standards designed for embedded systems and video streaming performance requirements.

## 1. Naming Conventions
- Use `camelCase` for variables.
- Use `PascalCase` for class and function names.
- Use `UPPER_CASE` for constants.
- Prefix member variables with `m` (e.g., `mBufferSize`).
- Prefix global variables with `g` (e.g., `gConfigValue`).

## 2. Code Structure
- All classes must have declarations in `.h` files and implementations in `.cpp` files.
- Use namespaces to group functionality and avoid collisions.
- Prefer forward declarations when possible.
- Use `const` and `constexpr` appropriately.
- Pass by reference or pointer to avoid unnecessary copies.

## 3. Commenting & Documentation
- Use C-style Doxygen comment blocks (`/** ... */`) for all public API documentation.
- Do not use `///` style for AAMP public API documentation.
- Place function documentation with the declaration in the header file; do not duplicate it in the `.cpp` definition.
- Document non-obvious logic with concise `//` inline comments.
- All major classes must include a brief “Purpose” description.

## 4. Memory Management & Ownership
- Prefer modern C++ smart pointers for ownership (`std::unique_ptr`, `std::shared_ptr`).
- Use `std::unique_ptr` as the default for single ownership; use `std::shared_ptr` only when shared ownership is genuinely required.
- Use raw pointers or references only for non-owning access; never use raw owning pointers in new code.
- Avoid raw `new`/`delete` except when dealing with legacy code paths.
- Use RAII for all resource management.
- Follow the Rule of Zero: prefer classes that use smart pointers and containers so that no custom destructor, copy, or move operations are needed.

## 5. Coding Rules
- Braces are required for all conditional and loop blocks, including single-line bodies.
- Use constructor initializer lists to initialize data members where appropriate.
- Keep data members `private` where possible; provide accessor methods when needed.
- Avoid `friend` functions and classes unless there is a strong justification.
- Use `bool` for variables representing logical true/false state.
- Use appropriate standard container size and index types (e.g., `size_t`, `std::vector::size_type`) when indexing or sizing containers.
- Use `#pragma once` or traditional include guards in all header files.

## 6. Printf Format Specifier Reference
When formatting log output or diagnostic strings, use the correct specifiers:

| Type | Specifier |
|------|----------|
| `int` | `%d` |
| `unsigned int` | `%u` |
| `long` | `%ld` |
| `unsigned long` | `%lu` |
| `long long` | `%lld` |
| `unsigned long long` | `%llu` |
| `float` | `%f` |
| `double` | `%lf` |
| `size_t` | `%zu` |
| `uint64_t` | `PRIu64` (from `<cinttypes>`) |

Use `PRIu64` and related macros from `<cinttypes>` for fixed-width types to ensure portability.

## Cross-Language Interoperability (ctypes)

When defining data structures or function signatures that will be accessed from other languages (like Python's `ctypes` library), avoid using built-in C++ types like `int`, `long`, or `unsigned int`, as their size can vary across different platforms and compilers.

Instead, use the fixed-width integer types from the `<cstdint>` header to ensure a consistent memory layout.

### Guideline: Prefer `<cstdint>` types for external interfaces.

-   `int8_t`, `int16_t`, `int32_t`, `int64_t`
-   `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`

### Example: C++ Structure and Python `ctypes` Mapping

#### C++ Side (`player_stats.h`)

```cpp
#include <cstdint>

// This structure is designed to be shared with Python.
// Using fixed-width types ensures a predictable layout.
struct PlayerStats {
    uint64_t frames_decoded;
    uint32_t frames_dropped;
    int32_t current_bitrate;
    uint8_t audio_stream_id;
    uint8_t video_stream_id;
    // Note: Add padding if necessary for alignment on some platforms.
};

// C-style function to be exported for ctypes
extern "C" {
    void get_player_stats(PlayerStats* stats);
}
```

## Modern C++ Features to Leverage

### C++11 Features
- Smart pointers (unique_ptr, shared_ptr, weak_ptr)
- Range-based for loops
- Auto keyword for type deduction
- Lambda expressions
- Move semantics and rvalue references
- Initializer lists
- nullptr instead of NULL
- enum class instead of enum

### C++14 Features
- Generic lambdas
- std::make_unique (Remind that this is not available in C++11)
- Variable templates
- Return type deduction for functions

### C++17 Features
- std::optional for nullable values
- std::variant for type-safe unions
- Structured bindings
- if constexpr for conditional compilation
- std::string_view for efficient string handling

## Legacy Code Modernization Patterns

### Common Anti-patterns to Address
- Raw pointer ownership → Smart pointers
- Manual memory management → RAII
- C-style casts → static_cast, dynamic_cast, const_cast
- char* strings → std::string or std::string_view
- Manual loops → STL algorithms
- Macros for constants → constexpr variables
- void* for generic programming → Templates

### Refactoring Guidelines
- When modifying existing functions, suggest modern C++ equivalents
- Provide migration paths that maintain binary compatibility when needed
- Highlight opportunities to reduce complexity through modern features
- Suggest incremental improvements rather than complete rewrites

## Memory Safety Patterns

Smart pointers express ownership intent. Use `std::unique_ptr` by default for single ownership. Reserve `std::shared_ptr` for genuinely shared ownership. Use raw pointers or references only for non-owning access.

### Ownership Models
```cpp
// Unique ownership
std::unique_ptr<Resource> resource = std::make_unique<Resource>();

// Shared ownership
std::shared_ptr<Resource> shared_resource = std::make_shared<Resource>();

// Non-owning reference (prefer to raw pointers)
Resource& ref = *resource;
```

### RAII Examples
```cpp
class ResourceManager {
public:
    ResourceManager() : resource_(acquire_resource()) {}
    ~ResourceManager() { release_resource(resource_); }
    
    // Non-copyable, movable
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = default;
    ResourceManager& operator=(ResourceManager&&) = default;
    
private:
    ResourceHandle resource_;
};
```

## Error Handling Patterns

### Prefer Exceptions for Exceptional Cases
```cpp
// Good: Use exceptions for truly exceptional situations
void parse_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }
    // ... parsing logic
}
```

### Use std::optional for Expected Failures
```cpp
// Good: Use optional for operations that may legitimately fail
std::optional<int> parse_integer(const std::string& str) {
    int ret:
    try
    {
        ret = std::stoi(str);
    }
    catch (const std::exception&)
    {
        ret = std::nullopt;
    }
    return ret;
    }
```

### PROHIBITED: No Assert Statements in Production Code
**NEVER use assert() in production code.** Assert statements are disabled in release builds and should not be relied upon for error handling or validation in production environments.

#### Rationale
- `assert()` is compiled out in release builds (when `NDEBUG` is defined)
- Production code must handle all error conditions gracefully
- Proper error handling and logging must be used instead
- Unit tests should validate conditions, not production assertions

#### Instead of assert(), use:
```cpp
// BAD: Using assert in production code
assert(ptr != nullptr);
assert(index < container.size());

// GOOD: Proper error handling with logging
if (ptr == nullptr) {
    AAMPLOG_ERR("Null pointer detected in %s", __FUNCTION__);
    return ERROR_NULL_POINTER;
}

if (index >= container.size()) {
    AAMPLOG_WARN("Index %d out of bounds (size: %d)", index, container.size());
    return ERROR_INDEX_OUT_OF_BOUNDS;
}

// GOOD: Use exceptions for truly exceptional cases
if (critical_resource == nullptr) {
    throw std::runtime_error("Critical resource initialization failed");
}

// GOOD: Use std::optional for operations that may fail
std::optional<Value> get_value_safely(int index) {
    if (index >= 0 && index < container.size()) {
        return container[index];
    }
    return std::nullopt;
}
```

#### For Development/Debug Builds Only
If you need debug-time validation, use conditional compilation:
```cpp
#ifdef DEBUG
    if (condition_that_should_never_fail) {
        AAMPLOG_ERR("Debug assertion failed: %s", "condition description");
        // Handle gracefully even in debug builds
    }
#endif
```

## Performance Considerations for Embedded Systems

### Move Semantics and Efficient Resource Transfer

Move semantics are critical for performance in embedded systems. Always prefer moving over copying when transferring ownership or when temporary objects are involved.

#### Key Principles
- Use `std::move()` to explicitly move from lvalue references when appropriate
- Implement move constructors and move assignment operators for resource-owning classes
- Return large objects by value and rely on move semantics for efficiency
- Use perfect forwarding in templates to preserve value categories

#### Move Constructor and Assignment Examples
```cpp
class StreamBuffer {
public:
    // Move constructor
    StreamBuffer(StreamBuffer&& other) noexcept 
        : data_(std::exchange(other.data_, nullptr))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0)) {
    }
    
    // Move assignment operator
    StreamBuffer& operator=(StreamBuffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }
    
private:
    uint8_t* data_;
    size_t size_;
    size_t capacity_;
};
```

#### Perfect Forwarding in Factory Functions
```cpp
template<typename T, typename... Args>
std::unique_ptr<T> make_stream_component(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
```

#### Efficient Container Operations
```cpp
// Prefer emplace operations to avoid unnecessary copies
std::vector<StreamData> streams;
streams.emplace_back(stream_id, std::move(buffer_data));

// Use move when transferring ownership
auto new_stream = std::move(old_stream);

// Return by value - move semantics handle efficiency
StreamConfig create_default_config() {
    StreamConfig config;  // Local object
    // ... configure object
    return config;  // Moved automatically (NRVO/move)
}
```

### Minimize Allocations
- Prefer stack allocation over heap allocation
- Use object pools for frequently allocated/deallocated objects
- Consider std::array over std::vector for fixed-size collections
- Use reserve() for std::vector when size is known

### Efficient String Handling
```cpp
// Prefer string_view for read-only string parameters
void process_string(std::string_view str);

// Use string concatenation efficiently
std::string result;
result.reserve(estimated_size);  // Avoid reallocations
```

### Template Usage
- Use templates for zero-cost abstractions
- Prefer constexpr functions for compile-time computation
- Use SFINAE or concepts to constrain templates appropriately

## Documentation Examples

```cpp
/**
 * @brief Manages video stream playback with embedded system optimizations
 * 
 * This class provides efficient video stream handling optimized for
 * embedded systems with limited resources. It implements RAII principles
 * for automatic resource cleanup.
 * 
 * @note Thread-safe for concurrent read operations
 * @warning Not thread-safe for write operations
 */
class VideoStreamManager {
public:
    /**
     * @brief Constructs a video stream manager with specified buffer size
     * 
     * @param buffer_size Size of the internal buffer in bytes
     * @param stream_url URL of the video stream to manage
     * 
     * @throws std::invalid_argument if buffer_size is zero
     * @throws std::runtime_error if stream_url is malformed
     */
    VideoStreamManager(size_t buffer_size, std::string_view stream_url);
    
    /**
     * @brief Starts playback of the configured stream
     * 
     * @return true if playback started successfully, false otherwise
     * 
     * @pre Stream must be configured and resources allocated
     * @post If successful, playback state is active
     */
    bool start_playback() noexcept;
    
private:
    std::unique_ptr<StreamBuffer> buffer_;  ///< Internal stream buffer
    std::string stream_url_;                ///< URL of the current stream
};
```

## Doxygen Style Guide

Use C-style comment blocks (`/** ... */`) for Doxygen documentation in all C++ header and source files.

### Function Documentation Example

```cpp
/**
 * @brief A brief description of what the function does.
 *
 * A more detailed description of the function's behavior, purpose,
 * and any relevant context.
 *
 * @param param1 Description of the first parameter.
 * @param param2 Description of the second parameter.
 *
 * @return A description of the return value.
 *
 * @note Optional note about the function.
 * @warning Optional warning about potential issues or side effects.
 */
int exampleFunction(int param1, const std::string& param2);
```

### Class Documentation Example
```cpp
/**
 * @class MyClass
 * @brief A brief description of the class.
 *
 * More detailed information about the class's responsibilities
 * and how it should be used.
 */
class MyClass {
public:
    // ...
};
```

### File Documentation Example
```cpp
/**
 * @file AampConfig.h
 * @brief Configuration management for AAMP player.
 *
 * Provides centralized handling of player configuration
 * settings loaded from file or set programmatically.
 */
```

### Data Member Documentation Example
```cpp
class StreamManager {
private:
    std::unique_ptr<StreamBuffer> mBuffer;  /**< Internal stream buffer */
    std::string mStreamUrl;                 /**< URL of the current stream */
    size_t mBufferSize;                     /**< Buffer size in bytes */
};
```

### Enum Documentation Example
```cpp
/**
 * @enum PlaybackState
 * @brief Represents the current state of the player.
 */
enum class PlaybackState
{
    eIDLE,       /**< Player is idle */
    ePLAYING,    /**< Playback is active */
    ePAUSED,     /**< Playback is paused */
    eSTOPPED     /**< Playback has stopped */
};
```

### Macro Documentation Example
```cpp
/**
 * @def AAMP_MAX_BUFFER_SIZE
 * @brief Maximum buffer size in bytes for stream buffering.
 */
#define AAMP_MAX_BUFFER_SIZE (4 * 1024 * 1024)
```

### Static / Global Variable Documentation Example
```cpp
/**
 * @brief Default timeout for network requests in milliseconds.
 */
static constexpr int kDefaultNetworkTimeoutMs = 10000;
```
