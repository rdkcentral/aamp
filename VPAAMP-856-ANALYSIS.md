# VPAAMP-856: Remove Unit Test Dependencies on aamp/middleware

## Objective

Remove all unit test dependencies on the deprecated `aamp/middleware` folder so it can be completely removed from the repository. Unit tests should use the external middleware repository (installed to `.libs/include/` and `.libs/lib/`).

---

## Current State Analysis

### Files with middleware/ Dependencies

1. **test/utests/tests/AampUtilsTests/AampUtilsTests.cpp**
2. **test/utests/tests/lstringTests/FakeLstringDeps.cpp**
3. **test/utests/tests/AampGstPlayer/FunctionalTests.cpp**
4. **test/utests/tests/AampGstPlayer/NullGuardTests.cpp**
5. **test/utests/fakes/FakeSocUtils.cpp**
6. **test/utests/fakes/FakeInterfacePlayerRDK.cpp**

### Middleware Headers Used

```cpp
#include "middleware/InterfacePlayerRDK.h"
#include "middleware/SocUtils.h"
#include "middleware/baseConversion/_base64.h"
```

### CMakeLists.txt Dependencies

**File**: `test/utests/CMakeLists.txt`

**Lines 32-38**: Include directories pointing to `aamp/middleware`:
```cmake
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/closedcaptions)
include_directories(${AAMP_ROOT}/middleware/drm)
include_directories(${AAMP_ROOT}/middleware/drm/helper)
include_directories(${AAMP_ROOT}/middleware/drm/ocdm)
include_directories(${AAMP_ROOT}/middleware/playerJsonObject)
include_directories(${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
```

---

## Verification: Headers Available in External Middleware

All required headers are available in `.libs/include/`:

```bash
$ ls .libs/include/ | grep -E "InterfacePlayerRDK|SocUtils|base64"
InterfacePlayerRDK.h
SocUtils.h
_base64.h
```

✅ **All middleware headers needed by unit tests are available in the installed external middleware.**

---

## Migration Strategy

### Phase 1: Update Include Paths

**Change**: Update all `#include "middleware/X.h"` to `#include "X.h"`

**Files to modify**:
1. `test/utests/tests/AampUtilsTests/AampUtilsTests.cpp`
2. `test/utests/tests/lstringTests/FakeLstringDeps.cpp`
3. `test/utests/tests/AampGstPlayer/FunctionalTests.cpp`
4. `test/utests/tests/AampGstPlayer/NullGuardTests.cpp`
5. `test/utests/fakes/FakeSocUtils.cpp`
6. `test/utests/fakes/FakeInterfacePlayerRDK.cpp`

**Example**:
```cpp
// Before
#include "middleware/InterfacePlayerRDK.h"

// After
#include "InterfacePlayerRDK.h"
```

### Phase 2: Update CMakeLists.txt

**Change**: Replace `aamp/middleware` include directories with `.libs/include`

**File**: `test/utests/CMakeLists.txt`

**Before**:
```cmake
include_directories(${AAMP_ROOT}/middleware)
include_directories(${AAMP_ROOT}/middleware/closedcaptions)
include_directories(${AAMP_ROOT}/middleware/drm)
include_directories(${AAMP_ROOT}/middleware/drm/helper)
include_directories(${AAMP_ROOT}/middleware/drm/ocdm)
include_directories(${AAMP_ROOT}/middleware/playerJsonObject)
include_directories(${AAMP_ROOT}/middleware/externals/contentsecuritymanager)
```

**After**:
```cmake
# Use installed external middleware headers
include_directories(${AAMP_ROOT}/.libs/include)
```

**Rationale**: The external middleware install process already flattens all headers into `.libs/include/`, so we don't need separate subdirectory includes.

### Phase 3: Verify Linking

**Check**: Ensure unit tests link against installed middleware libraries

The unit tests likely already link against the middleware libraries through existing CMake targets. We need to verify this and ensure they're using the external middleware libraries from `.libs/lib/`.

### Phase 4: Testing

1. Build unit tests with updated includes
2. Run all unit tests to verify no regressions
3. Confirm no compilation errors related to missing headers
4. Verify tests still pass with external middleware

---

## Potential Issues & Solutions

### Issue 1: Nested Headers

**Problem**: Some middleware headers might include other middleware headers using relative paths like `#include "drm/DrmHelper.h"`.

**Solution**: The external middleware install should handle this by either:
- Flattening all headers to `.libs/include/`
- Maintaining subdirectory structure in `.libs/include/`

**Action**: Verify the structure of `.libs/include/` to understand how nested includes are handled.

### Issue 2: Fake Implementations

**Problem**: Files like `FakeInterfacePlayerRDK.cpp` and `FakeSocUtils.cpp` provide mock implementations for testing.

**Solution**: These fakes should continue to work as-is, just with updated include paths. They don't need to link against the real middleware libraries.

### Issue 3: Private Headers

**Problem**: Some headers in `aamp/middleware` might not be installed to `.libs/include/` if they're considered private/internal.

**Solution**: 
- Identify which headers are missing
- Either: Add them to middleware install process
- Or: Refactor tests to not depend on private headers

---

## Implementation Checklist

- [ ] Phase 1: Update all `#include "middleware/X.h"` → `#include "X.h"`
- [ ] Phase 2: Update `test/utests/CMakeLists.txt` to use `.libs/include`
- [ ] Phase 3: Verify linking against external middleware libraries
- [ ] Phase 4: Build and run all unit tests
- [ ] Verify no compilation errors
- [ ] Verify all tests pass
- [ ] Document any issues found
- [ ] Create PR with changes

---

## Success Criteria

- [ ] All unit tests compile without errors
- [ ] All unit tests pass
- [ ] No references to `aamp/middleware` in test code
- [ ] No references to `aamp/middleware` in test CMakeLists.txt
- [ ] Tests use only installed external middleware headers from `.libs/include/`
- [ ] Tests link only against external middleware libraries from `.libs/lib/`

---

## Next Steps After This Ticket

Once VPAAMP-856 is complete:

1. **Verify no other dependencies**: Search entire codebase for any remaining `aamp/middleware` references
2. **Add deprecation warning**: Update `aamp/middleware/README.md` with stronger deprecation notice
3. **Plan removal**: Create follow-up ticket to completely remove `aamp/middleware` folder
4. **Update documentation**: Update build instructions to reflect external middleware only

---

## Files to Modify

### Source Files (6 files)
1. `test/utests/tests/AampUtilsTests/AampUtilsTests.cpp`
2. `test/utests/tests/lstringTests/FakeLstringDeps.cpp`
3. `test/utests/tests/AampGstPlayer/FunctionalTests.cpp`
4. `test/utests/tests/AampGstPlayer/NullGuardTests.cpp`
5. `test/utests/fakes/FakeSocUtils.cpp`
6. `test/utests/fakes/FakeInterfacePlayerRDK.cpp`

### Build Files (1 file)
1. `test/utests/CMakeLists.txt`

**Total**: 7 files to modify
