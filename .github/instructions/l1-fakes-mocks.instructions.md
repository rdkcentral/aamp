---
description: AAMP L1 test fakes and mocks — patterns and anti-patterns
applyTo:
  - "test/utests/**"
---

# L1 Fakes & Mocks Guide

---

## Golden Rule

**Test YOUR component's behavior, not the fake or mock behavior.**

Every `EXPECT_*` assertion must answer: "Does my component behave correctly?"
If it instead answers "Does the fake behave correctly?", the test is wrong.

---

## AAMP Fake/Mock Architecture

- **Fakes** live in `test/utests/fakes/` (e.g., `FakeAampConfig.cpp`).
  They are intentionally simplified stand-ins — not full reimplementations.
- **Mocks** live in `test/utests/mocks/` as Google Mock interfaces
  (e.g., `MockAampConfig.h`).
- AAMP uses **both** in non-textbook ways. Follow repo-local patterns,
  not generic GoogleTest handbook advice.

### Call Chain (Critical)

The AAMP L1 test architecture uses a specific indirection:

1. **Test** calls the **component under test** (production code).
2. **Component** calls its dependencies — but at link time, **fakes** are
   linked instead of real implementations.
3. **Fakes** delegate to **mocks** (via a global mock pointer), enabling
   `EXPECT_CALL` verification and return-value control.

```
Test → Component (real code) → Fake (linked in place of dep) → Mock (for verification)
```

This means: your test exercises real component logic, fakes provide the
seams, and mocks give you observability and control.

### Key Fake Behavior Differences

| Fake | Real behavior | Fake behavior |
|---|---|---|
| `AampConfig::SetConfigValue(bool)` | Persists value in config store with owner priority | No-op — value is discarded |
| `AampConfig::GetConfigValue(int)` | Reads from config store | Returns `-1` or delegates to `MockAampConfig` |
| `AampConfig::IsConfigSet(bool)` | Checks whether config was explicitly set | Returns `false` or delegates to `MockAampConfig` |

**Adapt test expectations to fake behavior.** Do not expect real implementation
semantics from fakes.

---

## Wrong vs Correct Patterns

### Anti-pattern 1: Asserting on fake return values

```cpp
// WRONG — proves the fake works, not your component
config.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_EnableABR, true);
EXPECT_TRUE(config.IsConfigSet(eAAMPConfig_EnableABR));  // tests the fake
EXPECT_TRUE(config.GetConfigValue(eAAMPConfig_EnableABR)); // tests the fake
```

```cpp
// CORRECT — use EXPECT_CALL to control the mock, then test component behavior
EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableABR))
    .WillOnce(testing::Return(true));
EXPECT_TRUE(component.isAdaptiveBitrateEnabled());
```

### Anti-pattern 2: Testing fake default return values

```cpp
// WRONG — fake returns -1 by default, not a meaningful config value
int val = config.GetConfigValue(eAAMPConfig_ABRCacheLife);
EXPECT_EQ(val, -1);  // tests the fake's hardcoded default
```

```cpp
// CORRECT — if you need to verify data flow, use EXPECT_CALL on a mock
EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLife))
    .WillOnce(testing::Return(5000));
component.refreshCache();
EXPECT_EQ(component.getCacheLifetime(), 5000);
```

### Anti-pattern 3: Verifying fake state without testing component response

```cpp
// WRONG — only checks that the fake was called
config.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_MaxABRNWBufferRampUp, 10);
EXPECT_EQ(config.GetConfigValue(eAAMPConfig_MaxABRNWBufferRampUp), 10);
```

```cpp
// CORRECT — checks how the component responded to the config
EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxABRNWBufferRampUp))
    .WillOnce(testing::Return(10));
component.configure();
EXPECT_EQ(component.getBufferRampUp(), 10);
```

### Anti-pattern 4: Using EXPECT_EQ / ASSERT_EQ on floating-point values

```cpp
// WRONG — exact equality is unreliable for float/double
EXPECT_EQ(component.GetRate(), 1.0);
ASSERT_EQ(result, 0.0);
```

```cpp
// CORRECT — use the GoogleTest floating-point matchers
EXPECT_DOUBLE_EQ(component.GetRate(), 1.0);   // tolerance: 4 ULPs
EXPECT_NEAR(result, 0.0, 1e-9);               // explicit tolerance
EXPECT_FLOAT_EQ(resultF, 0.0f);               // for float types
```

Never use `EXPECT_EQ` or `ASSERT_EQ` to compare `float` or `double` values.
Use `EXPECT_DOUBLE_EQ` / `EXPECT_FLOAT_EQ` (4 ULPs tolerance) or
`EXPECT_NEAR` (explicit epsilon) instead.

---

## When to Use EXPECT_CALL

Use `EXPECT_CALL` to:
1. **Control** what a mocked dependency returns.
2. **Verify** that your component calls the dependency correctly.

```cpp
// Set mock return value
EXPECT_CALL(mockProcessor, process(testing::_))
    .WillOnce(testing::Return(SUCCESS));

// Then test component response to that return value
EXPECT_TRUE(component.initialize());
```

```cpp
// Verify component calls dependency with correct arguments
EXPECT_CALL(mockDownloader, fetch(testing::HasSubstr("manifest"), testing::_))
    .Times(1);
component.startTune(url);
```

### Multiple return values

```cpp
EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ABRCacheLife))
    .WillOnce(testing::Return(5000))
    .WillOnce(testing::Return(10000))
    .WillOnce(testing::Return(0));
```

---

## When Fake Behavior Makes a Test Impossible

If a fake's simplification makes a test meaningless:

1. **Preferred:** Adapt the test to verify component logic a different way.
2. **Acceptable:** Comment out the test with a clear explanation:

```cpp
/**
 * @brief Skipped — FakeAampConfig::SetConfigValue(bool) is a no-op,
 * making config-persistence verification impossible at L1 level.
 * Tested at L2 integration level instead.
 */
```

3. **Not acceptable:** Silently deleting the test or including the real
   dependency implementation to make the test pass.

---

## Quick Self-Check

Before writing any assertion, ask:
- Am I testing my component or the dependency?
- Does this assertion still pass if the fake is replaced with a different stub
  that returns the same values?
- Would this test catch a real bug in my component?

If the answer to any of these is "no", refactor the test.
