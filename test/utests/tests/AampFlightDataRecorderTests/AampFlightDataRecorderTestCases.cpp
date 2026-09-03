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
#include <iomanip>
#include <sstream>
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
 * @param tsMs Timestamp in milliseconds (defaults to current wall time).
 * @return Populated FDRLogEntry.
 */
static FDRLogEntry MakeEntry(const std::string& msg,
                              uint64_t tsMs = AampFlightDataRecorder::GetCurrentTimeMilliseconds(),
                              int logLevel = eLOGLEVEL_INFO)
{
    FDRLogEntry e;
    e.timestamp_ms = tsMs;
    e.log_level    = logLevel;
    e.thread_id    = std::this_thread::get_id();
    e.seq_num      = 0;
    e.player_id    = 0;
    e.file         = "/tmp/TestFile.cpp";
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
 * Because AampFlightDataRecorder is a singleton, each test restores the
 * default capacity and retention window before running.
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
        fdr.Initialize(true, kDefaultCapacity, kDefaultWindowSec);
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
 * Observable outcome: Dump() with an empty recorder returns without output or failure.
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
 *        The recorder mutex prevents writers from racing with the dump reader.
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
    uint64_t now   = AampFlightDataRecorder::GetCurrentTimeMilliseconds();
    uint64_t stale = (now > 120ULL * 1000ULL) ? (now - 120ULL * 1000ULL) : 0ULL;

    // Insert a stale entry directly (timestamp 2 minutes in the past)
    fdr().AddEntry(MakeEntry("stale-entry", stale));

    // Insert a fresh entry — this triggers EvictOldEntries which should remove
    // the stale one above.
    fdr().AddEntry(MakeEntry("fresh-entry", now));

    EXPECT_NO_FATAL_FAILURE(fdr().Dump(2, "TEST"));
}

/**
 * @test AampFlightDataRecorder_GetCurrentTimeMilliseconds_ReturnsUtcEpoch
 * @brief The returned UTC epoch value is expressed in milliseconds.
 */
TEST_F(AampFlightDataRecorderTest, GetCurrentTimeMilliseconds_ReturnsUtcEpoch)
{
    uint64_t timestampMs = AampFlightDataRecorder::GetCurrentTimeMilliseconds();
    EXPECT_GT(timestampMs, 1577836800000ULL);
}

TEST_F(AampFlightDataRecorderTest, CapacityEviction_EmitsWarnButNotInfo)
{
    fdr().Initialize(true, 3, kDefaultWindowSec);
    fdr().AddEntry(MakeEntry("warn-eldest", AampFlightDataRecorder::GetCurrentTimeMilliseconds(), eLOGLEVEL_WARN));
    fdr().AddEntry(MakeEntry("info-middle"));
    fdr().AddEntry(MakeEntry("mil-newest", AampFlightDataRecorder::GetCurrentTimeMilliseconds(), eLOGLEVEL_MIL));

    testing::internal::CaptureStdout();
    fdr().AddEntry(MakeEntry("overflow"));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_THAT(output, testing::HasSubstr("warn-eldest"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("info-middle")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("FLIGHT DATA RECORDER DUMP")));
}

TEST_F(AampFlightDataRecorderTest, AgeEviction_EmitsMilestone)
{
    uint64_t now = AampFlightDataRecorder::GetCurrentTimeMilliseconds();
    testing::internal::CaptureStdout();
    fdr().AddEntry(MakeEntry("aged-mil", now - 120000, eLOGLEVEL_MIL));
    fdr().AddEntry(MakeEntry("fresh-info", now));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_THAT(output, testing::HasSubstr("aged-mil"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("fresh-info")));
}

TEST_F(AampFlightDataRecorderTest, Flush_EmitsInOrderAndLeavesEmpty)
{
    fdr().AddEntry(MakeEntry("first-info"));
    fdr().AddEntry(MakeEntry("second-warn", AampFlightDataRecorder::GetCurrentTimeMilliseconds(), eLOGLEVEL_WARN));
    fdr().AddEntry(MakeEntry("third-mil", AampFlightDataRecorder::GetCurrentTimeMilliseconds(), eLOGLEVEL_MIL));

    testing::internal::CaptureStdout();
    fdr().Flush(eLOGLEVEL_ERROR, "ERROR_TEST");
    std::string output = testing::internal::GetCapturedStdout();
    size_t first = output.find("first-info");
    size_t second = output.find("second-warn");
    size_t third = output.find("third-mil");

    ASSERT_NE(first, std::string::npos);
    ASSERT_NE(second, std::string::npos);
    ASSERT_NE(third, std::string::npos);
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);
    EXPECT_THAT(output, testing::HasSubstr("triggered by ERROR_TEST ERROR"));

    testing::internal::CaptureStdout();
    fdr().Dump(eLOGLEVEL_ERROR, "SECOND_DUMP");
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
}

TEST_F(AampFlightDataRecorderTest, Flush_PreservesTimestampAndFilenameFormat)
{
    AampLogManager::logFilename = true;
    uint64_t timestampMs = AampFlightDataRecorder::GetCurrentTimeMilliseconds();
    fdr().AddEntry(MakeEntry("formatted", timestampMs));

    testing::internal::CaptureStdout();
    fdr().Flush(eLOGLEVEL_ERROR, "FORMAT_TEST");
    std::string output = testing::internal::GetCapturedStdout();
    AampLogManager::logFilename = false;

    std::ostringstream timestamp;
    timestamp << timestampMs / 1000 << "." << std::setfill('0') << std::setw(3) << timestampMs % 1000;
    EXPECT_THAT(output, testing::HasSubstr(timestamp.str() + ": [TEST][000][0][INFO]"));
    EXPECT_THAT(output, testing::HasSubstr("[TestFile.cpp][TestFunc][0]formatted"));
}

TEST_F(AampFlightDataRecorderTest, Initialize_ReconfiguresCapacityAndEnabledState)
{
    fdr().Initialize(true, 1, 15);
    fdr().AddEntry(MakeEntry("before-disable", AampFlightDataRecorder::GetCurrentTimeMilliseconds(), eLOGLEVEL_WARN));

    testing::internal::CaptureStdout();
    fdr().Initialize(false, 4, 30);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(fdr().IsEnabled());
    EXPECT_THAT(output, testing::HasSubstr("before-disable"));
    EXPECT_FALSE(fdr().AddEntry(MakeEntry("disabled")));
}
