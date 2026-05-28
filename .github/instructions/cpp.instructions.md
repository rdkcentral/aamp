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

- The existing AAMP codebase is predominantly C++11. **All new production and L1 test code must target C++17.** Do not introduce C++20-only language or library features (e.g. `std::span`, concepts, ranges, `std::format`, coroutines) into the active codebase. C++20 may be referenced only as forward-looking context, clearly marked as non-current guidance.
- Legacy C++11 patterns exist for compatibility and interoperability with surrounding APIs. They should not be copied into new code unless the surrounding API or architecture requires it.
- Follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) where they do not conflict with repository-specific conventions in this directory.
- When generating new code, follow this guidance directly. When reviewing
  or editing existing code, apply this guidance to the lines being
  changed; do not raise drive-by Core-Guidelines comments on unrelated
  surrounding code unless the user asks for a broader review.
- Discourage the use of C-style code within C++ (e.g. avoid memcpy(), memcmp() and char* for strings). Emphasise memory safety
- Apply DRY (Don't Repeat Yourself) principles throughout the codebase
- Ensure cyclomatic complexity is minimised both for new code and when refactoring
- Use design patterns where possible
- Use const correctness throughout the codebase
- Use `auto` where it removes redundant type repetition (e.g. iterator
  declarations, `std::make_unique` results, range-based `for` over
  containers) or where the deduced type is obvious from the initializer.
  Prefer an explicit type where it makes ownership, conversions, numeric
  width, or the public API contract clearer to a reader.
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
- Use Doxygen `///` style comments for public API.
- Document non-obvious logic with concise `//` inline comments.
- All major classes must include a brief “Purpose” description.

## 4. Memory Management
- Prefer modern C++ smart pointers (`std::unique_ptr`, `std::shared_ptr`).
- Avoid raw new/delete except when dealing with legacy code paths.
- Use RAII for all resource management.

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
- `auto` for type deduction (apply readability rules from the C++
  Guidelines section above)
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

### C++20 and Later (Not Currently Permitted in New Code)
Features such as concepts, ranges, `std::span`, `std::format`, and
coroutines are out of scope for active development. Do not propose them
in new production or L1 test code. They are listed here only so reviewers
recognise them as deliberately deferred, not omitted by oversight.

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
- When modifying existing functions, suggest modern C++17 equivalents
  that are **local to the change being made**.
- Provide migration paths that maintain binary compatibility when needed.
- Highlight opportunities to reduce complexity through modern features.
- Suggest incremental improvements rather than complete rewrites.

### Modernization Scope Discipline
Modernization must be proportionate to the task at hand.

**Encouraged (local, safe improvements):**
- Applying RAII to a resource introduced or touched by the current change.
- Replacing raw owning pointers with `std::unique_ptr` / `std::shared_ptr`
  in code already being edited.
- Swapping C-style buffers for STL containers (`std::string`,
  `std::vector`) in functions under modification.
- Tightening ownership and lifetime semantics on classes already in scope.

**Discouraged:**
- Opportunistic repository-wide rewrites unrelated to the current change.
- Large stylistic refactors across files that are not otherwise modified.
- Drive-by API changes that broaden the diff beyond the task description.
- Modernization that alters observable behaviour without a clear test
  strategy.

When in doubt, prefer the smaller change. Larger refactors should be
proposed as their own task with their own review.

## Memory Safety Patterns

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

### Exception Usage in AAMP

AAMP runs on embedded targets and inside real-time media playback paths.
Exception usage must be **conservative and consistent with surrounding
code**. Do not introduce exceptions into hot playback, buffering, ABR, or
GStreamer-callback paths where the existing convention is error codes,
status enums, or `bool` return values.

Guidelines:
- Match the error-handling style of the surrounding module. Do not mix
  paradigms within a single component.
- Prefer explicit, predictable failure handling (return codes,
  `std::optional`, status enums) on runtime/playback paths.
- Reserve exceptions for genuinely exceptional construction-time or
  configuration-time failures where the caller has no recovery path and
  the surrounding code already uses exceptions.
- Never throw across a C ABI boundary (GStreamer callbacks, `extern "C"`
  entry points, ctypes-facing functions).
- Ensure ownership of failure handling is explicit at the API boundary.

```cpp
// Acceptable: construction-time failure in a module that already uses
// exceptions, with no viable recovery path for the caller.
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
- Use templates for zero-cost abstractions.
- Prefer constexpr functions for compile-time computation.
- Use SFINAE / `std::enable_if` / type traits to constrain templates.
  C++20 concepts are not currently permitted in new code.

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
