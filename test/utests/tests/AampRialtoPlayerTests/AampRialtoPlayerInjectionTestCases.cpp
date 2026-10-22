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
 * @file AampRialtoPlayerInjectionTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Sample injection: SendCopy, SendSample, needData dispatch, back-pressure, injector pause/resume and StopBuffering.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase 5 — Segment injection baseline
// ===========================================================================

/**
 * @test SendTransfer_MediaFragment_EnqueuesSamples
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_MediaFragment_EnqueuesSamples)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();
	// Mirrors real AAMP usage: Stream() always follows Configure()/attach
	// and is what clears the gate set by Configure() (via
	// UngateAllSources()) so injection can proceed.
	m_player->Stream();

	PostNeedData(0, 1, 42);

	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.2; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));

	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, addSegment(42, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 42))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.2, 0.2, 0.033, 0, /*initFragment=*/false);

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
}

/**
 * @test EndOfStreamReached_SetsEosFlag
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	EndOfStreamReached_SetsEosFlag)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();
	// Mirrors real usage: EOS is reached during active playback, i.e. after
	// Stream() has cleared the gate set by Configure().
	m_player->Stream();

	PostNeedData(0, 0, 7);

	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::EOS, 7))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
}

/**
 * @test OnNeedMediaData_EnqueuesRequest
 */
TEST_F(AampRialtoPlayerTest,
	OnNeedMediaData_EnqueuesRequest)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _)).Times(::testing::AtLeast(0));
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	EXPECT_NO_THROW(client->notifyNeedMediaData(0, 5, 100, nullptr));
}

/**
 * @test OnCancelNeedMediaData_ClearsRequests
 */
TEST_F(AampRialtoPlayerTest,
	OnCancelNeedMediaData_ClearsRequests)
{
	Configure();
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyNeedMediaData(0, 5, 200, nullptr);
	EXPECT_NO_THROW(client->notifyCancelNeedMediaData(0));
}

/**
 * @test InjectSamples_EosOnly_CallsHaveDataWithEos
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_EosOnly_CallsHaveDataWithEos)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();
	// Mirrors real usage: EOS is reached during active playback, i.e. after
	// Stream() has cleared the gate set by Configure().
	m_player->Stream();

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);

	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::EOS, 55))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	PostNeedData(0, 1, 55);

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
}

/**
 * @test InjectSamples_NullPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	InjectSamples_NullPipeline_DoesNotCrash)
{
	ON_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillByDefault(Invoke(
			[this](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			       const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				m_capturedClient = client;
				return nullptr;
			}));
	Configure();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	EXPECT_NO_THROW({
		m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);
		client->notifyNeedMediaData(0, 1, 1, nullptr);
	});
	WaitFor([]{ return false; }, std::chrono::milliseconds(10));
}

/**
 * @test Stream_CallsPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stream_CallsPlay)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));

	m_player->Stream();
}

/**
 * @test PlaybackStateFailure_ReportsErrorOnce
 */
TEST_F(AampRialtoPlayerWithDemuxTest, PlaybackStateFailure_ReportsErrorOnce)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, true)).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::ERROR);

	// A repeat FAILURE for the same session must not be reported again.
	PostPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
}

/**
 * @test PlaybackStateFailure_NoActiveSession_NotReported
 */
TEST_F(AampRialtoPlayerWithDemuxTest, PlaybackStateFailure_NoActiveSession_NotReported)
{
	Configure();
	m_player->Stop(false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);

	EXPECT_CALL(m_mockNotifiable, NotifyPlaybackError(_, _, _)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

/**
 * @test PlaybackStateFailure_DuringFlush_LeavesFlushingAndReports
 */
TEST_F(AampRialtoPlayerWithDemuxTest, PlaybackStateFailure_DuringFlush_LeavesFlushingAndReports)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, _)).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::FAILURE);

	// Stop() blocks while FLUSHING, so reaching ERROR is what lets it proceed.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::ERROR);
	m_player->Stop(false);
}

/**
 * @test Regression (H7): a flush that ends in FAILURE never sees SEEK_DONE, so
 *       its Flushing hold must not outlive the session and defer play() in the
 *       next tune.
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_AfterFailureDuringFlush_NextSessionPlays)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(5.0, 1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
	PostPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
	m_player->Stop(false);

	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
}

/**
 * @test Regression (H7): fragment caching that was never completed before
 *       Stop() must not defer play() in the next tune.
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_DuringFragmentCaching_NextSessionPlays)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	m_player->NotifyFragmentCachingOngoing();
	m_player->Stop(false);

	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
}

/**
 * @test Stream_DeferredDuringFlush_SeekDoneIssuesPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stream_DeferredDuringFlush_SeekDoneIssuesPlay)
{
	/**
	 * @brief Regression: when Stream() is called while a flush is in
	 *        progress, the play request is recorded but play() is deferred.
	 *        The SEEK_DONE handler completing the flush cycle must issue
	 *        play() because a request is outstanding, even though the
	 *        restored pre-flush state is SOURCES_ATTACHED (not PLAYING).
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);

	// Flush: state → FLUSHING, pre-flush state = SOURCES_ATTACHED.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Precondition: Flush() must move state to FLUSHING";

	// Stream() while FLUSHING records the play request; the Flushing hold
	// defers play().  play() must be issued exactly once — by the SEEK_DONE
	// handler once the flush cycle completes.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();

	// Simulate Rialto confirming the pipeline-level flushing seek completed.
	// OnPlaybackState(SEEK_DONE) calls onFlushComplete(), then releases the
	// Flushing hold, which issues the outstanding play().
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
}

/**
 * @test Regression (M5): the SEEK_DONE handler must release the Flushing hold
 *       (and so issue the deferred play()) while the state machine is still
 *       FLUSHING. Once it leaves FLUSHING a Flush() blocked in
 *       WaitForFlushToComplete() can apply its own hold, and a release that
 *       came afterwards would drop it and play() mid-flush.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekDone_ReleasesFlushingHoldBeforeLeavingFlushing)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(5.0, 1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PlayerStateId stateAtPlay = PlayerStateId::IDLE;
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.WillOnce(Invoke([&](bool &) {
			stateAtPlay = m_player->GetCurrentPlayerState();
			return true;
		}));
	m_player->Stream();
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	EXPECT_EQ(stateAtPlay, PlayerStateId::FLUSHING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);
}

/**
 * @test Regression (M5): Flush() is documented as control-thread only but the
 *       deferred flush also runs it from injector threads. Two overlapping
 *       calls must not both be in flight (both claim FLUSHING and issue
 *       setPosition) at once.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_ConcurrentCallers_NeverOverlapInFlight)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	constexpr int kIterations = 40;
	std::atomic<int> inFlight{0};
	std::atomic<int> maxInFlight{0};
	std::atomic<int> calls{0};
	ON_CALL(*m_mockPipelinePtr, setPosition(_))
		.WillByDefault(Invoke([&](int64_t) {
			const int now = ++inFlight;
			int seen = maxInFlight.load();
			while (now > seen && !maxInFlight.compare_exchange_weak(seen, now))
			{
			}
			++calls;
			std::this_thread::sleep_for(std::chrono::microseconds(300));
			--inFlight;
			if (auto client = m_capturedClient.lock())
			{
				client->notifyPlaybackState(
					firebolt::rialto::PlaybackState::SEEK_DONE);
			}
			return true;
		}));

	std::atomic<int> arrived{0};
	std::atomic<int> round{0};
	auto worker = [&](double pos) {
		for (int i = 0; i < kIterations; ++i)
		{
			++arrived;
			while (round.load() <= i)
			{
				std::this_thread::yield();
			}
			m_player->Flush(pos, 1, /*shouldTearDown=*/false);
		}
	};
	std::thread a(worker, 5.0);
	std::thread b(worker, 6.0);
	for (int i = 0; i < kIterations; ++i)
	{
		while (arrived.load() < 2 * (i + 1))
		{
			std::this_thread::yield();
		}
		++round;
	}
	a.join();
	b.join();

	EXPECT_EQ(maxInFlight.load(), 1);
	EXPECT_EQ(calls.load(), 2 * kIterations);
}

/**
 * @test Stream_PlayRequestReset_SubsequentFlushWhilePausedNoPlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stream_PlayRequestReset_SubsequentFlushWhilePausedNoPlay)
{
	/**
	 * @brief After Stream() successfully issues play(), the play request is
	 *        consumed.  A subsequent seek-while-paused flush must not
	 *        spuriously re-issue play() once Rialto's post-flush PAUSED
	 *        notification arrives.
	 */
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Stream(): sources already attached, play() issued immediately and
	// the play request consumed.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Stream();
	::testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// Rialto confirms PLAYING, then user pauses.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);

	// Seek-while-paused: Flush() → FLUSHING, pre-flush state = PAUSED.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// play() must NOT be called: the play request was consumed when Stream()
	// issued play(). SEEK_DONE only lands in FLUSHED; Rialto's own PAUSED
	// notification (guaranteed to follow SEEK_DONE) drives FLUSHED -> PAUSED.
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(0);
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);  // triggers onFlushComplete() → FLUSHED
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);

	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);  // FLUSHED -> PAUSED

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED)
		<< "Seek-while-paused flush must settle in PAUSED, not resume play";
}

/**
 * @test Stop_CallsPipelineStop
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_CallsPipelineStop)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Stop(false);
}

/**
 * @test Stop_NullPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest, Stop_NullPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->Stop(false));
}

/**
 * @test Stop_DestroysPipeline
 */
TEST_F(AampRialtoPlayerTest, Stop_DestroysPipeline)
{
	/**
	 * @brief m_pipeline is private, so destruction is observed indirectly:
	 *        a tracked pipeline subclass flips a flag from its destructor,
	 *        installed in place of the fixture's default mock pipeline.
	 */
	class TrackedPipeline : public NiceMock<MockIMediaPipeline>
	{
	public:
		explicit TrackedPipeline(bool *destroyedFlag) : m_destroyedFlag(destroyedFlag) {}
		~TrackedPipeline() override { *m_destroyedFlag = true; }
	private:
		bool *m_destroyedFlag;
	};

	bool pipelineDestroyed = false;
	auto trackedPipeline = std::make_unique<TrackedPipeline>(&pipelineDestroyed);
	ON_CALL(*trackedPipeline, load(_, _, _, _)).WillByDefault(Return(true));
	ON_CALL(*trackedPipeline, stop()).WillByDefault(Return(true));

	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillOnce(Invoke(
			[&trackedPipeline](std::weak_ptr<firebolt::rialto::IMediaPipelineClient>,
			                    const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				return std::move(trackedPipeline);
			}));

	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false, /*setReadyAfterPipelineCreation=*/false);

	EXPECT_FALSE(pipelineDestroyed)
		<< "Pipeline must stay alive while the player is configured";

	m_player->Stop(false);

	EXPECT_TRUE(pipelineDestroyed)
		<< "Stop() must release the pipeline so its destructor runs immediately";
}

/**
 * @test Stop_Idempotent_DoesNotCrash
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_Idempotent_DoesNotCrash)
{
	Configure();

	// Stop() destroys the pipeline, so a second call has no pipeline left
	// to call stop() on again.
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Stop(false);
	EXPECT_NO_THROW(m_player->Stop(false));
}

/**
 * @test Stop_KeepLastFrame_StillCallsPipelineStop
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_KeepLastFrame_StillCallsPipelineStop)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Stop(/*keepLastFrame=*/true);
}

/**
 * @test Stop_WakesBlockedInjectionThread
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_WakesBlockedInjectionThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	std::atomic<bool> injectionReturned{false};
	std::thread injector([this, &injectionReturned]() {
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample s{};
				s.mPts      = 1.0;
				s.mDuration = 0.033;
				samples.push_back(std::move(s));
				return samples;
			});
		std::vector<uint8_t> buf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			1.0, 1.0, 0.033, 0, /*initFragment=*/false);
		injectionReturned.store(true);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(injectionReturned.load())
		<< "Injector should be blocked waiting for needData";

	m_player->Stop(false);

	injector.join();
	EXPECT_TRUE(injectionReturned.load())
		<< "Injector should have been unblocked by Stop()";
}

/**
 * @test Stop_NeedDataAfterStop_DoesNotCrash
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_NeedDataAfterStop_DoesNotCrash)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);

	m_player->Stop(false);

	// Stop() destroys the pipeline (m_mockPipelinePtr is now a dangling
	// pointer - no EXPECT_CALL can be set against it). The stale needData
	// arriving after Stop() has nothing left to respond to and must be
	// dropped safely rather than crashing (see AampRialtoMediaSource::
	// respondAbandonedRequest's null-pipeline branch).
	EXPECT_NO_THROW(client->notifyNeedMediaData(/*sourceId=*/0, /*frameCount=*/3,
		/*requestId=*/99, /*shmInfo=*/nullptr));

	// Give the (synchronous, but exercised via a callback) needData handler
	// a moment to run so a use-after-free on the destroyed pipeline would
	// actually surface here rather than after the test has already passed.
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

/**
 * @test Stop_EosThenNeedDataAfterStop_DoesNotCrash
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_EosThenNeedDataAfterStop_DoesNotCrash)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);
	m_player->EndOfStreamReached(eMEDIATYPE_AUDIO);

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);

	m_player->Stop(false);

	// Stop() destroys the pipeline (m_mockPipelinePtr is now a dangling
	// pointer - no EXPECT_CALL can be set against it), so it must not
	// resurrect the EOS state and re-fire haveData(EOS, ...); the stale
	// needData arriving after Stop() has nothing left to respond to and
	// must be dropped safely rather than crashing.
	EXPECT_NO_THROW(client->notifyNeedMediaData(/*sourceId=*/0, /*frameCount=*/3,
		/*requestId=*/100, /*shmInfo=*/nullptr));

	// Give the (synchronous, but exercised via a callback) needData handler
	// a moment to run so a use-after-free on the destroyed pipeline would
	// actually surface here rather than after the test has already passed.
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// ===========================================================================
// Phase 5b — NotifyInjectorToPause / wake blocked injection threads
// ===========================================================================

/**
 * @test NotifyInjectorToPause_WakesBlockedInjectionThread
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyInjectorToPause_WakesBlockedInjectionThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	std::atomic<bool> injectionReturned{false};
	std::thread injector([this, &injectionReturned]() {
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample s{};
				s.mPts      = 1.0;
				s.mDuration = 0.033;
				samples.push_back(std::move(s));
				return samples;
			});
		std::vector<uint8_t> buf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			1.0, 1.0, 0.033, 0, /*initFragment=*/false);
		injectionReturned.store(true);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(injectionReturned.load())
		<< "Injector should be blocked waiting for needData";

	m_player->NotifyInjectorToPause();

	injector.join();
	EXPECT_TRUE(injectionReturned.load())
		<< "Injector should have been unblocked by NotifyInjectorToPause()";
}

/**
 * @test NotifyInjectorToPause_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	NotifyInjectorToPause_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->NotifyInjectorToPause());
}

/**
 * @test NotifyInjectorToPause_DoesNotCallPipelineStop
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyInjectorToPause_DoesNotCallPipelineStop)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);

	m_player->NotifyInjectorToPause();
}

// ===========================================================================
// Phase 5c — NotifyInjectorToResume / clear gateMode
// ===========================================================================

/**
 * @test NotifyInjectorToResume_ClearsGateSoSubsequentSampleIsNotDropped
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyInjectorToResume_ClearsGateSoSubsequentSampleIsNotDropped)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	// Gate injection, mirroring DisableDownloads() -> NotifyInjectorToPause().
	// This leaves gateMode == DROPPED, so a sample submitted right now would
	// be discarded immediately (see the Phase 5d test below) rather than
	// waiting for a future ungate.
	m_player->NotifyInjectorToPause();

	// Mirrors EnableDownloads() -> NotifyInjectorToResume(): clears the
	// gate back to NONE so a sample submitted after this point is
	// processed normally (blocks waiting for needData) instead of being
	// dropped immediately.
	m_player->NotifyInjectorToResume();

	std::atomic<bool> injectionReturned{false};
	std::thread injector([this, &injectionReturned]() {
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample s{};
				s.mPts      = 1.0;
				s.mDuration = 0.033;
				samples.push_back(std::move(s));
				return samples;
			});
		std::vector<uint8_t> buf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			1.0, 1.0, 0.033, 0, /*initFragment=*/false);
		injectionReturned.store(true);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(injectionReturned.load())
		<< "Injector should be blocked waiting for needData (not dropped) "
		   "since NotifyInjectorToResume() cleared the gate";

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/200);

	WaitFor([&]{ return injectionReturned.load(); });
	const bool completed = injectionReturned.load();
	if (!completed)
	{
		// Safety net: force the stuck injector to bail via a generation
		// bump so the test process does not hang if the assertion below
		// fails (i.e. NotifyInjectorToResume() did not clear the gate).
		m_player->NotifyInjectorToPause();
	}
	injector.join();

	EXPECT_TRUE(completed)
		<< "Injector should complete once needData arrives after "
		   "NotifyInjectorToResume() cleared the gate";
}

/**
 * @test NotifyInjectorToResume_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	NotifyInjectorToResume_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->NotifyInjectorToResume());
}


// ===========================================================================
// Phase 5d — NotifyInjectorToPause must never block the injector thread
//
// Unlike Flush()/Configure() (which must block genuinely-wanted post-seek
// data until Stream() resumes), NotifyInjectorToPause() mirrors GStreamer's
// InterfacePlayerRDK::mPauseInjector, which drops new buffers synchronously
// and never blocks the pushing thread.  A sample submitted after
// NotifyInjectorToPause() must therefore be dropped immediately - it must
// not rely on a subsequent NotifyInjectorToResume() (or any other ungate
// event) to release the caller thread, otherwise a fragment-injector thread
// being torn down while paused (but never resumed) hangs forever.
// ===========================================================================

/**
 * @test NotifyInjectorToPause_NewSampleAfterPause_DoesNotBlockCallerThread
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyInjectorToPause_NewSampleAfterPause_DoesNotBlockCallerThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	m_player->NotifyInjectorToPause();

	std::atomic<bool> injectionReturned{false};
	std::thread injector([this, &injectionReturned]() {
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample s{};
				s.mPts      = 1.0;
				s.mDuration = 0.033;
				samples.push_back(std::move(s));
				return samples;
			});
		std::vector<uint8_t> buf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			1.0, 1.0, 0.033, 0, /*initFragment=*/false);
		injectionReturned.store(true);
	});

	WaitFor([&]{ return injectionReturned.load(); });
	const bool completed = injectionReturned.load();
	if (!completed)
	{
		// Safety net: force the stuck injector to bail via a generation
		// bump so the test process does not hang if the assertion below
		// fails (i.e. the sample was blocked rather than dropped).
		m_player->NotifyInjectorToPause();
	}
	injector.join();

	EXPECT_TRUE(completed)
		<< "A sample submitted after NotifyInjectorToPause() must be "
		   "dropped immediately and must never block the caller thread "
		   "waiting for a future NotifyInjectorToResume()/ungate event";
}

// ===========================================================================
// SendCopy — fragment injection via legacy copy path
// ===========================================================================

/**
 * @test SendCopy_AudioWhenUnattached_AttachesViaSetStreamCapsAndForwardsFragment
 */
TEST_F(AampRialtoPlayerTest,
	SendCopy_AudioWhenUnattached_AttachesViaSetStreamCapsAndForwardsFragment)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, _, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> buffer = {0x01, 0x02, 0x03, 0x04};
	bool result = m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(buffer),
		/*fpts=*/1.25, /*fdts=*/1.20, /*fDuration=*/0.05);

	EXPECT_TRUE(result);
	EXPECT_TRUE(m_player->PipelineConfiguredForMedia(eMEDIATYPE_AUDIO));
}

/**
 * @test SendCopy_AudioAlreadyAttached_DoesNotReattach
 */
TEST_F(AampRialtoPlayerTest,
	SendCopy_AudioAlreadyAttached_DoesNotReattach)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());

	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, _, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> buffer = {0x0A, 0x0B, 0x0C, 0x0D};
	bool result = m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(buffer),
		/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/0.1);

	EXPECT_TRUE(result);
}

/**
 * @test SendCopy_SteadyState_DoesNotLogAtInfo
 */
TEST_F(AampRialtoPlayerTest,
	SendCopy_SteadyState_DoesNotLogAtInfo)
{
	/**
	 * @brief HLS-TS calls SendCopy() once per elementary-stream frame, so a
	 *        successful call on an attached source must not log at INFO.
	 */
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);

	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, _, _, _, _))
		.WillRepeatedly(Return(true));

	// The first sample resolves the pending position with a flush, which is
	// logged; only the calls after it are steady state.
	std::vector<uint8_t> first = {0x01, 0x02, 0x03, 0x04};
	ASSERT_TRUE(m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(first),
		/*fpts=*/1.9, /*fdts=*/1.9, /*fDuration=*/0.1));

	std::vector<uint8_t> buffer = {0x0A, 0x0B, 0x0C, 0x0D};
	testing::internal::CaptureStdout();
	const bool result = m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(buffer),
		/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/0.1);
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_TRUE(result);
	EXPECT_EQ(log.find("[INFO]"), std::string::npos) << log;
}

// ---------------------------------------------------------------------------
// ID3 metadata: the sink reports ID3-headed audio, video and DSM_CC buffers
// ---------------------------------------------------------------------------

namespace
{
struct Id3Call
{
	AampMediaType mediaType{eMEDIATYPE_VIDEO};
	std::vector<uint8_t> data;
	double pts{0.0};
	double dts{0.0};
	double duration{0.0};
};

/// "ID3" v2.4 header with a zero-length body.
const std::vector<uint8_t> kId3Packet = {'I', 'D', '3', 4, 0, 0, 0, 0, 0, 0};
const std::vector<uint8_t> kNotId3Packet =
	{0x47, 0x40, 0x11, 0x10, 0x00, 0x02, 0xB0, 0x0D, 0x00, 0x01};
}

class AampRialtoPlayerId3Test : public AampRialtoPlayerTest
{
protected:
	void InstallHandler()
	{
		m_player->ChangeAamp(
			reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
			[this](AampMediaType type, const uint8_t *ptr, size_t len,
				const SegmentInfo_t &info, const char *)
			{
				m_calls.push_back(Id3Call{type,
					std::vector<uint8_t>(ptr, ptr + len),
					info.pts_s, info.dts_s, info.duration});
			});
	}

	std::vector<Id3Call> m_calls;
};

/**
 * @test SendCopy_Id3HeadedDsmCcPacket_ReachesHandlerWithTiming
 */
TEST_F(AampRialtoPlayerId3Test,
	SendCopy_Id3HeadedDsmCcPacket_ReachesHandlerWithTiming)
{
	InstallHandler();

	std::vector<uint8_t> packet = kId3Packet;
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(packet),
		/*fpts=*/1.5, /*fdts=*/1.4, /*fDuration=*/0.1);

	ASSERT_EQ(m_calls.size(), 1u);
	EXPECT_EQ(m_calls[0].mediaType, eMEDIATYPE_DSM_CC);
	EXPECT_EQ(m_calls[0].data, kId3Packet);
	EXPECT_DOUBLE_EQ(m_calls[0].pts, 1.5);
	EXPECT_DOUBLE_EQ(m_calls[0].dts, 1.4);
	EXPECT_DOUBLE_EQ(m_calls[0].duration, 0.1);
}

/**
 * @test SendCopy_PacketWithoutId3Header_IsNotReported
 */
TEST_F(AampRialtoPlayerId3Test, SendCopy_PacketWithoutId3Header_IsNotReported)
{
	InstallHandler();

	std::vector<uint8_t> packet = kNotId3Packet;
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(packet), 1.5, 1.4, 0.1);

	EXPECT_TRUE(m_calls.empty());
}

/**
 * @test SendCopy_ShortOrEmptyBuffer_IsNotReported
 */
TEST_F(AampRialtoPlayerId3Test, SendCopy_ShortOrEmptyBuffer_IsNotReported)
{
	InstallHandler();

	std::vector<uint8_t> shortBuffer = {'I', 'D'};
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(shortBuffer), 1.0, 1.0, 0.1);
	std::vector<uint8_t> empty;
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(empty), 1.0, 1.0, 0.1);

	EXPECT_TRUE(m_calls.empty());
}

/**
 * @test SendCopy_NoHandlerInstalled_DoesNotCrash
 */
TEST_F(AampRialtoPlayerId3Test, SendCopy_NoHandlerInstalled_DoesNotCrash)
{
	std::vector<uint8_t> packet = kId3Packet;
	EXPECT_NO_THROW(m_player->SendCopy(
		eMEDIATYPE_DSM_CC, std::move(packet), 1.5, 1.4, 0.1));
}

/**
 * @test SendCopy_Id3HeadedAudioBuffer_IsReportedAndStillInjected
 */
TEST_F(AampRialtoPlayerId3Test,
	SendCopy_Id3HeadedAudioBuffer_IsReportedAndStillInjected)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	InstallHandler();

	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, _, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> packet = kId3Packet;
	EXPECT_TRUE(m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(packet),
		/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/0.1));

	ASSERT_EQ(m_calls.size(), 1u);
	EXPECT_EQ(m_calls[0].mediaType, eMEDIATYPE_AUDIO);
}

/**
 * @test SendTransfer_Id3HeadedDsmCcPacket_ReachesHandler
 */
TEST_F(AampRialtoPlayerId3Test, SendTransfer_Id3HeadedDsmCcPacket_ReachesHandler)
{
	InstallHandler();

	std::vector<uint8_t> packet = kId3Packet;
	m_player->SendTransfer(eMEDIATYPE_DSM_CC, std::move(packet),
		/*fpts=*/3.0, /*fdts=*/2.9, /*fDuration=*/0.2,
		/*fragmentPTSoffset=*/0.0, /*initFragment=*/false,
		/*discontinuity=*/false);

	ASSERT_EQ(m_calls.size(), 1u);
	EXPECT_EQ(m_calls[0].mediaType, eMEDIATYPE_DSM_CC);
	EXPECT_DOUBLE_EQ(m_calls[0].pts, 3.0);
	EXPECT_DOUBLE_EQ(m_calls[0].dts, 2.9);
	EXPECT_DOUBLE_EQ(m_calls[0].duration, 0.2);
}

/**
 * @test SendSample_Id3HeadedDsmCcSample_ReachesHandler
 */
TEST_F(AampRialtoPlayerId3Test, SendSample_Id3HeadedDsmCcSample_ReachesHandler)
{
	InstallHandler();

	auto buffer = std::make_shared<std::vector<uint8_t>>(kId3Packet);
	AampMediaSample sample;
	sample.mData = std::shared_ptr<const uint8_t>(buffer, buffer->data());
	sample.mDataSize = buffer->size();
	sample.mPts = 4.0;
	sample.mDts = 3.9;
	sample.mDuration = 0.3;
	m_player->SendSample(eMEDIATYPE_DSM_CC, std::move(sample), false);

	ASSERT_EQ(m_calls.size(), 1u);
	EXPECT_EQ(m_calls[0].data, kId3Packet);
	EXPECT_DOUBLE_EQ(m_calls[0].pts, 4.0);
	EXPECT_DOUBLE_EQ(m_calls[0].dts, 3.9);
	EXPECT_DOUBLE_EQ(m_calls[0].duration, 0.3);
}

/**
 * @test ChangeAamp_ReplacesTheHandler
 */
TEST_F(AampRialtoPlayerId3Test, ChangeAamp_ReplacesTheHandler)
{
	InstallHandler();
	int replacementCalls = 0;
	m_player->ChangeAamp(
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
		[&](AampMediaType, const uint8_t *, size_t, const SegmentInfo_t &,
			const char *) { ++replacementCalls; });

	std::vector<uint8_t> packet = kId3Packet;
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(packet), 1.5, 1.4, 0.1);

	EXPECT_TRUE(m_calls.empty());
	EXPECT_EQ(replacementCalls, 1);
}

/**
 * @test ChangeAamp_ConcurrentWithSendCopy_IsRaceFree
 */
TEST_F(AampRialtoPlayerId3Test, ChangeAamp_ConcurrentWithSendCopy_IsRaceFree)
{
	/**
	 * @brief Injector threads read the handler while ChangeAamp() replaces it;
	 *        run under ThreadSanitizer to see the race on an unguarded member.
	 */
	std::atomic<int> calls{0};
	std::thread injector([&] {
		for (int i = 0; i < 2000; ++i)
		{
			std::vector<uint8_t> packet = kId3Packet;
			m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(packet), 1.0, 1.0, 0.1);
		}
	});
	for (int i = 0; i < 2000; ++i)
	{
		m_player->ChangeAamp(
			reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
			[&](AampMediaType, const uint8_t *, size_t, const SegmentInfo_t &,
				const char *) { ++calls; });
	}
	injector.join();

	// After the race the last handler installed must receive the next packet.
	const int before = calls.load();
	std::vector<uint8_t> packet = kId3Packet;
	m_player->SendCopy(eMEDIATYPE_DSM_CC, std::move(packet), 2.0, 2.0, 0.1);
	EXPECT_EQ(calls.load(), before + 1);
}

/**
 * @test SendCopy_WhenProcessDataFragmentFails_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
	SendCopy_WhenProcessDataFragmentFails_ReturnsFalse)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, _, _, _, _))
		.WillOnce(Return(false));

	std::vector<uint8_t> buffer = {0xA1, 0xA2, 0xA3, 0xA4};
	bool result = m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(buffer),
		/*fpts=*/3.0, /*fdts=*/3.0, /*fDuration=*/0.1);

	EXPECT_FALSE(result);
}

/**
 * @test SendCopy() must also drive the deferred pending-position flush
 *       (MaybeFlushForPendingPosition()), not just SendSample() - muxed
 *       HLS-TS content is injected exclusively via SendCopy(), so a
 *       discontinuity on that content would otherwise never resolve.
 */
TEST_F(AampRialtoPlayerTest,
	SendCopy_PositionPending_FlushesToFragmentPts)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());

	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	const int64_t expectedPosNs = 4'500'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, /*fpts=*/4.5, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> buffer = {0x01, 0x02, 0x03, 0x04};
	const bool result = m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(buffer),
		/*fpts=*/4.5, /*fdts=*/4.5, /*fDuration=*/0.1);

	EXPECT_TRUE(result);
}

/**
 * @test When ProcessPendingDiscontinuity() is guaranteed to issue its own
 *       explicit Flush() (WillFlushOnDiscontinuity() == true), Discontinuity()
 *       must not apply PlayHold::PositionPending - PTS-restamped
 *       samples keep flowing across Discontinuity(), so arming here would let
 *       the next sample drive an early implicit Flush() that races the
 *       guaranteed explicit one, discarding whatever it buffered in between.
 */
TEST_F(AampRialtoPlayerTest,
	Discontinuity_WillFlushOnDiscontinuity_DoesNotArmPositionPending)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());

	// First sample resolves Configure()'s own unconditional arm, so the
	// window is provably closed before Discontinuity() runs below.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(false));
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, /*fpts=*/1.0, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> firstBuffer = {0x01, 0x02, 0x03, 0x04};
	ASSERT_TRUE(m_player->SendCopy(eMEDIATYPE_AUDIO, std::move(firstBuffer),
		/*fpts=*/1.0, /*fdts=*/1.0, /*fDuration=*/0.1));

	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);
	testing::Mock::VerifyAndClearExpectations(m_mockSources[eMEDIATYPE_AUDIO]);

	ON_CALL(*m_mockSources[eMEDIATYPE_AUDIO], firstPtsMs())
		.WillByDefault(Return(0));
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockPrivateInstanceAAMP, ReconfigureForElementaryStreamUpdate())
		.WillByDefault(Return(false));
	ON_CALL(*g_mockPrivateInstanceAAMP, WillFlushOnDiscontinuity())
		.WillByDefault(Return(true));
	ASSERT_TRUE(m_player->Discontinuity(eMEDIATYPE_AUDIO));

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).Times(0);
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_AUDIO],
		processDataFragment(_, _, /*fpts=*/4.5, _, _, _))
		.WillOnce(Return(true));

	std::vector<uint8_t> secondBuffer = {0x05, 0x06, 0x07, 0x08};
	const bool result = m_player->SendCopy(eMEDIATYPE_AUDIO,
		std::move(secondBuffer), /*fpts=*/4.5, /*fdts=*/4.5,
		/*fDuration=*/0.1);

	EXPECT_TRUE(result);
}

// ===========================================================================
// SendSample — per-sample injection via external demuxer path
// ===========================================================================

/**
 * @test SendSample_Video_InjectsSample
 */
TEST_F(AampRialtoPlayerTest,
	SendSample_Video_InjectsSample)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
	// Mirrors real AAMP usage: Stream() always follows Configure()/attach
	// and is what clears the gate set by Configure() (via
	// UngateAllSources()) so injection can proceed.
	m_player->Stream();

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/1);

	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, addSegment(1, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 1))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(0.1, 0.033));

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
	EXPECT_TRUE(haveDataCalled.load());
}

/**
 * @test SendSample_NoSourceAttached_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
	SendSample_NoSourceAttached_ReturnsFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	bool result = m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample());
	EXPECT_FALSE(result);
}

/**
 * @test SendSample_AfterFlush_DropsRemainingSamples
 */
TEST_F(AampRialtoPlayerTest,
	SendSample_AfterFlush_DropsRemainingSamples)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	std::atomic<bool> sampleReturned{false};
	std::thread sender([this, &sampleReturned]() {
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(1.0, 0.033));
		sampleReturned.store(true);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(sampleReturned.load())
		<< "SendSample should be blocked waiting for needData";

	RepeatUntil([&sampleReturned]{ return sampleReturned.load(); },
		[this]{ m_player->Flush(5.0); });
	EXPECT_TRUE(sampleReturned.load());

	sender.join();
}

/**
 * @test SendSample_AfterStop_DropsRemainingSamples
 */
TEST_F(AampRialtoPlayerTest,
	SendSample_AfterStop_DropsRemainingSamples)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	std::atomic<bool> sampleReturned{false};
	std::thread sender([this, &sampleReturned]() {
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(1.0, 0.033));
		sampleReturned.store(true);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	m_player->Stop(false);

	WaitFor([&sampleReturned]{ return sampleReturned.load(); });
	EXPECT_TRUE(sampleReturned.load());

	sender.join();
}

// ===========================================================================
// OnNeedMediaData dispatches to the correct source
// ===========================================================================

/**
 * @test OnNeedMediaData_DoesNotBlockCallerThread
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnNeedMediaData_DoesNotBlockCallerThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto dispatch = std::async(std::launch::async, [this] {
		for (int i = 0; i < 30; ++i)
		{
			PostNeedData(0, 1, static_cast<uint32_t>(100 + i));
			PostNeedData(1, 1, static_cast<uint32_t>(200 + i));
		}
	});
	ASSERT_EQ(dispatch.wait_for(std::chrono::seconds(5)),
		std::future_status::ready)
		<< "needData dispatch blocked the Rialto event thread";
}

// ===========================================================================
// Back-pressure (synchronous pacing)
// ===========================================================================

/**
 * @test BackPressure_SendTransfer_BlocksUntilNeedData
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	BackPressure_SendTransfer_BlocksUntilNeedData)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();
	// Mirrors real AAMP usage: Stream() always follows Configure()/attach
	// and is what clears the gate set by Configure() (via
	// UngateAllSources()) so injection can proceed.
	m_player->Stream();

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});

	std::atomic<bool> sendDone{false};
	std::atomic<int>  addSegmentCalls{0};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](uint32_t, const auto &)
				-> firebolt::rialto::AddSegmentStatus
			{
				++addSegmentCalls;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	std::vector<uint8_t> buf = {0x01};
	std::thread sender([this, b = std::move(buf), &sendDone]() mutable {
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
		sendDone = true;
	});

	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 0);

	PostNeedData(0, 1, 1);
	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 1);

	sender.join();
}

/**
 * @test BackPressure_FlushWhileBlocked_UnblocksSendTransfer
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	BackPressure_FlushWhileBlocked_UnblocksSendTransfer)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});

	std::atomic<bool> sendDone{false};
	std::vector<uint8_t> buf = {0x01};
	std::thread sender([this, b = std::move(buf), &sendDone]() mutable {
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
		sendDone = true;
	});

	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());

	RepeatUntil([&sendDone]{ return sendDone.load(); },
		[this]{ EXPECT_NO_THROW(m_player->Flush(0.0, 1, false)); });
	EXPECT_TRUE(sendDone.load());
	sender.join();
}

// ===========================================================================
// StopBuffering
// ===========================================================================

/**
 * @test StopBuffering_ForceStop_CallsPipelinePlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	StopBuffering_ForceStop_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.Times(1).WillOnce(Return(true));
	m_player->StopBuffering(/*forceStop=*/true);
}

/**
 * @test StopBuffering_NotForced_CallsPipelinePlay
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	StopBuffering_NotForced_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.Times(1).WillOnce(Return(true));
	m_player->StopBuffering(/*forceStop=*/false);
}

/**
 * @test StopBuffering_NullPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	StopBuffering_NullPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->StopBuffering(true));
}
