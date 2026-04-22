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

### Anti-pattern 5: EXPECT_TRUE / EXPECT_FALSE with comparison operators

```cpp
// WRONG — on failure prints "Expected: true, Actual: false" with no values
EXPECT_TRUE(result == nullptr);
EXPECT_TRUE(url.find("video_p0_5.m4s") != std::string::npos);
EXPECT_TRUE(position >= seekPos && position <= seekPos + 1);
```

```cpp
// CORRECT — prints both operands on failure
EXPECT_EQ(result, nullptr);
EXPECT_THAT(url, ::testing::HasSubstr("video_p0_5.m4s"));
EXPECT_GE(position, seekPos);
EXPECT_LE(position, seekPos + 1);
```

Never wrap a comparison in `EXPECT_TRUE()` / `EXPECT_FALSE()`. Use the
specific GoogleTest macro (`EXPECT_EQ`, `EXPECT_NE`, `EXPECT_GE`,
`EXPECT_THAT` with matchers) so failure messages show actual values.

### Anti-pattern 6: EXPECT_EQ with bool literals

```cpp
// WRONG — verbose and less readable
EXPECT_EQ(result, true);
EXPECT_EQ(aampConfig.CustomSearch(url, playerId, appname), false);
```

```cpp
// CORRECT
EXPECT_TRUE(result);
EXPECT_FALSE(aampConfig.CustomSearch(url, playerId, appname));
```

Use `EXPECT_TRUE` / `EXPECT_FALSE` for boolean assertions, not
`EXPECT_EQ(x, true)` or `EXPECT_EQ(x, false)`.

### Anti-pattern 7: sleep() / usleep() / sleep_for() as synchronization

Raw sleeps in L1 tests are **not acceptable**. They cause:
- **CI flakiness** — timing varies across build machines and load.
- **Slow test suites** — a single `sleep(20)` wastes 20 seconds of CI time.
- **False passes** — the sleep may mask a race the test should detect.

```cpp
// WRONG — blocks for a fixed duration, slow and flaky
sleep(5);
EXPECT_TRUE(component.isReady());

std::this_thread::sleep_for(std::chrono::milliseconds(25));
EXPECT_EQ(component.getState(), kIdle);
```

```cpp
// CORRECT — poll with a deadline
bool ready = WaitForCondition(
    [&]() { return component.isReady(); },
    std::chrono::milliseconds(500));
EXPECT_TRUE(ready);

// CORRECT — use a latch or condition variable
std::latch done(1);
component.onComplete([&]() { done.count_down(); });
component.start();
EXPECT_TRUE(done.try_wait_for(std::chrono::milliseconds(500)));
```

When you must wait for an asynchronous result:
1. **Preferred:** Poll with a tight loop + deadline (see `WaitForRate()`
   pattern in AampLatencyMonitorTests).
2. **Acceptable:** `std::condition_variable` or `std::latch` with a timeout.
3. **Not acceptable:** Any fixed `sleep()`, `usleep()`, or `sleep_for()`.

If a short sleep is genuinely the only option for a negative assertion
(proving something did *not* happen), add a comment explaining why, keep
it under 100 ms, and flag it in the PR description.

---

## Choosing Mock Strictness

| Type | Behavior on uninteresting calls | When to use |
|---|---|---|
| `NiceMock<T>` | Silently returns default | Default choice — most AAMP components trigger many incidental dependency calls |
| `T` (bare mock) | Prints a warning per call | When you want visibility into unexpected interactions during development |
| `StrictMock<T>` | **Fails the test** | Only when the test specifically needs to assert that *no other* calls are made |

Prefer `NiceMock<T>` for AAMP L1 tests. Bare mocks produce noisy warnings
that hide real failures. Use `StrictMock<T>` sparingly and only with a
clear rationale.

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
