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
 * @file AampRialtoPlayerConfigureTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Configure(), pipeline creation and recreation, the control backend and waiting for a flush to finish.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase 2 — Configure / pipeline creation
// ===========================================================================

TEST_F(AampRialtoPlayerTest, Configure_ValidFormats_CreatesPipeline)
{
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, load(_, _, _, _))
		.WillOnce(Return(true));

	Configure();
}

TEST_F(AampRialtoPlayerTest, Configure_NullSourceCreator_DoesNotCrash)
{
	g_mockPipelineFactory = nullptr;
	m_player.reset();

	SourceCreator nullCreator = [](AampMediaType) { return nullptr; };

	m_player = std::make_unique<AampRialtoPlayer>(
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
		&m_mockNotifiable,
		std::unique_ptr<IRialtoControlBackend>(nullptr),
		/*id3HandlerCallback=*/nullptr,
		std::move(nullCreator));
	EXPECT_NO_THROW(
		m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID, false, false));
	g_mockPipelineFactory = m_mockFactory;
}

TEST_F(AampRialtoPlayerTest, Configure_CallsSourceCreatorForEachFormat)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	EXPECT_EQ(m_createSourceCallCount, 3);  // video + audio + inband CC subtitle
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);  // inband CC
}

TEST_F(AampRialtoPlayerTest, Configure_VideoOnly_CreatesVideoSourceOnly)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	EXPECT_EQ(m_createSourceCallCount, 2);  // video + inband CC subtitle
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);  // inband CC
}

TEST_F(AampRialtoPlayerTest, Configure_FormatUnknown_CreatesSourceWithoutDemuxer)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_UNKNOWN, FORMAT_UNKNOWN);
	EXPECT_EQ(m_createSourceCallCount, 3);  // video + audio + inband CC subtitle
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->hasDemuxer());
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->hasDemuxer());
}

TEST_F(AampRialtoPlayerTest, Configure_FormatIsoBmff_DoesNotCreateDemuxer)
{
	// Demuxer creation is deferred until the first SendTransfer call so
	// that sources using the SendSample path (already-demuxed data) never
	// allocate a demuxer they do not need.
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->hasDemuxer());
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->hasDemuxer());
}

TEST_F(AampRialtoPlayerTest, Configure_AudioOnly_CreatesAudioSourceOnly)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	EXPECT_EQ(m_createSourceCallCount, 1);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
}

TEST_F(AampRialtoPlayerTest, Configure_AllThreeFormats_CreatesAllThreeSources)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	EXPECT_EQ(m_createSourceCallCount, 3);
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
}

TEST_F(AampRialtoPlayerTest, Configure_SubtitleAttach_HoldsAttachMutex)
{
	/**
	 * @brief Regression (H3): CreateSubtitleSource() calls AttachSource(),
	 *        which requires m_attachMutex.  SetVideoMute() takes the same
	 *        mutex, so a call started from inside attachSource() must wait
	 *        until Configure() has released it.
	 */
	std::atomic<bool> muteReturned{false};
	bool returnedDuringAttach = true;
	std::thread muteThread;

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[&](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				muteThread = std::thread([&]
				{
					m_player->SetVideoMute(true);
					muteReturned = true;
				});
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				returnedDuringAttach = muteReturned.load();
				return true;
			}));

	Configure(FORMAT_INVALID, FORMAT_INVALID, FORMAT_SUBTITLE_TTML);

	if (muteThread.joinable())
	{
		muteThread.join();
	}
	EXPECT_FALSE(returnedDuringAttach)
		<< "subtitle attach ran without holding m_attachMutex";
	EXPECT_TRUE(muteReturned.load());
}

TEST_F(AampRialtoPlayerTest,
	Configure_PipelineCreationFails_AudioOnlyWithSubtitle_DoesNotCrash)
{
	/**
	 * @brief Regression (H3): with no video source the subtitle attach is not
	 *        deferred, so a failed pipeline creation must not be dereferenced.
	 */
	ON_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillByDefault(Invoke(
			[this](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			       const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				m_capturedClient = client;
				return nullptr;
			}));

	EXPECT_NO_THROW(
		Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML));
	EXPECT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
}

// ===========================================================================
// Phase 3 — CheckAllSourcesAttached
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_CreatesDemuxerLazily)
{
	// No demuxer should exist after Configure — it is created on the
	// first SendTransfer call, so sources that only ever receive
	// already-demuxed samples via SendSample never allocate one.
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->hasDemuxer());
	SendVideoInitFragment();
	EXPECT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->hasDemuxer());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_BothSources_CallsAllSourcesAttachedOnce)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendVideoInitFragment();
	SendAudioInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_VideoOnlyStream_CallsAllSourcesAttachedAfterVideo)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_AudioOnlyStream_CallsAllSourcesAttachedAfterAudio)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendAudioInitFragment();
}

// ===========================================================================
// IRialtoControlBackend integration
// ===========================================================================

TEST_F(AampRialtoPlayerTest, Configure_CallsWaitForRunningBeforeCreatingPipeline)
{
	testing::InSequence seq;
	EXPECT_CALL(*m_mockControlBackend, waitForRunning(_)).WillOnce(Return(true));
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillOnce(Invoke(
			[this](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
				const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				m_capturedClient = client;
				return std::move(m_mockPipeline);
			}));

	Configure();
}

TEST_F(AampRialtoPlayerTest, Configure_ProceedsWhenWaitForRunningTimesOut)
{
	EXPECT_CALL(*m_mockControlBackend, waitForRunning(_)).WillOnce(Return(false));
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillOnce(Invoke(
			[this](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			       const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				m_capturedClient = client;
				return std::move(m_mockPipeline);
			}));

	Configure();
}

// ===========================================================================
// Phase N — Configure idempotency and smart pipeline recreation
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	Configure_SameFormats_SecondCallDoesNotRecreate)
{
	/**
	 * @brief Calling Configure() a second time with identical formats must
	 *        not recreate the Rialto pipeline.  createMediaPipeline must be
	 *        invoked exactly once across both calls.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	// Second call with same formats — early return expected.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_DifferentVideoFormat_RecreatesPipeline)
{
	/**
	 * @brief Changing the video format between Configure() calls must
	 *        trigger a full pipeline teardown and recreation.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ResetMockPipeline();
	m_player->Configure(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_VideoUnknownThenKnown_NoRecreate_UpdatesStoredFormat)
{
	/**
	 * @brief A video source created with FORMAT_UNKNOWN (e.g. muxed HLS-TS
	 *        before PMT parsing) that is later Configure()'d with the real,
	 *        now-known codec must NOT trigger a pipeline recreation, and the
	 *        source's stored format must be updated to the known value.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_UNKNOWN, FORMAT_UNKNOWN);

	auto *videoSource = m_mockSources[eMEDIATYPE_VIDEO];
	ASSERT_NE(videoSource, nullptr);
	EXPECT_EQ(videoSource->format(), FORMAT_UNKNOWN);

	m_player->Configure(FORMAT_VIDEO_ES_H264, FORMAT_UNKNOWN, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(videoSource->format(), FORMAT_VIDEO_ES_H264);
}

TEST_F(AampRialtoPlayerTest,
	Configure_VideoKnownThenUnknown_NoRecreate_KeepsStoredFormat)
{
	/**
	 * @brief Once a video source's codec is known, a later Configure() call
	 *        reporting FORMAT_UNKNOWN must NOT recreate the pipeline and
	 *        must NOT overwrite the already-known stored format.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF);

	auto *videoSource = m_mockSources[eMEDIATYPE_VIDEO];
	ASSERT_NE(videoSource, nullptr);
	EXPECT_EQ(videoSource->format(), FORMAT_VIDEO_ES_H264);

	m_player->Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(videoSource->format(), FORMAT_VIDEO_ES_H264);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioUnknownThenKnown_NoRecreate_UpdatesStoredFormat)
{
	/**
	 * @brief Same as the video case above, but for the audio source.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_UNKNOWN, FORMAT_UNKNOWN);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);
	EXPECT_EQ(audioSource->format(), FORMAT_UNKNOWN);

	m_player->Configure(FORMAT_UNKNOWN, FORMAT_AUDIO_ES_AAC, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(audioSource->format(), FORMAT_AUDIO_ES_AAC);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioKnownThenUnknown_NoRecreate_KeepsStoredFormat)
{
	/**
	 * @brief Once an audio source's codec is known, a later Configure() call
	 *        reporting FORMAT_UNKNOWN must NOT recreate the pipeline and
	 *        must NOT overwrite the already-known stored format.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_AUDIO_ES_AAC);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);
	EXPECT_EQ(audioSource->format(), FORMAT_AUDIO_ES_AAC);

	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(audioSource->format(), FORMAT_AUDIO_ES_AAC);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioGoesInvalid_NoPipelineRecreation_EOSSignaled)
{
	/**
	 * @brief When audio transitions from a valid format to FORMAT_INVALID
	 *        (trickplay entry) the pipeline must NOT be recreated.  Instead,
	 *        EOS is signalled on the audio source so it drains cleanly and
	 *        video continues without interruption.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);

	// Trickplay entry: audio goes FORMAT_INVALID.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_TRUE(audioSource->state().eos);
}

TEST_F(AampRialtoPlayerTest,
	Configure_VideoGoesInvalid_RecreatesPipeline)
{
	/**
	 * @brief When video transitions to FORMAT_INVALID the pipeline must be
	 *        fully recreated — video going away is not a trickplay-EOS
	 *        scenario.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ResetMockPipeline();
	m_player->Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioReturnsAfterEOS_SameFormatNoRecreation)
{
	/**
	 * @brief After trickplay (audio EOS'd via FORMAT_INVALID), restoring
	 *        the original audio format must NOT recreate the pipeline.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	// Trickplay entry: audio → FORMAT_INVALID (EOS signalled, no recreation).
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	// Trickplay exit: audio returns to the same original format — still no
	// recreation because the source set is unchanged.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	// Times(1) verification is implicit at end of test scope.
}

TEST_F(AampRialtoPlayerTest,
	Configure_ESChangeStatus_ForcesRecreation)
{
	/**
	 * @brief bESChangeStatus=true must force pipeline recreation even when
	 *        the stream formats are otherwise identical.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ResetMockPipeline();
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/true,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_SetReadyAfterPipeline_ForcesRecreation)
{
	/**
	 * @brief setReadyAfterPipelineCreation=true must force pipeline
	 *        recreation even when stream formats are unchanged.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ResetMockPipeline();
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/true);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AfterStop_RecreatesPipeline)
{
	/**
	 * @brief Stop() marks the pipeline as stopped, so the next Configure()
	 *        with identical formats must still recreate the pipeline.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	m_player->Stop(false);
	ResetMockPipeline();
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_Recreate_StopsAndReleasesOldPipelineBeforeCreatingNewOne)
{
	/**
	 * @brief Regression (L1): rebuilding the pipeline on an elementary-stream
	 *        change must stop and release the old pipeline first.  Rialto
	 *        opens a server session per createMediaPipeline(), so two live
	 *        sessions can fail where only one primary video session exists.
	 */
	auto firstDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<TrackedMockPipeline>(firstDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();
	auto *firstPipeline = m_mockPipelinePtr;
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	ResetMockPipeline();
	::testing::Sequence order;
	EXPECT_CALL(*firstPipeline, stop())
		.InSequence(order)
		.WillOnce(Return(true));
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.InSequence(order)
		.WillOnce(Invoke(
			[&](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			    const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				EXPECT_TRUE(firstDestroyed->load())
					<< "old pipeline still alive when the new one is created";
				m_capturedClient = client;
				return std::move(m_mockPipeline);
			}));

	m_player->Configure(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_Recreate_ReleasesOldPipelineHeldByMonitorAV)
{
	/**
	 * @brief Regression (L1): the AV monitor keeps its own reference to the
	 *        pipeline, so it must be released with the pipeline or the old
	 *        Rialto session outlives the creation of the new one.
	 */
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_MonitorAV))
		.WillByDefault(Return(true));
	ON_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ReportProgressInterval))
		.WillByDefault(Return(0.75));
	ON_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MonitorAVReportingInterval))
		.WillByDefault(Return(5000));

	auto firstDestroyed = std::make_shared<std::atomic<bool>>(false);
	m_mockPipeline = std::make_unique<TrackedMockPipeline>(firstDestroyed);
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	ResetMockPipeline();
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _))
		.WillOnce(Invoke(
			[&](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
			    const firebolt::rialto::VideoRequirements &)
				-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
			{
				EXPECT_TRUE(firstDestroyed->load())
					<< "old pipeline still alive (held by the AV monitor?)";
				m_capturedClient = client;
				return std::move(m_mockPipeline);
			}));

	m_player->Configure(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_Recreate_KeepsReplacedSourceAliveWhileInjectorIsInsideIt)
{
	/**
	 * @brief Regression (L1): an injector thread inside a source when
	 *        Configure() replaces it must not have the source destroyed
	 *        under it; the source is released once the injector leaves.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	auto *oldVideo = m_mockSources[eMEDIATYPE_VIDEO];
	ASSERT_NE(oldVideo, nullptr);
	const auto oldVideoDestroyed = m_sourceDestroyed[eMEDIATYPE_VIDEO];
	ASSERT_NE(oldVideoDestroyed, nullptr);

	std::promise<void> entered;
	std::promise<void> release;
	auto released = release.get_future();
	ON_CALL(*oldVideo, injectSingleSample(_, _, _))
		.WillByDefault(Invoke(
			[&](firebolt::rialto::IMediaPipeline &, AampMediaSample &&, bool)
			{
				entered.set_value();
				released.wait();
				return false;
			}));

	std::thread injector([&]
	{
		m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample(), false);
	});
	entered.get_future().wait();

	ResetMockPipeline();
	m_player->Configure(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_FALSE(oldVideoDestroyed->load())
		<< "source destroyed while an injector was still inside it";

	release.set_value();
	injector.join();
	EXPECT_TRUE(oldVideoDestroyed->load())
		<< "replaced source was not released after the injector left";
}

TEST_F(AampRialtoPlayerTest,
	Constructor_EmptySourceCreator_ConfigureDoesNotThrow)
{
	/**
	 * @brief Regression (L8): the injected-notifiable constructor defaulted its
	 *        source creator to an empty callable, so Configure() threw
	 *        std::bad_function_call.  An empty creator now selects the default.
	 */
	auto backend = std::make_unique<NiceMock<MockIRialtoControlBackend>>();
	ON_CALL(*backend, waitForRunning(_)).WillByDefault(Return(true));
	AampRialtoPlayer player(
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
		&m_mockNotifiable,
		std::move(backend),
		/*id3HandlerCallback=*/nullptr);

	EXPECT_NO_THROW(player.Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false));
}

TEST_F(AampRialtoPlayerTest,
	Configure_CreatesPipelineWithFixedRequirementsAndLoadsMse)
{
	/**
	 * @brief Pins the values behind the L13 constants: Rialto must be asked for
	 *        a 4K-capable (primary) pipeline and load() must get the MSE
	 *        placeholder URL with isLive=false.
	 */
	using ::testing::AllOf;
	using ::testing::Field;
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_,
		AllOf(Field(&firebolt::rialto::VideoRequirements::maxWidth, 3840u),
		      Field(&firebolt::rialto::VideoRequirements::maxHeight, 2160u))))
		.Times(1);
	EXPECT_CALL(*m_mockPipelinePtr,
		load(firebolt::rialto::MediaType::MSE, "", "mse://1", false))
		.WillOnce(Return(true));

	Configure();
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioOnlyNoSubtitleSource_SecondCallDoesNotRecreate)
{
	/**
	 * @brief Regression (L2): audio-only content creates no subtitle source,
	 *        and a second Configure() with the same formats and no subtitle
	 *        format must not rebuild the pipeline (it used to, on every
	 *        normal-rate Configure()).  The null subtitle source must also
	 *        not be dereferenced.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_INVALID);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	m_player->Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_AudioOnly_SubtitleFormatAppears_RecreatesPipeline)
{
	/**
	 * @brief A subtitle format that was not present before still requires a
	 *        rebuild: the missing subtitle source is a change only when a
	 *        subtitle format is requested.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(2);
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_INVALID);
	ASSERT_EQ(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	ResetMockPipeline();
	m_player->Configure(FORMAT_INVALID, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_WEBVTT,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

TEST_F(AampRialtoPlayerTest,
	Configure_Trickplay_EosPersistsThroughFlush)
{
	/**
	 * @brief Regression: Bug B from L2 TESTDATA0 rewind failure.
	 *
	 * When trickplay entry (audio → FORMAT_INVALID) is followed by a
	 * Flush() with rate != 1, the audio source's EOS must NOT be cleared.
	 * Without this the Flush() clears EOS, Rialto issues needData for
	 * audio, AAMP never responds, and the Rialto/GStreamer pipeline clock
	 * stalls so no video is rendered.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);

	// Trickplay entry: audio goes FORMAT_INVALID.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	// EOS must be set immediately after trickplay Configure().
	EXPECT_TRUE(audioSource->state().eos);

	// A subsequent Flush (the position-seek flush that follows in trickplay)
	// must NOT clear the audio EOS because rate != 1 (trickplay).
	m_player->Flush(/*position=*/12.0, /*rate=*/-2, /*shouldTearDown=*/false);

	EXPECT_TRUE(audioSource->state().eos)
		<< "Audio EOS must survive Flush() when rate != AAMP_NORMAL_PLAY_RATE";
}

TEST_F(AampRialtoPlayerTest,
	Flush_NormalRate_ClearsAudioEos)
{
	/**
	 * @brief Flush() with rate == AAMP_NORMAL_PLAY_RATE must clear audio EOS
	 *        so the injection path can resume normally after trickplay exit.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);

	// Trickplay entry: audio goes FORMAT_INVALID.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
	EXPECT_TRUE(audioSource->state().eos);

	// Flush at normal rate (trickplay exit seek) must clear audio EOS.
	m_player->Flush(/*position=*/5.0, /*rate=*/1, /*shouldTearDown=*/false);

	EXPECT_FALSE(audioSource->state().eos)
		<< "Audio EOS must be cleared when Flush() rate == AAMP_NORMAL_PLAY_RATE";
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_Trickplay_FlushingStateRespondsToRialtoCallbacks)
{
	/**
	 * @brief Regression: Bug A from L2 TESTDATA0 rewind failure.
	 *
	 * The sequence Flush(shouldTearDown=false) → SEEK_DONE →
	 * Configure(audio=INVALID) → Rialto PLAYING previously left the state
	 * machine stuck in FLUSHING when the Configure() did not rebuild the
	 * pipeline.
	 *
	 * Fix: onFlushComplete() is now called from the SEEK_DONE handler once
	 * the pipeline-level flushing seek completes, taking the state machine
	 * to FLUSHED. FLUSHED does not remember or restore the pre-flush state -
	 * it simply finds a non-FLUSHING state for the pipeline reuse path in
	 * Configure(), and the subsequent Rialto PLAYING notification (guaranteed
	 * to follow SEEK_DONE) transitions FLUSHED → PLAYING via
	 * FlushedState::onPlaybackStarted().
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PIPELINE_CREATED)
		<< "Precondition: state must be PIPELINE_CREATED after Configure()";

	// Advance state machine to SOURCES_ATTACHED by sending init fragments
	// for both sources so that Flush() can transition to FLUSHING.
	SendVideoInitFragment();
	SendAudioInitFragment();

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED)
		<< "Precondition: state must be SOURCES_ATTACHED before the flush";

	// Move state machine to FLUSHING via a flush without teardown.
	// shouldTearDown=false allows flush to proceed even when not in PLAYING/PAUSED.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(/*position=*/0.0, /*rate=*/-2, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Precondition: Flush(shouldTearDown=false) must move state to FLUSHING";

	// Simulate Rialto confirming the pipeline-level flushing seek completed.
	// This fixture does not stub isVideoMaster(), so computeAppliedRate()
	// falls back to 1.0 (normal) and the SEEK_DONE handler does not issue the
	// extra video-source setSourcePosition() call.  onFlushComplete() takes
	// the state machine to FLUSHED and notifies WaitForFlushToComplete() so
	// Configure() can proceed.  The subtitle source's position is still
	// updated unconditionally on every SEEK_DONE.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId(),
			/*posNs=*/0, /*resetTime=*/false, _, _))
		.WillOnce(Return(true));
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// After the flushing seek completes, onFlushComplete() lands in FLUSHED
	// (it does not restore SOURCES_ATTACHED).
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED)
		<< "onFlushComplete() must transition FLUSHING -> FLUSHED";

	// Trickplay Configure: audio → FORMAT_INVALID, no pipeline recreation.
	// The pipeline reuse path does not touch the state machine, so state
	// remains FLUSHED.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED)
		<< "Pipeline reuse must not change the state from FLUSHED";

	// Verify the normal transition: FLUSHED responds to Rialto PLAYING - this
	// is the mechanism that used to get stuck in FLUSHING before the fix.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING)
		<< "FLUSHED must transition to PLAYING when Rialto sends PLAYING";
}

TEST_F(AampRialtoPlayerTest,
	Configure_TrickplayExit_ClearsEos)
{
	/**
	 * @brief When audio returns from FORMAT_INVALID to a valid format
	 *        without pipeline recreation, EOS must be cleared so the
	 *        audio injection path can resume.
	 */
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	auto *audioSource = m_mockSources[eMEDIATYPE_AUDIO];
	ASSERT_NE(audioSource, nullptr);

	// Trickplay entry.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
	EXPECT_TRUE(audioSource->state().eos);

	// Trickplay exit: same original audio format — no recreation, but
	// EOS must be cleared.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	EXPECT_FALSE(audioSource->state().eos)
		<< "EOS must be cleared on trickplay exit so audio injection resumes";
}

// ===========================================================================
// WaitForFlushToComplete — Configure blocks until flush cycle finishes
// ===========================================================================

/**
 * @test When Configure() is called while the player is FLUSHING, it must block
 *       until the flush cycle completes so that m_rate reflects the staged
 *       flush rate before ShouldRecreatePipeline checks it.
 *
 * Scenario: Flush at rate=4 → state=FLUSHING → Configure() on another thread
 * → SEEK_DONE completes the flush cycle → Configure() unblocks with
 * the correct m_rate.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_WhileFlushing_BlocksUntilFlushComplete)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Enter PLAYING so Flush() transitions to FLUSHING.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	// Flush at trickplay rate — moves to FLUSHING and sets m_pendingFlushRate.
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(10.0, 4, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// At this point m_rate has NOT been committed yet (awaiting SEEK_DONE).
	// Launch Configure() on a background thread; it should block in
	// WaitForFlushToComplete().
	std::atomic<bool> configureFinished{false};
	std::thread configureThread([&]() {
		// Force pipeline recreation via bESChangeStatus=true so we always
		// hit ShouldRecreatePipeline() path.
		ResetMockPipeline();
		m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
			/*bESChangeStatus=*/true,
			/*setReadyAfterPipelineCreation=*/false);
		configureFinished.store(true, std::memory_order_release);
	});

	// Spin until Configure() finishes or the timeout elapses.
	// If WaitForFlushToComplete() is working, Configure() must still be blocked.
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
	while (!configureFinished.load(std::memory_order_acquire) &&
	       std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::yield();
	}
	EXPECT_FALSE(configureFinished.load(std::memory_order_acquire))
		<< "Configure() must block while the flush is still in progress";

	// Complete the flush cycle by posting SEEK_DONE.
	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);

	// Configure() should now unblock.
	configureThread.join();
	EXPECT_TRUE(configureFinished.load(std::memory_order_acquire));
}

/**
 * @test When Configure() is called and the player is NOT flushing,
 *       WaitForFlushToComplete() returns immediately without blocking.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_NotFlushing_DoesNotBlock)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	// No flush in progress — Configure must complete immediately.
	ResetMockPipeline();
	EXPECT_NO_THROW(
		m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
			/*bESChangeStatus=*/true, /*setReadyAfterPipelineCreation=*/false));
}

/**
 * @test Regression (H6): if Rialto never sends SEEK_DONE, Stop() - the
 *       recovery path - must not block forever.
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Stop_FlushNeverCompletes_ReturnsAfterTimeout)
{
	m_player->SetFlushCompleteTimeout(std::chrono::milliseconds(50));
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(10.0, 4, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	m_player->Stop(false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

/**
 * @test Regression (H6): a Flush() that follows a flush whose SEEK_DONE was
 *       lost must recover and issue its own seek rather than block.
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Flush_PreviousFlushLost_RecoversAfterTimeout)
{
	m_player->SetFlushCompleteTimeout(std::chrono::milliseconds(50));
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).Times(2).WillRepeatedly(Return(true));
	m_player->Flush(5.0, 1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	m_player->Flush(6.0, 1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	PostPlaybackState(firebolt::rialto::PlaybackState::SEEK_DONE);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHED);
}

/**
 * @test Regression (H6): abandoning a flush must release PlayHold::Flushing so
 *       a play request made during it is not deferred forever.
 */
TEST_F(AampRialtoPlayerWithDemuxTest, Configure_FlushNeverCompletes_IssuesDeferredPlay)
{
	m_player->SetFlushCompleteTimeout(std::chrono::milliseconds(50));
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, setPosition(_)).WillOnce(Return(true));
	m_player->Flush(5.0, 1, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Deferred behind the Flushing hold.
	m_player->Stream();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false, /*setReadyAfterPipelineCreation=*/false);
}
