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
 * @file AampRialtoVideoSourceInjectionTestCases.cpp
 * @brief AampRialtoVideoSource L1 tests: the needData/haveData handshake: unblock, EOS, needData, injectOneSample, encryption metadata and gating.
 */

#include "AampRialtoVideoSourceTestFixture.h"

// ---------------------------------------------------------------------------
// unblockInjection
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_UnblockInjection_BumpsGeneration
 * @brief Verify unblockInjection increments the generation counter.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_UnblockInjection_BumpsGeneration)
{
	uint64_t gen1 = m_source.captureGeneration();
	m_source.unblockInjection(m_pipelinePtr);
	uint64_t gen2 = m_source.captureGeneration();

	EXPECT_GT(gen2, gen1);
}

/**
 * @test AampRialtoVideoSource_UnblockInjection_WithPendingRequestNoInjector_SendsNoAvailableSamples
 * @brief Verify unblockInjection() closes out an abandoned needData
 *        request with haveData(NO_AVAILABLE_SAMPLES) when no injector is
 *        active to answer it (e.g. Flush()/Stop() racing a needData that
 *        arrived just before the pipeline-level seek).
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_UnblockInjection_WithPendingRequestNoInjector_SendsNoAvailableSamples)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(1, /*requestId=*/33, m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 33))
		.WillOnce(Return(true));

	m_source.unblockInjection(m_pipelinePtr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

/**
 * @test AampRialtoVideoSource_UnblockInjection_WithInjectorActive_DoesNotRespondItself
 * @brief Verify unblockInjection() does NOT send haveData itself when an
 *        injector is active — the injector owns the request and must close
 *        it out when it observes the generation change.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_UnblockInjection_WithInjectorActive_DoesNotRespondItself)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(1, /*requestId=*/44, m_pipelinePtr);
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.injectorActive = true;
	}

	EXPECT_CALL(*m_pipelinePtr, haveData(_, _)).Times(0);

	m_source.unblockInjection(m_pipelinePtr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.hasPending);
	EXPECT_EQ(st.pendingRequestId, 44u);
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_GenerationInvalidatedWhileWaiting_ClosesOutRequest
 * @brief Verify that when unblockInjection() defers to an active
 *        injector (because a needData was staged concurrently), the
 *        injector itself sends haveData(NO_AVAILABLE_SAMPLES) for the
 *        abandoned request when it wakes and aborts.  This is the other
 *        half of the handshake exercised by the "DoesNotRespondItself"
 *        test above.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_GenerationInvalidatedWhileWaiting_ClosesOutRequest)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();

	AampMediaSample sample;
	uint8_t data[] = {0x55, 0x66};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 5.0;
	sample.mDuration = 0.033;

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 66))
		.WillOnce(Return(true));

	std::atomic<bool> injected{true};
	std::thread injector([&]{
		injected = m_source.injectOneSample(
			*m_pipelinePtr, gen, std::move(sample), nullptr);
	});

	// Poll for the injector to enter its wait (injectorActive is set) rather
	// than relying on a fixed sleep duration.
	{
		auto &st = m_source.state();
		const auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(200);
		bool active = false;
		while (!active && std::chrono::steady_clock::now() < deadline)
		{
			{
				std::lock_guard<std::mutex> lock(st.mu);
				active = st.injectorActive;
			}
			if (!active)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		ASSERT_TRUE(active);
	}

	// Stage a pending request as if a needData arrived just before the
	// flush/seek, then invalidate the generation (as Flush()/Stop() would).
	// Since the injector is active, unblockInjection() must defer the
	// request to it rather than answering itself.
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 66;
		st.pendingFrameCount = 1;
	}
	m_source.unblockInjection(m_pipelinePtr);

	injector.join();
	EXPECT_FALSE(injected.load());

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_GenerationChangedDuringAddSegment_RespondsAbandoned
 * @brief Regression: if unblockInjection() runs while an injector is
 *        blocked inside pipeline.addSegment() (injectorActive=true, so
 *        unblockInjection() defers to the injector rather than
 *        answering itself), handleAddSegmentCompletion() must still answer
 *        the request with NO_AVAILABLE_SAMPLES once addSegment() returns -
 *        previously this generation-mismatch case was silently dropped,
 *        leaking the request forever since nothing else would ever
 *        answer it.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_GenerationChangedDuringAddSegment_RespondsAbandoned)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 55;
		st.pendingFrameCount = 1;
	}

	std::atomic<bool> addSegmentEntered{false};
	std::atomic<bool> releaseAddSegment{false};

	EXPECT_CALL(*m_pipelinePtr, addSegment(55, _))
		.WillOnce(Invoke([&](uint32_t,
			const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &)
			{
				addSegmentEntered.store(true, std::memory_order_release);
				while (!releaseAddSegment.load(std::memory_order_acquire))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 55))
		.WillOnce(Return(true));

	AampMediaSample sample;
	uint8_t data[] = {0x01, 0x02};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 1.0;
	sample.mDuration = 0.033;

	std::thread injector([&] {
		m_source.injectOneSample(*m_pipelinePtr, gen, std::move(sample), nullptr);
	});

	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds(200);
	while (!addSegmentEntered.load(std::memory_order_acquire) &&
		std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ASSERT_TRUE(addSegmentEntered.load(std::memory_order_acquire));

	// Simulate Flush()/Stop() happening while addSegment() is in flight.
	// injectorActive is true, so unblockInjection() must defer this
	// request to the injector rather than answering it itself.
	m_source.unblockInjection(m_pipelinePtr, "test-flush-race");

	releaseAddSegment.store(true, std::memory_order_release);
	injector.join();

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// EOS signaling
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_SignalEos_SetsEosFlag
 * @brief Verify signalEos sets the EOS flag on the source state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_SetsEosFlag)
{
	m_source.signalEos(nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
}

/**
 * @test AampRialtoVideoSource_SignalEos_WithPendingRequest_SendsHaveDataEOS
 * @brief Verify signalEos closes a pending needData request with EOS.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_WithPendingRequest_SendsHaveDataEOS)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(1, /*requestId=*/99, m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 99))
		.WillOnce(Return(true));

	m_source.signalEos(m_pipelinePtr);
}

// ---------------------------------------------------------------------------
// handleNeedData / handleCancelNeedData
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_HandleNeedData_SetsPendingState
 * @brief Verify handleNeedData sets the pending request state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleNeedData_SetsPendingState)
{
	m_source.handleNeedData(5, /*requestId=*/42, nullptr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.hasPending);
	EXPECT_EQ(st.pendingRequestId, 42u);
	EXPECT_EQ(st.pendingFrameCount, 5u);
}

/**
 * @test AampRialtoVideoSource_HandleNeedData_WhenEos_SendsEos
 * @brief Verify handleNeedData replies with EOS when source is in EOS state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleNeedData_WhenEos_SendsEos)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.signalEos(m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 55))
		.WillOnce(Return(true));

	m_source.handleNeedData(1, /*requestId=*/55, m_pipelinePtr);
}

/**
 * @test AampRialtoVideoSource_HandleNeedData_SupersededByNewRequest_ClosesOutStaleRequest
 * @brief Verify a second handleNeedData() call, arriving without an
 *        intervening handleCancelNeedData(), closes out the previous
 *        unclaimed request with haveData(NO_AVAILABLE_SAMPLES) instead of
 *        silently overwriting it.  Silently dropping the old requestId
 *        leaves Rialto's own bookkeeping for it unanswered, which can
 *        desync AAMP and Rialto once the newer request is eventually served.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_HandleNeedData_SupersededByNewRequest_ClosesOutStaleRequest)
{
	m_source.handleNeedData(1, /*requestId=*/42, m_pipelinePtr);

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 42))
		.WillOnce(Return(true));

	m_source.handleNeedData(1, /*requestId=*/43, m_pipelinePtr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.hasPending);
	EXPECT_EQ(st.pendingRequestId, 43u);
}

/**
 * @test AampRialtoVideoSource_HandleCancelNeedData_ClearsPending
 * @brief Verify handleCancelNeedData clears the pending state.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_HandleCancelNeedData_ClearsPending)
{
	m_source.handleNeedData(5, /*requestId=*/42, nullptr);

	m_source.handleCancelNeedData();

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// signalEos — with segmentsAddedInBatch > 0
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_SignalEos_WithInjectorActive_DoesNotSendHaveData
 * @brief Verify signalEos does NOT send haveData when an injector is active.
 *        The injector owns the request and will send haveData(EOS) itself.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_WithInjectorActive_DoesNotSendHaveData)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(10, /*requestId=*/77, m_pipelinePtr);

	// Simulate that injectOneSample is active and has pushed some segments.
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.segmentsAddedInBatch  = 3;
		st.injectorActive  = true;
	}

	// haveData should NOT be called — injectOneSample owns this request.
	EXPECT_CALL(*m_pipelinePtr, haveData(_, _)).Times(0);

	m_source.signalEos(m_pipelinePtr);

	// EOS flag is set, hasPending remains true for injectOneSample.
	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
	EXPECT_TRUE(st.hasPending);
}

/**
 * @test AampRialtoVideoSource_SignalEos_WithAddedSamplesNoInjector_SendsEos
 * @brief Verify signalEos sends haveData(EOS) immediately when no injector is
 *        active, even if some samples were already added for the request.
 *        Rialto will deliver the buffered segments before propagating EOS.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_SignalEos_WithAddedSamplesNoInjector_SendsEos)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	m_source.handleNeedData(10, /*requestId=*/88, m_pipelinePtr);

	// Simulate that a previous injector added samples and exited
	// (injectorActive is false).
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.segmentsAddedInBatch = 3;
		// injectorActive remains false (default)
	}

	// signalEos should fire immediately — no injector holds the slot.
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 88))
		.WillOnce(Return(true));

	m_source.signalEos(m_pipelinePtr);

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_TRUE(st.eos);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// injectOneSample — EOS already set, returns immediately
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InjectOneSample_EosAlreadySet_ReturnsFalse
 * @brief Verify injectOneSample returns false immediately when EOS is already
 *        set and no pending request is available.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InjectOneSample_EosAlreadySet_ReturnsFalse)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	// Signal EOS without a pending request.
	m_source.signalEos(m_pipelinePtr);

	uint64_t gen = m_source.captureGeneration();

	AampMediaSample sample;
	uint8_t data[] = {0xDE, 0xAD};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 1.0;
	sample.mDuration = 0.033;

	bool result = m_source.injectOneSample(
		*m_pipelinePtr, gen, std::move(sample), nullptr);

	EXPECT_FALSE(result);
}

// ---------------------------------------------------------------------------
// injectOneSample — EOS set during injection sends haveData(EOS)
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InjectOneSample_EosDuringInjection_SendsEos
 * @brief Verify that when EOS is set while a batch is in progress,
 *        injectOneSample injects the sample and sends haveData(EOS).
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InjectOneSample_EosDuringInjection_SendsEos)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();

	// Simulate mid-batch state: needData arrived, some samples already
	// pushed, then signalEos was called (which saw injectorActive=true
	// and did NOT steal the request).
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 42;
		st.pendingFrameCount     = 10;
		st.segmentsAddedInBatch  = 3;
		st.eos                   = true;
		st.injectorActive        = true;
	}

	// Expect addSegment to succeed, then haveData(EOS).
	EXPECT_CALL(*m_pipelinePtr, addSegment(42, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 42))
		.WillOnce(Return(true));

	AampMediaSample sample;
	uint8_t data[] = {0xCA, 0xFE};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 2.0;
	sample.mDuration = 0.033;

	bool result = m_source.injectOneSample(
		*m_pipelinePtr, gen, std::move(sample), nullptr);

	EXPECT_TRUE(result);
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_PtsNotExactInBinary_IsRounded
 * @brief 1.001 s and 1.003 s are not exact doubles, so truncating
 *        seconds * 1e9 gave 1000999999 ns and 1002999999 ns (and 1000 ms for
 *        the first PTS).  Conversions must round to nearest.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_PtsNotExactInBinary_IsRounded)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 43;
		st.pendingFrameCount     = 10;
		st.segmentsAddedInBatch  = 3;
		st.eos                   = true;
		st.injectorActive        = true;
	}

	int64_t timestampNs = 0;
	int64_t durationNs  = 0;
	EXPECT_CALL(*m_pipelinePtr, addSegment(43, _))
		.WillOnce(Invoke([&](uint32_t,
			const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				timestampNs = seg->getTimeStamp();
				durationNs  = seg->getDuration();
				return firebolt::rialto::AddSegmentStatus::OK;
			}));
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 43))
		.WillOnce(Return(true));

	AampMediaSample sample;
	uint8_t data[] = {0xCA, 0xFE};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *) {});
	sample.mDataSize = 2;
	sample.mPts = 1.001;
	sample.mDuration = 1.003;

	EXPECT_TRUE(m_source.injectOneSample(
		*m_pipelinePtr, gen, std::move(sample), nullptr));

	EXPECT_EQ(timestampNs, 1'001'000'000LL);
	EXPECT_EQ(durationNs, 1'003'000'000LL);
	EXPECT_EQ(m_source.firstPtsMs(), 1001);
}

// ---------------------------------------------------------------------------
// injectOneSample — EOS set while waiting, needData arrives
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InjectOneSample_EosSetThenNeedData_InjectsAndSendsEos
 * @brief Verify that when EOS is set while waiting and then a needData slot
 *        becomes available, the sample is still injected with EOS status.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InjectOneSample_EosSetThenNeedData_InjectsAndSendsEos)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();

	EXPECT_CALL(*m_pipelinePtr, addSegment(77, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 77))
		.WillOnce(Return(true));

	AampMediaSample sample;
	uint8_t data[] = {0xBE, 0xEF};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 3.0;
	sample.mDuration = 0.033;

	std::atomic<bool> injected{false};
	std::thread injector([&]{
		injected = m_source.injectOneSample(
			*m_pipelinePtr, gen, std::move(sample), nullptr);
	});

	// Wait until the injector has entered the handshake; from then on it
	// only checks the state under the lock, so the update below lands while
	// it waits.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	bool active = false;
	while (!active && std::chrono::steady_clock::now() < deadline)
	{
		{
			auto &st = m_source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			active = st.injectorActive;
		}
		if (!active)
		{
			std::this_thread::yield();
		}
	}
	ASSERT_TRUE(active);

	// Simultaneously set eos and provide a needData slot (simulating
	// both events arriving while the thread is waiting).
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.eos                   = true;
		st.hasPending            = true;
		st.pendingRequestId      = 77;
		st.pendingFrameCount     = 10;
		st.segmentsAddedInBatch  = 0;
		st.gateMode              = AampRialtoMediaSource::GateMode::NONE;
	}
	m_source.state().cv.notify_all();

	injector.join();
	EXPECT_TRUE(injected.load());
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_NoSpace_DoesNotSetFirstPts
 * @brief Verify firstPtsMs remains unset when addSegment does not accept
 *        the sample and the injection loop aborts.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_NoSpace_DoesNotSetFirstPts)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	uint64_t gen = m_source.captureGeneration();

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending            = true;
		st.pendingRequestId      = 81;
		st.pendingFrameCount     = 1;
		st.segmentsAddedInBatch  = 0;
		st.injectorActive        = true;
	}

	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 81))
		.WillOnce(Return(true));

	AampMediaSample sample;
	uint8_t data[] = {0x10, 0x11};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 2;
	sample.mPts = 4.0;
	sample.mDuration = 0.033;

	std::atomic<bool> injected{true};
	std::atomic<bool> addSegmentReturned{false};

	EXPECT_CALL(*m_pipelinePtr, addSegment(81, _))
		.WillOnce(Invoke([&](uint32_t,
			const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &)
			{
				addSegmentReturned.store(true, std::memory_order_release);
				return firebolt::rialto::AddSegmentStatus::NO_SPACE;
			}));

	std::thread injector([&] {
		injected = m_source.injectOneSample(
			*m_pipelinePtr, gen, std::move(sample), nullptr);
	});

	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds(200);
	while (!addSegmentReturned.load(std::memory_order_acquire) &&
		std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ASSERT_TRUE(addSegmentReturned.load(std::memory_order_acquire));
	EXPECT_EQ(m_source.firstPtsMs(), AampRialtoMediaSource::kFirstPtsNotSet);

	m_source.unblockInjection(m_pipelinePtr);

	injector.join();

	EXPECT_FALSE(injected.load());
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_CreateSegmentNull_RespondsNoAvailableSamples
 * @brief Regression: when createSegment() returns nullptr for a non-EOS
 *        request, the request must still be answered (with
 *        NO_AVAILABLE_SAMPLES) rather than silently dropped - previously
 *        only the EOS case was handled here, leaking the request in all
 *        other createSegment() failures.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_CreateSegmentNull_RespondsNoAvailableSamples)
{
	TestableAampRialtoVideoSourceNullSegment source;
	source.mReturnNullSegment = true;

	auto codecInfo = MakeH264CodecInfo();
	source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	{
		auto &st = source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 909;
		st.pendingFrameCount = 1;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 909))
		.WillOnce(Return(true));

	uint64_t gen = source.captureGeneration();
	AampMediaSample sample;
	uint8_t data[] = {0x01};
	sample.mData = std::shared_ptr<const uint8_t>(data, [](const uint8_t *){});
	sample.mDataSize = 1;
	sample.mPts = 1.0;
	sample.mDuration = 0.033;

	bool result = source.injectOneSample(
		*m_pipelinePtr, gen, std::move(sample), nullptr);

	EXPECT_FALSE(result);

	auto &st = source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

// ---------------------------------------------------------------------------
// injectOneSample — encrypted samples that cannot be injected correctly (L6)
// ---------------------------------------------------------------------------


/**
 * @test AampRialtoVideoSource_InjectOneSample_EncryptedWithoutDrmSession_IsDroppedAndAnswered
 * @brief Regression (L6): an encrypted sample for a source with no DRM
 *        session was injected without encryption metadata (WARN only).  It
 *        must be dropped, and the request still answered so it is not leaked.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_EncryptedWithoutDrmSession_IsDroppedAndAnswered)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	ASSERT_EQ(m_source.mksId(), -1);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 910;
		st.pendingFrameCount = 1;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 910))
		.WillOnce(Return(true));

	const uint64_t gen = m_source.captureGeneration();
	EXPECT_FALSE(m_source.injectOneSample(
		*m_pipelinePtr, gen, MakeEncryptedSample(0, 0), nullptr));

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_ShortSubSampleTable_IsDroppedAndAnswered
 * @brief Regression (L6): a sub-sample table shorter than the declared count
 *        was cut short silently and the sample injected anyway.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_ShortSubSampleTable_IsDroppedAndAnswered)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(42));
	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, &mockDrm, -1, prot);
	ASSERT_EQ(m_source.mksId(), 42);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 911;
		st.pendingFrameCount = 1;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 911))
		.WillOnce(Return(true));

	// Two entries declared, one present.
	const uint64_t gen = m_source.captureGeneration();
	EXPECT_FALSE(m_source.injectOneSample(
		*m_pipelinePtr, gen, MakeEncryptedSample(2, 6), nullptr));
}

// ---------------------------------------------------------------------------
// injectOneSample — encryption metadata on the segment
// ---------------------------------------------------------------------------

namespace
{
/// The encryption fields of the segment passed to addSegment(), copied while
/// the segment is still alive.
struct SegmentEncryption
{
	bool encrypted{false};
	int32_t mksId{-1};
	std::vector<uint8_t> keyId;
	std::vector<uint8_t> iv;
	firebolt::rialto::CipherMode cipher{firebolt::rialto::CipherMode::UNKNOWN};
	bool patternSet{false};
	uint32_t crypt{0};
	uint32_t skip{0};
	std::vector<std::pair<size_t, size_t>> subSamples;
};
} // namespace

/**
 * @class AampRialtoVideoSourceEncryptionTest
 * @brief Attaches the source with DRM session 42 and records the encryption
 *        fields of the next segment handed to Rialto.
 */
class AampRialtoVideoSourceEncryptionTest : public AampRialtoVideoSourceTest
{
protected:
	static constexpr int32_t kMksId = 42;

	void SetUp() override
	{
		AampRialtoVideoSourceTest::SetUp();
		EXPECT_CALL(m_drm, createSession(_, _, _, eMEDIATYPE_VIDEO))
			.WillOnce(Return(kMksId));
		AampRialtoMediaSource::ProtectionParams prot;
		prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
		prot.initData = {0xCA, 0xFE};
		prot.type     = eMEDIATYPE_VIDEO;
		auto codecInfo = MakeH264CodecInfo();
		m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, &m_drm, -1, prot);
		ASSERT_EQ(m_source.mksId(), kMksId);
	}

	/// Inject @p sample against a fresh request and return what Rialto got.
	SegmentEncryption Inject(AampMediaSample &&sample)
	{
		{
			auto &st = m_source.state();
			std::lock_guard<std::mutex> lock(st.mu);
			st.hasPending        = true;
			st.pendingRequestId  = 920;
			st.pendingFrameCount = 1;
		}
		SegmentEncryption seen;
		EXPECT_CALL(*m_pipelinePtr, addSegment(920, _))
			.WillOnce(Invoke([&seen](uint32_t, const auto &segment) {
				seen.encrypted = segment->isEncrypted();
				seen.mksId = segment->getMediaKeySessionId();
				seen.keyId = segment->getKeyId();
				seen.iv = segment->getInitVector();
				seen.cipher = segment->getCipherMode();
				seen.patternSet = segment->getEncryptionPattern(seen.crypt, seen.skip);
				for (const auto &pair : segment->getSubSamples())
				{
					seen.subSamples.emplace_back(
						pair.numClearBytes, pair.numEncryptedBytes);
				}
				return firebolt::rialto::AddSegmentStatus::OK;
			}));
		EXPECT_TRUE(m_source.injectOneSample(*m_pipelinePtr,
			m_source.captureGeneration(), std::move(sample), nullptr));
		return seen;
	}

	NiceMock<MockDrmBridge> m_drm;
};

/**
 * @test AampRialtoVideoSourceEncryptionTest_CencSubSamples_AreDecodedBigEndian
 * @brief Each 6-byte senc entry is 16-bit big-endian clear bytes then 32-bit
 *        big-endian encrypted bytes (ISO/IEC 23001-7); key id, IV, session and
 *        cipher reach the segment unchanged, and CENC sets no pattern.
 */
TEST_F(AampRialtoVideoSourceEncryptionTest,
	AampRialtoVideoSourceEncryptionTest_CencSubSamples_AreDecodedBigEndian)
{
	auto sample = MakeEncryptedSample(2, 0);
	sample.mDrmMetadata.mKeyId = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
	sample.mDrmMetadata.mIV = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18};
	sample.mDrmMetadata.mSubSamples = {
		0x00, 0x10, 0x00, 0x00, 0x01, 0x00,   // clear 16, encrypted 256
		0x01, 0x02, 0x01, 0x02, 0x03, 0x04};  // clear 258, encrypted 0x01020304

	const auto seen = Inject(std::move(sample));

	EXPECT_TRUE(seen.encrypted);
	EXPECT_EQ(seen.mksId, kMksId);
	EXPECT_EQ(seen.keyId, (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04, 0x05,
		0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}));
	EXPECT_EQ(seen.iv, (std::vector<uint8_t>{0x11, 0x12, 0x13, 0x14, 0x15,
		0x16, 0x17, 0x18}));
	EXPECT_EQ(seen.cipher, firebolt::rialto::CipherMode::CENC);
	EXPECT_FALSE(seen.patternSet);
	EXPECT_EQ(seen.subSamples, (std::vector<std::pair<size_t, size_t>>{
		{16, 256}, {258, 0x01020304}}));
}

/**
 * @test AampRialtoVideoSourceEncryptionTest_NoSubSamples_WholeSampleIsEncrypted
 * @brief Without a sub-sample table the whole sample is encrypted: one entry
 *        with no clear bytes and the full sample size.
 */
TEST_F(AampRialtoVideoSourceEncryptionTest,
	AampRialtoVideoSourceEncryptionTest_NoSubSamples_WholeSampleIsEncrypted)
{
	const auto seen = Inject(MakeEncryptedSample(0, 0));

	EXPECT_TRUE(seen.encrypted);
	EXPECT_EQ(seen.subSamples,
		(std::vector<std::pair<size_t, size_t>>{{0, 3}}));
}

/**
 * @test AampRialtoVideoSourceEncryptionTest_Cbcs_SetsCipherAndPattern
 * @brief CBCS carries the crypt/skip pattern (e.g. 1:9 for video); the other
 *        cipher schemes map to their Rialto modes.
 */
TEST_F(AampRialtoVideoSourceEncryptionTest,
	AampRialtoVideoSourceEncryptionTest_Cbcs_SetsCipherAndPattern)
{
	auto sample = MakeEncryptedSample(0, 0);
	sample.mDrmMetadata.mCipher = CIPHER_TYPE_CBCS;
	sample.mDrmMetadata.mCryptByteBlock = 1;
	sample.mDrmMetadata.mSkipByteBlock = 9;

	const auto seen = Inject(std::move(sample));

	EXPECT_EQ(seen.cipher, firebolt::rialto::CipherMode::CBCS);
	ASSERT_TRUE(seen.patternSet);
	EXPECT_EQ(seen.crypt, 1u);
	EXPECT_EQ(seen.skip, 9u);
}

/**
 * @test AampRialtoVideoSourceEncryptionTest_OtherCiphers_MapToRialtoModes
 * @brief CBC1 and CENS map to their Rialto modes and set no pattern.
 */
TEST_F(AampRialtoVideoSourceEncryptionTest,
	AampRialtoVideoSourceEncryptionTest_OtherCiphers_MapToRialtoModes)
{
	auto cbc1 = MakeEncryptedSample(0, 0);
	cbc1.mDrmMetadata.mCipher = CIPHER_TYPE_CBC1;
	const auto seenCbc1 = Inject(std::move(cbc1));
	EXPECT_EQ(seenCbc1.cipher, firebolt::rialto::CipherMode::CBC1);
	EXPECT_FALSE(seenCbc1.patternSet);

	auto cens = MakeEncryptedSample(0, 0);
	cens.mDrmMetadata.mCipher = CIPHER_TYPE_CENS;
	const auto seenCens = Inject(std::move(cens));
	EXPECT_EQ(seenCens.cipher, firebolt::rialto::CipherMode::CENS);
	EXPECT_FALSE(seenCens.patternSet);
}

/**
 * @test AampRialtoVideoSourceEncryptionTest_ClearSample_HasNoEncryptionMetadata
 * @brief A clear sample on a source with a DRM session (clear lead-in) is sent
 *        without encryption metadata.
 */
TEST_F(AampRialtoVideoSourceEncryptionTest,
	AampRialtoVideoSourceEncryptionTest_ClearSample_HasNoEncryptionMetadata)
{
	auto sample = MakeEncryptedSample(0, 0);
	sample.mDrmMetadata.mIsEncrypted = false;

	const auto seen = Inject(std::move(sample));

	EXPECT_FALSE(seen.encrypted);
	EXPECT_TRUE(seen.keyId.empty());
	EXPECT_TRUE(seen.subSamples.empty());
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_SampleOver32BitSize_IsDroppedAndAnswered
 * @brief Regression (L6): a sample size was cast to 32 bits for the segment,
 *        so a size above 4 GiB reached Rialto truncated.  The check runs before
 *        the payload is read, so a small buffer stands in for the payload.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_SampleOver32BitSize_IsDroppedAndAnswered)
{
	if (sizeof(size_t) < 8)
	{
		GTEST_SKIP() << "size_t is 32 bit";
	}

	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 913;
		st.pendingFrameCount = 1;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES, 913))
		.WillOnce(Return(true));

	AampMediaSample sample;
	static const uint8_t kData[] = {0x01};
	sample.mData = std::shared_ptr<const uint8_t>(kData, [](const uint8_t *){});
	sample.mDataSize = (static_cast<size_t>(1) << 32) + 1;
	sample.mPts = 1.0;
	sample.mDuration = 0.033;

	const uint64_t gen = m_source.captureGeneration();
	EXPECT_FALSE(m_source.injectOneSample(
		*m_pipelinePtr, gen, std::move(sample), nullptr));
}

/**
 * @test AampRialtoVideoSource_InjectOneSample_CompleteSubSampleTable_IsInjected
 * @brief Guard for the L6 checks: an encrypted sample with a DRM session and
 *        a complete sub-sample table is still injected.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_InjectOneSample_CompleteSubSampleTable_IsInjected)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(42));
	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, &mockDrm, -1, prot);
	ASSERT_EQ(m_source.mksId(), 42);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending        = true;
		st.pendingRequestId  = 912;
		st.pendingFrameCount = 1;
	}

	EXPECT_CALL(*m_pipelinePtr, addSegment(912, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));

	const uint64_t gen = m_source.captureGeneration();
	EXPECT_TRUE(m_source.injectOneSample(
		*m_pipelinePtr, gen, MakeEncryptedSample(2, 12), nullptr));
}

// ---------------------------------------------------------------------------
// gateMode — consolidated ungate-timing design
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_Reset_DoesNotClearGateMode
 * @brief reset() must NOT clear gateMode.
 *
 * AampRialtoPlayer::Configure() calls reset() mid-flight while a subsequent
 * Flush() may still be pending in a multi-step trickplay sequence
 * (Flush(pos=0) -> Configure() -> Flush(correctPos) -> Stream()). Clearing
 * the gate here would let an early needData slip through with
 * stale-position data. The gate must only be cleared by
 * gateInjection(false)/UngateAllSources() at genuine play()-issuance points.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_Reset_DoesNotClearGateMode)
{
	m_source.unblockInjection(m_pipelinePtr, "test-setup");
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);

	m_source.reset();

	EXPECT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);
}

/**
 * @test AampRialtoVideoSource_UnblockInjection_SetsGateModeDropped
 * @brief unblockInjection() must set gateMode to DROPPED and bump generation.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_UnblockInjection_SetsGateModeDropped)
{
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	uint64_t genBefore = m_source.state().generation;
	m_source.unblockInjection(m_pipelinePtr, "test");

	EXPECT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);
	EXPECT_GT(m_source.state().generation, genBefore);
}

/**
 * @test AampRialtoVideoSource_GateInjection_False_ClearsGateAndNotifies
 * @brief gateInjection(false) must clear gateMode to NONE regardless of the
 *        prior value, and wake any thread waiting on the state cv.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_GateInjection_False_ClearsGateAndNotifies)
{
	m_source.unblockInjection(m_pipelinePtr, "test-setup");
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);

	m_source.gateInjection(m_pipelinePtr, false, "test");

	EXPECT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test AampRialtoVideoSource_GateInjection_False_WhenAlreadyClear_IsNoop
 * @brief gateInjection(false) must be safe to call when the gate is already
 *        clear (no crash, remains NONE).
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_GateInjection_False_WhenAlreadyClear_IsNoop)
{
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	EXPECT_NO_THROW(m_source.gateInjection(m_pipelinePtr, false, "test"));

	EXPECT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test AampRialtoVideoSource_GateInjection_False_ReplaysDeferredEos
 * @brief Regression: gateInjection(false) must replay a deferred EOS
 *        resolution for a request that was left pending (with eos already
 *        set) while gateMode==BLOCKED, so the request is not left
 *        hanging forever.
 *
 *        Only BLOCKED defers an EOS resolution (the handshake claims it
 *        immediately when gateMode==DROPPED or NONE) - see the gateMode
 *        field comment. handleNeedData() itself does NOT stage a request
 *        while gated - it answers such requests immediately with
 *        NO_AVAILABLE_SAMPLES instead of deferring them (see
 *        AampRialtoVideoSource_HandleNeedData_DoesNotClearGateMode and the
 *        handleNeedData() rejectGated branch). The pending+eos state
 *        exercised here instead models the case where an active injector
 *        owns the request when the gate is set (injectorActive), and is
 *        prepared directly here to isolate gateInjection(false)'s own
 *        deferred-EOS replay logic.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_GateInjection_False_ReplaysDeferredEos)
{
	// Gate injection to BLOCKED, then seed a pending, EOS-eligible request
	// as if an active injector had left it outstanding when the gate was set.
	m_source.gateInjection(m_pipelinePtr, true, "test-setup");
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.hasPending       = true;
		st.pendingRequestId = 321;
		st.eos              = true;
	}

	// Clearing the gate must replay the deferred EOS resolution.
	EXPECT_CALL(*m_pipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, 321))
		.WillOnce(Return(true));
	m_source.gateInjection(m_pipelinePtr, false, "test");

	auto &st = m_source.state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.hasPending);
}

/**
 * @test AampRialtoVideoSource_HandleNeedData_DoesNotClearGateMode
 * @brief handleNeedData() must NOT clear gateMode even though it
 *        stages a pending request. The staged request is answered with
 *        NO_AVAILABLE_SAMPLES (never silently dropped) once an injector
 *        observes the gate, so it is safe for the gate to remain set until
 *        a genuine play()-issuance point clears it.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_HandleNeedData_DoesNotClearGateMode)
{
	m_source.unblockInjection(m_pipelinePtr, "test-setup");
	ASSERT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);

	m_source.handleNeedData(1, /*requestId=*/123, m_pipelinePtr);

	EXPECT_EQ(m_source.state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);
}
