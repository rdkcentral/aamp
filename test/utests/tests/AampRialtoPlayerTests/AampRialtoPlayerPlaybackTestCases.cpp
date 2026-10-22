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
 * @file AampRialtoPlayerPlaybackTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Playback state: pause and rate, Rialto playback-state notifications, gating, underflow, playback errors and pre-roll.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase 8 — Pause / SetPlayBackRate
// ===========================================================================

/**
 * @test Pause_True_CallsPipelinePause
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_True_CallsPipelinePause)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, pause()).Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->Pause(/*pause=*/true, false));
}

/**
 * @test Pause_False_CallsPipelinePlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_False_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->Pause(/*pause=*/false, false));
}

/**
 * @test SetPlayBackRate_CallsPipelineSetPlaybackRate
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetPlayBackRate_CallsPipelineSetPlaybackRate)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, setPlaybackRate(2.0))
		.Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->SetPlayBackRate(2.0));
}

/**
 * @test Pause_NullPipeline_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
	Pause_NullPipeline_ReturnsFalse)
{
	EXPECT_FALSE(m_player->Pause(true, false));
}

/**
 * @test SetPlayBackRate_NullPipeline_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
	SetPlayBackRate_NullPipeline_ReturnsFalse)
{
	EXPECT_FALSE(m_player->SetPlayBackRate(1.5));
}

/**
 * @test ChangeAamp_NullNewAamp_KeepsExistingAssociation
 */
TEST_F(AampRialtoPlayerTest,
	ChangeAamp_NullNewAamp_KeepsExistingAssociation)
{
	auto *originalAamp =
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get());

	EXPECT_TRUE(m_player->IsAssociatedAamp(originalAamp));

	m_player->ChangeAamp(nullptr, nullptr);

	EXPECT_TRUE(m_player->IsAssociatedAamp(originalAamp));
}

// ===========================================================================
// IStreamSinkNotifiable — OnPlaybackState notifications
// ===========================================================================

/**
 * @test OnPlaybackState_Playing_FirstTime_CallsAllFirstFrameNotifications
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_CallsAllFirstFrameNotifications)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

	EXPECT_CALL(m_mockNotifiable, LogFirstFrame()).Times(1);
	EXPECT_CALL(m_mockNotifiable, LogTuneComplete()).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	// The direct-rialto CC path passes a non-zero IDirectRialtoCC* handle.
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(Ne(0UL)))
		.Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstVideoFrameDisplayed()).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

/**
 * @test OnPlaybackState_Playing_FirstTime_DoesNotCallSpeedChanged
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_DoesNotCallSpeedChanged)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

	EXPECT_CALL(m_mockNotifiable, NotifySpeedChanged(_, _)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

/**
 * @test OnPlaybackState_Playing_PostSeek_CallsNotifyFirstBufferAndFrame
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_PostSeek_CallsNotifyFirstBufferAndFrame)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_SEEKING));

	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	// CC source already created on first PLAYING; handle is still non-zero.
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(Ne(0UL))).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstVideoFrameDisplayed()).Times(1);
	EXPECT_CALL(m_mockNotifiable, LogFirstFrame()).Times(0);
	EXPECT_CALL(m_mockNotifiable, LogTuneComplete()).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

/**
 * @test OnPlaybackState_Playing_ResumeFromPause_CallsNotifySpeedChanged
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_ResumeFromPause_CallsNotifySpeedChanged)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_PAUSED));

	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	EXPECT_CALL(m_mockNotifiable,
		NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, /*changeState=*/true))
		.Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

/**
 * @test OnPlaybackState_EndOfStream_CallsNotifyEOSReached
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_EndOfStream_CallsNotifyEOSReached)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyEOSReached()).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
}

/**
 * @test OnPlaybackState_EndOfStream_DoesNotCallFirstFrameNotifications
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_EndOfStream_DoesNotCallFirstFrameNotifications)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
}

/**
 * @test OnPlaybackState_Paused_DoesNotCallNotifiable
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Paused_DoesNotCallNotifiable)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyEOSReached()).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifySpeedChanged(_, _)).Times(0);
	EXPECT_CALL(m_mockNotifiable, MonitorProgress(_, _)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
}

/**
 * @test OnPlaybackState_Paused_DoesNotChangeGateMode
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Paused_DoesNotChangeGateMode)
{
	// Rialto sends PAUSED for buffering-pause (e.g. AampUnderflowMonitor).
	// Injection must be allowed to continue so the buffer refills —
	// aborting it here would discard samples and delay recovery.
	// Only unblockInjection()/gateInjection() (flush/stop/pause-notify
	// paths) should change gateMode.
	Configure();

	ASSERT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	// Configure() gates new sources to BLOCKED until Stream() confirms
	// play() (not exercised here) — see the gateInjection(true) call sites
	// in Configure().
	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test NotifyFragmentCachingOngoing_ThenComplete_IssuesPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyFragmentCachingOngoing_ThenComplete_IssuesPlay)
{
	// SetStateBufferingIfRequired() calls NotifyFragmentCachingOngoing() when
	// mFragmentCachingRequired is true. NotifyFragmentCachingComplete() must
	// then issue play() to resume the pipeline — mirroring GSTPlayer's
	// pendingPlayState mechanism to close the seek-while-paused race.
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Pipeline is not yet PAUSED (SOURCES_ATTACHED), so NotifyFragmentCachingOngoing
	// must pause it before waiting for the cache to fill.
	EXPECT_CALL(*m_mockPipelinePtr, pause()).WillOnce(Return(true));
	m_player->NotifyFragmentCachingOngoing();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->NotifyFragmentCachingComplete();
}

/**
 * @test NotifyFragmentCachingComplete_AfterPauseTrue_DoesNotIssuePlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyFragmentCachingComplete_AfterPauseTrue_DoesNotIssuePlay)
{
	// Pause(true) cancels the pending play: an explicit pause (e.g. user
	// paused during buffering) must take precedence over the deferred resume.
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// NotifyFragmentCachingOngoing pauses (pipeline not yet PAUSED), then
	// Pause(true) pauses again after clearing the pending-play flag.
	EXPECT_CALL(*m_mockPipelinePtr, pause()).Times(2).WillRepeatedly(Return(true));
	m_player->NotifyFragmentCachingOngoing();

	m_player->Pause(/*pause=*/true, /*forceStop=*/false);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	m_player->NotifyFragmentCachingComplete();
}

/**
 * @test OnPlaybackState_Playing_DoesNotChangeGateMode
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Playing_DoesNotChangeGateMode)
{
	// A PLAYING notification is not a genuine ungate point (see the
	// consolidated ungate-timing design comment below) - it can be a late
	// echo of a play() issued elsewhere (e.g. StopBuffering() inside
	// Discontinuity()) while a position is still pending, and must not
	// prematurely clear a gate applied by Configure()/Flush().
	Configure();

	// Force sources into a gated state first.
	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

// ===========================================================================
// gateMode — consolidated ungate-timing design (UngateAllSources())
// ===========================================================================
//
// The gate is set to BLOCKED by gateInjection(true) (Configure()/Flush()'s
// flushable path) or to DROPPED by unblockInjection() (Flush()/Stop()/
// NotifyInjectorToPause()/dtor), and must be cleared to NONE ONLY at the
// points that genuinely issue play(): Stream(), CheckAllSourcesAttached()
// (deferred play), Pause(false), StopBuffering(), and
// OnPlaybackState(SEEK_DONE)'s shouldPlay branch. It must NOT be cleared by
// reset(), AttachSource(), handleNeedData(), or OnPlaybackState(PLAYING)
// (covered above) — see AampRialtoVideoSourceTests for the former group.


/**
 * @test Flush_SetsGateModeBlockedOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_SetsGateModeBlockedOnAllSources)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	// Mirrors real usage: Flush() (e.g. for a seek) normally happens once
	// already playing, i.e. after Stream() has cleared the gate set by
	// Configure().
	m_player->Stream();

	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test Stop_SetsGateModeDroppedOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_SetsGateModeDroppedOnAllSources)
{
	// Stop() ends the session with no follow-up gateInjection(true) — it
	// must leave sources DROPPED (never blocking a future caller thread)
	// rather than BLOCKED, until the next Configure()/Flush() re-gates
	// them explicitly.
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	// Mirrors real usage: Stop() normally happens once already playing,
	// i.e. after Stream() has cleared the gate set by Configure().
	m_player->Stream();

	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	m_player->Stop(/*keepLastFrame=*/false);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::DROPPED);
}

/**
 * @test AttachSource_DoesNotClearGateMode
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	AttachSource_DoesNotClearGateMode)
{
	/**
	 * @brief AttachSource() runs mid-Configure(), potentially before a
	 *        subsequent Flush() completes a multi-step trickplay sequence.
	 *        It must NOT clear gateMode - only UngateAllSources() at
	 *        a genuine play()-issuance point may do so.
	 */
	Configure();
	ASSERT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	// Only video attaches; audio remains unattached so
	// CheckAllSourcesAttached() (called from AttachSource()) cannot fire
	// and confound the result via its own UngateAllSources() call.
	SendVideoInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test Stream_ClearsGateModeOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stream_ClearsGateModeOnAllSources)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	m_player->Stream();

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test Stream_AlreadyPlaying_ClearsGateModeAndCallsPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stream_AlreadyPlaying_ClearsGateModeAndCallsPlay)
{
	/**
	 * @brief Regression: UngateAllSources() must be called unconditionally
	 *        in Stream() - not only when promoting to PLAYING - so a
	 *        second Stream() call (e.g. immediately following a Flush()
	 *        that already restored PLAYING) still clears a gate left set
	 *        by that flush.  play() is now issued unconditionally on every
	 *        Stream() call (even when the pipeline already reports PLAYING)
	 *        so a genuine play()-issuance point does not depend on the
	 *        state machine's view of Rialto's state being perfectly in
	 *        sync.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test CheckAllSourcesAttached_DeferredPlay_ClearsGateModeOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckAllSourcesAttached_DeferredPlay_ClearsGateModeOnAllSources)
{
	/**
	 * @brief When Stream() is called before all sources are attached,
	 *        play() is deferred to CheckAllSourcesAttached(). That deferred
	 *        path must also clear the gate immediately before issuing
	 *        play(), not just the immediate Stream() path.
	 */
	Configure();
	SendVideoInitFragment();

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	// Sources not all attached yet - Stream() defers play().
	m_player->Stream();
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	SendAudioInitFragment();  // completes attachment - fires deferred play()

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test Pause_False_ClearsGateModeOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_False_ClearsGateModeOnAllSources)
{
	Configure();

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	EXPECT_TRUE(m_player->Pause(/*pause=*/false, false));

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test Pause_True_DoesNotClearGateMode
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_True_DoesNotClearGateMode)
{
	// Pausing must not clear the gate - only resuming (Pause(false)) does.
	Configure();

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	EXPECT_TRUE(m_player->Pause(/*pause=*/true, false));

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
}

/**
 * @test StopBuffering_ClearsGateModeOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	StopBuffering_ClearsGateModeOnAllSources)
{
	Configure();

	m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;
	m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode = AampRialtoMediaSource::GateMode::BLOCKED;

	m_player->StopBuffering(/*forceStop=*/true);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test OnPlaybackState_SeekDone_ShouldPlay_ClearsGateModeOnAllSources
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_SeekDone_ShouldPlay_ClearsGateModeOnAllSources)
{
	/**
	 * @brief Regression coverage for the trickplay ungate-timing race:
	 *        Flush() sets the gate to BLOCKED (real gateInjection(true)
	 *        call, not a manual override); Stream() while FLUSHING defers
	 *        play() without touching the gate; only once SEEK_DONE
	 *        completes the flush cycle and decides to play does the gate
	 *        finally clear.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	// Mirrors real usage: Flush() (e.g. for a seek) normally happens once
	// already playing, i.e. after Stream() has cleared the gate set by
	// Configure().
	m_player->Stream();
	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED)
		<< "Flush() must set the gate";
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED)
		<< "Stream() while FLUSHING must defer without clearing the gate";

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test SendSample_BlockedByFlushGate_InjectsOnceGateClearsAfterStream
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_BlockedByFlushGate_InjectsOnceGateClearsAfterStream)
{
	/**
	 * @brief Regression: reproduces AAMP's real Flush()->Stream()->
	 *        SendSample() sequence where Stream() is called while a flush
	 *        is still completing (play() deferred, gateMode still
	 *        BLOCKED).  SendSample() must BLOCK the sample until the gate
	 *        clears, not drop it - it is genuinely-wanted playback data,
	 *        not stale data.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	// Stream() while FLUSHING defers play() without clearing the gate.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	ASSERT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	// SendSample() called now (matching AAMP's real Flush()->Stream()->
	// SendSample() sequence) must block rather than drop the sample.
	std::atomic<bool> sendDone{false};
	std::atomic<bool> sendResult{false};
	std::thread sender([this, &sendDone, &sendResult]() {
		sendResult = m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(5.0, 0.033));
		sendDone = true;
	});

	WaitFor([&sendDone]{ return sendDone.load(); }, std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load())
		<< "SendSample must block while the flush gate is still set, "
		   "not drop the sample";

	// SEEK_DONE completes the flush; a play request is outstanding so the
	// gate must clear (and play() is issued, satisfying the EXPECT_CALL above).
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	// The unblocked injector now waits for needData; post it and confirm
	// the originally-submitted sample is actually delivered (not dropped).
	std::atomic<bool> haveDataCalled{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(7, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 7))
		.WillByDefault(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/7);

	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	EXPECT_TRUE(sendResult.load());
	EXPECT_TRUE(haveDataCalled.load());

	sender.join();
}

/**
 * @test OnPlaybackState_SeekDone_PreFlushStatePlaying_ClearsGateModeAndCallsPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_SeekDone_PreFlushStatePlaying_ClearsGateModeAndCallsPlay)
{
	/**
	 * @brief Regression: seek-while-playing.  onFlushComplete() only takes
	 *        the state machine to FLUSHED - it does not restore PLAYING.
	 *        The gate must still be cleared based on the play request alone -
	 *        it must not depend on Rialto's subsequent PLAYING notification
	 *        (guaranteed to follow SEEK_DONE) to eventually clear it via the
	 *        PLAYING case handler.  play() is issued whenever a request is
	 *        outstanding, regardless of the state machine still being in
	 *        FLUSHED at that point.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Establish PLAYING as the pre-flush state.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// Seek-while-playing: Flush() → FLUSHING, pre-flush state = PLAYING,
	// setting the gate.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	// Stream() while FLUSHING records the play request and defers play().
	m_player->Stream();
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED)
		<< "Stream() while FLUSHING must defer without clearing the gate";

	// SEEK_DONE alone lands in FLUSHED (no restore to PLAYING) and must
	// clear the gate on both sources, and must issue play() since a request
	// is outstanding - regardless of the state machine still being in
	// FLUSHED.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE)
		<< "Gate must clear based on the play request alone, not on a "
		   "subsequent Rialto PLAYING notification";
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);

	// Rialto's own PLAYING notification (guaranteed to follow SEEK_DONE)
	// drives the state machine the rest of the way.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

/**
 * @test OnPlaybackState_SeekDone_PositionStillPendingAfterConcurrentDiscontinuity_DefersPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_SeekDone_PositionStillPendingAfterConcurrentDiscontinuity_DefersPlay)
{
	/**
	 * @brief Regression: a Discontinuity() that arrives for a newer period
	 *        while an earlier flush cycle is still FLUSHING re-applies
	 *        PlayHold::PositionPending (see MaybeFlushForPendingPosition()'s own
	 *        FLUSHING-guard comment: its SEEK_DONE resolves the earlier,
	 *        now-stale position only). The SEEK_DONE that completes THIS
	 *        stale flush cycle must not ungate/play - a position is still
	 *        owed to the newer period, and playing now would run past it.
	 *        The play request must be left outstanding so the next
	 *        flush cycle's own SEEK_DONE - once it resolves the newer
	 *        position - performs the deferred ungate/play instead.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);

	// First (stale) flush cycle.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Stream() while FLUSHING records the play request and defers play().
	m_player->Stream();
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED);

	// A concurrent Discontinuity() for a newer period re-applies
	// PlayHold::PositionPending while the above flush cycle is in flight.
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ON_CALL(*g_mockPrivateInstanceAAMP, WillFlushOnDiscontinuity())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_VIDEO));

	// SEEK_DONE completing the STALE flush cycle must not play - a position
	// is still owed to the newer, re-armed period.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::BLOCKED)
		<< "Gate must remain BLOCKED - a newer position is still pending";

	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// The subsequent flush cycle that finally resolves the re-armed
	// position must still perform the deferred ungate/play once ITS
	// SEEK_DONE arrives.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/6.0, /*rate=*/1, /*shouldTearDown=*/false);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode, AampRialtoMediaSource::GateMode::NONE);
}

// ===========================================================================
// notifyBufferUnderflow — OnBufferUnderflow dispatch
// ===========================================================================

/**
 * @test OnBufferUnderflow_KnownVideoSourceId_CallsNotifyBufferUnderflowWithVideoType
 */
TEST_F(AampRialtoPlayerTest,
	OnBufferUnderflow_KnownVideoSourceId_CallsNotifyBufferUnderflowWithVideoType)
{
	/**
	 * @brief Verifies that a Rialto buffer-underflow notification for the
	 *        video source is forwarded to the notifiable as
	 *        NotifyBufferUnderflow(eMEDIATYPE_VIDEO).
	 *
	 *        SetStreamCaps is used (not SendVideoInitFragment) because it
	 *        attaches the source synchronously, ensuring sourceId=0 is
	 *        registered before PostBufferUnderflow fires.
	 */
	Configure();
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
	// Video source was assigned sourceId=0 (first m_nextSourceId++).

	EXPECT_CALL(m_mockNotifiable,
		NotifyBufferUnderflow(eMEDIATYPE_VIDEO))
		.Times(1);

	PostBufferUnderflow(/*sourceId=*/0);
}

/**
 * @test OnBufferUnderflow_KnownAudioSourceId_CallsNotifyBufferUnderflowWithAudioType
 */
TEST_F(AampRialtoPlayerTest,
	OnBufferUnderflow_KnownAudioSourceId_CallsNotifyBufferUnderflowWithAudioType)
{
	/**
	 * @brief Verifies that a Rialto buffer-underflow notification for the
	 *        audio source is forwarded to the notifiable as
	 *        NotifyBufferUnderflow(eMEDIATYPE_AUDIO).
	 *
	 *        SetStreamCaps is used (synchronous attach) so sourceIds are
	 *        deterministic: video=0, audio=1.
	 */
	Configure();
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());
	// Source IDs: video=0, inband CC subtitle=1 (deferred, drained with
	// video), audio=2 (third m_nextSourceId++).

	EXPECT_CALL(m_mockNotifiable,
		NotifyBufferUnderflow(eMEDIATYPE_AUDIO))
		.Times(1);

	PostBufferUnderflow(/*sourceId=*/2);
}

/**
 * @test OnBufferUnderflow_UnknownSourceId_DoesNotCallNotifiable
 */
TEST_F(AampRialtoPlayerTest,
	OnBufferUnderflow_UnknownSourceId_DoesNotCallNotifiable)
{
	/**
	 * @brief Verifies that an underflow notification for an unrecognised
	 *        sourceId is silently ignored — no crash, no spurious call.
	 */
	Configure();

	EXPECT_CALL(m_mockNotifiable,
		NotifyBufferUnderflow(_))
		.Times(0);

	PostBufferUnderflow(/*sourceId=*/99);
}

// ===========================================================================
// notifyPlaybackError — OnPlaybackError dispatch / mapping
// ===========================================================================

/**
 * @test OnPlaybackError_Decryption_MapsToDrmDecryptFailedWithRetry
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackError_Decryption_MapsToDrmDecryptFailedWithRetry)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_DRM_DECRYPT_FAILED, _,
			/*isRetryEnabled=*/true))
		.Times(1);

	PostPlaybackError(/*sourceId=*/0,
		firebolt::rialto::PlaybackError::DECRYPTION);
}

/**
 * @test OnPlaybackError_OutputProtection_SchedulesRetune
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackError_OutputProtection_SchedulesRetune)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyPlaybackError(_, _, _))
		.Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyOutputProtectionRecovered())
		.Times(1);

	PostPlaybackError(/*sourceId=*/0,
		firebolt::rialto::PlaybackError::OUTPUT_PROTECTION);
}

/**
 * @test OnPlaybackError_Unknown_MapsToGstPipelineErrorWithRetry
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackError_Unknown_MapsToGstPipelineErrorWithRetry)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _,
			/*isRetryEnabled=*/true))
		.Times(1);

	PostPlaybackError(/*sourceId=*/0,
		firebolt::rialto::PlaybackError::UNKNOWN);
}

/**
 * @test OnPlaybackError_UnknownSourceId_StillReportsError
 */
TEST_F(AampRialtoPlayerTest,
	OnPlaybackError_UnknownSourceId_StillReportsError)
{
	// Unlike underflow, a playback error for an unrecognised sourceId is
	// still forwarded (with a generic description) — the error itself is
	// real even if the specific source can no longer be identified.
	Configure();

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, _))
		.Times(1);

	PostPlaybackError(/*sourceId=*/99,
		firebolt::rialto::PlaybackError::UNKNOWN);
}

// ===========================================================================
// Pre-roll before play() (gstBufferAndPlay)
// ===========================================================================

/**
 * @class AampRialtoPlayerPreRollTest
 * @brief Enables eAAMPConfig_GStreamerBufferingBeforePlay and captures the
 *        10 ms pre-roll poll timer so tests can drive it.
 *
 * Oracle: InterfacePlayerRDK holds the pipeline PAUSED with a target of
 * PLAYING while buffering_enabled, and buffering_timeout promotes it once
 * enough frames are buffered.  Here that means an explicit pause() and a
 * floor on frames Rialto has accepted, not on the server's decoder queue;
 * Rialto reporting NO_SPACE also ends the pre-roll.
 * Only normal-rate playback pre-rolls;
 * Pause(true, forceStopGstreamerPreBuffering=true) abandons it.
 */
class AampRialtoPlayerPreRollTest : public AampRialtoPlayerWithDemuxTest
{
protected:
	static constexpr guint kPreRollPollMs = 10u;

	void SetUp() override
	{
		AampRialtoPlayerWithDemuxTest::SetUp();
		ON_CALL(*g_mockAampConfig,
			IsConfigSet(eAAMPConfig_GStreamerBufferingBeforePlay))
			.WillByDefault(Return(true));
		ON_CALL(*g_mockGLib, g_timeout_add(kPreRollPollMs, _, _))
			.WillByDefault(Invoke(
				[this](guint, GSourceFunc fn, gpointer data)
				{
					m_preRollFn = fn;
					m_preRollData = data;
					++m_preRollStarts;
					return m_nextTimerId++;
				}));
	}

	/// Tune to the point where Stream() has been called and all sources
	/// are attached.
	void TuneAndStream()
	{
		Configure();
		SendVideoInitFragment();
		SendAudioInitFragment();
		ASSERT_EQ(m_player->GetCurrentPlayerState(),
			PlayerStateId::SOURCES_ATTACHED);
		m_player->Stream();
	}

	void SetAcceptedFrames(uint32_t frames)
	{
		ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], acceptedFrames())
			.WillByDefault(Return(frames));
	}

	gboolean PollPreRoll()
	{
		return m_preRollFn(m_preRollData);
	}

	GSourceFunc m_preRollFn{nullptr};
	gpointer m_preRollData{nullptr};
	int m_preRollStarts{0};
};

/**
 * @test Tune_DefersPlayAndUngatesSources
 */
TEST_F(AampRialtoPlayerPreRollTest, Tune_DefersPlayAndUngatesSources)
{
	/**
	 * @brief With pre-roll enabled, Stream() lets data flow into the paused
	 *        pipeline but must not issue play() yet.
	 */
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);

	TuneAndStream();

	EXPECT_EQ(m_preRollStarts, 1);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO]->state().gateMode,
		AampRialtoMediaSource::GateMode::NONE);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO]->state().gateMode,
		AampRialtoMediaSource::GateMode::NONE);
}

/**
 * @test BelowFloor_HoldsPlay
 */
TEST_F(AampRialtoPlayerPreRollTest, BelowFloor_HoldsPlay)
{
	TuneAndStream();
	SetAcceptedFrames(3);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	EXPECT_EQ(PollPreRoll(), G_SOURCE_CONTINUE);
}

/**
 * @test FloorReached_IssuesPlay
 */
TEST_F(AampRialtoPlayerPreRollTest, FloorReached_IssuesPlay)
{
	TuneAndStream();
	SetAcceptedFrames(4);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	EXPECT_EQ(PollPreRoll(), G_SOURCE_REMOVE);
}

/**
 * @test BufferFullBelowFloor_IssuesPlay
 */
TEST_F(AampRialtoPlayerPreRollTest, BufferFullBelowFloor_IssuesPlay)
{
	TuneAndStream();
	SetAcceptedFrames(2);
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], bufferFull())
		.WillByDefault(Return(true));

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	EXPECT_EQ(PollPreRoll(), G_SOURCE_REMOVE);
}

/**
 * @test Tune_PausesPipelineForPreRoll
 */
TEST_F(AampRialtoPlayerPreRollTest, Tune_PausesPipelineForPreRoll)
{
	/**
	 * @brief The Rialto API does not promise data requests before play(), so
	 *        pre-roll asks for PAUSED explicitly, as the reference does.
	 */
	EXPECT_CALL(*m_mockPipelinePtr, pause()).Times(1).WillOnce(Return(true));

	TuneAndStream();
}

/**
 * @test PausedDuringPreRoll_ThenPlay_ReachesPlaying
 */
TEST_F(AampRialtoPlayerPreRollTest, PausedDuringPreRoll_ThenPlay_ReachesPlaying)
{
	/**
	 * @brief Rialto confirms pre-roll's pause() before play() is issued; the
	 *        player must track that rather than ignore it.
	 */
	TuneAndStream();
	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);

	SetAcceptedFrames(4);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PollPreRoll();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

/**
 * @test PauseRejected_StillPreRolls
 */
TEST_F(AampRialtoPlayerPreRollTest, PauseRejected_StillPreRolls)
{
	ON_CALL(*m_mockPipelinePtr, pause()).WillByDefault(Return(false));
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);

	TuneAndStream();

	EXPECT_EQ(m_preRollStarts, 1);
}

/**
 * @test DecoderQueueFull_NothingAccepted_HoldsPlay
 */
TEST_F(AampRialtoPlayerPreRollTest, DecoderQueueFull_NothingAccepted_HoldsPlay)
{
	/**
	 * @brief The gate must not depend on the server's decoder: what it
	 *        reports through getQueuedFrames() is implementation-specific.
	 */
	TuneAndStream();
	ON_CALL(*m_mockPipelinePtr, getQueuedFrames(_, _))
		.WillByDefault(DoAll(SetArgReferee<1>(10u), Return(true)));
	SetAcceptedFrames(0);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	EXPECT_EQ(PollPreRoll(), G_SOURCE_CONTINUE);
}

/**
 * @test Disabled_PlaysImmediately
 */
TEST_F(AampRialtoPlayerPreRollTest, Disabled_PlaysImmediately)
{
	ON_CALL(*g_mockAampConfig,
		IsConfigSet(eAAMPConfig_GStreamerBufferingBeforePlay))
		.WillByDefault(Return(false));

	EXPECT_CALL(*m_mockPipelinePtr, pause()).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	TuneAndStream();

	EXPECT_EQ(m_preRollStarts, 0);
}

/**
 * @test SeekAtNormalRate_PreRollsAgain
 */
TEST_F(AampRialtoPlayerPreRollTest, SeekAtNormalRate_PreRollsAgain)
{
	TuneAndStream();
	SetAcceptedFrames(4);
	PollPreRoll();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/false);
	m_player->Stream();
	SetAcceptedFrames(0);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	EXPECT_EQ(m_preRollStarts, 2);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	SetAcceptedFrames(4);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PollPreRoll();
}

/**
 * @test TrickplayFlush_SkipsPreRoll
 */
TEST_F(AampRialtoPlayerPreRollTest, TrickplayFlush_SkipsPreRoll)
{
	/**
	 * @brief The reference only pre-rolls at normal play rate.
	 */
	TuneAndStream();
	SetAcceptedFrames(4);
	PollPreRoll();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/10.0, /*rate=*/4, /*shouldTearDown=*/false);
	m_player->Stream();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	EXPECT_EQ(m_preRollStarts, 1);
}

/**
 * @test PauseWithForceStop_AbandonsPreRoll
 */
TEST_F(AampRialtoPlayerPreRollTest, PauseWithForceStop_AbandonsPreRoll)
{
	/**
	 * @brief Mirrors InterfacePlayerRDK::Pause(): forceStopGstreamerPreBuffering
	 *        ends pre-buffering so it cannot later promote a paused pipeline.
	 */
	TuneAndStream();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	m_player->Pause(/*pause=*/true, /*forceStopGstreamerPreBuffering=*/true);
	SetAcceptedFrames(4);
	EXPECT_EQ(PollPreRoll(), G_SOURCE_REMOVE);
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	EXPECT_EQ(m_preRollStarts, 1);
}

/**
 * @test PauseWithoutForceStop_KeepsPreRoll
 */
TEST_F(AampRialtoPlayerPreRollTest, PauseWithoutForceStop_KeepsPreRoll)
{
	TuneAndStream();
	m_player->Pause(/*pause=*/true, /*forceStopGstreamerPreBuffering=*/false);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	m_player->Stream();
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	SetAcceptedFrames(4);
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	PollPreRoll();
}

/**
 * @test Stop_StalePollDoesNotPlay
 */
TEST_F(AampRialtoPlayerPreRollTest, Stop_StalePollDoesNotPlay)
{
	TuneAndStream();
	SetAcceptedFrames(4);

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	m_player->Stop(false);

	EXPECT_EQ(PollPreRoll(), G_SOURCE_REMOVE);
}

/**
 * @brief Mock pipeline that reports its own destruction.
 *
 * The Rialto server frees a media-pipeline session only when the client's
 * pipeline object is destroyed, so a lingering shared_ptr keeps a session
 * (and its decoders) alive after Stop() and exhausts the server's limit.
 */
class DestructionTrackingPipeline : public NiceMock<MockIMediaPipeline>
{
public:
	explicit DestructionTrackingPipeline(std::shared_ptr<std::atomic<bool>> destroyed)
		: m_destroyed(std::move(destroyed)) {}
	~DestructionTrackingPipeline() override { m_destroyed->store(true); }

private:
	/// Shared so a leaked pipeline destroyed after the test body returns
	/// writes to live memory rather than a dead stack frame.
	std::shared_ptr<std::atomic<bool>> m_destroyed;
};

/**
 * @test Stop_AfterPreRollCompleted_ReleasesPipeline
 */
TEST_F(AampRialtoPlayerPreRollTest, Stop_AfterPreRollCompleted_ReleasesPipeline)
{
	/**
	 * @brief Regression: the pre-roll probe kept a shared_ptr to the pipeline
	 *        after pre-roll finished, so Stop()'s reset() did not destroy it
	 *        and its Rialto session leaked.  Observed on a real STB as a
	 *        Stop() that returned in ~2 ms (instead of blocking until the
	 *        server tore the session down) followed by
	 *        "Max session number reached" on the next tune.
	 */
	auto pipelineDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<DestructionTrackingPipeline>(pipelineDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();

	TuneAndStream();
	SetAcceptedFrames(4);
	PollPreRoll();
	ASSERT_FALSE(pipelineDestroyed->load()) << "pipeline must still be live while playing";

	m_player->Stop(false);

	EXPECT_TRUE(pipelineDestroyed->load())
		<< "Stop() must drop the last reference to the Rialto pipeline";
}

/**
 * @test Stop_DuringPreRoll_ReleasesPipeline
 */
TEST_F(AampRialtoPlayerPreRollTest, Stop_DuringPreRoll_ReleasesPipeline)
{
	auto pipelineDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<DestructionTrackingPipeline>(pipelineDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();

	TuneAndStream();
	SetAcceptedFrames(0);
	ASSERT_EQ(PollPreRoll(), G_SOURCE_CONTINUE) << "pre-roll must still be running";

	m_player->Stop(false);

	EXPECT_TRUE(pipelineDestroyed->load());
}

/**
 * @test StopThenReconfigure_SecondPipelineCreatedAfterFirstDestroyed
 */
TEST_F(AampRialtoPlayerPreRollTest, StopThenReconfigure_SecondPipelineCreatedAfterFirstDestroyed)
{
	/**
	 * @brief The server allows a fixed number of concurrent sessions, so the
	 *        old pipeline must already be gone when Configure() asks for the
	 *        next one.
	 */
	auto firstDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<DestructionTrackingPipeline>(firstDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();

	TuneAndStream();
	SetAcceptedFrames(4);
	PollPreRoll();
	m_player->Stop(false);

	bool firstGoneAtCreate = false;
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillOnce(Invoke(
			[&](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			    const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				firstGoneAtCreate = firstDestroyed->load();
				m_capturedClient = client;
				ResetMockPipeline();
				return std::move(m_mockPipeline);
			}));
	Configure();

	EXPECT_TRUE(firstGoneAtCreate);
}

/**
 * @test Stop_DuringPipelineCall_KeepsPipelineAliveUntilCallReturns
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_DuringPipelineCall_KeepsPipelineAliveUntilCallReturns)
{
	/**
	 * @brief Regression (H1): a pipeline call made on one thread must not
	 *        have the pipeline destroyed under it when Stop() runs on
	 *        another.  Stop() is invoked from inside getPosition() to model
	 *        that overlap deterministically.
	 */
	auto pipelineDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<DestructionTrackingPipeline>(pipelineDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();

	Configure();
	m_player->Flush(/*position=*/1.0, /*rate=*/1, /*shouldTearDown=*/false);
	SendVideoInitFragment();

	bool destroyedDuringCall = true;
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(Invoke([&](int64_t &position)
		{
			m_player->Stop(false);
			destroyedDuringCall = pipelineDestroyed->load();
			position = 2'000'000'000LL;
			return true;
		}));

	m_player->GetPositionMilliseconds();

	EXPECT_FALSE(destroyedDuringCall)
		<< "pipeline destroyed while a call on it was still in progress";
	EXPECT_TRUE(pipelineDestroyed->load())
		<< "pipeline must be released once the in-flight call has returned";
}
