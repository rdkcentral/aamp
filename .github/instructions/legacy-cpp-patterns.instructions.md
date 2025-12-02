---
description: C++ refactoring instructions
applyTo:
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cxx"
  - "**/*.hxx"
---

# Legacy C++ Modernization Instructions

## Analyzing Legacy Code

When reviewing, refactoring or updating existing complex C++ code, focus on identifying these common legacy patterns and suggest modern alternatives:

## Common Legacy Patterns and Modernization

### 1. Raw Pointer Management
```cpp
// Legacy pattern (avoid)
class LegacyResource {
    char* buffer_;
    size_t size_;
public:
    LegacyResource(size_t size) : size_(size) {
        buffer_ = new char[size];
    }
    ~LegacyResource() {
        delete[] buffer_;
    }
};

// Modern alternative (prefer)
class ModernResource {
    std::vector<char> buffer_;
public:
    explicit ModernResource(size_t size) : buffer_(size) {}
    // Automatic cleanup, exception-safe
};
```

### 2. C-style Casts
```cpp
// Legacy pattern (avoid)
void* ptr = get_some_pointer();
SomeClass* obj = (SomeClass*)ptr;

// Modern alternative (prefer)
void* ptr = get_some_pointer();
auto* obj = static_cast<SomeClass*>(ptr);
// Or better yet, avoid void* entirely with templates
```

### 3. String Handling
```cpp
// Legacy pattern (avoid)
void process_string(const char* str) {
    size_t len = strlen(str);
    char* copy = new char[len + 1];
    strcpy(copy, str);
    // ... process
    delete[] copy;
}

// Modern alternative (prefer)
void process_string(std::string_view str) {
    std::string copy{str};  // Safe, automatic memory management
    // ... process
}
```

### 4. Error Codes vs Exceptions
```cpp
// Legacy pattern (often found in embedded systems)
enum ErrorCode {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    MEMORY_ERROR = 2
};

ErrorCode load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return FILE_NOT_FOUND;
    
    char* buffer = malloc(1024);
    if (!buffer) {
        fclose(file);
        return MEMORY_ERROR;
    }
    
    // ... rest of function
    return SUCCESS;
}

// Modern alternative with std::optional for expected failures
std::optional<Config> load_config(std::string_view filename) {
    std::ifstream file{std::string(filename)};
    if (!file) return std::nullopt;
    
    // Use exceptions only for truly exceptional cases
    Config config;
    if (!parse_config(file, config)) {
        return std::nullopt;
    }
    
    return config;
}
```

## Incremental Modernization Strategy

### Phase 1: Safety Improvements
1. Replace raw pointers with smart pointers
2. Add const correctness
3. Replace C-style casts with C++ casts
4. Use RAII for resource management

### Phase 2: Modern Features
1. Use auto for type deduction
2. Replace manual loops with STL algorithms
3. Use range-based for loops
4. Apply move semantics where beneficial

### Phase 3: Advanced Features
1. Use std::optional and std::variant
2. Apply constexpr where possible
3. Use structured bindings (C++17)
4. Consider concepts (C++20) for template constraints

## Maintaining Compatibility

### When Working with Existing APIs
```cpp
// If you must interface with C APIs
extern "C" {
    int legacy_c_function(const char* str, int len);
}

// Wrap in modern C++ interface
class ModernWrapper {
public:
    int call_legacy_function(std::string_view str) {
        return legacy_c_function(str.data(), 
                                static_cast<int>(str.length()));
    }
};
```

### Gradual Interface Evolution
```cpp
// Step 1: Add new modern interface alongside old one
class DataProcessor {
public:
    // Legacy interface - mark as deprecated
    [[deprecated("Use process_data(std::span<const byte>) instead")]]
    void process_data(const char* data, size_t len);
    
    // New modern interface
    void process_data(std::span<const std::byte> data);
    
private:
    void process_data_impl(std::span<const std::byte> data);
};
```

## Decoupling and Dependency Management

A primary goal of modernization is to reduce tight coupling between components. This makes the code more modular, testable, and maintainable.

### Common Legacy Coupling Patterns to Address

-   **Direct Instantiation of Concrete Classes:** A class directly creating an instance of another concrete class (`new OtherClass()`).
-   **Global Singletons:** Components directly accessing a global singleton instance (`Singleton::getInstance()->doSomething()`).
-   **Static Class Dependencies:** Calling static methods on other classes directly (`Utils::someHelperFunction()`).
-   **Lack of Abstractions:** Depending directly on concrete implementations rather than interfaces or abstract base classes.

### Modern Decoupling Patterns

-   **Dependency Injection (DI):** Pass dependencies into a class's constructor instead of letting the class create them.
-   **Interfaces/Abstract Base Classes:** Depend on abstractions, not on concrete types. This allows for mock objects in testing and flexible implementations.
-   **Events/Callbacks:** Use event-based communication for components that should not be directly aware of each other.

### Example: Refactoring from Direct Instantiation to Dependency Injection

#### Legacy Pattern (Tightly Coupled)

```cpp
// A tightly coupled class that creates its own logger.
// This makes it hard to test without creating a real file.
class DataProcessor {
public:
    DataProcessor() {
        // Direct dependency on a concrete class.
        logger_ = new FileLogger("processor.log");
    }
    ~DataProcessor() {
        delete logger_;
    }
    void process() {
        logger_->log("Processing data...");
        // ...
    }
private:
    FileLogger* logger_; // Depends on a concrete type.
};
```

#### Modern Alternative (Decoupled via DI and Abstraction)

```cpp
// 1. Define an abstraction (interface).
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const std::string& message) = 0;
};

// 2. The class now depends on the abstraction, not the concrete type.
class DataProcessor {
public:
    // The dependency is "injected" via the constructor.
    explicit DataProcessor(std::unique_ptr<ILogger> logger)
        : logger_(std::move(logger)) {}

    void process() {
        logger_->log("Processing data...");
        // ...
    }
private:
    std::unique_ptr<ILogger> logger_; // Depends on the abstraction.
};

// 3. In production, inject the real logger.
auto processor = std::make_unique<DataProcessor>(
    std::make_unique<FileLogger>("processor.log")
);

// 4. In tests, inject a mock logger.
auto mock_logger = std::make_unique<MockLogger>();
auto test_processor = std::make_unique<DataProcessor>(std::move(mock_logger));
```

## Performance Considerations for Legacy Code

### When Modernizing Performance-Critical Sections
1. **Measure first**: Profile before and after changes
2. **Preserve semantics**: Ensure behavior remains identical
3. **Incremental changes**: Modernize one pattern at a time
4. **Consider embedded constraints**: Memory and CPU limitations

### Safe Optimizations
```cpp
// Legacy: Manual loop
int sum = 0;
for (int i = 0; i < size; ++i) {
    sum += array[i];
}

// Modern: STL algorithm (often optimized by compiler)
int sum = std::accumulate(array, array + size, 0);

// Or with ranges (C++20)
int sum = std::ranges::fold_left(std::span(array, size), 0, std::plus{});
```

## Red Flags in Legacy Code

When analyzing existing code, watch for these warning signs:
- Manual memory management with new/delete
- Unchecked pointer arithmetic
- C-style casts, especially with void*
- Missing const correctness
- Resource leaks in exception paths
- Deep inheritance hierarchies
- Tight coupling between components
- Macros used for simple constants or functions

## Suggesting Improvements

When proposing modernization:
1. **Explain the benefit**: Why the modern approach is better
2. **Show the migration path**: How to get from old to new
3. **Consider compatibility**: Impact on existing code
4. **Highlight safety improvements**: Memory safety, exception safety
5. **Mention performance implications**: Better or equivalent performance