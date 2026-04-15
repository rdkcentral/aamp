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

- **Fakes** live in `test/utests/fakes/` (e.g., `FakeAampGrowableBuffer.cpp`).
  They are intentionally simplified stand-ins — not full reimplementations.
- **Mocks** live in `test/utests/mocks/` as Google Mock interfaces.
- AAMP uses **both** in non-textbook ways. Follow repo-local patterns,
  not generic GoogleTest handbook advice.

### Key Fake Behavior Differences

| Fake | Real behavior | Fake behavior |
|---|---|---|
| `AampGrowableBuffer::AppendBytes` | Deep-copies data into allocated buffer | Assigns pointer directly (`ptr = srcPtr`) |
| `AampGrowableBuffer::Free` | Frees memory, resets length to 0 | No-op — length unchanged |
| `AampGrowableBuffer::Clear` | Resets all fields | May not reset all fields |

**Adapt test expectations to fake behavior.** Do not expect real implementation
semantics from fakes.

---

## Wrong vs Correct Patterns

### Anti-pattern 1: Asserting on fake return values

```cpp
// WRONG — proves the fake works, not your component
cachedFragment->fragment.AppendBytes(testData, testDataSize);
EXPECT_GT(cachedFragment->fragment.GetLen(), 0);        // tests the fake
EXPECT_NE(cachedFragment->fragment.GetPtr(), nullptr);  // tests the fake
```

```cpp
// CORRECT — proves your component set its own state correctly
EXPECT_DOUBLE_EQ(cachedFragment->position, expectedPosition);
EXPECT_EQ(cachedFragment->type, expectedType);
```

### Anti-pattern 2: Testing memory contents of a fake

```cpp
// WRONG — fake uses pointer assignment, not memcpy
EXPECT_EQ(memcmp(buffer.GetPtr(), expected, size), 0);
```

```cpp
// CORRECT — if you need to verify data flow, use EXPECT_CALL on a mock
EXPECT_CALL(mockSink, ReceiveData(testing::_, testing::Eq(size)))
    .Times(1);
component.push(data, size);
```

### Anti-pattern 3: Verifying fake state without testing component response

```cpp
// WRONG — only checks that the fake changed
mockBuffer.AppendBytes(data, size);
EXPECT_GT(mockBuffer.GetLen(), 0);
```

```cpp
// CORRECT — checks how the component responded
component.ingest(data, size);
EXPECT_EQ(component.getStatus(), READY);
```

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
EXPECT_CALL(mockBuffer, GetLen())
    .WillOnce(testing::Return(50))
    .WillOnce(testing::Return(100))
    .WillOnce(testing::Return(0));
```

---

## When Fake Behavior Makes a Test Impossible

If a fake's simplification makes a test meaningless:

1. **Preferred:** Adapt the test to verify component logic a different way.
2. **Acceptable:** Comment out the test with a clear explanation:

```cpp
/**
 * @brief Skipped — FakeAampGrowableBuffer uses pointer assignment,
 * making memory-isolation verification impossible at L1 level.
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
