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
 * @file AampRialtoNeedDataHandshakeTestCases.cpp
 * @brief L1 tests for AampRialtoNeedDataHandshake.
 *
 * Oracle: the Rialto pull-mode contract.  Every needData request is answered
 * exactly once; a request issued while gated is answered at once with no
 * samples; EOS is never answered ahead of samples held behind a BLOCKED gate;
 * a flush/stop (generation change) abandons the request to whoever owns it.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "AampRialtoNeedDataHandshake.h"

using Handshake = AampRialtoNeedDataHandshake;
using GateMode = Handshake::GateMode;
using Wait = Handshake::Wait;
using Reply = Handshake::Reply;

namespace
{
constexpr auto kBlocked = std::chrono::milliseconds(50);
constexpr auto kTimeout = std::chrono::seconds(5);

Handshake::Segment AnySegment(double ptsSec = 1.0)
{
	return Handshake::Segment{ptsSec, 0.04, 100, 0};
}
}

class AampRialtoNeedDataHandshakeTest : public ::testing::Test
{
protected:
	void SetGate(bool gate)
	{
		GateMode previous;
		GateMode current;
		(void)m_handshake.SetGate(gate, previous, current);
	}

	Handshake m_handshake;
};

// ---------------------------------------------------------------------------
// needData
// ---------------------------------------------------------------------------

/**
 * @test NeedData_IsHandedToTheInjector
 */
TEST_F(AampRialtoNeedDataHandshakeTest, NeedData_IsHandedToTheInjector)
{
	const auto result = m_handshake.OnNeedData(3, 42);
	EXPECT_FALSE(result.rejected);
	EXPECT_FALSE(result.superseded);
	EXPECT_FALSE(result.eos);

	ASSERT_TRUE(m_handshake.BeginInject());
	const auto gen = m_handshake.Generation();
	EXPECT_EQ(m_handshake.WaitForGate(gen).outcome, Wait::Proceed);
	const auto request = m_handshake.WaitForRequest(gen);
	EXPECT_EQ(request.outcome, Wait::Proceed);
	EXPECT_EQ(request.requestId, 42u);
}

/**
 * @test NeedDataWhileGated_IsRejectedNotStaged
 */
TEST_F(AampRialtoNeedDataHandshakeTest, NeedDataWhileGated_IsRejectedNotStaged)
{
	SetGate(true);
	const auto result = m_handshake.OnNeedData(3, 42);
	EXPECT_TRUE(result.rejected);
	EXPECT_FALSE(m_handshake.state().hasPending);
	// The gate itself is not cleared by the rejection.
	EXPECT_EQ(m_handshake.state().gateMode, GateMode::BLOCKED);
}

/**
 * @test SecondNeedData_ClosesOutTheUnansweredOne
 */
TEST_F(AampRialtoNeedDataHandshakeTest, SecondNeedData_ClosesOutTheUnansweredOne)
{
	(void)m_handshake.OnNeedData(1, 7);
	const auto result = m_handshake.OnNeedData(1, 8);
	ASSERT_TRUE(result.superseded);
	EXPECT_EQ(result.superseded->requestId, 7u);
	EXPECT_EQ(m_handshake.state().pendingRequestId, 8u);
}

/**
 * @test CancelNeedData_DropsTheRequest
 */
TEST_F(AampRialtoNeedDataHandshakeTest, CancelNeedData_DropsTheRequest)
{
	(void)m_handshake.OnNeedData(1, 7);
	m_handshake.OnCancelNeedData();
	EXPECT_FALSE(m_handshake.state().hasPending);
}

// ---------------------------------------------------------------------------
// Batch completion
// ---------------------------------------------------------------------------

/**
 * @test Complete_RepliesOkWhenTheBatchIsFull
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Complete_RepliesOkWhenTheBatchIsFull)
{
	(void)m_handshake.OnNeedData(2, 5);
	const auto gen = m_handshake.Generation();

	ASSERT_TRUE(m_handshake.BeginInject());
	auto first = m_handshake.Complete(gen, 5, /*morePending=*/true,
		AnySegment(1.0));
	EXPECT_EQ(first.reply, Reply::None);

	ASSERT_TRUE(m_handshake.BeginInject());
	auto second = m_handshake.Complete(gen, 5, true, AnySegment(1.04));
	EXPECT_EQ(second.reply, Reply::Ok);
	EXPECT_EQ(second.batch.frameCount, 2u);
	EXPECT_TRUE(second.batch.hasFirstPts);
	EXPECT_DOUBLE_EQ(second.batch.firstPtsSec, 1.0);
	EXPECT_EQ(second.batch.mediaBytes, 200u);
	EXPECT_FALSE(m_handshake.state().hasPending);
	EXPECT_FALSE(m_handshake.state().injectorActive);
}

/**
 * @test Complete_LastAvailableSampleRepliesOk
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Complete_LastAvailableSampleRepliesOk)
{
	(void)m_handshake.OnNeedData(10, 5);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto done = m_handshake.Complete(m_handshake.Generation(), 5,
		/*morePending=*/false, AnySegment());
	EXPECT_EQ(done.reply, Reply::Ok);
}

/**
 * @test Complete_AfterEosRepliesEos
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Complete_AfterEosRepliesEos)
{
	(void)m_handshake.OnNeedData(10, 5);
	ASSERT_TRUE(m_handshake.BeginInject());
	// The injector is active, so EOS is left for it to send.
	EXPECT_FALSE(m_handshake.SignalEos());
	const auto done = m_handshake.Complete(m_handshake.Generation(), 5, true,
		AnySegment());
	EXPECT_EQ(done.reply, Reply::Eos);
}

/**
 * @test Complete_AfterFlushRepliesAbandoned
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Complete_AfterFlushRepliesAbandoned)
{
	(void)m_handshake.OnNeedData(10, 5);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto gen = m_handshake.Generation();
	GateMode previous;
	// An active injector owns the request, so the flush leaves it.
	EXPECT_FALSE(m_handshake.Unblock(std::nullopt, previous));

	const auto done = m_handshake.Complete(gen, 5, true, AnySegment());
	EXPECT_EQ(done.reply, Reply::Abandoned);
	EXPECT_FALSE(m_handshake.state().hasPending);
}

/**
 * @test NoSpace_ClaimsTheRequestAndMarksBufferFull
 */
TEST_F(AampRialtoNeedDataHandshakeTest, NoSpace_ClaimsTheRequestAndMarksBufferFull)
{
	(void)m_handshake.OnNeedData(10, 5);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto claim = m_handshake.OnNoSpace(m_handshake.Generation(), 5);
	ASSERT_TRUE(claim);
	EXPECT_EQ(claim->requestId, 5u);
	EXPECT_TRUE(m_handshake.BufferFull());
	// The injector keeps its sample for the next needData.
	EXPECT_TRUE(m_handshake.state().injectorActive);

	GateMode previous;
	(void)m_handshake.Unblock(std::nullopt, previous);
	EXPECT_FALSE(m_handshake.BufferFull());
}

/**
 * @test NoSpace_FromAnOldGeneration_IsIgnored
 */
TEST_F(AampRialtoNeedDataHandshakeTest, NoSpace_FromAnOldGeneration_IsIgnored)
{
	const auto gen = m_handshake.Generation();
	m_handshake.Reset();
	(void)m_handshake.OnNeedData(10, 5);
	EXPECT_FALSE(m_handshake.OnNoSpace(gen, 5));
	EXPECT_FALSE(m_handshake.BufferFull());
	EXPECT_TRUE(m_handshake.state().hasPending);
}

/**
 * @test DropSample_HandsBackTheRequest
 */
TEST_F(AampRialtoNeedDataHandshakeTest, DropSample_HandsBackTheRequest)
{
	(void)m_handshake.OnNeedData(10, 5);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto drop = m_handshake.DropSample(5);
	ASSERT_TRUE(drop.claim);
	EXPECT_EQ(drop.claim->requestId, 5u);
	EXPECT_FALSE(drop.eos);
	EXPECT_FALSE(m_handshake.state().injectorActive);
}

// ---------------------------------------------------------------------------
// EOS
// ---------------------------------------------------------------------------

/**
 * @test SignalEos_AnswersAnIdleRequest
 */
TEST_F(AampRialtoNeedDataHandshakeTest, SignalEos_AnswersAnIdleRequest)
{
	(void)m_handshake.OnNeedData(1, 9);
	const auto claim = m_handshake.SignalEos();
	ASSERT_TRUE(claim);
	EXPECT_EQ(claim->requestId, 9u);
}

/**
 * @test NeedDataAfterEos_IsAnsweredWithEos
 */
TEST_F(AampRialtoNeedDataHandshakeTest, NeedDataAfterEos_IsAnsweredWithEos)
{
	(void)m_handshake.SignalEos();
	const auto result = m_handshake.OnNeedData(1, 9);
	ASSERT_TRUE(result.eos);
	EXPECT_EQ(result.eos->requestId, 9u);
}

/**
 * @test EosWhileBlocked_IsDeferredUntilUngate
 */
TEST_F(AampRialtoNeedDataHandshakeTest, EosWhileBlocked_IsDeferredUntilUngate)
{
	(void)m_handshake.OnNeedData(1, 9);
	SetGate(true);
	EXPECT_FALSE(m_handshake.SignalEos());
	EXPECT_TRUE(m_handshake.state().hasPending);

	GateMode previous;
	GateMode current;
	const auto claim = m_handshake.SetGate(false, previous, current);
	EXPECT_EQ(previous, GateMode::BLOCKED);
	EXPECT_EQ(current, GateMode::NONE);
	ASSERT_TRUE(claim);
	EXPECT_EQ(claim->requestId, 9u);
}

/**
 * @test BeginInject_AfterEosWasAnswered_ReturnsFalse
 */
TEST_F(AampRialtoNeedDataHandshakeTest, BeginInject_AfterEosWasAnswered_ReturnsFalse)
{
	(void)m_handshake.SignalEos();
	EXPECT_FALSE(m_handshake.BeginInject());
	EXPECT_FALSE(m_handshake.state().injectorActive);
}

/**
 * @test SetEos_ReportsOnlyChanges
 */
TEST_F(AampRialtoNeedDataHandshakeTest, SetEos_ReportsOnlyChanges)
{
	EXPECT_TRUE(m_handshake.SetEos(true));
	EXPECT_FALSE(m_handshake.SetEos(true));
	EXPECT_TRUE(m_handshake.IsEos());
	EXPECT_TRUE(m_handshake.SetEos(false));
}

// ---------------------------------------------------------------------------
// Flush / stop / reset
// ---------------------------------------------------------------------------

/**
 * @test Unblock_WithNoInjector_ClaimsTheRequest
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Unblock_WithNoInjector_ClaimsTheRequest)
{
	(void)m_handshake.OnNeedData(1, 9);
	GateMode previous;
	const auto claim = m_handshake.Unblock(true, previous);
	ASSERT_TRUE(claim);
	EXPECT_EQ(claim->requestId, 9u);
	EXPECT_EQ(previous, GateMode::NONE);
	EXPECT_EQ(m_handshake.state().gateMode, GateMode::DROPPED);
	EXPECT_TRUE(m_handshake.IsEos());
}

/**
 * @test Reset_KeepsTheGate
 */
TEST_F(AampRialtoNeedDataHandshakeTest, Reset_KeepsTheGate)
{
	SetGate(true);
	(void)m_handshake.SignalEos();
	const auto gen = m_handshake.Generation();
	m_handshake.Reset();
	EXPECT_EQ(m_handshake.state().gateMode, GateMode::BLOCKED);
	EXPECT_FALSE(m_handshake.IsEos());
	EXPECT_NE(m_handshake.Generation(), gen);
}

// ---------------------------------------------------------------------------
// Injector waits (threads)
// ---------------------------------------------------------------------------

/**
 * @test WaitForGate_HoldsWhileBlockedAndReleasesOnUngate
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForGate_HoldsWhileBlockedAndReleasesOnUngate)
{
	SetGate(true);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto gen = m_handshake.Generation();
	auto waiter = std::async(std::launch::async,
		[&]{ return m_handshake.WaitForGate(gen).outcome; });

	EXPECT_EQ(waiter.wait_for(kBlocked), std::future_status::timeout);
	SetGate(false);
	ASSERT_EQ(waiter.wait_for(kTimeout), std::future_status::ready);
	EXPECT_EQ(waiter.get(), Wait::Proceed);
}

/**
 * @test WaitForGate_DroppedAbortsWithoutBlocking
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForGate_DroppedAbortsWithoutBlocking)
{
	GateMode previous;
	(void)m_handshake.Unblock(std::nullopt, previous);
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto result = m_handshake.WaitForGate(m_handshake.Generation());
	EXPECT_EQ(result.outcome, Wait::Abort);
	EXPECT_FALSE(m_handshake.state().injectorActive);
}

/**
 * @test WaitForRequest_FlushAbortsAndHandsBackTheRequest
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForRequest_FlushAbortsAndHandsBackTheRequest)
{
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto gen = m_handshake.Generation();
	auto waiter = std::async(std::launch::async,
		[&]{ return m_handshake.WaitForRequest(gen); });
	EXPECT_EQ(waiter.wait_for(kBlocked), std::future_status::timeout);

	// A request arrives and is flushed before the injector wakes; the
	// active injector, not Unblock(), must answer it.
	{
		std::lock_guard<std::mutex> lock(m_handshake.state().mu);
		m_handshake.state().hasPending = true;
		m_handshake.state().pendingRequestId = 11;
		++m_handshake.state().generation;
	}
	m_handshake.WakeWaiters();

	ASSERT_EQ(waiter.wait_for(kTimeout), std::future_status::ready);
	const auto result = waiter.get();
	EXPECT_EQ(result.outcome, Wait::Abort);
	ASSERT_TRUE(result.abandoned);
	EXPECT_EQ(result.abandoned->requestId, 11u);
}

/**
 * @test WaitForRequest_GateReassertedSendsTheInjectorBack
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForRequest_GateReassertedSendsTheInjectorBack)
{
	ASSERT_TRUE(m_handshake.BeginInject());
	const auto gen = m_handshake.Generation();
	auto waiter = std::async(std::launch::async,
		[&]{ return m_handshake.WaitForRequest(gen).outcome; });
	EXPECT_EQ(waiter.wait_for(kBlocked), std::future_status::timeout);
	SetGate(true);
	ASSERT_EQ(waiter.wait_for(kTimeout), std::future_status::ready);
	EXPECT_EQ(waiter.get(), Wait::GateReasserted);
}

/**
 * @test WaitForAttach_NotPending_ReturnsAtOnce
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForAttach_NotPending_ReturnsAtOnce)
{
	bool blocked = false;
	EXPECT_EQ(m_handshake.WaitForAttach([]{ return false; },
			[&]{ blocked = true; }),
		Handshake::AttachWait::NotPending);
	EXPECT_FALSE(blocked);
}

/**
 * @test WaitForAttach_FlushAbortsTheWait
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForAttach_FlushAbortsTheWait)
{
	m_handshake.state().attachPending = true;
	std::atomic<bool> blocked{false};
	auto waiter = std::async(std::launch::async, [&]{
		return m_handshake.WaitForAttach([]{ return false; },
			[&]{ blocked = true; });
	});
	EXPECT_EQ(waiter.wait_for(kBlocked), std::future_status::timeout);
	EXPECT_TRUE(blocked.load());

	GateMode previous;
	(void)m_handshake.Unblock(std::nullopt, previous);
	ASSERT_EQ(waiter.wait_for(kTimeout), std::future_status::ready);
	EXPECT_EQ(waiter.get(), Handshake::AttachWait::Aborted);
}

/**
 * @test WaitForAttach_AttachedReleasesTheWait
 */
TEST_F(AampRialtoNeedDataHandshakeTest, WaitForAttach_AttachedReleasesTheWait)
{
	m_handshake.state().attachPending = true;
	std::atomic<bool> attached{false};
	auto waiter = std::async(std::launch::async, [&]{
		return m_handshake.WaitForAttach([&]{ return attached.load(); },
			[]{});
	});
	EXPECT_EQ(waiter.wait_for(kBlocked), std::future_status::timeout);
	{
		std::lock_guard<std::mutex> lock(m_handshake.state().mu);
		attached = true;
	}
	m_handshake.WakeWaiters();
	ASSERT_EQ(waiter.wait_for(kTimeout), std::future_status::ready);
	EXPECT_EQ(waiter.get(), Handshake::AttachWait::Attached);
}
