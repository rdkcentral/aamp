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
 * @file AampRialtoPlayerPositionTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Position, duration and PTS reporting.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// IStreamSinkNotifiable — position and duration callbacks
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyPosition_DoesNotCallMonitorProgress)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, MonitorProgress(/*sync=*/false, /*bos=*/false))
		.Times(0);

	constexpr int64_t kTwoSecondsNs = 2'000'000'000LL;
	PostPosition(kTwoSecondsNs);
}

TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Playing_StartsProgressTimer_UsesConfiguredInterval)
{
	constexpr guint kExpectedIntervalMs = 250;
	EXPECT_CALL(m_mockNotifiable, GetProgressReportIntervalSeconds())
		.WillOnce(Return(0.25));
	EXPECT_CALL(*g_mockGLib,
		g_timeout_add(kExpectedIntervalMs, _, _))
		.WillOnce(DoAll(
			SaveArg<1>(&m_progressTimerCallback),
			SaveArg<2>(&m_progressTimerUserData),
			Return(101)));

	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

TEST_F(AampRialtoPlayerTest,
	Configure_DoesNotStartProgressTimer)
{
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)).Times(0);

	Configure();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	ProgressTimer_WhenPaused_StillReportsProgress)
{
	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_TRUE(m_player->Pause(
		/*pause=*/true,
		/*forceStopGstreamerPreBuffering=*/false));

	EXPECT_CALL(m_mockNotifiable,
		MonitorProgress(/*sync=*/false, /*bos=*/false))
		.Times(1);

	TriggerProgressTimerTick();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_RemovesProgressTimer)
{
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _))
		.WillOnce(DoAll(
			SaveArg<1>(&m_progressTimerCallback),
			SaveArg<2>(&m_progressTimerUserData),
			Return(77)));
	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_CALL(*g_mockGLib, g_source_remove(77))
		.WillOnce(Return(TRUE));
	m_player->Stop(/*keepLastFrame=*/false);
}

TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Playing_DoesNotRestartProgressTimerWhenAlreadyRunning)
{
	// First PLAYING: start() fires immediately then schedules g_timeout_add(id=88).
	// Second PLAYING: kick() calls g_source_remove(88) then a new g_timeout_add(id=89).
	// Teardown: player dtor calls stop() which calls g_source_remove(89).
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)).Times(2)
		.WillOnce(Return(88))
		.WillOnce(Return(89));
	EXPECT_CALL(*g_mockGLib, g_source_remove(88)).WillOnce(Return(TRUE));
	EXPECT_CALL(*g_mockGLib, g_source_remove(89)).WillOnce(Return(TRUE));

	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StartProgressTimer_WhenAlreadyRunning_KicksTimerForImmediateDispatch)
{
	// start() fires once immediately then schedules the periodic timer (id=101).
	// kick() removes id=101 and reschedules (id=102).
	// Teardown: player dtor calls stop() which calls g_source_remove(102).
	constexpr guint kIntervalMs = 500;
	EXPECT_CALL(m_mockNotifiable, GetProgressReportIntervalSeconds())
		.WillOnce(Return(0.5));
	EXPECT_CALL(*g_mockGLib, g_timeout_add(kIntervalMs, _, _)).Times(2)
		.WillOnce(Return(101))
		.WillOnce(Return(102));
	EXPECT_CALL(*g_mockGLib, g_source_remove(101)).WillOnce(Return(TRUE));
	EXPECT_CALL(*g_mockGLib, g_source_remove(102)).WillOnce(Return(TRUE));

	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// StartProgressTimer() on an already-running timer kicks it:
	// one immediate MonitorProgress() call.
	EXPECT_CALL(m_mockNotifiable, MonitorProgress(/*sync=*/false, /*bos=*/false))
		.Times(1);

	m_player->StartProgressTimer();
}

TEST_F(AampRialtoPlayerWithDemuxTest, Stop_WaitsForMonitorAvTickInProgress)
{
	/**
	 * @brief Regression (M2): Stop() resets the MonitorAV, whose timers run on
	 *        the GLib thread; it must wait for a tick in progress rather than
	 *        destroy the monitor under it.
	 */
	constexpr guint kSampleIntervalMs = 750;
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_MonitorAV))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ReportProgressInterval))
		.WillByDefault(Return(0.75));
	ON_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MonitorAVReportingInterval))
		.WillByDefault(Return(5000));

	GSourceFunc sampleDispatch = nullptr;
	gpointer sampleData = nullptr;
	ON_CALL(*g_mockGLib, g_timeout_add(kSampleIntervalMs, _, _))
		.WillByDefault(Invoke(
			[&](guint, GSourceFunc fn, gpointer data) -> guint
			{
				sampleDispatch = fn;
				sampleData = data;
				return 9001;
			}));

	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
	ASSERT_NE(sampleDispatch, nullptr) << "MonitorAV sample timer was not started";

	std::promise<void> tickEntered;
	std::promise<void> releaseTick;
	auto released = releaseTick.get_future();
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(Invoke([&](int64_t &position)
		{
			tickEntered.set_value();
			released.wait();
			position = 1'000'000'000LL;
			return true;
		}));

	std::thread tickThread([&] { sampleDispatch(sampleData); });
	tickEntered.get_future().wait();

	std::atomic<bool> stopReturned{false};
	std::thread stopThread([&] {
		m_player->Stop(false);
		stopReturned = true;
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(stopReturned.load())
		<< "Stop() returned while a MonitorAV tick was still running";

	releaseTick.set_value();
	tickThread.join();
	stopThread.join();
	EXPECT_TRUE(stopReturned.load());
}

TEST_F(AampRialtoPlayerWithDemuxTest, StopProgressTimer_WaitsForTickInProgress)
{
	/**
	 * @brief Regression (M1): Stop() must not return while a tick is still
	 *        running on the GLib thread, or the owner can be torn down under it.
	 */
	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_NE(m_progressTimerCallback, nullptr);

	std::promise<void> tickEntered;
	std::promise<void> releaseTick;
	auto released = releaseTick.get_future();
	EXPECT_CALL(m_mockNotifiable, MonitorProgress(false, false))
		.WillOnce(Invoke([&](bool, bool)
		{
			tickEntered.set_value();
			released.wait();
		}));

	std::thread tickThread([this] {
		m_progressTimerCallback(m_progressTimerUserData);
	});
	tickEntered.get_future().wait();

	std::atomic<bool> stopReturned{false};
	std::thread stopThread([&] {
		m_player->StopProgressTimer();
		stopReturned = true;
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(stopReturned.load())
		<< "StopProgressTimer() returned while a tick was still running";

	releaseTick.set_value();
	tickThread.join();
	stopThread.join();
	EXPECT_TRUE(stopReturned.load());
}

TEST_F(AampRialtoPlayerWithDemuxTest, ProgressTimer_DispatchAfterStop_DoesNothing)
{
	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_NE(m_progressTimerCallback, nullptr);
	m_player->StopProgressTimer();

	EXPECT_CALL(m_mockNotifiable, MonitorProgress(_, _)).Times(0);

	EXPECT_EQ(m_progressTimerCallback(m_progressTimerUserData), G_SOURCE_REMOVE);
}

TEST_F(AampRialtoPlayerWithDemuxTest, ProgressTimer_DispatchAfterPlayerDestroyed_IsSafe)
{
	/**
	 * @brief Regression (M1): a dispatch GLib had already picked up when the
	 *        player was destroyed must find the timer inactive rather than
	 *        use freed memory.
	 */
	Configure();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_NE(m_progressTimerCallback, nullptr);
	GSourceFunc dispatch = m_progressTimerCallback;
	gpointer data = m_progressTimerUserData;

	m_player.reset();

	EXPECT_CALL(m_mockNotifiable, MonitorProgress(_, _)).Times(0);
	EXPECT_EQ(dispatch(data), G_SOURCE_REMOVE);
}

// Configure() unconditionally applies PlayHold::PositionPending (it cannot tell
// whether the caller will follow up with an AttachSource() or an explicit
// Flush() to resolve the real position - see Configure()'s comment).  With
// no AttachSource()/Flush() having run yet to resolve it,
// GetPositionMilliseconds() must report 0 rather than a raw, unresolved
// pipeline position.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_ReturnsElapsedTimeSinceSegmentStart)
{
	Configure();

	// positionPending short-circuits before ever querying the pipeline.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).Times(0);

	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);
}

// When Flush() pre-stages a non-zero position before the video source
// attaches (e.g. the DASH known-PTS-discontinuity path, where
// priv_aamp.cpp calls sink->Flush() immediately after Configure() and
// before any source is attached), AttachSource() commits that position as
// the segment-start baseline.  GetPositionMilliseconds() must then return
// elapsed time from that baseline, not the raw pipeline position.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_WithNonZeroSegmentStart_ReturnsElapsedTime)
{
	Configure();

	// Flush() before any source attaches: not a flushable state, so the
	// position is only staged (no SEEK_DONE) - AttachSource() will consume
	// it below, committing 13440 ms as the segment-start baseline.
	constexpr int64_t  kCurrentPosNs   = 14'921'000'000LL;  // 14921 ms
	constexpr long long kExpectedMs    = 1'481LL;            // elapsed

	m_player->Flush(/*position=*/13.44, /*rate=*/1, /*shouldTearDown=*/false);
	SendVideoInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kCurrentPosNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpectedMs);
}

// Unlike PlayHold::PositionPending (released per-flush cycle), the
// segment-start baseline is a plain committed value: Configure() does not
// clear it, and Stop() resets it to 0 rather than to the prior session's
// value.  Configure() unconditionally re-applies
// PlayHold::PositionPending, so a re-tune that does not
// issue its own Flush() before the first AttachSource() reports 0 (not the
// stale carried-over baseline) until that AttachSource()/Flush() resolves
// the position afresh.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_AfterReconfigureWithoutNewFlush_ReportsZeroUntilResolved)
{
	// First session: Flush() stages 5000 ms before the video source attaches.
	Configure();
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	SendVideoInitFragment();
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(7'000'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 2'000LL);

	// Reconfigure — simulates a re-tune (Stop then Configure) with no
	// intervening Flush() call.
	m_player->Stop(false);
	Configure();

	// Configure() unconditionally re-applies PlayHold::PositionPending, so until this
	// session's own AttachSource()/Flush() resolves it, position reports 0
	// rather than the stale prior-session baseline.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).Times(0);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);

	// Second session: a fresh Flush() before attach commits a new baseline.
	m_player->Flush(/*position=*/2.0, /*rate=*/1, /*shouldTearDown=*/false);
	SendVideoInitFragment();
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(3'500'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 1'500LL);
}

TEST_F(AampRialtoPlayerTest,
	GetPositionMilliseconds_BeforeConfigure_ReturnsZero)
{
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);
}

// When the pipeline query fails after a segment start has been recorded,
// GetPositionMilliseconds() must return 0.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_WhenPipelineQueryFails_ReturnsZero)
{
	Configure();
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(13'440LL));  // segment start = 13440 ms

	ON_CALL(*m_mockPipelinePtr, getPosition(_)).WillByDefault(Return(false));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);
}

// After a trickplay rewind flush (rate=-2), GetPositionMilliseconds() must
// return elapsed * rate (negative) so priv_aamp's
//   reported_pos = seek_pos + GetPositionMilliseconds()
// decrements correctly — mirroring GStreamer's
//   rc = (pos - segmentStart) * rate.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_TrickplayRewind_ReturnsNegativeElapsed)
{
	/**
	 * @brief With PTS restamping enabled, Rialto plays restamped frames
	 *        (0, 1, 2, … seconds) in forward order. GetPositionMilliseconds()
	 *        must multiply the elapsed restamped time by rate so that the
	 *        returned delta is negative for reverse trickplay.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure();

	// Attach both sources first (segment-start baseline commits to 0, and
	// AttachSource() releases PlayHold::PositionPending) so the player reaches
	// SOURCES_ATTACHED - a flushable state - before Flush() below runs.
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Trickplay rewind: seek to 12 s at rate=-2.  The mock pipeline has no
	// default action for setPosition(), so it returns false; Flush() then
	// commits the rate immediately without touching the segment-start
	// baseline (still 0 from the attach above).
	m_player->Flush(12.0, -2, /*shouldTearDown=*/false);

	// With PTS restamping the first video sample arrives with PTS = 0 ms.
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0LL));

	// After 616 ms of pipeline time (restamped domain) the content position
	// elapsed at rate=-2 should be 616 × (-2) = -1232 ms.
	constexpr int64_t  kElapsedNs = 616'000'000LL;  // 616 ms
	constexpr long long kExpected = -1232LL;          // 616 × (-2)

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kElapsedNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpected);
}

// After returning from trickplay (rate resets to 1), GetPositionMilliseconds()
// must revert to non-negative, forward-incrementing behaviour.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_AfterTrickplayExit_RateReturnsToOne)
{
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure();

	// Attach both sources first (segment-start baseline commits to 0, and
	// AttachSource() releases PlayHold::PositionPending) so the player reaches
	// SOURCES_ATTACHED - a flushable state - before the Flush() calls below.
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Enter trickplay rewind.  The mock pipeline has no default action for
	// setPosition(), so it returns false and Flush() commits the rate
	// immediately (state -> FLUSHED, itself flushable) without touching the
	// segment-start baseline.
	m_player->Flush(12.0, -2, /*shouldTearDown=*/false);

	// Resume normal play: flush back to 1× forward.
	m_player->Flush(8.0, 1, /*shouldTearDown=*/false);

	// After the second flush, first video sample arrives with PTS = 0 ms.
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0LL));

	constexpr int64_t  kElapsedNs = 500'000'000LL;  // 500 ms
	constexpr long long kExpected = 500LL;            // 500 × 1

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kElapsedNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpected);
}

// During a mid-segment seek, Flush() stages the seek position so that
// GetPositionMilliseconds() can return it
// immediately while the pipeline is in FLUSHING state (before SEEK_DONE
// commits it as the new segment-start baseline).  This prevents position
// reporting from snapping to zero during the flush window.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_WhileFlushing_ReturnsSeekPositionWithoutQueryingPipeline)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Seek to 30 s mid-segment.  setPosition returning true keeps the
	// player in FLUSHING so the SEEK_DONE path is not yet taken.
	constexpr double   kSeekSec   = 30.0;
	constexpr int64_t  kSeekPosMs = 30'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(kSeekSec, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// getPosition() must not be called while in FLUSHING state.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).Times(0);

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kSeekPosMs);

	// Exit FLUSHING so TearDown() can clean up without blocking.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
}

// After a mid-fragment seek the segment-start baseline is the seek position
// (not the PTS of the first byte of the fragment): OnPlaybackState(SEEK_DONE)
// commits the staged position as the segment-start baseline.  When the
// pipeline reports that same seek position as its current position on the
// first query after SEEK_DONE, the elapsed calculation (queried - segmentStart)
// == 0 so the reported position is 0 — correctly representing "just seeked,
// no elapsed playback yet" rather than surfacing a non-zero fragment-offset
// blip.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_AfterMidFragmentSeek_FirstQueryReturnsZero)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	constexpr double   kSeekSec   = 30.0;
	constexpr int64_t  kSeekPosNs = 30'000'000'000LL;

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(kSeekSec, /*rate=*/1, /*shouldTearDown=*/false);

	// Rialto confirms the seek; player exits FLUSHING and commits the
	// segment-start baseline to the seek position.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// Pipeline reports exactly the seek position immediately after SEEK_DONE.
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kSeekPosNs), Return(true)));

	// (30000 - 30000) * 1 = 0: no elapsed time since the seek target.
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetDurationMilliseconds_ReturnsLatestNotifiedDuration)
{
	Configure();

	constexpr int64_t kOneHourNs  = 3'600'000'000'000LL;
	constexpr long     kExpectedMs = 3'600'000L;

	PostDuration(kOneHourNs);

	EXPECT_EQ(m_player->GetDurationMilliseconds(), kExpectedMs);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	PositionAndDurationGetters_DoNotLogAtInfo)
{
	/**
	 * @brief AAMP polls the position and duration continuously, so the
	 *        getters must not log at INFO.
	 */
	Configure();
	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));

	testing::internal::CaptureStdout();
	m_player->GetPositionMilliseconds();
	m_player->GetDurationMilliseconds();
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(log.find("[INFO]"), std::string::npos) << log;
}

// ===========================================================================
// GetVideoPTS
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPTS_ConvertsPipelinePositionTo90kHzTicks)
{
	Configure();

	constexpr int64_t   kPositionNs = 5'000'000'000LL;  // 5000 ms
	constexpr long long kExpectedPts = 450'000LL;         // 5000 ms * 90

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kPositionNs), Return(true)));

	EXPECT_EQ(m_player->GetVideoPTS(), kExpectedPts);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPTS_WhenPipelineQueryFails_ReturnsZero)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).WillOnce(Return(false));

	EXPECT_EQ(m_player->GetVideoPTS(), 0LL);
}

TEST_F(AampRialtoPlayerTest,
	GetVideoPTS_NoPipeline_ReturnsZero)
{
	EXPECT_EQ(m_player->GetVideoPTS(), 0LL);
}

// ===========================================================================
// CheckForPTSChangeWithTimeout
// ===========================================================================
//
// Mirrors InterfacePlayerRDK::CheckForPTSChangeWithTimeout(), reached via
// AAMPGstPlayer::CheckForPTSChangeWithTimeout().  A false return makes
// PrivateInstanceAAMP::CheckForDiscontinuityStall() schedule a retune, so
// false must be reported only when the PTS is genuinely available and has
// not advanced for longer than the timeout.

/**
 * @test With no queryable position the PTS is unknown, so the check must
 *       optimistically report "still moving" rather than force a retune.
 */
TEST_F(AampRialtoPlayerTest,
	CheckForPTSChangeWithTimeout_NoPipeline_ReturnsTrue)
{
	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(0));
}

/**
 * @test A failed position query yields PTS 0, which must also be treated as
 *       "unknown" rather than "stalled".
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckForPTSChangeWithTimeout_PositionQueryFails_ReturnsTrue)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, getPosition(_)).WillByDefault(Return(false));

	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(0));
}

/**
 * @test An advancing PTS must report no stall, and must refresh the
 *       last-updated timestamp so a subsequent zero timeout does not
 *       immediately trip.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckForPTSChangeWithTimeout_PtsAdvancing_ReturnsTrue)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(int64_t{1'000'000'000}), Return(true)))
		.WillOnce(DoAll(SetArgReferee<0>(int64_t{2'000'000'000}), Return(true)));

	// No sleep needed here: the PTS itself changes between calls, which
	// resets the baseline regardless of elapsed wall-clock time.
	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(1));
	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(1));
}

/**
 * @test A static PTS within the timeout window must not be reported as a
 *       stall.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckForPTSChangeWithTimeout_PtsStaticWithinTimeout_ReturnsTrue)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(int64_t{1'000'000'000}),
			Return(true)));

	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(60'000));
	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(60'000));
}

/**
 * @test Only a PTS that has been static for longer than the timeout may be
 *       reported as a stall.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckForPTSChangeWithTimeout_PtsStaticBeyondTimeout_ReturnsFalse)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(int64_t{1'000'000'000}),
			Return(true)));

	// First call establishes the baseline timestamp for this PTS.
	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(1));
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

	EXPECT_FALSE(m_player->CheckForPTSChangeWithTimeout(1));
}

/**
 * @test Stop() must clear the PTS baseline so a new session starting at the
 *       same PTS is not immediately judged stalled using the previous
 *       session's timestamp.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	CheckForPTSChangeWithTimeout_AfterStop_BaselineIsReset)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(int64_t{1'000'000'000}),
			Return(true)));

	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(1));
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	ASSERT_FALSE(m_player->CheckForPTSChangeWithTimeout(1));

	m_player->Stop(false);
	Configure();
	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(int64_t{1'000'000'000}),
			Return(true)));

	EXPECT_TRUE(m_player->CheckForPTSChangeWithTimeout(1));
}
