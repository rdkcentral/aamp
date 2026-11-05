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
 * @file AampRialtoPlayerAvSettingsTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: Audio/video settings: video rectangle and size, subtitles, mute, volume, playback quality and closed captions.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// SetVideoRectangle / GetVideoRectangle
// ===========================================================================

/**
 * @test GetVideoRectangle_ReturnsStringSetBySetVideoRectangle
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoRectangle_ReturnsStringSetBySetVideoRectangle)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, setVideoWindow(_, _, _, _))
		.WillByDefault(Return(true));

	m_player->SetVideoRectangle(10, 20, 640, 480);

	EXPECT_EQ(m_player->GetVideoRectangle(), "10,20,640,480");
}

/**
 * @test Configure_EventDeliveredDuringLoad_IsNotDropped
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_EventDeliveredDuringLoad_IsNotDropped)
{
	/**
	 * @brief Regression (M8): the pipeline callbacks were installed only after
	 *        load() succeeded, so anything Rialto reported while loading was
	 *        dropped.  A duration reported from inside load() must reach the
	 *        player.
	 */
	ON_CALL(*m_mockPipelinePtr, load(_, _, _, _))
		.WillByDefault(Invoke([this](auto, auto, auto, auto) {
			if (auto client = m_capturedClient.lock())
			{
				client->notifyDuration(7'000'000'000LL);
			}
			return true;
		}));

	Configure();

	EXPECT_EQ(m_player->GetDurationMilliseconds(), 7000);
}

/**
 * @test SetVideoRectangle_CallsPipelineSetVideoWindow
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetVideoRectangle_CallsPipelineSetVideoWindow)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr,
		setVideoWindow(/*x=*/0u, /*y=*/0u, /*w=*/1920u, /*h=*/1080u))
		.Times(1)
		.WillOnce(Return(true));

	m_player->SetVideoRectangle(0, 0, 1920, 1080);
}

/**
 * @test VideoRectangle_ConcurrentSetAndGet_NeverReadsATornValue
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	VideoRectangle_ConcurrentSetAndGet_NeverReadsATornValue)
{
	/**
	 * @brief Regression (M7): m_videoRectangle is a std::string written by
	 *        SetVideoRectangle() and read by GetVideoRectangle() and
	 *        GetVideoSize() on other threads.  Every value read must be one
	 *        that was written whole ("n,n,n,n").  Under a thread sanitizer the
	 *        unguarded string is also reported as a data race.
	 */
	Configure();
	ON_CALL(*m_mockPipelinePtr, setVideoWindow(_, _, _, _))
		.WillByDefault(Return(true));

	std::atomic<bool> done{false};
	std::atomic<bool> torn{false};
	auto reader = [&]
	{
		while (!done.load())
		{
			const std::string rect = m_player->GetVideoRectangle();
			int x = 0, y = 0, w = 0, h = 0;
			if (!rect.empty() &&
				(sscanf(rect.c_str(), "%d,%d,%d,%d", &x, &y, &w, &h) != 4 ||
				 x != y || y != w || w != h))
			{
				torn = true;
			}
			int sw = 0, sh = 0;
			m_player->GetVideoSize(sw, sh);
			if (sw != sh)
			{
				torn = true;
			}
		}
	};
	std::thread r1(reader);
	std::thread r2(reader);
	for (int i = 1; i <= 400; ++i)
	{
		m_player->SetVideoRectangle(i, i, i, i);
	}
	done = true;
	r1.join();
	r2.join();

	EXPECT_FALSE(torn.load());
	EXPECT_EQ(m_player->GetVideoRectangle(), "400,400,400,400");
}

/**
 * @test SetAudioVolume_ConcurrentCallers_ForwardOnlyWrittenValues
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetAudioVolume_ConcurrentCallers_ForwardOnlyWrittenValues)
{
	/**
	 * @brief Regression (M7): m_audioVolume was a plain int written by
	 *        SetAudioVolume() and read by applyAudioVolume() (also run on the
	 *        injector thread when audio attaches), and read twice per apply.
	 *        Each setVolume() must carry one of the values written.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	std::atomic<bool> unexpected{false};
	ON_CALL(*m_mockPipelinePtr, setVolume(_, _, _))
		.WillByDefault(Invoke([&](double v, auto, auto) {
			if (v != 0.25 && v != 0.75)
			{
				unexpected = true;
			}
			return true;
		}));

	std::thread a([this]{
		for (int i = 0; i < 300; ++i) { m_player->SetAudioVolume(25); }
	});
	std::thread b([this]{
		for (int i = 0; i < 300; ++i) { m_player->SetAudioVolume(75); }
	});
	a.join();
	b.join();

	EXPECT_FALSE(unexpected.load());
}

/**
 * @test OnPlaybackState_Playing_FirstTime_PassesVideoRectangleToNotifyFirstBuffer
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_PassesVideoRectangleToNotifyFirstBuffer)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, setVideoWindow(_, _, _, _))
		.WillByDefault(Return(true));
	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

	m_player->SetVideoRectangle(5, 10, 320, 240);

	EXPECT_CALL(m_mockNotifiable,
		NotifyFirstBufferProcessed(std::string{"5,10,320,240"}))
		.Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

// ===========================================================================
// GetVideoSize
// ===========================================================================

/**
 * @test GetVideoSize_AfterSetVideoRectangle_ReturnsWidthAndHeight
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoSize_AfterSetVideoRectangle_ReturnsWidthAndHeight)
{
	Configure();
	ON_CALL(*m_mockPipelinePtr, setVideoWindow(_, _, _, _))
		.WillByDefault(Return(true));

	m_player->SetVideoRectangle(10, 20, 640, 480);

	int w = 0;
	int h = 0;
	m_player->GetVideoSize(w, h);

	EXPECT_EQ(w, 640);
	EXPECT_EQ(h, 480);
}

/**
 * @test GetVideoSize_BeforeSetVideoRectangle_LeavesOutputsUnchanged
 */
TEST_F(AampRialtoPlayerTest, GetVideoSize_BeforeSetVideoRectangle_LeavesOutputsUnchanged)
{
	int w = 111;
	int h = 222;
	m_player->GetVideoSize(w, h);

	// No rectangle has been set yet, so the (unparseable) empty string
	// must leave the caller-supplied values untouched.
	EXPECT_EQ(w, 111);
	EXPECT_EQ(h, 222);
}

// ===========================================================================
// Phase N — Subtitle source injection
// ===========================================================================

/**
 * @test SendTransfer_SubtitleRawFragment_RoutesViaProcessDataFragment
 */
TEST_F(AampRialtoPlayerTest,
	SendTransfer_SubtitleRawFragment_RoutesViaProcessDataFragment)
{
	/**
	 * @brief Verifies that a non-init subtitle fragment is routed through
	 *        processDataFragment() (the subtitle source override), with the
	 *        correct fpts, fdts, fDuration and fragmentPTSoffset parameters.
	 *
	 *        The mock's processDataFragment default (Return(true)) means
	 *        the test does not need a demuxer or a Rialto NeedData handshake.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	// Expect processDataFragment proxy called with the exact parameters
	// passed to SendTransfer (fragmentPTSoffset == 0.0).
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_SUBTITLE],
		processDataFragment(
			Ref(*m_mockPipelinePtr), _,
			/*fpts=*/1.0, /*fdts=*/1.0, /*fDuration=*/0.5,
			/*fragmentPTSoffset=*/0.0))
		.WillOnce(Return(true));

	std::vector<uint8_t> buf = {0x3C, 0x74, 0x74, 0x3E}; // "<tt>"
	const bool result = m_player->SendTransfer(
		eMEDIATYPE_SUBTITLE, std::move(buf),
		/*fpts=*/1.0, /*fdts=*/1.0, /*fDuration=*/0.5,
		/*fragmentPTSoffset=*/0.0, /*initFragment=*/false);

	EXPECT_TRUE(result);
}

/**
 * @test SendTransfer_SubtitleRawFragmentWithOffset_ForwardsFragmentPTSoffset
 */
TEST_F(AampRialtoPlayerTest,
	SendTransfer_SubtitleRawFragmentWithOffset_ForwardsFragmentPTSoffset)
{
	/**
	 * @brief Verifies that a non-zero fragmentPTSoffset is forwarded
	 *        unchanged to processDataFragment().  The subtitle source's
	 *        processDataFragment override stores it as
	 *        sample.mDisplayOffsetMs = fragmentPTSoffset * 1000 before
	 *        calling injectSingleSample.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	constexpr double kOffsetSec = 5.0;

	EXPECT_CALL(*m_mockSources[eMEDIATYPE_SUBTITLE],
		processDataFragment(
			Ref(*m_mockPipelinePtr), _,
			/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/1.0,
			/*fragmentPTSoffset=*/kOffsetSec))
		.WillOnce(Return(true));

	std::vector<uint8_t> buf = {0x3C, 0x74, 0x74, 0x3E};
	m_player->SendTransfer(
		eMEDIATYPE_SUBTITLE, std::move(buf),
		/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/1.0,
		/*fragmentPTSoffset=*/kOffsetSec, /*initFragment=*/false);
}

// ===========================================================================
// SignalSubtitleClock
// ===========================================================================

/**
 * @test SignalSubtitleClock_WithAttachedSubtitle_SyncsPosition
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
        SignalSubtitleClock_WithAttachedSubtitle_SyncsPosition)
{
        /**
         * @brief When a subtitle source is attached, SignalSubtitleClock()
         *        queries the pipeline position and forwards it to
         *        setSourcePosition() for the subtitle source with
         *        resetTime=false.  Returns true on success.
         *
         * This is the primary fix for the 10-15 s subtitle delay: the
         * AAMP UpdateSubtitleClockTask calls SignalSubtitleClock() every
         * 500 ms at startup.  Implementing it to call getPosition +
         * setSourcePosition sends sendSessionTimestamp() to the Thunder
         * text-track renderer within 500 ms instead of waiting for the
         * Rialto server's 10-second periodic SynchroniseSubtitleClock.
         */
        Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
        SendVideoInitFragment();
        ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
        ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());

        constexpr int64_t kPositionNs     = 23'780'000'000LL;
        const int32_t     subtitleSrcId   = m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId();

        EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
                .WillOnce(DoAll(SetArgReferee<0>(kPositionNs), Return(true)));
        EXPECT_CALL(*m_mockPipelinePtr,
                setSourcePosition(subtitleSrcId, kPositionNs, false, _, _))
                .WillOnce(Return(true));

        EXPECT_TRUE(m_player->SignalSubtitleClock());
}

/**
 * @test SignalSubtitleClock_NoSubtitleSource_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
        SignalSubtitleClock_NoSubtitleSource_ReturnsFalse)
{
        /**
         * @brief When no subtitle source is configured, SignalSubtitleClock()
         *        returns false without querying the pipeline.
         */
        Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, /*sub=*/FORMAT_INVALID);

        EXPECT_CALL(*m_mockPipelinePtr, getPosition(_)).Times(0);
        EXPECT_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);

        EXPECT_FALSE(m_player->SignalSubtitleClock());
}

/**
 * @test SignalSubtitleClock_GetPositionFails_ReturnsFalse
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
        SignalSubtitleClock_GetPositionFails_ReturnsFalse)
{
        /**
         * @brief When getPosition() fails, SignalSubtitleClock() returns
         *        false without calling setSourcePosition().
         */
        Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
        SendVideoInitFragment();

        EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
                .WillOnce(Return(false));
        EXPECT_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);

        EXPECT_FALSE(m_player->SignalSubtitleClock());
}

/**
 * @test SignalSubtitleClock_NoPipeline_ReturnsFalse
 */
TEST_F(AampRialtoPlayerTest,
        SignalSubtitleClock_NoPipeline_ReturnsFalse)
{
        /**
         * @brief Before Configure() there is no pipeline; SignalSubtitleClock()
         *        returns false without crashing.
         */
        EXPECT_FALSE(m_player->SignalSubtitleClock());
}

// ===========================================================================
// SetSubtitleMute
// ===========================================================================

/**
 * @test SetSubtitleMute_AttachedSubtitle_CallsPipelineSetMute
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetSubtitleMute_AttachedSubtitle_CallsPipelineSetMute)
{
	/**
	 * @brief When the subtitle source is attached, SetSubtitleMute(true)
	 *        must call m_pipeline->setMute(sourceId, true) and
	 *        SetSubtitleMute(false) must call setMute(sourceId, false).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	SendVideoInitFragment();
	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());

	const int32_t subtitleSrcId =
		m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId();

	EXPECT_CALL(*m_mockPipelinePtr, setMute(subtitleSrcId, true)).Times(1);
	m_player->SetSubtitleMute(true);

	EXPECT_CALL(*m_mockPipelinePtr, setMute(subtitleSrcId, false)).Times(1);
	m_player->SetSubtitleMute(false);
}

/**
 * @test SetSubtitleMute_SubtitleNotYetAttached_MuteAppliedWhenAttached
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetSubtitleMute_SubtitleNotYetAttached_MuteAppliedWhenAttached)
{
	/**
	 * @brief SetSubtitleMute(true) called before the subtitle source is
	 *        attached must cache the state and call setMute once the
	 *        source attaches (triggered here by SendVideoInitFragment).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);

	// After Configure(), subtitle attachment is deferred — video hasn't
	// attached yet so isAttached() must be false here.
	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());

	// Mute before attach — must NOT trigger setMute yet.
	m_player->SetSubtitleMute(true);

	// setMute must be called exactly once when the subtitle source attaches.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(_, true)).Times(1);

	// SendVideoInitFragment triggers synchronous video attach which then
	// drains the deferred subtitle attach, applying the cached mute.
	SendVideoInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());
}

/**
 * @test SetSubtitleMute_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	SetSubtitleMute_NoPipeline_DoesNotCrash)
{
	/**
	 * @brief SetSubtitleMute called before Configure() (no pipeline)
	 *        must not crash.
	 */
	EXPECT_NO_FATAL_FAILURE(m_player->SetSubtitleMute(true));
}

// ===========================================================================
// SetAudioVolume
// ===========================================================================

/**
 * @test SetAudioVolume_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	SetAudioVolume_NoPipeline_DoesNotCrash)
{
	/**
	 * @brief SetAudioVolume called before Configure() (no pipeline) must
	 *        not crash.
	 */
	EXPECT_NO_FATAL_FAILURE(m_player->SetAudioVolume(50));
}

/**
 * @test SetAudioVolume_NonZero_CallsPipelineSetVolume
 */
TEST_F(AampRialtoPlayerTest,
	SetAudioVolume_NonZero_CallsPipelineSetVolume)
{
	/**
	 * @brief SetAudioVolume(50) with an existing pipeline must forward the
	 *        0-100 value to IMediaPipeline::setVolume() as 0.0-1.0, without
	 *        touching setMute() (the audio source has no sourceId yet).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, setVolume(DoubleEq(0.5), _, _)).Times(1);
	m_player->SetAudioVolume(50);
}

/**
 * @test SetAudioVolume_ZeroWithAttachedAudio_CallsPipelineSetMuteOnly
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetAudioVolume_ZeroWithAttachedAudio_CallsPipelineSetMuteOnly)
{
	/**
	 * @brief SetAudioVolume(0) with an attached audio source must call
	 *        setMute(sourceId, true) and must NOT call setVolume() -
	 *        mirrors InterfacePlayerRDK::SetVolumeOrMuteUnMute(), which
	 *        skips the volume write while muted.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->isAttached());

	const int32_t audioSrcId = m_mockSources[eMEDIATYPE_AUDIO]->sourceId();

	EXPECT_CALL(*m_mockPipelinePtr, setMute(audioSrcId, true)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, setVolume(_, _, _)).Times(0);
	m_player->SetAudioVolume(0);
}

/**
 * @test SetAudioVolume_NonZeroAfterMute_UnmutesAndSetsVolume
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetAudioVolume_NonZeroAfterMute_UnmutesAndSetsVolume)
{
	/**
	 * @brief SetAudioVolume(volume>0) after a previous SetAudioVolume(0)
	 *        must unmute the audio source and forward the new volume.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	const int32_t audioSrcId = m_mockSources[eMEDIATYPE_AUDIO]->sourceId();

	m_player->SetAudioVolume(0);

	EXPECT_CALL(*m_mockPipelinePtr, setMute(audioSrcId, false)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, setVolume(DoubleEq(0.3), _, _)).Times(1);
	m_player->SetAudioVolume(30);
}

/**
 * @test SetAudioVolume_MutedBeforeAudioAttached_MuteAppliedOnAttach
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetAudioVolume_MutedBeforeAudioAttached_MuteAppliedOnAttach)
{
	/**
	 * @brief SetAudioVolume(0) called before the audio source attaches
	 *        must cache the value and apply setMute() once the audio
	 *        source attaches (mirrors the SetSubtitleMute deferred-apply
	 *        pattern).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->isAttached());

	// Mute before attach - no sourceId yet, so setMute must NOT fire here.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(_, _)).Times(0);
	m_player->SetAudioVolume(0);
	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// Inband CC attaches before audio and applies its default cached mute.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(1, true)).Times(1);
	// The audio source follows video and inband CC, so it receives sourceId 2.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(2, true)).Times(1);
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_AUDIO]->isAttached());
}

/**
 * @test Configure_Retune_ReappliesCachedAudioVolumeToNewPipeline
 */
TEST_F(AampRialtoPlayerTest,
	Configure_Retune_ReappliesCachedAudioVolumeToNewPipeline)
{
	/**
	 * @brief A freshly (re)created pipeline defaults to full volume, so a
	 *        previously requested SetAudioVolume() value must be re-sent
	 *        to the new pipeline once Configure() recreates it.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	m_player->SetAudioVolume(40);

	ResetMockPipeline();
	EXPECT_CALL(*m_mockPipelinePtr, setVolume(DoubleEq(0.4), _, _)).Times(1);
	m_player->Configure(MakeCodecInfo(FORMAT_VIDEO_ES_H264, FORMAT_ISO_BMFF, FORMAT_INVALID),
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

// ===========================================================================
// SetVideoMute
// ===========================================================================

/**
 * @test SetVideoMute_AttachedVideo_CallsPipelineSetMute
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetVideoMute_AttachedVideo_CallsPipelineSetMute)
{
	/**
	 * @brief When the video source is attached, SetVideoMute(true) must
	 *        call m_pipeline->setMute(sourceId, true) and
	 *        SetVideoMute(false) must call setMute(sourceId, false).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	ASSERT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());

	const int32_t videoSrcId = m_mockSources[eMEDIATYPE_VIDEO]->sourceId();

	EXPECT_CALL(*m_mockPipelinePtr, setMute(videoSrcId, true)).Times(1);
	m_player->SetVideoMute(true);

	EXPECT_CALL(*m_mockPipelinePtr, setMute(videoSrcId, false)).Times(1);
	m_player->SetVideoMute(false);
}

/**
 * @test SetVideoMute_VideoNotYetAttached_MuteAppliedWhenAttached
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetVideoMute_VideoNotYetAttached_MuteAppliedWhenAttached)
{
	/**
	 * @brief SetVideoMute(true) called before the video source is attached
	 *        must cache the state and call setMute once the source
	 *        attaches (triggered here by SendVideoInitFragment).
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ASSERT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());

	// Mute before attach — must NOT trigger setMute yet.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(_, _)).Times(0);
	m_player->SetVideoMute(true);
	testing::Mock::VerifyAndClearExpectations(m_mockPipelinePtr);

	// Inband CC attaches before the cached video mute is applied.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(1, true)).Times(1);
	// The video source is attached first and receives sourceId 0.
	EXPECT_CALL(*m_mockPipelinePtr, setMute(0, true)).Times(1);
	SendVideoInitFragment();
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());
}

/**
 * @test SetVideoMute_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	SetVideoMute_NoPipeline_DoesNotCrash)
{
	/**
	 * @brief SetVideoMute called before Configure() (no pipeline) must not
	 *        crash.
	 */
	EXPECT_NO_FATAL_FAILURE(m_player->SetVideoMute(true));
}

// ===========================================================================
// GetVideoPlaybackQuality
// ===========================================================================

/**
 * @test GetVideoPlaybackQuality_Playing_ReturnsStats
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPlaybackQuality_Playing_ReturnsStats)
{
	/**
	 * @brief In PLAYING state with the video source attached,
	 *        GetVideoPlaybackQuality() must query
	 *        m_pipeline->getStats(videoSourceId, ...) and return a pointer
	 *        populated with the rendered/dropped frame counts.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	const int32_t videoSrcId = m_mockSources[eMEDIATYPE_VIDEO]->sourceId();
	EXPECT_CALL(*m_mockPipelinePtr, getStats(videoSrcId, _, _))
		.WillOnce(DoAll(SetArgReferee<1>(100), SetArgReferee<2>(5),
			Return(true)));

	PlaybackQualityStruct *result = m_player->GetVideoPlaybackQuality();

	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->rendered, 100);
	EXPECT_EQ(result->dropped, 5);
}

/**
 * @test GetVideoPlaybackQuality_Paused_ReturnsStats
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPlaybackQuality_Paused_ReturnsStats)
{
	/**
	 * @brief PAUSED (like PLAYING) is a queryable state.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);

	const int32_t videoSrcId = m_mockSources[eMEDIATYPE_VIDEO]->sourceId();
	EXPECT_CALL(*m_mockPipelinePtr, getStats(videoSrcId, _, _))
		.WillOnce(DoAll(SetArgReferee<1>(42), SetArgReferee<2>(1),
			Return(true)));

	PlaybackQualityStruct *result = m_player->GetVideoPlaybackQuality();

	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->rendered, 42);
	EXPECT_EQ(result->dropped, 1);
}

/**
 * @test GetVideoPlaybackQuality_NotPlayingOrPaused_ReturnsNullptrWithoutQuery
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPlaybackQuality_NotPlayingOrPaused_ReturnsNullptrWithoutQuery)
{
	/**
	 * @brief Outside PLAYING/PAUSED (e.g. right after Configure(), still
	 *        SOURCES_ATTACHED), the query must not be issued at all.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	ASSERT_NE(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
	ASSERT_NE(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);

	EXPECT_CALL(*m_mockPipelinePtr, getStats(_, _, _)).Times(0);

	EXPECT_EQ(m_player->GetVideoPlaybackQuality(), nullptr);
}

/**
 * @test GetVideoPlaybackQuality_VideoNotAttached_ReturnsNullptr
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPlaybackQuality_VideoNotAttached_ReturnsNullptr)
{
	/**
	 * @brief Even in PLAYING state, if the video source has not attached
	 *        there is no sourceId to query stats for.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	ASSERT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->isAttached());
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, getStats(_, _, _)).Times(0);

	EXPECT_EQ(m_player->GetVideoPlaybackQuality(), nullptr);
}

/**
 * @test GetVideoPlaybackQuality_PipelineGetStatsFails_ReturnsNullptr
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoPlaybackQuality_PipelineGetStatsFails_ReturnsNullptr)
{
	/**
	 * @brief If IMediaPipeline::getStats() itself fails, the method must
	 *        return nullptr rather than a stale/partial struct.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	EXPECT_CALL(*m_mockPipelinePtr, getStats(_, _, _)).WillOnce(Return(false));

	EXPECT_EQ(m_player->GetVideoPlaybackQuality(), nullptr);
}

/**
 * @test GetVideoPlaybackQuality_NoPipeline_DoesNotCrash
 */
TEST_F(AampRialtoPlayerTest,
	GetVideoPlaybackQuality_NoPipeline_DoesNotCrash)
{
	/**
	 * @brief Called before Configure() (no pipeline) must not crash and
	 *        must return nullptr.
	 */
	EXPECT_EQ(m_player->GetVideoPlaybackQuality(), nullptr);
}

// ===========================================================================
// Inband Closed Caption (CC) — PlayerDirectRialtoCCManager integration
// ===========================================================================

/**
 * @test After Configure(video, audio) [no subtitle], AampRialtoPlayer
 *       eagerly creates an inband-CC subtitle source at Configure() time
 *       so that the Rialto server can route closed-caption data internally.
 */
TEST_F(AampRialtoPlayerTest,
	OnPlayingState_VideoAndAudio_CreatesInbandCCSubtitleSource)
{
	// CC source is now created eagerly at Configure() time.
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	EXPECT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr)
		<< "CC source must be created at Configure() time when no subtitle "
		   "format is supplied";
}

/**
 * @test When inband CC mode is active, NotifyFirstFrameReceived() must carry a
 *       non-zero ccDecoderHandle so that PlayerDirectRialtoCCManager::Initialize()
 *       receives a valid IDirectRialtoCC pointer.
 */
TEST_F(AampRialtoPlayerTest,
	OnPlayingState_InbandCC_PassesNonZeroDecoderHandle)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	EXPECT_CALL(m_mockNotifiable,
		NotifyFirstFrameReceived(Ne(static_cast<unsigned long>(0))));

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

/**
 * @test AampRialtoPlayer implements IDirectRialtoCC; its setTextTrackIdentifier()
 *       must forward the identifier string to IMediaPipeline::setTextTrackIdentifier().
 */
TEST_F(AampRialtoPlayerTest,
	SetTextTrackIdentifier_InbandCC_ForwardsToPipeline)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	auto *cc = dynamic_cast<IDirectRialtoCC *>(m_player.get());
	ASSERT_NE(cc, nullptr) << "AampRialtoPlayer must implement IDirectRialtoCC";

	EXPECT_CALL(*m_mockPipelinePtr, setTextTrackIdentifier(std::string("CC1")))
		.WillOnce(Return(true));

	EXPECT_TRUE(cc->setTextTrackIdentifier("CC1"));
}

/**
 * @test ~AampRialtoPlayer() must invalidate any PlayerCCManager handle that
 *       still refers to this instance. In multi-pipeline mode the CC manager
 *       singleton (and its GetId()/Release() refcount) is shared across
 *       independently-lived AampRialtoPlayer instances, so it cannot be
 *       relied on to clear the handle before this object is destroyed.
 */
TEST_F(AampRialtoPlayerTest,
	Destructor_InvalidatesCCManagerHandle)
{
	g_mockPlayerCCManager = std::make_shared<StrictMock<MockPlayerCCManager>>();
	// Force the (fake) CC manager singleton to exist, mirroring production
	// where Init() is called once the first frame/decoder handle arrives.
	PlayerCCManager::GetInstance();

	auto *cc = dynamic_cast<IDirectRialtoCC *>(m_player.get());
	ASSERT_NE(cc, nullptr);
	void *expectedHandle = static_cast<void *>(cc);

	EXPECT_CALL(*g_mockPlayerCCManager, InvalidateHandle(expectedHandle)).Times(1);

	m_player.reset();

	g_mockPlayerCCManager.reset();
}

/**
 * @test Regression (H2): the pipeline must be released while the player's
 *       other members are still alive, so a notification fired during pipeline
 *       destruction is still handled by a valid player.
 */
TEST_F(AampRialtoPlayerTest, Destructor_PipelineReleasedWhilePlayerStillValid)
{
	class NotifyOnDestroyPipeline : public NiceMock<MockIMediaPipeline>
	{
	public:
		std::function<void()> onDestroy;
		~NotifyOnDestroyPipeline() override
		{
			if (onDestroy)
			{
				onDestroy();
			}
		}
	};

	auto pipeline = std::make_unique<NotifyOnDestroyPipeline>();
	auto *rawPipeline = pipeline.get();
	m_mockPipeline = std::move(pipeline);
	m_mockPipelinePtr = rawPipeline;
	ResetMockPipelineDefaults();

	Configure();
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	rawPipeline->onDestroy = [&client]()
	{
		client->notifyPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
	};

	EXPECT_CALL(m_mockNotifiable, NotifyEOSReached()).Times(1);

	m_player.reset();
}

/**
 * @test Regression (H2): once the player is destroyed, a client that is still
 *       referenced elsewhere must not call back into it.
 */
TEST_F(AampRialtoPlayerTest, Destructor_ClearsClientCallbacks)
{
	Configure();
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);

	m_player.reset();

	EXPECT_CALL(m_mockNotifiable, NotifyEOSReached()).Times(0);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
	client->notifyNeedMediaData(0, 1, 1, nullptr);
}

// ===========================================================================
// Closed-caption / setTextTrackIdentifier / setCCMute
// ===========================================================================

/**
 * @test AampRialtoPlayer::setCCMute(true) must call IMediaPipeline::setMute()
 *       with the CC subtitle source's Rialto sourceId and mute=true.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	SetCCMute_InbandCC_ForwardsPipelineSetMute)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);

	// Attach video source so the CC subtitle source is not deferred.
	SendVideoInitFragment();

	// PLAYING triggers CC subtitle source creation and attachment.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);
	ASSERT_TRUE(m_mockSources[eMEDIATYPE_SUBTITLE]->isAttached());

	auto *cc = dynamic_cast<IDirectRialtoCC *>(m_player.get());
	ASSERT_NE(cc, nullptr);

	const int32_t expectedSourceId =
		m_mockSources[eMEDIATYPE_SUBTITLE]->sourceId();
	EXPECT_CALL(*m_mockPipelinePtr, setMute(expectedSourceId, true))
		.WillOnce(Return(true));

	EXPECT_TRUE(cc->setCCMute(true));
}
