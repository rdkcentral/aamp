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
 * @file AampRialtoPlayerFlushTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Flush and seek: SeekStreamSink, Flush() teardown paths, applied rate, Discontinuity and the deferred discontinuity flush.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase 6 — setSourcePosition before first injection
// ===========================================================================

/**
 * @test InjectSamples_FirstInjection_CallsSetSourcePosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_FirstInjection_CallsSetSourcePosition)
{
	m_player->Flush(10.0, 1, false);
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// setSourcePosition fires for both video (id=0) and the inband CC
	// subtitle source (id=1) created alongside video in Configure().
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 10000000000LL, true, _, _)).Times(2);

	SendVideoInitFragment();
}

/**
 * @test AttachSource_NoFlush_DoesNotCallSetSourcePosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	AttachSource_NoFlush_DoesNotCallSetSourcePosition)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);

	SendVideoInitFragment();
}

// Stop() ends the session outright (as opposed to Flush()'s internal
// teardown-recovery call, which pre-stages a position for the very next
// Configure()/AttachSource()).  A stale staged position left over from
// the ended session must not be reused by a fresh tune's own AttachSource()
// when that tune issues no Flush() of its own (e.g. an ordinary HLS tune).
/**
 * @test Stop_ResetsPendingPositionForNextSessionsAttach
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_ResetsPendingPositionForNextSessionsAttach)
{
	// First session: Flush() stages a non-zero position before attach.
	m_player->Flush(10.0, 1, false);
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	m_player->Stop(false);

	// Second session (fresh tune): no Flush() before the video source
	// attaches - the stale 10s position must not be reused.
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	EXPECT_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);
	SendVideoInitFragment();
}

// ===========================================================================
// Phase 7 — Flush calls pipeline flush + resets queues
// ===========================================================================

/**
 * @test Flush_CallsPipelineSetPositionAtFlushPosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_CallsPipelineSetPositionAtFlushPosition)
{
	/**
	 * @brief Flush() now performs a pipeline-level flushing seek via
	 *        setPosition() instead of the old per-source flush() IPC.
	 *        position=5.0s -> 5,000,000,000ns.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _)).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(5'000'000'000LL))
		.WillOnce(Return(true));

	m_player->Flush(5.0, 1, false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

/**
 * @test Flush_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	Flush_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
}

// ===========================================================================
// SeekStreamSink — Delegates to Flush() (matches AAMPGstPlayer::SeekStreamSink())
// ===========================================================================

/**
 * @test SeekStreamSink_CallsSetPositionWithCorrectValue
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_CallsSetPositionWithCorrectValue)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// SeekStreamSink delegates to Flush(), which calls pipeline->setPosition()
	// with position in nanoseconds. position = 10.5 seconds = 10,500,000,000ns.
	const int64_t expectedPosNs = 10'500'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));

	m_player->SeekStreamSink(10.5, 1.0);
}

/**
 * @test SeekStreamSink_TrickplayRate_CallsSetPositionAndStoresRate
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_TrickplayRate_CallsSetPositionAndStoresRate)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// position = 20.0 seconds = 20,000,000,000 nanoseconds
	const int64_t expectedPosNs = 20'000'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));

	m_player->SeekStreamSink(20.0, 4.0);
}

/**
 * @test SeekStreamSink_ReverseRate_CallsSetPosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_ReverseRate_CallsSetPosition)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// position = 15.0 seconds = 15,000,000,000 nanoseconds
	const int64_t expectedPosNs = 15'000'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));

	m_player->SeekStreamSink(15.0, -2.0);
}

/**
 * @test SeekStreamSink_GatesAllSourcesDuringFlush
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_GatesAllSourcesDuringFlush)
{
	/**
	 * @brief A mid-playback discontinuity (e.g. AampMp4Demuxer's delayed
	 *        video-track flush) must gate every source, not just adjust
	 *        position - otherwise other tracks keep injecting new-period
	 *        data completely unsynchronized with the flush.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->SeekStreamSink(8.76, 1.0);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test SeekStreamSink_AutoResumesAllSourcesAfterSeekDone
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_AutoResumesAllSourcesAfterSeekDone)
{
	/**
	 * @brief Nothing else calls Stream() again for a mid-playback
	 *        discontinuity flush, so SeekStreamSink() must itself request
	 *        playback resumption - otherwise sources stay gated forever
	 *        once SEEK_DONE arrives, freezing playback.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->SeekStreamSink(8.76, 1.0);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test SeekStreamSink_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	SeekStreamSink_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->SeekStreamSink(0.0, 1.0));
}

/**
 * @test SeekStreamSink_NoSourcesAttached_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	SeekStreamSink_NoSourcesAttached_DoesNotCrash)
{
	Configure();
	EXPECT_NO_THROW(m_player->SeekStreamSink(5.0, 1.0));
}

// ===========================================================================
// Flush — shouldTearDown parameter tests
// ===========================================================================

/**
 * @test Flush_NullPipeline_ShouldTearDownTrue_StaysIdle
 */
TEST_F(AampRialtoPlayerTest,
	Flush_NullPipeline_ShouldTearDownTrue_StaysIdle)
{
	/**
	 * @brief When the player is in IDLE state (no pipeline) and shouldTearDown=true,
	 *        there is nothing to tear down: Flush() leaves the player IDLE.
	 */
	// Setup: DON'T call Configure() so player remains in IDLE state.
	// Player is constructed in SetUp() but no pipeline is created yet.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Precondition: player must be in IDLE state";

	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/true);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Player remains in IDLE after Flush(shouldTearDown=true) with no pipeline";
}

/**
 * @test Flush_NullPipeline_ShouldTearDownTrue_DoesNotRunRecoveryStop
 */
TEST_F(AampRialtoPlayerTest,
	Flush_NullPipeline_ShouldTearDownTrue_DoesNotRunRecoveryStop)
{
	/**
	 * @brief Regression (L7): Flush(shouldTearDown=true) before Configure() ran
	 *        the recovery Stop() although nothing existed to stop, which
	 *        deregistered the control backend; the following Configure() then
	 *        had to create a new one.  The existing backend must survive and
	 *        be the one Configure() waits on.
	 */
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
	EXPECT_CALL(*m_mockControlBackend, waitForRunning(_))
		.Times(1)
		.WillOnce(Return(true));

	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/true);
	Configure();
}

/**
 * @test Flush_PipelineNotFlushable_ShouldTearDownTrue_StillStopsPipeline
 */
TEST_F(AampRialtoPlayerTest,
	Flush_PipelineNotFlushable_ShouldTearDownTrue_StillStopsPipeline)
{
	/**
	 * @brief Guard for L7: when a pipeline exists but the state is not
	 *        flushable, the recovery Stop() still runs.
	 */
	Configure();
	ASSERT_NE(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1).WillOnce(Return(true));

	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/true);
}

/**
 * @test Flush_NullPipeline_ShouldTearDownFalse_DoesNotCallStop
 */
TEST_F(AampRialtoPlayerTest,
	Flush_NullPipeline_ShouldTearDownFalse_DoesNotCallStop)
{
	/**
	 * @brief When the player is in IDLE state (no pipeline) and shouldTearDown=false,
	 *        Flush() must NOT call Stop(), only log and return.
	 *
	 * This mirrors GStreamer: if shouldTearDown=false, the pipeline
	 * error is logged but no recovery action is taken.
	 */
	// Setup: DON'T call Configure() so player remains in IDLE state.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);

	// Flush() with shouldTearDown=false should NOT change state.
	EXPECT_NO_FATAL_FAILURE(
		m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false));

	// Verify player remains in IDLE state (no Stop() called).
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Player must remain in IDLE when shouldTearDown=false";
}

/**
 * @test Flush_NullPipeline_ShouldTearDownTrue_StagesPendingPositionAndRate
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_NullPipeline_ShouldTearDownTrue_StagesPendingPositionAndRate)
{
	/**
	 * @brief Flush() must stage the position and rate before
	 *        the shouldTearDown early-return branch, so a subsequent
	 *        Configure()/attach still observes the requested position and
	 *        rate even though this Flush() call took the Stop(true) path.
	 */
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Precondition: player must be in IDLE state";

	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/true);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// setSourcePosition fires for both video (id=0) and the inband CC
	// subtitle source (id=1) created alongside video in Configure().
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 10000000000LL, true, _, _)).Times(2);

	SendVideoInitFragment();
}

/**
 * @test Flush_PipelineStopped_ShouldTearDownTrue_DoesNotCrash
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PipelineStopped_ShouldTearDownTrue_DoesNotCrash)
{
	/**
	 * @brief When the player is already stopped (pipeline destroyed) and
	 *        shouldTearDown=true, Flush() re-enters Stop(true); with no
	 *        pipeline left this must be a safe no-op, not a crash.
	 */
	Configure();
	m_player->Stop(false);

	// onStop() transitions to IDLE.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Precondition: player must be in IDLE state after Stop()";

	// Stop() already destroyed the pipeline (m_mockPipelinePtr is now
	// dangling) - no EXPECT_CALL can be set against it.
	EXPECT_NO_THROW(
		m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/true));

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

/**
 * @test Flush_PipelineStopped_ShouldTearDownFalse_DoesNotCrash
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PipelineStopped_ShouldTearDownFalse_DoesNotCrash)
{
	/**
	 * @brief When the player is already stopped (pipeline destroyed) and
	 *        shouldTearDown=false, Flush() must return early without
	 *        touching the already-gone pipeline.
	 */
	Configure();
	m_player->Stop(false);

	// onStop() transitions to IDLE.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);

	// Stop() already destroyed the pipeline (m_mockPipelinePtr is now
	// dangling) - no EXPECT_CALL can be set against it.
	EXPECT_NO_THROW(
		m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/false));

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

/**
 * @test Flush_ValidPipeline_ShouldTearDownTrue_DoesNotCallStop
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_ValidPipeline_ShouldTearDownTrue_DoesNotCallStop)
{
	/**
	 * @brief When the player is in PLAYING state (valid for flushing),
	 *        shouldTearDown has no effect — Flush() proceeds with normal flush.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Transition to PLAYING state.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING)
		<< "Precondition: player must be in PLAYING state";

	// Player is in PLAYING state; expect the pipeline-level flushing seek,
	// NOT stop() and NOT the old per-source flush() IPC.
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _)).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));

	m_player->Flush(/*position=*/20.0, /*rate=*/1, /*shouldTearDown=*/true);

	// Verify player transitions to FLUSHING state.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Player must transition to FLUSHING after successful Flush()";
}

/**
 * @test Configure_PipelineReused_CallsResumeTrackDownloadsForAllExistingSources
 */
TEST_F(AampRialtoPlayerTest,
	Configure_PipelineReused_CallsResumeTrackDownloadsForAllExistingSources)
{
	/**
	 * @brief When Configure() reuses the existing pipeline (same formats,
	 *        no forced recreation), ResumeTrackDownloads must still be
	 *        called for every existing source so AAMP's track worker
	 *        threads are unblocked.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	// Second Configure with identical formats — pipeline is reused.
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ResumeTrackDownloads(eMEDIATYPE_VIDEO)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ResumeTrackDownloads(eMEDIATYPE_AUDIO)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ResumeTrackDownloads(eMEDIATYPE_SUBTITLE)).Times(1);

	m_player->Configure(MakeCodecInfo(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID),
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

/**
 * @test Flush_ValidPipeline_ShouldTearDownFalse_DoesNotCallStop
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_ValidPipeline_ShouldTearDownFalse_DoesNotCallStop)
{
	/**
	 * @brief When the player is in PLAYING state (valid for flushing),
	 *        shouldTearDown has no effect — Flush() proceeds with normal flush.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Transition to PLAYING state.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	// Player is in PLAYING state; expect the pipeline-level flushing seek,
	// NOT stop() and NOT the old per-source flush() IPC.
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _)).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));

	m_player->Flush(/*position=*/15.0, /*rate=*/1, /*shouldTearDown=*/false);

	// Verify player transitions to FLUSHING state.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

static void SetupCapabilities(
	std::shared_ptr<NiceMock<MockIMediaPipelineCapabilitiesFactory>> &factory,
	bool querySucceeds,
	bool videoMaster);

/**
 * @test Flush_SequentialFlushes_SecondFlushIssuesNewPipelineSetPositionWithNewPositionAndRate
 */
TEST_F(AampRialtoPlayerTest,
	Flush_SequentialFlushes_SecondFlushIssuesNewPipelineSetPositionWithNewPositionAndRate)
{
	/**
	 * @brief Flush() calls WaitForFlushToComplete() at entry, so a second
	 *        Flush() issued after SEEK_DONE completes the first cycle issues
	 *        a fresh pipeline-level setPosition() with its own position and
	 *        rate.
	 *
	 * This replaces the old re-entrant-flush test.  With the blocking design
	 * there is no re-entrant path: the second Flush() simply waits for the
	 * first cycle to finish, then starts its own.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	// First flush: pipeline-level seek to the requested position.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(10'000'000'000LL))
		.WillOnce(Return(true));
	m_player->Flush(/*position=*/10.0, /*rate=*/2, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Complete first flush cycle via SEEK_DONE.  Capabilities are not
	// stubbed here, so computeAppliedRate() falls back to 1.0 (normal) and
	// no extra setSourcePosition() call is issued.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	ASSERT_NE(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "First flush cycle must be complete before second Flush() call";

	// Second flush: fresh pipeline-level seek with the new position/rate.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(33'000'000'000LL))
		.WillOnce(Return(true));
	m_player->Flush(/*position=*/33.0, /*rate=*/-4, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Complete second flush cycle; Rialto's PLAYING follows SEEK_DONE and
	// moves the player out of FLUSHED, where position is not yet reported.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// GetPositionMilliseconds() must use both the rate and the segment-start
	// baseline (33000 ms) committed by the second flush's SEEK_DONE:
	// (500 - 33000) * -4 = 130000 ms.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 130'000LL);
}

/**
 * @test Flush_CommitsRateOnlyAfterSeekDone
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_CommitsRateOnlyAfterSeekDone)
{
	/**
	 * @brief Flush() stages the pending rate, but the rate multiplier must
	 *        only be applied once SEEK_DONE confirms the flushing seek
	 *        completed.  While FLUSHING or FLUSHED, GetPositionMilliseconds()
	 *        short-circuits to 0 without querying the pipeline or applying
	 *        the pending rate.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	ASSERT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->isAttached());

	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/false);

	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/12.0, /*rate=*/-4, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Still FLUSHING: no valid position yet, so report 0 and ignore both
	// the not-yet-committed rate and the raw pipeline query.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).Times(0);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);

	// SEEK_DONE commits the rate and baseline, but the pipeline may still
	// be pre-rolling so FLUSHED also reports 0.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);
	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);
	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));

	// Rialto's PLAYING leaves FLUSHED; the committed rate AND the
	// segment-start baseline (12000 ms) now apply:
	// (500 - 12000) * -4 = 46000 ms.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 46'000LL);
}

/**
 * @test Flush_PositionNotExactInBinary_IsRoundedToNearestMillisecond
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PositionNotExactInBinary_IsRoundedToNearestMillisecond)
{
	/**
	 * @brief 1.001 s is not an exact double, so truncating seconds * 1e9
	 *        staged 1000999999 ns and the segment-start baseline committed
	 *        at SEEK_DONE became 1000 ms instead of 1001 ms.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/1.001, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// (1002 - 1001) * 1 = 1 ms; a truncated baseline of 1000 ms gives 2 ms.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(1'002'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 1LL);
}

// ===========================================================================
// appliedRate selection in setSourcePosition — isVideoMaster integration
// ===========================================================================

 /// Helper: program the IMediaPipelineCapabilitiesFactory mock to return a
 /// NiceMock<MockIMediaPipelineCapabilities> whose isVideoMaster() behavior is
 /// controlled by querySucceeds/videoMaster.
static void SetupCapabilities(
	std::shared_ptr<NiceMock<MockIMediaPipelineCapabilitiesFactory>> &factory,
	bool querySucceeds,
	bool videoMaster)
{
	ON_CALL(*factory, createMediaPipelineCapabilities())
		.WillByDefault(Invoke(
			[querySucceeds, videoMaster]()
				-> std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities>
			{
				auto caps =
					std::make_unique<NiceMock<MockIMediaPipelineCapabilities>>();
				ON_CALL(*caps, isVideoMaster(_))
					.WillByDefault(DoAll(
						SetArgReferee<0>(videoMaster),
						Return(querySucceeds)));
				return caps;
			}));
}

/**
 * @test SetStreamCaps_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition
 */
TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition)
{
	/**
	 * @brief When the platform reports isVideoMaster==false, setSourcePosition
	 * must be called with appliedRate equal to the flushed rate (2.0) for the
	 * video source, but the accompanying inband CC subtitle source (which
	 * carries no trickplay rate of its own) must get appliedRate==1.0.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/false);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);

	// The branch adds an inband CC subtitle source alongside the video source,
	// so both attachSource and setSourcePosition are called twice.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.Times(2)
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 2.0, _))
		.Times(1)
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 1.0, _))
		.Times(1)
		.WillOnce(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

/**
 * @test SetStreamCaps_FlushAtRate2_VideoMaster_UsesRate1InSetSourcePosition
 */
TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_FlushAtRate2_VideoMaster_UsesRate1InSetSourcePosition)
{
	/**
	 * @brief When the platform reports isVideoMaster==true, setSourcePosition
	 * must be called with appliedRate == 1.0 regardless of stored rate.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/true);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);

	// The branch adds an inband CC subtitle source alongside the video source,
	// so both attachSource and setSourcePosition are called twice.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.Times(2)
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 1.0, _))
		.Times(2)
		.WillRepeatedly(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

/**
 * @test SetStreamCaps_FlushAtRate2_CapabilityQueryFails_UsesRate1InSetSourcePosition
 */
TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_FlushAtRate2_CapabilityQueryFails_UsesRate1InSetSourcePosition)
{
	/**
	 * @brief When isVideoMaster() call itself fails (returns false), fall back
	 * to appliedRate == 1.0.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/false,
		/*videoMaster=*/false);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);

	// The branch adds an inband CC subtitle source alongside the video source,
	// so both attachSource and setSourcePosition are called twice.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.Times(2)
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 1.0, _))
		.Times(2)
		.WillRepeatedly(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

/**
 * @test SeekDone_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition
 */
TEST_F(AampRialtoPlayerTest,
	SeekDone_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition)
{
	/**
	 * @brief SEEK_DONE must forward appliedRate == stored rate to the video
	 * source's setSourcePosition() when the platform reports
	 * isVideoMaster==false.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/false);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// Attach source first so the video source exists for setSourcePosition().
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// appliedRate=2.0 != normal, so the SEEK_DONE handler issues the extra
	// video-source setSourcePosition() call (resetTime=false - the
	// pipeline-level setPosition() already flushed the source buffers).
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(m_mockSources[eMEDIATYPE_VIDEO]->sourceId(),
			testing::Ge(10'000'000'000LL),
			/*resetTime=*/false, 2.0, _))
		.WillOnce(Return(true));

	// The subtitle source's position is also updated unconditionally on
	// every SEEK_DONE, independent of the applied playback rate.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId(),
			testing::Ge(10'000'000'000LL),
			/*resetTime=*/false, _, _))
		.WillOnce(Return(true));

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
}

/**
 * @test SeekDone_FlushAtRate2_VideoMaster_SkipsSetSourcePosition
 */
TEST_F(AampRialtoPlayerTest,
	SeekDone_FlushAtRate2_VideoMaster_SkipsSetSourcePosition)
{
	/**
	 * @brief When the platform reports isVideoMaster==true, computeAppliedRate()
	 * collapses to AAMP_NORMAL_PLAY_RATE (1.0), so the SEEK_DONE handler must
	 * NOT issue the extra setSourcePosition() call - the pipeline-level
	 * setPosition() already established the correct (normal-rate) segment.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/true);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// Attach source first so the video source exists for setSourcePosition().
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Video appliedRate collapses to 1.0 (normal), so no extra video-source
	// setSourcePosition() call is issued.  The subtitle source's position is
	// still updated unconditionally on every SEEK_DONE.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId(),
			testing::Ge(10'000'000'000LL),
			/*resetTime=*/false, _, _))
		.WillOnce(Return(true));

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// SEEK_DONE only drives FLUSHING -> FLUSHED. No Rialto PLAYING/PAUSED
	// notification has been posted in this test, so the state machine
	// remains in FLUSHED awaiting one (it does not restore SOURCES_ATTACHED).
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);
}

/**
 * @test SeekDone_SubtitleAttached_UpdatesSubtitlePosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekDone_SubtitleAttached_UpdatesSubtitlePosition)
{
	/**
	 * @brief SEEK_DONE must forward the flushed position to the subtitle
	 *        source's setSourcePosition() with resetTime=false, independent
	 *        of appliedRate, so the Thunder text-track renderer learns the
	 *        new position without waiting for the next
	 *        SignalSubtitleClock() call.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/12.5, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId(),
			12'500'000'000LL, /*resetTime=*/false, _, _))
		.WillOnce(Return(true));

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
}

/**
 * @test SendTransfer() must also drive the deferred pending-position flush
 *       (MaybeFlushForPendingPosition()) for its direct processDataFragment()
 *       path (non-init fragment, no external demuxer), not just SendSample().
 */
TEST_F(AampRialtoPlayerTest,
	SendTransfer_PositionPending_FlushesToFragmentPts)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	const int64_t expectedPosNs = 9'500'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_VIDEO],
		processDataFragment(_, _, /*fpts=*/9.5, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> buf = {0x01, 0x02, 0x03, 0x04};
	const bool result = m_player->SendTransfer(
		eMEDIATYPE_VIDEO, std::move(buf),
		/*fpts=*/9.5, /*fdts=*/9.5, /*fDuration=*/0.04,
		/*fragmentPTSoffset=*/0.0, /*initFragment=*/false);

	EXPECT_TRUE(result);
}

// ===========================================================================
// Discontinuity
// ===========================================================================
//
// Mirrors InterfacePlayerRDK::CheckDiscontinuity() (middleware-player-
// interface/InterfacePlayerRDK.cpp), reached via
// AAMPGstPlayer::Discontinuity():
//  - a discontinuity before this track's first injected sample is ignored
//    (mirrors the stream->firstBufferProcessed guard) - returns false, no
//    side effects.
//  - PTS restamping enabled + no elementary-stream change pending:
//    CompleteDiscontinuityDataDeliverForPTSRestamp() only, no EOS signal
//    and no buffering disruption.
//  - otherwise (the default / no-PTS-restamp / ES-change-pending case):
//    signal EOS on the track (mirrors GstPlayer_SignalEOS()), resume
//    buffering unconditionally (StopBuffering(true) semantics), and
//    disarm the underflow monitor when AampUnderflowMonitor is enabled.
//
// Additionally, and unlike the reference, a media type with no source at
// all is rejected outright, and the EOS branch propagates EOS to the
// subtitle source (keyed off video rather than the reference's audio,
// because audio may be absent) so Rialto can still report END_OF_STREAM.

/**
 * @test A discontinuity that arrives before this track has ever injected a
 *       sample must be ignored: no EOS signal, no buffering change, and the
 *       call reports the discontinuity as not handled.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_BeforeFirstSample_IgnoredReturnsFalse)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(AampRialtoMediaSource::kFirstPtsNotSet));

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyPipelinePausedToUnderflowMonitor())
		.Times(0);
	EXPECT_CALL(m_mockNotifiable,
		CompleteDiscontinuityDataDeliverForPTSRestamp(_))
		.Times(0);

	EXPECT_FALSE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);
}

/**
 * @test Default configuration (PTS restamp disabled) must signal EOS on the
 *       track, resume buffering (equivalent to StopBuffering(true): sources
 *       ungated, pipeline resumed), and report the discontinuity as handled.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_PtsRestampDisabled_SignalsEosAndResumesBuffering)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode =
		AampRialtoMediaSource::GateMode::BLOCKED;

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode,
		AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test When AampUnderflowMonitor is enabled, the halt-buffering branch
 *       must also notify the monitor so it disarms its deadline during
 *       the transition.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_UnderflowMonitorEnabled_NotifiesPipelinePaused)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig,
		IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillByDefault(Return(true));

	EXPECT_CALL(m_mockNotifiable, NotifyPipelinePausedToUnderflowMonitor())
		.Times(1);

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->state().eos);
}

/**
 * @test When AampUnderflowMonitor is disabled (default), the halt-buffering
 *       branch must not notify the monitor.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_UnderflowMonitorDisabled_DoesNotNotifyPipelinePaused)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));

	EXPECT_CALL(m_mockNotifiable, NotifyPipelinePausedToUnderflowMonitor())
		.Times(0);

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->state().eos);
}

/**
 * @test PTS restamping enabled with no elementary-stream change pending
 *       must complete the discontinuity via the restamp path only, without
 *       signalling EOS or resuming/halting buffering.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_PtsRestampActiveNoEsChange_CompletesWithoutTouchingBuffering)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));

	EXPECT_CALL(m_mockNotifiable,
		CompleteDiscontinuityDataDeliverForPTSRestamp(eMEDIATYPE_VIDEO))
		.Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);
}

/**
 * @test Even with PTS restamping enabled, a pending elementary-stream
 *       change must fall back to the halt-buffering branch (EOS signal +
 *       resumed buffering) instead of the restamp-only completion.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_PtsRestampActiveButEsChangePending_SignalsEosAndResumesBuffering)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(true));

	EXPECT_CALL(m_mockNotifiable,
		CompleteDiscontinuityDataDeliverForPTSRestamp(_))
		.Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);
}

/**
 * @test A discontinuity for a media type that has no source (here, before
 *       Configure() has created any) must be rejected outright: returning
 *       true would make AAMP wait for an EOS that can never be signalled.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_NoSourceForMediaType_ReturnsFalse)
{
	EXPECT_CALL(m_mockNotifiable, NotifyPipelinePausedToUnderflowMonitor())
		.Times(0);
	EXPECT_CALL(m_mockNotifiable,
		CompleteDiscontinuityDataDeliverForPTSRestamp(_))
		.Times(0);

	EXPECT_FALSE(m_player->Discontinuity(eMEDIATYPE_VIDEO));
}

/**
 * @test The first-sample guard is evaluated per track: a video
 *       discontinuity must still be ignored when only audio has injected
 *       a sample.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_FirstSampleGuardIsPerTrack_IgnoresVideoOnly)
{
	Configure();

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(AampRialtoMediaSource::kFirstPtsNotSet));
	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));

	// Only the audio call reaches the halt-buffering branch.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));

	EXPECT_FALSE(m_player->Discontinuity(eMEDIATYPE_VIDEO));
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));
	EXPECT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->state().eos);
}

/**
 * @test Rialto reports END_OF_STREAM only once every attached source has
 *       delivered haveData(EOS), so a video discontinuity that signals EOS
 *       must also signal the always-attached subtitle source.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_VideoEos_AlsoSignalsSubtitleEos)
{
	Configure();
	SendVideoInitFragment();
	// PLAYING triggers the inband CC subtitle source attachment.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_SUBTITLE]->state().eos);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->state().eos);
	EXPECT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->state().eos);
}

/**
 * @test Subtitle EOS is keyed off video only - an audio discontinuity must
 *       leave the subtitle source untouched, since video will still be
 *       delivering samples.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_AudioEos_LeavesSubtitleUntouched)
{
	Configure();
	SendVideoInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	EXPECT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->state().eos);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_SUBTITLE]->state().eos);
}

/**
 * @test The PTS-restamp branch must not signal EOS on the subtitle source
 *       either - no track is being taken to EOS in that path.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_PtsRestampActiveNoEsChange_LeavesSubtitleUntouched)
{
	Configure();
	SendVideoInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_FALSE(m_mockSources[eMEDIATYPE_SUBTITLE]->state().eos);
}

// ===========================================================================
// Deferred discontinuity flush — pending-position tracking
// ===========================================================================

/**
 * @test An accepted discontinuity no longer moves the player to a dedicated
 *       FSM state - it only arms the internal "position pending" tracking
 *       (exercised behaviourally by the Configure()/Stream()/SendSample()
 *       tests below).  Returning true from Discontinuity() commits AAMP to
 *       a flush cycle, but for HLS fMP4 that cycle issues no Flush() of its
 *       own, so the sink has to mark the window and supply the position
 *       itself.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_Accepted_LeavesStateUnchanged)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

/**
 * @test Every track signals its own discontinuity; the repeat notification
 *       must not disturb the state or re-arm the pending flush.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Discontinuity_SecondTrack_LeavesStateUnchanged)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));

	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));
	EXPECT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

/**
 * @test Configure() is reached from ProcessPendingDiscontinuity() only after
 *       StopInjection() has joined every injector thread, so gating there
 *       loses no pre-discontinuity content while stopping other tracks
 *       racing new-period samples in before the deferred Flush().
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_PositionPending_GatesAllSources)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	// Same formats: the pipeline is reused, so the pending position survives.
	Configure();

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode,
		AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode,
		AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test For HLS MP4, DoEarlyStreamSinkFlush() returns false even for an
 *       ordinary seek (TuneHelper(eTUNETYPE_SEEK) -> Configure(), pipeline
 *       reused, no Discontinuity() call and no follow-up Flush()). The
 *       real seek position is only known from the first post-seek sample's
 *       own PTS, so Configure() must arm the deferred-flush mechanism even
 *       when no prior Discontinuity() has set it.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_ReusedPipelineWithoutDiscontinuity_StillDefersPositionToFirstSample)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// Simulate an ordinary seek: same formats, pipeline reused, no
	// Discontinuity() call. Call the player's Configure() directly (not the
	// test's Configure() helper) - the helper would call ResetMockPipeline()
	// again (m_mockPipeline was already consumed by the first Configure())
	// and swap m_mockPipelinePtr to an unused mock, even though the pipeline
	// is actually reused here.
	m_player->Configure(MakeCodecInfo(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID),
		/*bESChangeStatus=*/false, /*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode,
		AampRialtoMediaSource::GateMode::BLOCKED);

	const int64_t expectedPosNs = 42'000'000'000LL;
	std::atomic<bool> playIssued{false};
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.WillOnce(Invoke([&playIssued](bool &) {
			playIssued = true;
			return true;
		}));

	std::atomic<bool> sendDone{false};
	std::thread sender([this, &sendDone]() {
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(42.0, 0.04));
		sendDone = true;
	});

	WaitFor([this]{
		return m_player->GetCurrentPlayerState() == PlayerStateId::FLUSHING;
	});
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// play() ungates the sources.  The sender's Stream() may run before or
	// after SEEK_DONE, so wait for it: a needData posted while still gated is
	// answered NO_AVAILABLE_SAMPLES and the sample is never injected.
	WaitFor([&playIssued]{ return playIssued.load(); });
	EXPECT_TRUE(playIssued.load());

	ON_CALL(*m_mockPipelinePtr, addSegment(0, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 0))
		.WillByDefault(Return(true));
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/0);

	WaitFor([&sendDone]{ return sendDone.load(); });
	if (!sendDone.load())
	{
		ADD_FAILURE() << "sample was never injected";
		m_player->Stop(false);  // releases the blocked injector
	}
	sender.join();
}

/**
 * @test Mirrors AampStreamSinkManager::SetActive() (single-pipeline-mode ad
 *       transition): an explicit Flush(positionIsAuthoritative=true)
 *       resolves the real, already-known position and settles in FLUSHED
 *       without ungating (Stream() for this session has not run yet, so
 *       SEEK_DONE's playRequested check is false). The subsequent
 *       reused-pipeline Configure() call must not re-arm the
 *       deferred-flush window: the position is already known, and
 *       re-arming would leave Stream() deferring play() forever waiting on
 *       a sample that can never be injected while sources stay gated.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_ReusedPipelineAfterSessionHandoffFlushSettledToFlushed_DoesNotReArmPositionPending)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	// Mirrors AampStreamSinkManager::SetActive(): ChangeAamp() switches the
	// pipeline to serve a different session, then an explicit
	// Flush(positionIsAuthoritative=true) resolves that session's real,
	// already-known resume position.
	auto *newAamp =
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get());
	m_player->ChangeAamp(newAamp, nullptr);
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/true,
		/*positionIsAuthoritative=*/true);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// No Stream() call precedes this SEEK_DONE, so no play request is
	// outstanding: as in production, sources are not ungated here and the player
	// settles in FLUSHED rather than resuming PLAYING.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);

	m_player->Configure(MakeCodecInfo(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID),
		/*bESChangeStatus=*/false, /*setReadyAfterPipelineCreation=*/false);

	// The position is already resolved by the Flush() above - Stream() must
	// issue play() immediately rather than deferring for a sample that can
	// never be injected.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
}

/**
 * @test Distinguishes the session-handoff case above from an ordinary
 *       same-session seek: TeardownStream() issues Flush(0, rate) (default
 *       positionIsAuthoritative=false) ahead of TuneHelper()'s
 *       reused-pipeline Configure(), and that placeholder position is NOT
 *       the real seek target - only the following sample's own PTS (via
 *       MaybeFlushForPendingPosition()) supplies it. Configure() must still
 *       arm and gate in this case even though the pre-flush state is
 *       FLUSHED.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_ReusedPipelineAfterSameSessionFlushSettledToFlushed_StillArmsPositionPending)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	// Mirrors TeardownStream()'s pre-seek discard flush: positionIsAuthoritative
	// defaults to false, so this position is treated as a placeholder.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/true);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);

	// TuneHelper()'s reused-pipeline Configure() for the seek.
	m_player->Configure(MakeCodecInfo(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID),
		/*bESChangeStatus=*/false, /*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode,
		AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode,
		AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test Configure() applies PlayHold::PositionPending unconditionally, but
 *       AttachSource() already establishes a definitive baseline for an
 *       ordinary fresh tune - the first ordinary sample must not trigger a
 *       second, redundant flush cycle.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_AfterFreshConfigureAndAttach_DoesNotIssueRedundantFlush)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).Times(0);
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(_, _))
		.WillByDefault(Return(true));

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/0);

	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(10.0, 0.04));

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
	EXPECT_TRUE(haveDataCalled.load());
}

/**
 * @test ProcessPendingDiscontinuity() calls Stream() before the new period's
 *       PTS is known.  Playing then would resume at the pre-discontinuity
 *       position, so play() must be deferred until the flush resolves.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stream_PositionPending_DefersPlay)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);

	m_player->Stream();

	// state machine still reports PLAYING (no dedicated FSM state); the
	// pending position is what actually deferred play() above.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

/**
 * @test The first video sample after the discontinuity carries the new
 *       period's actual PTS, which is the position the sink must flush to.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_PositionPending_VideoFlushesToSamplePts)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	const int64_t expectedPosNs = 8'760'000'000LL;
	std::atomic<bool> playIssued{false};
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.WillOnce(Invoke([&playIssued](bool &) {
			playIssued = true;
			return true;
		}));

	// The flushing sample gates itself, so it blocks until SEEK_DONE -
	// exactly as it would on the injector thread in production.
	std::atomic<bool> sendDone{false};
	std::thread sender([this, &sendDone]() {
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(8.76, 0.04));
		sendDone = true;
	});

	WaitFor([this]{
		return m_player->GetCurrentPlayerState() == PlayerStateId::FLUSHING;
	}, std::chrono::seconds(5));  // the sender thread can be starved under load
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// play() ungates the sources; a needData posted while still gated is
	// answered NO_AVAILABLE_SAMPLES and the sample is never injected.
	WaitFor([&playIssued]{ return playIssued.load(); },
		std::chrono::seconds(5));
	EXPECT_TRUE(playIssued.load());

	ON_CALL(*m_mockPipelinePtr, addSegment(7, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 7))
		.WillByDefault(Return(true));
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/7);

	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::seconds(5));
	if (!sendDone.load())
	{
		ADD_FAILURE() << "sample was never injected";
		m_player->Stop(false);  // releases the blocked injector
	}
	sender.join();
}

/**
 * @test Audio must not resolve the discontinuity while an attached video
 *       source exists - video carries the reference timeline, and audio's
 *       first PTS may differ from it.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_PositionPending_AudioDoesNotFlushWhenVideoAttached)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).Times(0);

	std::atomic<bool> sendDone{false};
	std::thread sender([this, &sendDone]() {
		m_player->SendSample(eMEDIATYPE_AUDIO, MakeSample(8.70, 0.02));
		sendDone = true;
	});

	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load())
		<< "audio must leave the pending position unresolved for video";

	// Release the blocked injector so the test can join.
	m_player->Stop(false);
	WaitFor([&sendDone]{ return sendDone.load(); });
	sender.join();
}

/**
 * @test When AAMP knows the position itself (DASH), its own Flush() arrives
 *       first and must take the player straight to FLUSHING, so no later
 *       sample can issue a second, competing seek.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PositionPending_TakesOverDiscontinuity)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	const int64_t aampPosNs = 12'000'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(aampPosNs))
		.WillOnce(Return(true));

	m_player->Flush(12.0, AAMP_NORMAL_PLAY_RATE, false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

/**
 * @test MaybeFlushForPendingPosition() must not start a second, overlapping
 *       flush while one is already resolving.  Discontinuity() applies
 *       PlayHold::PositionPending, but here an unrelated explicit seek
 *       (SeekStreamSink(), e.g. a trickplay rate change) begins resolving a
 *       position before the first post-discontinuity sample arrives - the
 *       injection thread must simply wait behind that in-flight flush
 *       (state==FLUSHING) rather than kicking off its own.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_PositionPending_DuringActiveFlush_DoesNotIssueOverlappingFlush)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	// An unrelated explicit seek (e.g. a trickplay rate change) begins
	// resolving a position while PlayHold::PositionPending is still applied from
	// Discontinuity() above; setPosition() is async, so the player stays
	// FLUSHING until SEEK_DONE arrives. This is the ONLY setPosition() call
	// expected - if MaybeFlushForPendingPosition() were to issue a second,
	// overlapping flush below, this expectation is violated.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_))
		.Times(1).WillOnce(Return(true));
	m_player->SeekStreamSink(10.0, 4.0);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// The still-pending discontinuity's first sample now arrives on the
	// injection thread while the flush above is in flight.
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(_, _))
		.WillByDefault(Return(true));

	std::atomic<bool> sendDone{false};
	std::thread sender([this, &sendDone]() {
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(8.76, 0.04));
		sendDone = true;
	});

	// The sample must simply wait behind the in-flight flush's gate, not
	// complete early via a redundant flush of its own.
	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load())
		<< "sample must wait behind the in-flight flush, not resolve early "
		   "via a redundant Flush() of its own";
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/1);

	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	sender.join();
}

/**
 * @test Stop() during the window ends the session; the pending position is
 *       moot and must not leave the player stuck (e.g. gated forever).
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_PositionPending_EndsSession)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	m_player->Stop(false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}
