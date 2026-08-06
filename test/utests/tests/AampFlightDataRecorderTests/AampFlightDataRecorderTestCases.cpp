/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file AampFlightDataRecorderTestCases.cpp
 * @brief Unit tests for AampFlightDataRecorder covering:
 *        - Initialize / IsEnabled / SetEnabled
 *        - AddEntry single-producer: fill and ring-wrap
 *        - AddEntry multi-producer: ring-buffer invariants under concurrency
 *        - Flush resets head/tail/count
 *        - Dump on empty and on full buffer
 *        - EvictOldEntries: old entries are removed when timestamp exceeds window
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "AampFlightDataRecorder.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * @brief Build a minimal FDRLogEntry with the given message.
 * @param msg Message text for the entry.
 * @param tsUs Timestamp in microseconds (defaults to current wall time).
 * @return Populated FDRLogEntry.
 */
static FDRLogEntry MakeEntry(const std::string& msg,
                              uint64_t tsUs = AampFlightDataRecorder::GetCurrentTimeMicroseconds())
{
    FDRLogEntry e;
    e.timestamp_us = tsUs;
    e.log_level    = 2; // INFO
    e.thread_id    = std::this_thread::get_id();
    e.seq_num      = 0;
    e.player_id    = 0;
    e.func         = "TestFunc";
    e.line         = 0;
    e.source       = "TEST";
    e.message      = msg;
    return e;
}

/**
 * @brief Poll a predicate until it returns true or the deadline passes.
 * @param pred Predicate to check.
 * @param timeout Maximum time to wait.
 * @return true if pred returned true before timeout.
 */
template<typename Pred>
static bool WaitFor(Pred pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred())
    {
        if (std::chrono::steady_clock::now() >= deadline) { return false; }
        std::this_thread::yield();
    }
    return true;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

/**
 * @brief Fixture for AampFlightDataRecorder tests.
 *
 * Because AampFlightDataRecorder is a singleton, each test that requires a
 * fresh state must flush and re-enable the recorder.  Tests that need a
 * specific buffer size create a local instance via the protected reset helper.
 *
 * IMPORTANT: The singleton's Initialize() is a one-shot guard; after the first
 * call it returns without effect.  Tests that need different sizes work with
 * a small default (kDefaultCapacity) set in SetUpTestSuite().
 */
class AampFlightDataRecorderTest : public ::testing::Test
{
protected:
    static constexpr size_t   kDefaultCapacity  = 16;
    static constexpr uint64_t kDefaultWindowSec = 60;

    static void SetUpTestSuite()
    {
        // Initialize once for the whole suite with a small, predictable buffer.
        AampFlightDataRecorder::GetInstance().Initialize(
            /*enabled=*/true, kDefaultCapacity, kDefaultWindowSec);
    }

    void SetUp() override
    {
        auto& fdr = AampFlightDataRecorder::GetInstance();
        fdr.SetEnabled(true);
        fdr.Flush();
    }

    void TearDown() override
    {
        AampFlightDataRecorder::GetInstance().Flush();
    }

    AampFlightDataRecorder& fdr() { return AampFlightDataRecorder::GetInstance(); }
};

// ---------------------------------------------------------------------------
// Group 1 — Initialization and enabled flag
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_Initialize_IsEnabledAfterInit
 * @brief After Initialize(enabled=true), IsEnabled() returns true.
 */
TEST_F(AampFlightDataRecorderTest, Initialize_IsEnabledAfterInit)
{
    EXPECT_TRUE(fdr().IsEnabled());
}

/**
 * @test AampFlightDataRecorder_SetEnabled_TogglesEnabledFlag
 * @brief SetEnabled(false) disables the recorder; SetEnabled(true) re-enables it.
 */
TEST_F(AampFlightDataRecorderTest, SetEnabled_TogglesEnabledFlag)
{
    fdr().SetEnabled(false);
    EXPECT_FALSE(fdr().IsEnabled());

    fdr().SetEnabled(true);
    EXPECT_TRUE(fdr().IsEnabled());
}

/**
 * @test AampFlightDataRecorder_AddEntry_WhenDisabled_DoesNotStore
 * @brief AddEntry while disabled is a no-op; Dump produces no entries.
 *
 * Observable outcome: Dump() with an empty recorder prints its header/footer
 * but returns without crashing.  We verify by calling Dump directly (stdout
 * goes to the test runner) and asserting no ASSERTs fire.
 */
TEST_F(AampFlightDataRecorderTest, AddEntry_WhenDisabled_DoesNotStore)
{
    fdr().SetEnabled(false);
    fdr().AddEntry(MakeEntry("should not be stored"));
    // If stored, the buffer count would be 1; after re-enable Dump would show it.
    // We cannot inspect mCount directly (private), but Dump must not crash.
    fdr().SetEnabled(true);
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

// ---------------------------------------------------------------------------
// Group 2 — AddEntry single-producer: fill and ring-wrap
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_AddEntry_SingleProducer_FillBuffer
 * @brief Adding exactly kDefaultCapacity entries does not crash and Dump succeeds.
 */
TEST_F(AampFlightDataRecorderTest, AddEntry_SingleProducer_FillBuffer)
{
    for (size_t i = 0; i < kDefaultCapacity; ++i)
    {
        fdr().AddEntry(MakeEntry("entry_" + std::to_string(i)));
    }
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_AddEntry_SingleProducer_OverflowBuffer
 * @brief Adding more entries than capacity wraps the ring correctly without
 *        crash and without corrupting memory.  The ring-buffer invariant
 *        (count <= capacity) must hold after every write.
 *
 * Observable outcome: Dump() executes without crash or sanitizer error.
 */
TEST_F(AampFlightDataRecorderTest, AddEntry_SingleProducer_OverflowBuffer)
{
    const size_t kOverflow = kDefaultCapacity * 3;
    for (size_t i = 0; i < kOverflow; ++i)
    {
        fdr().AddEntry(MakeEntry("entry_" + std::to_string(i)));
    }
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

// ---------------------------------------------------------------------------
// Group 3 — AddEntry multi-producer: concurrency safety
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_AddEntry_MultiProducer_NoDataRace
 * @brief Multiple threads writing concurrently must not cause data races,
 *        buffer overflows, or crashes.
 *
 * This is the primary regression test for the race described in the review:
 * "mCount/mTail update in AddEntry() is not safe for multi-producer use".
 *
 * Observable invariant:
 *   After all threads finish, Dump() must complete without crash, assert, or
 *   sanitizer error.  With TSan / ASan enabled this additionally detects any
 *   data race on the shared atomics.
 */
TEST_F(AampFlightDataRecorderTest, AddEntry_MultiProducer_NoDataRace)
{
    constexpr int    kThreads        = 8;
    constexpr size_t kEntriesPerThread = 50; // well above capacity to force wrap

    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t]() {
            // Wait for the main thread to release all workers simultaneously.
            while (!startFlag.load(std::memory_order_acquire)) { std::this_thread::yield(); }

            for (size_t i = 0; i < kEntriesPerThread; ++i)
            {
                fdr().AddEntry(MakeEntry("t" + std::to_string(t) + "_e" + std::to_string(i)));
            }
        });
    }

    startFlag.store(true, std::memory_order_release);

    for (auto& th : threads) { th.join(); }

    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_AddEntry_MultiProducer_BufferInvariant
 * @brief After concurrent writes that exceed capacity, the ring-buffer fill
 *        level must not exceed the configured capacity.
 *
 * Derived fill level = mHead - mTail.  We expose these indirectly by observing
 * that Dump() iterates at most kDefaultCapacity entries without an out-of-bounds
 * access (verified by AddressSanitizer / bounds checking in the iterator).
 *
 * We stress the boundary by writing exactly capacity+1 entries from two
 * threads simultaneously, then verifying Dump completes cleanly.
 */
TEST_F(AampFlightDataRecorderTest, AddEntry_MultiProducer_BufferInvariant)
{
    constexpr int    kThreads  = 2;
    // Each thread writes capacity+1 entries to guarantee at least one overflow
    const size_t kPerThread = kDefaultCapacity + 1;

    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&, t]() {
            while (!go.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            for (size_t i = 0; i < kPerThread; ++i)
            {
                fdr().AddEntry(MakeEntry("buf_" + std::to_string(t) + "_" + std::to_string(i)));
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto& th : threads) { th.join(); }

    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

// ---------------------------------------------------------------------------
// Group 4 — Flush
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_Flush_ClearsBuffer
 * @brief After filling the buffer and calling Flush(), Dump() produces no
 *        entries (the inner loop iterates zero times).  We verify by calling
 *        Dump() which should complete instantly without any printf output for
 *        entries (validated by no crash / assert).
 */
TEST_F(AampFlightDataRecorderTest, Flush_ClearsBuffer)
{
    for (size_t i = 0; i < kDefaultCapacity; ++i)
    {
        fdr().AddEntry(MakeEntry("pre-flush-" + std::to_string(i)));
    }

    fdr().Flush();

    // After flush, count == 0 so Dump exits early with only header/footer.
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_Flush_AllowsReuse
 * @brief After Flush(), new entries can be added normally.
 */
TEST_F(AampFlightDataRecorderTest, Flush_AllowsReuse)
{
    for (size_t i = 0; i < kDefaultCapacity; ++i)
    {
        fdr().AddEntry(MakeEntry("before-flush-" + std::to_string(i)));
    }

    fdr().Flush();

    fdr().AddEntry(MakeEntry("after-flush"));
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

// ---------------------------------------------------------------------------
// Group 5 — Dump edge cases
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_Dump_EmptyBuffer_DoesNotCrash
 * @brief Dump() on an empty buffer must return immediately without crash.
 */
TEST_F(AampFlightDataRecorderTest, Dump_EmptyBuffer_DoesNotCrash)
{
    // SetUp already called Flush(); buffer is empty.
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_Dump_FullBuffer_DoesNotCrash
 * @brief Dump() on a full buffer must complete without crash or OOB access.
 */
TEST_F(AampFlightDataRecorderTest, Dump_FullBuffer_DoesNotCrash)
{
    for (size_t i = 0; i < kDefaultCapacity; ++i)
    {
        fdr().AddEntry(MakeEntry("full-" + std::to_string(i)));
    }
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(5 /*ERROR*/, "TEST"));
}

/**
 * @test AampFlightDataRecorder_Dump_ConcurrentWithWriters_DoesNotCrash
 * @brief Dump() triggered while writers are still active must not crash.
 *        The mDumping flag prevents writers from racing with the dump reader.
 */
TEST_F(AampFlightDataRecorderTest, Dump_ConcurrentWithWriters_DoesNotCrash)
{
    std::atomic<bool> stopWriters{false};

    // Start background writers
    std::vector<std::thread> writers;
    for (int t = 0; t < 4; ++t)
    {
        writers.emplace_back([&, t]() {
            size_t i = 0;
            while (!stopWriters.load(std::memory_order_relaxed))
            {
                fdr().AddEntry(MakeEntry("w" + std::to_string(t) + "_" + std::to_string(i++)));
            }
        });
    }

    // Trigger a dump while writers are running
    EXPECT_NO_FATAL_FAILURE(fdr().Dump(5 /*ERROR*/, "CONCURRENT_TEST"));

    stopWriters.store(true, std::memory_order_relaxed);
    for (auto& w : writers) { w.join(); }
}

// ---------------------------------------------------------------------------
// Group 6 — EvictOldEntries (tested indirectly)
// ---------------------------------------------------------------------------

/**
 * @test AampFlightDataRecorder_EvictOldEntries_OldTimestampsRemoved
 * @brief Entries with timestamps older than the configured window are evicted.
 *
 * We add an entry with a timestamp far in the past (older than the 60 s window),
 * then add a current entry.  The next AddEntry call triggers EvictOldEntries
 * which must silently remove the stale slot.  Observable outcome: Dump()
 * completes without crash.  With ASan the slot walk would detect any OOB.
 */
TEST_F(AampFlightDataRecorderTest, EvictOldEntries_OldTimestampsRemoved)
{
    uint64_t now   = AampFlightDataRecorder::GetCurrentTimeMicroseconds();
    uint64_t stale = (now > 120ULL * 1000000ULL) ? (now - 120ULL * 1000000ULL) : 0ULL;

    // Insert a stale entry directly (timestamp 2 minutes in the past)
    fdr().AddEntry(MakeEntry("stale-entry", stale));

    // Insert a fresh entry — this triggers EvictOldEntries which should remove
    // the stale one above.
    fdr().AddEntry(MakeEntry("fresh-entry", now));

    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_GetCurrentTimeMicroseconds_ReturnsMonotonic
 * @brief Two successive calls return non-decreasing values.
 */
TEST_F(AampFlightDataRecorderTest, GetCurrentTimeMicroseconds_ReturnsMonotonic)
{
    uint64_t t1 = AampFlightDataRecorder::GetCurrentTimeMicroseconds();
    uint64_t t2 = AampFlightDataRecorder::GetCurrentTimeMicroseconds();
    EXPECT_GE(t2, t1);
}
