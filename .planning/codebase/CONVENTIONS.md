# Coding Conventions

**Analysis Date:** 2026-06-08

## Naming Patterns

**Files:**
- PascalCase for all production C++ files: `AampCMCDCollector.cpp`, `AampEventManager.h`
- Header guard style: traditional `#ifndef __AAMP_BUFFER_CONTROL_H__` (not `#pragma once`)
- Header and implementation files are strictly paired: every `.h` has a matching `.cpp`
- Test directories: `[ComponentName]Tests/` (e.g., `AampCMCDCollectorTests/`)
- Test runner: `[ComponentName]Tests.cpp` — test cases: `[ComponentName]TestCases.cpp`
- Fakes: `Fake[ComponentName].cpp` in `test/utests/fakes/`
- Mocks: `Mock[ComponentName].h` in `test/utests/mocks/`

**Functions and Methods:**
- PascalCase for all function and method names: `GetSourceID()`, `FlushPendingEvents()`, `Initialize()`
- Accessor methods follow `Get`/`Set` prefix: `getRate()`, `SetConfigValue()`, `GetConfigValue()`
- Lowercase `get`/`set` appears in some newer code (e.g., `AampBufferControl`), PascalCase in older code

**Variables:**
- `camelCase` for local variables and parameters: `bCMCDEnabled_Test`, `traceId_Test`
- Member variables prefixed with `m`: `mBufferSize`, `mIsFakeTune`, `mPlayerId`
- Global variables prefixed with `g`: `gpGlobalConfig`, `g_mockAampConfig`
- Config macro access via `mConfig` prefix in method names: `ISCONFIGSET_PRIV`, `GETCONFIGVALUE_PRIV`

**Constants and Macros:**
- `UPPER_CASE` for all `#define` constants: `AAMP_VERSION`, `DEFAULT_ABR_CACHE_LIFE`, `SAFE_DELETE`
- `UPPER_CASE` for macro wrappers: `ISCONFIGSET`, `GETCONFIGVALUE`, `SETCONFIGVALUE`
- `constexpr` used for compile-time constants in new code: `static constexpr long kTargetLatencyMs`

**Types and Classes:**
- PascalCase for all classes: `AampCMCDCollector`, `BufferControlMaster`, `MockAampConfig`
- `enum class` for scoped enums in new code: `enum class TrickmodeState`, `enum class AdEvent`
- Legacy plain `enum` still common: `enum AampMediaType`, `enum AAMP_LogLevel`
- Enum values: `ePREFIX_NAME` for legacy enums, `kName` or plain `NAME` for `enum class` values

**Namespaces:**
- Module-level namespaces used to group related classes: `namespace AampBufferControl`, `namespace TSB`

## Code Style

**Formatting:**
- No `.clang-format` file detected — formatting is manually maintained
- C++ Standard: **C++17** required for all new production and test code (set via `CMAKE_CXX_STANDARD 17`)
- C++20 features explicitly prohibited; do not introduce `std::span`, concepts, ranges, `std::format`, coroutines
- Legacy C++11 patterns still present in older files — do not copy them into new code
- Compiler flags: `-Werror=format -Wno-multichar -Wno-non-virtual-dtor -Wno-psabi`

**Linting:**
- No `.eslintrc` or `clang-tidy` config detected
- Compiler warnings enforced via CMake: `-Werror=format`
- `-Werror=effc++` is explicitly stripped in unit test builds to reduce friction

**Bracing:**
- Braces required for all conditional and loop blocks, including single-line bodies

**Constructor Initializer Lists:**
- Use constructor initializer lists to initialize data members:
  ```cpp
  AampEventManager::AampEventManager(int playerId): mIsFakeTune(false),
      mAsyncTuneEnabled(false), mEventPriority(G_PRIORITY_DEFAULT_IDLE), ...
  ```

**Auto:**
- Use `auto` where it removes redundant type repetition (iterator declarations, `make_unique` results, range-based `for`)
- Prefer explicit types where ownership, numeric width, or API contract matters

**Universal Initialisation:**
- Use universal (brace) initialisation in all new generated code

## Import / Include Organization

**Order:**
1. Paired header (in `.cpp` files): `#include "AampEventManager.h"`
2. Standard library: `#include <iostream>`, `#include <mutex>`
3. Third-party: `#include <glib.h>`, `#include <curl/curl.h>`, `#include <cjson/cJSON.h>`
4. Project headers: `#include "AampDefine.h"`, `#include "AampConfig.h"`

**Include Guards:**
- Traditional `#ifndef / #define / #endif` guards (not `#pragma once`):
  ```cpp
  #ifndef __AAMP_BUFFER_CONTROL_H__
  #define __AAMP_BUFFER_CONTROL_H__
  // ...
  #endif
  ```

**Forward Declarations:**
- Prefer forward declarations in headers to reduce compilation coupling:
  ```cpp
  class AAMPGstPlayerPriv;
  struct media_stream;
  ```

## Error Handling

**Strategy:**
- **No `assert()` in production code** — explicitly prohibited. Assertions are compiled out in release builds.
- Match the error-handling style of the surrounding module — do not mix paradigms within a component.
- Return codes, status enums, and `bool` return values on hot playback/buffering/ABR/GStreamer paths.
- Prefer explicit failure handling: return codes, `std::optional`, status enums.
- Reserve exceptions for construction-time or configuration-time failures where the caller has no recovery path.
- Never throw across a C ABI boundary (GStreamer callbacks, `extern "C"` entry points).

**Null Pointer Handling:**
- Check for null before use and log with `AAMPLOG_ERR`:
  ```cpp
  if (ptr == nullptr) {
      AAMPLOG_ERR("Null pointer detected in %s", __FUNCTION__);
      return ERROR_NULL_POINTER;
  }
  ```

**Memory Management:**
- Use `std::unique_ptr` as the default for single ownership; `std::shared_ptr` only for genuinely shared ownership.
- Use raw pointers/references only for non-owning access — never as owning pointers in new code.
- Avoid raw `new`/`delete` except in legacy code paths.
- RAII for all resource management; Rule of Zero preferred.
- Legacy helper macros (still used in existing code, do not introduce in new code):
  - `SAFE_DELETE(ptr)` — nulls after delete: `test/utests/fakes/`, `AampUtils.h:55`
  - `SAFE_DELETE_ARRAY(ptr)` — nulls after array delete: `AampUtils.h:57`

**Move Semantics:**
- Prefer moving over copying when transferring ownership or for temporary objects.
- Implement move constructors and move assignment operators for resource-owning classes.
- Use `std::exchange` in move operations.

## Logging

**Framework:** Custom `AAMPLOG` macro system defined in `AampLogManager.h`

**Log Levels (ascending severity):**
```cpp
AAMPLOG_TRACE(FORMAT, ...)   // eLOGLEVEL_TRACE
AAMPLOG_DEBUG(FORMAT, ...)   // eLOGLEVEL_DEBUG
AAMPLOG_INFO(FORMAT, ...)    // eLOGLEVEL_INFO
AAMPLOG_WARN(FORMAT, ...)    // eLOGLEVEL_WARN
AAMPLOG_MIL(FORMAT, ...)     // eLOGLEVEL_MIL — milestone events
AAMPLOG_ERR(FORMAT, ...)     // eLOGLEVEL_ERROR
```

**Patterns:**
- Use `AAMPLOG_ERR` for null pointer checks, invalid arguments, and failed operations.
- Use `AAMPLOG_WARN` for unexpected-but-recoverable situations.
- Use `AAMPLOG_INFO` for state transitions and listener registration events.
- Use `AAMPLOG_TRACE` for hot-path call tracing.
- Use `AAMPLOG_MIL` for milestone events (state changes visible at milestone level).
- Include `__FUNCTION__` in error messages when relevant.

**Printf Format Specifiers:**
| Type | Specifier |
|------|-----------|
| `int` | `%d` |
| `unsigned int` | `%u` |
| `long` | `%ld` |
| `size_t` | `%zu` |
| `uint64_t` | `PRIu64` (from `<cinttypes>`) |

## Comments and Documentation

**Comment Style:**
- `/** ... */` block-style Doxygen for all public API: classes, public functions, constructors, macros, file headers.
- `///<` trailing-line Doxygen for struct/class members, enum values, short field annotations.
- Plain `//` for non-Doxygen inline implementation notes.

**Placement:**
- Place function/class documentation with the declaration in the header file — do not duplicate in `.cpp`.
- Keep member comments trailing on the same line: `int mPlayerId; ///< unique player identifier`

**File Headers:**
- Every file must begin with the RDK Management Apache 2.0 copyright block.
- Followed by a Doxygen `@file` and `@brief` tag.

**Function Documentation (Doxygen):**
```cpp
/**
 * @brief Brief description of what the function does.
 * @param param1 Description of the first parameter.
 * @return Description of the return value.
 * @note Optional note.
 * @warning Optional warning.
 */
int ExampleFunction(int param1);
```

**Class Documentation:**
```cpp
/**
 * @class MyClass
 * @brief Brief description.
 * Purpose: [what the class owns and does]
 */
class MyClass { ... };
```

**Discouraged Patterns:**
- `///` (without `<`) for member documentation — use `///<` instead.
- Mixing `/** */` and `///<` within the same class block.
- Comments that merely restate the variable name or type.
- Huge block comments for trivial one-line members.
- Stale comments that no longer reflect behaviour.

## Function and Module Design

**Size:** Keep functions focused; large functions with high cyclomatic complexity are a known concern — prefer extract-method refactoring.

**Parameters:**
- Pass by reference or pointer to avoid unnecessary copies.
- Use `std::string_view` for read-only string parameters in new code.
- Use `const` correctness throughout.

**Return Values:**
- Use `bool` for logical true/false state.
- Use `std::optional<T>` for operations that may legitimately fail in new code.
- Use explicit error codes/enums on hot playback paths.

**Constructors:**
- Use `explicit` to prevent implicit conversions.
- Use constructor initializer lists for member initialization.
- Delete copy constructor and assignment operator for non-copyable classes:
  ```cpp
  AampCMCDCollector(const AampCMCDCollector&) = delete;
  AampCMCDCollector& operator=(const AampCMCDCollector&) = delete;
  ```

**Templates:**
- Use `std::enable_if` / type traits to constrain templates — C++20 concepts not permitted.
- Prefer `constexpr` functions for compile-time computation.

## Exports and Header Design

- Keep data members `private` where possible; provide accessor methods.
- Avoid `friend` functions unless strongly justified.
- Use forward declarations in headers to limit compilation dependencies.
- Use `#include <cstdint>` fixed-width types for any data structures shared across language boundaries (e.g., ctypes).

---

*Convention analysis: 2026-06-08*
