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
 * @file AampSourceWorkerTestCases.cpp
 * @brief L1 unit tests for SourceWorker — per-source injection thread.
 *
 * SourceWorker is tested via its public interface (postNeedData,
 * enqueueSamples, cancelNeedData, setEos, flush, stop) and a
 * lambda-based InjectFn spy that records every injection call made by
 * the worker thread.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "AampSourceWorker.h"

using ::testing::_;

// ---------------------------------------------------------------------------
// Helper: build a minimal QueuedSample
// ---------------------------------------------------------------------------
static QueuedSample MakeSample(double pts = 0.1, double duration = 0.033)
{
	QueuedSample qs{};
	qs.sample.mPts      = pts;
	qs.sample.mDuration = duration;
	return qs;
}

// ---------------------------------------------------------------------------
// InjectFn spy
// ---------------------------------------------------------------------------

/**
 * @class InjectSpy
 * @brief Records all InjectFn invocations made by the SourceWorker thread.
 *
 * All members are accessed from both the test thread (constructor,
 * assertions) and the worker thread (operator()), so access is guarded
 * by m_mutex.
 */
class InjectSpy
{
public:
	struct Call
	{
		int32_t  sourceId{0};
		uint32_t requestId{0};
		size_t   sampleCount{0};
		bool     eos{false};
	};

	/// Return an InjectFn that records calls and returns empty rejected list.
	SourceWorker::InjectFn MakeFn(
		firebolt::rialto::AddSegmentStatus status =
			firebolt::rialto::AddSegmentStatus::OK)
	{
		return [this, status](
			int32_t  sourceId,
			uint32_t requestId,
			std::vector<QueuedSample> samples,
			bool     eos) -> std::vector<QueuedSample>
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_calls.push_back({sourceId, requestId, samples.size(), eos});
				m_totalCalled.fetch_add(1, std::memory_order_relaxed);
			}
			m_cv.notify_all();

			if (status == firebolt::rialto::AddSegmentStatus::NO_SPACE)
			{
				return samples; // return all as rejected
			}
			return {};
		};
	}

	/// Block until at least @p count InjectFn calls have been recorded, or
	/// timeout after @p ms milliseconds.
	bool WaitForCalls(int count, int ms = 200)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_cv.wait_for(lock, std::chrono::milliseconds(ms),
			[this, count]() {
				return m_totalCalled.load(std::memory_order_relaxed) >= count;
			});
	}

	int CallCount() const
	{
		return m_totalCalled.load(std::memory_order_relaxed);
	}

	Call GetCall(int index) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_calls.at(static_cast<size_t>(index));
	}

private:
	mutable std::mutex          m_mutex;
	std::condition_variable     m_cv;
	std::vector<Call>           m_calls;
	std::atomic<int>            m_totalCalled{0};
};

// ===========================================================================
// Fixture
// ===========================================================================

class AampSourceWorkerTest : public ::testing::Test
{
protected:
	InjectSpy spy;
};

// ===========================================================================
// Basic injection — postNeedData + enqueueSamples
// ===========================================================================

/**
 * @test A single needData request processed after samples are enqueued
 *       results in exactly one InjectFn call carrying those samples.
 */
TEST_F(AampSourceWorkerTest, PostNeedData_WithSamples_CallsInjectFn)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	samples.push_back(MakeSample(0.1));

	worker.postNeedData(/*sourceId=*/0, /*requestId=*/1, /*frameCount=*/10);
	worker.enqueueSamples(std::move(samples));

	ASSERT_TRUE(spy.WaitForCalls(1));
	EXPECT_EQ(spy.GetCall(0).sourceId,    0);
	EXPECT_EQ(spy.GetCall(0).requestId,   1u);
	EXPECT_EQ(spy.GetCall(0).sampleCount, 1u);
	EXPECT_FALSE(spy.GetCall(0).eos);
}

/**
 * @test Samples enqueued before the needData request are still injected
 *       when the request arrives later.
 */
TEST_F(AampSourceWorkerTest, EnqueueBeforeNeedData_StillInjected)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	samples.push_back(MakeSample(0.2));
	worker.enqueueSamples(std::move(samples));

	worker.postNeedData(0, 2, 10);

	ASSERT_TRUE(spy.WaitForCalls(1));
	EXPECT_EQ(spy.GetCall(0).requestId,   2u);
	EXPECT_EQ(spy.GetCall(0).sampleCount, 1u);
}

/**
 * @test frameCount limits how many samples are sent per needData request.
 */
TEST_F(AampSourceWorkerTest, FrameCount_LimitsInjectedSamples)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	for (int i = 0; i < 5; ++i)
	{
		samples.push_back(MakeSample(static_cast<double>(i) * 0.1));
	}
	worker.enqueueSamples(std::move(samples));

	// Request only 3 frames.
	worker.postNeedData(0, 1, 3);

	ASSERT_TRUE(spy.WaitForCalls(1));
	EXPECT_EQ(spy.GetCall(0).sampleCount, 3u);
}

/**
 * @test Multiple consecutive needData requests are each fulfilled with the
 *       next available samples.
 */
TEST_F(AampSourceWorkerTest, MultipleNeedData_DrainsSamplesSequentially)
{
	SourceWorker worker(spy.MakeFn());

	// Enqueue 4 samples.
	std::vector<QueuedSample> samples;
	for (int i = 0; i < 4; ++i)
	{
		samples.push_back(MakeSample(static_cast<double>(i) * 0.1));
	}
	worker.enqueueSamples(std::move(samples));

	// Two requests of 2 samples each.
	worker.postNeedData(0, 1, 2);
	worker.postNeedData(0, 2, 2);

	ASSERT_TRUE(spy.WaitForCalls(2));
	EXPECT_EQ(spy.GetCall(0).sampleCount, 2u);
	EXPECT_EQ(spy.GetCall(1).sampleCount, 2u);
}

// ===========================================================================
// EOS behaviour
// ===========================================================================

/**
 * @test setEos() causes the worker to fire InjectFn with eos=true once the
 *       sample queue is drained.
 */
TEST_F(AampSourceWorkerTest, SetEos_CallsInjectFnWithEosTrue)
{
	SourceWorker worker(spy.MakeFn());

	worker.setEos();
	worker.postNeedData(0, 7, 5);

	ASSERT_TRUE(spy.WaitForCalls(1));
	EXPECT_TRUE(spy.GetCall(0).eos);
	EXPECT_EQ(spy.GetCall(0).sampleCount, 0u);
}

/**
 * @test EOS with pending samples: samples are injected first, then eos=true
 *       is reported on the same or subsequent call once the queue empties.
 */
TEST_F(AampSourceWorkerTest, SetEos_WithSamples_SendsSamplesThenEos)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	samples.push_back(MakeSample(0.1));
	worker.enqueueSamples(std::move(samples));
	worker.setEos();
	worker.postNeedData(0, 3, 10);

	ASSERT_TRUE(spy.WaitForCalls(1));
	// The first call must contain the sample AND eos=true (queue drains on same req).
	EXPECT_GE(spy.GetCall(0).sampleCount, 1u);
	EXPECT_TRUE(spy.GetCall(0).eos);
}

// ===========================================================================
// cancelNeedData
// ===========================================================================

/**
 * @test cancelNeedData() discards all pending requests so InjectFn is not called.
 */
TEST_F(AampSourceWorkerTest, CancelNeedData_DiscardsRequests)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	samples.push_back(MakeSample(0.5));

	worker.postNeedData(0, 9, 10);
	worker.cancelNeedData();

	// Give the worker time to (possibly) process the (cancelled) request.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// enqueueSamples after cancel should not trigger injection for the old req.
	worker.enqueueSamples(std::move(samples));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_EQ(spy.CallCount(), 0);
}

// ===========================================================================
// flush
// ===========================================================================

/**
 * @test flush() discards enqueued samples; a subsequent needData delivers
 *       an empty (or EOS) injection only after new samples are enqueued.
 */
TEST_F(AampSourceWorkerTest, Flush_ClearsSampleQueue)
{
	SourceWorker worker(spy.MakeFn());

	std::vector<QueuedSample> samples;
	samples.push_back(MakeSample(1.0));
	worker.enqueueSamples(std::move(samples));

	worker.flush();

	// Post a needData — no samples should be available after flush.
	// Worker must NOT call InjectFn (no samples, no eos).
	worker.postNeedData(0, 1, 5);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(spy.CallCount(), 0);
}

/**
 * @test After flush(), new samples enqueued are injected on the next needData.
 */
TEST_F(AampSourceWorkerTest, Flush_ThenNewSamples_AreInjected)
{
	SourceWorker worker(spy.MakeFn());

	// Enqueue, then flush, then new samples + needData.
	std::vector<QueuedSample> old;
	old.push_back(MakeSample(1.0));
	worker.enqueueSamples(std::move(old));
	worker.flush();

	std::vector<QueuedSample> fresh;
	fresh.push_back(MakeSample(2.0));
	worker.enqueueSamples(std::move(fresh));
	worker.postNeedData(0, 5, 10);

	ASSERT_TRUE(spy.WaitForCalls(1));
	EXPECT_EQ(spy.GetCall(0).sampleCount, 1u);
}

// ===========================================================================
// NO_SPACE re-queue
// ===========================================================================

/**
 * @test When InjectFn returns ALL samples as rejected (simulates NO_SPACE),
 *       those samples are re-queued and re-sent on the next needData request.
 */
TEST_F(AampSourceWorkerTest, InjectFn_NoSpace_RequeuesSamples)
{
	// First call rejects all; subsequent calls accept.
	std::atomic<int> callCount{0};
	std::vector<std::vector<QueuedSample>> capturedSamples;
	std::mutex capturedMutex;
	std::condition_variable capturedCv;

	SourceWorker::InjectFn fn =
		[&](int32_t, uint32_t, std::vector<QueuedSample> samples, bool)
			-> std::vector<QueuedSample>
		{
			int n = callCount.fetch_add(1, std::memory_order_relaxed) + 1;
			{
				std::lock_guard<std::mutex> lock(capturedMutex);
				capturedSamples.push_back(samples);
			}
			capturedCv.notify_all();
			if (n == 1)
			{
				return samples; // reject (NO_SPACE)
			}
			return {}; // accept
		};

	SourceWorker worker(fn);

	std::vector<QueuedSample> s;
	s.push_back(MakeSample(0.3));
	worker.enqueueSamples(std::move(s));

	// First needData → injection rejected (NO_SPACE simulation).
	worker.postNeedData(0, 1, 5);

	// Wait for the first rejection.
	{
		std::unique_lock<std::mutex> lock(capturedMutex);
		capturedCv.wait_for(lock, std::chrono::milliseconds(200),
			[&]() { return callCount.load() >= 1; });
	}
	ASSERT_GE(callCount.load(), 1);

	// Second needData → rejected samples should be retried.
	worker.postNeedData(0, 2, 5);

	{
		std::unique_lock<std::mutex> lock(capturedMutex);
		capturedCv.wait_for(lock, std::chrono::milliseconds(200),
			[&]() { return callCount.load() >= 2; });
	}
	EXPECT_GE(callCount.load(), 2);
	// The second call must contain the re-queued sample.
	{
		std::lock_guard<std::mutex> lock(capturedMutex);
		ASSERT_GE(capturedSamples.size(), 2u);
		EXPECT_EQ(capturedSamples[1].size(), 1u);
	}
}

// ===========================================================================
// stop / destructor
// ===========================================================================

/**
 * @test stop() is idempotent — calling it twice must not crash.
 */
TEST_F(AampSourceWorkerTest, Stop_CalledTwice_DoesNotCrash)
{
	SourceWorker worker(spy.MakeFn());
	EXPECT_NO_THROW({
		worker.stop();
		worker.stop();
	});
}

/**
 * @test Destructor stops the worker thread cleanly with no crash or hang.
 */
TEST_F(AampSourceWorkerTest, Destructor_StopsThreadCleanly)
{
	EXPECT_NO_THROW({
		SourceWorker worker(spy.MakeFn());
		// Let the thread start, then let the destructor stop it.
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	});
}

/**
 * @test stop() while a needData event is pending must not deadlock.
 */
TEST_F(AampSourceWorkerTest, Stop_WithPendingNeedData_DoesNotDeadlock)
{
	SourceWorker worker(spy.MakeFn());

	worker.postNeedData(0, 1, 10);
	// Do NOT enqueue samples — worker is blocked waiting.  Stop must unblock it.
	EXPECT_NO_THROW(worker.stop());
}

// ===========================================================================
// Concurrency / no-deadlock
// ===========================================================================

/**
 * @test Rapid interleaving of postNeedData, enqueueSamples, and
 *       cancelNeedData from multiple external threads does not deadlock.
 */
TEST_F(AampSourceWorkerTest, HighFrequency_MultiThreaded_NoDeadlock)
{
	SourceWorker worker(spy.MakeFn());

	std::thread producer([&]() {
		for (int i = 0; i < 30; ++i)
		{
			std::vector<QueuedSample> s;
			s.push_back(MakeSample(static_cast<double>(i) * 0.01));
			worker.enqueueSamples(std::move(s));
		}
	});

	std::thread requester([&]() {
		for (int i = 0; i < 30; ++i)
		{
			worker.postNeedData(0, static_cast<uint32_t>(i), 2);
			if (i % 5 == 0)
			{
				worker.cancelNeedData();
			}
		}
	});

	producer.join();
	requester.join();

	// Allow remaining work to drain; test passes if no deadlock or crash.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	SUCCEED();
}
