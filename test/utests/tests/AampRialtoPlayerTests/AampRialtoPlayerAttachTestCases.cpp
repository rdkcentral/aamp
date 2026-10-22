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
 * @file AampRialtoPlayerAttachTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Source attachment: SendTransfer and SetStreamCaps attach paths, video-first ordering, deferred audio and failed attaches.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase 4 — attachSource via SendTransfer
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesSourceToPipeline)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// video attaches first, then the deferred inband CC subtitle source.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(2);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesAudioSourceToPipeline)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);

	SendAudioInitFragment();
}

// ===========================================================================
// Phase 4b — Source attachment ordering (video before audio)
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_AudioInitFirst_DefersUntilVideoAttaches)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	// Audio init arrives first — should NOT trigger attachSource yet.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);
	SendAudioInitFragment();
	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// Video init arrives — should trigger video, deferred audio, and the
	// deferred inband CC subtitle source created in Configure().
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(
		testing::Truly([](const auto &src) {
			return dynamic_cast<
				const firebolt::rialto::IMediaPipeline::MediaSourceSubtitle *>(
					src.get()) != nullptr;
		})))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	{
		testing::InSequence seq;
		EXPECT_CALL(*m_mockPipelinePtr, attachSource(
			testing::Truly([](const auto &src) {
				return dynamic_cast<
					const firebolt::rialto::IMediaPipeline::MediaSourceVideo *>(
						src.get()) != nullptr;
			})))
			.WillOnce(Invoke(
				[this](const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSource> &src)
				{
					const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
						*src).setId(m_nextSourceId++);
					return true;
				}));
		EXPECT_CALL(*m_mockPipelinePtr, attachSource(
			testing::Truly([](const auto &src) {
				return dynamic_cast<
					const firebolt::rialto::IMediaPipeline::MediaSourceAudio *>(
						src.get()) != nullptr;
			})))
			.WillOnce(Invoke(
				[this](const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSource> &src)
				{
					const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
						*src).setId(m_nextSourceId++);
					return true;
				}));
	}
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_AudioOnlyConfig_AttachesImmediately)
{
	// No video source configured — audio should not be deferred.
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendAudioInitFragment();
}

// ===========================================================================
// Phase 4c — Failed attach is reported (H4)
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_AttachSourceFails_ReportsErrorAndReturnsFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	ON_CALL(*m_mockPipelinePtr, attachSource(_)).WillByDefault(Return(false));

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, false)).Times(1);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
	EXPECT_FALSE(m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0, 0, 0, 0, /*initFragment=*/true));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_DeferredSourceAttachFails_ReleasesBlockedInjectorAndReports)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ON_CALL(*m_mockPipelinePtr, attachSource(testing::Truly([](const auto &src) {
		return dynamic_cast<
			const firebolt::rialto::IMediaPipeline::MediaSourceAudio *>(
				src.get()) != nullptr;
	}))).WillByDefault(Return(false));

	// Audio arrives first and is deferred, so injectors would block on it.
	SendAudioInitFragment();
	{
		auto &st = m_mockSources[eMEDIATYPE_AUDIO]->state();
		std::lock_guard<std::mutex> lock(st.mu);
		ASSERT_TRUE(st.attachPending);
	}

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, false)).Times(1);
	SendVideoInitFragment();

	auto &st = m_mockSources[eMEDIATYPE_AUDIO]->state();
	std::lock_guard<std::mutex> lock(st.mu);
	EXPECT_FALSE(st.attachPending)
		<< "a failed deferred attach must release injectors blocked on it";
}

TEST_F(AampRialtoPlayerTest, SetStreamCaps_UnsupportedCodec_ReportsError)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], mapCodecToMime(_, _, _, _))
		.WillByDefault(Return(false));

	EXPECT_CALL(m_mockNotifiable,
		NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR, _, false)).Times(1);

	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_VIDEO_ES_MPEG2;
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, std::move(ci));
}

TEST_F(AampRialtoPlayerTest, SetStreamCaps_CodecNotYetIdentified_DoesNotReportError)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], mapCodecToMime(_, _, _, _))
		.WillByDefault(Return(false));

	EXPECT_CALL(m_mockNotifiable, NotifyPlaybackError(_, _, _)).Times(0);

	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_UNKNOWN;
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, std::move(ci));
}

// ===========================================================================
// SetStreamCaps — source attach via external demuxer path
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_VideoH264_AttachesVideoSource)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

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
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached())
		.WillOnce(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_AudioAAC_AttachesAudioSource)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached())
		.WillOnce(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_VideoAndAudio_AllSourcesAttached)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.Times(3)
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached())
		.Times(1)
		.WillOnce(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_VideoAlreadyAttached_StagesCodecData)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

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
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo(1280, 720));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo(1920, 1080));
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_UnknownCodecFormat_DoesNotAttach)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// Override mapCodecToMime to return false for invalid format
	if (m_mockSources[eMEDIATYPE_VIDEO])
	{
		ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], mapCodecToMime(_, _, _, _))
			.WillByDefault(Return(false));
	}

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);

	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_INVALID;
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, std::move(ci));
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_NoPipeline_DoesNotCrash)
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
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_NO_THROW(
		m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo()));
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_WithFlushPosition_SetsSourcePosition)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	m_player->Flush(10.0, 1, false);

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
	// Both video (id=0) and the deferred inband CC subtitle (id=1) call
	// setSourcePosition because they both attach during this SetStreamCaps.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL), _, _, _))
		.Times(2)
		.WillRepeatedly(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_Subtitle_ResumesTrackDownloads)
{
	// Regression test: after a period transition SelectSubtitleTrack calls
	// StopTrackDownloads(SUBTITLE), setting mbTrackDownloadsBlocked[SUBTITLE]=true.
	// For Rialto, the NeedData callback never clears that flag (Rialto uses
	// gateMode, not mbTrackDownloadsBlocked for subtitle backpressure).
	// SetStreamCaps must therefore call ResumeTrackDownloads(SUBTITLE) after
	// attaching the subtitle source so the inject loop is unblocked.
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ResumeTrackDownloads(eMEDIATYPE_SUBTITLE))
		.Times(1);

	m_player->SetStreamCaps(eMEDIATYPE_SUBTITLE, MakeSubtitleTtmlCodecInfo());
}

// ===========================================================================
// PipelineConfiguredForMedia
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	PipelineConfiguredForMedia_VideoAttached_ReturnsTrue)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	EXPECT_TRUE(m_player->PipelineConfiguredForMedia(eMEDIATYPE_VIDEO));
}

TEST_F(AampRialtoPlayerTest,
	PipelineConfiguredForMedia_NotAttached_ReturnsFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_FALSE(m_player->PipelineConfiguredForMedia(eMEDIATYPE_VIDEO));
}

TEST_F(AampRialtoPlayerTest,
	PipelineConfiguredForMedia_AudioAttached_ReturnsTrue)
{
	Configure(FORMAT_INVALID, FORMAT_ISO_BMFF);
	m_player->SetStreamCaps(eMEDIATYPE_AUDIO, MakeAudioAacCodecInfo());

	EXPECT_TRUE(m_player->PipelineConfiguredForMedia(eMEDIATYPE_AUDIO));
}

// ===========================================================================
// Deferred audio attachment — inject-thread blocking
// ===========================================================================

// When audio attachment is deferred (waiting for video), the audio source's
// attachPending flag must be set so the inject thread knows to block.
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_DeferredAudio_SetsAttachPendingOnAudioSource)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);
	SendAudioInitFragment();

	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->state().attachPending);
}

// When a media fragment arrives for a deferred-audio source, the inject
// thread must block until video attaches (which drains the pending audio
// attachment), then inject normally.
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_DeferredAudio_BlocksUntilVideoAttaches)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	// Mirrors real AAMP usage: Stream() always follows Configure(), recording
	// the play request so that once all sources finish attaching (below),
	// CheckAllSourcesAttached() releases the last hold and play() clears the
	// gate set by Configure().
	m_player->Stream();
	SendAudioInitFragment();  // deferred — attachPending=true

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::atomic<int> addSegmentCalls{0};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](uint32_t, const auto &)
				-> firebolt::rialto::AddSegmentStatus
			{
				++addSegmentCalls;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	// Audio media fragment should block because source is not attached yet.
	std::atomic<bool> sendDone{false};
	std::vector<uint8_t> buf = {0x01};
	std::thread sender([this, b = std::move(buf), &sendDone]() mutable {
		m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
		sendDone = true;
	});

	// Verify the inject thread is blocked — no injection yet.
	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 0);

	// Attaching video drains the pending audio attachment, which unblocks
	// the waiting inject thread.
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);
	SendVideoInitFragment();  // attaches video; deferred audio attachment fires
	// Audio sourceId=1 is now registered. Send NeedData so the inject
	// thread (unblocked from the attachPending wait) can proceed.
	PostNeedData(1, 1, 42);

	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 1);

	sender.join();
}

// If Flush() is called while the inject thread is blocked on a deferred
// attachment, the thread must unblock and discard the fragment.
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_DeferredAudio_FlushWhileBlockedAbortsSendTransfer)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendAudioInitFragment();  // deferred — attachPending=true

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
		m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
		sendDone = true;
	});

	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());

	// Flush increments the generation and sets paused=true, which must
	// wake the blocked inject thread.
	RepeatUntil([&sendDone]{ return sendDone.load(); },
		[this]{ EXPECT_NO_THROW(m_player->Flush(0.0, 1, false)); });
	EXPECT_TRUE(sendDone.load());

	sender.join();
}

// SendSample uses the same waitForAttach() helper as SendTransfer.  Verify
// that a pre-decoded audio sample is also held until the deferred Rialto
// attachment completes, then injected successfully.
TEST_F(AampRialtoPlayerWithDemuxTest,
	SendSample_DeferredAudio_BlocksUntilVideoAttaches)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	// Mirrors real AAMP usage: Stream() always follows Configure(), recording
	// the play request so that once all sources finish attaching (below),
	// CheckAllSourcesAttached() releases the last hold and play() clears the
	// gate set by Configure().
	m_player->Stream();
	SendAudioInitFragment();  // deferred — attachPending=true

	// Spawn a thread to call SendSample on the deferred audio source.
	std::atomic<bool> sendDone{false};
	std::thread sender([this, &sendDone]() {
		m_player->SendSample(eMEDIATYPE_AUDIO, MakeSample(0.1, 0.033));
		sendDone = true;
	});

	// Verify the inject thread is blocked — no injection yet.
	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());

	// Attaching video drains the pending audio attachment, unblocking the
	// waiting SendSample thread.
	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);
	SendVideoInitFragment();

	// Audio sourceId=1 is now registered.  Post NeedData so the unblocked
	// SendSample thread can complete its injection.
	std::atomic<bool> haveDataCalled{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(42, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));
	ON_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 42))
		.WillByDefault(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));
	PostNeedData(/*sourceId=*/1, /*frameCount=*/1, /*requestId=*/42);

	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());

	sender.join();
}
