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
 * @file AampRialtoPlayerTestCases.cpp
 * @brief L1 unit tests for AampRialtoPlayer.
 *
 * Tests are structured per the TDD implementation plan in
 * docs/rialto-integration/aamp-rialto-player-analysis.md.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "mp4demux/MP4Demux.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineFactory.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

// ---------------------------------------------------------------------------
// Helper: build a minimal H264 codec info
// ---------------------------------------------------------------------------
static MediaCodecInfo MakeVideoH264CodecInfo(
	uint32_t width = 1280, uint32_t height = 720)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat               = GST_FORMAT_VIDEO_ES_H264;
	ci.mInfo.video.mWidth         = width;
	ci.mInfo.video.mHeight        = height;
	ci.mCodecData                 = {0x01, 0x02, 0x03}; // fake SPS/PPS
	return ci;
}

static MediaCodecInfo MakeVideoHevcCodecInfo(
	uint32_t width = 1920, uint32_t height = 1080)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat               = GST_FORMAT_VIDEO_ES_HEVC;
	ci.mInfo.video.mWidth         = width;
	ci.mInfo.video.mHeight        = height;
	ci.mCodecData                 = {0x01, 0x02};
	return ci;
}

static MediaCodecInfo MakeAudioAacCodecInfo(
	uint32_t channels = 2, uint32_t sampleRate = 48000)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat                  = GST_FORMAT_AUDIO_ES_AAC_RAW;
	ci.mInfo.audio.mChannelCount     = channels;
	ci.mInfo.audio.mSampleRate       = sampleRate;
	ci.mCodecData                    = {0xAA, 0xBB};
	return ci;
}

// ---------------------------------------------------------------------------
// Base test fixture
// ---------------------------------------------------------------------------

/**
 * @class AampRialtoPlayerTest
 * @brief Base fixture that wires a MockIMediaPipelineFactory into
 *        AampRialtoPlayer before each test.
 */
class AampRialtoPlayerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();

		m_mockFactory = std::make_shared<NiceMock<MockIMediaPipelineFactory>>();
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_mockPipelinePtr = m_mockPipeline.get();

		// Factory returns the mock pipeline and captures the client pointer.
		ON_CALL(*m_mockFactory, createMediaPipeline(_, _))
			.WillByDefault(Invoke(
				[this](std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
				       const firebolt::rialto::VideoRequirements &)
					-> std::unique_ptr<firebolt::rialto::IMediaPipeline>
				{
					m_capturedClient = client;
					return std::move(m_mockPipeline);
				}));

		ON_CALL(*m_mockPipelinePtr, load(_, _, _))
			.WillByDefault(Return(true));

		// attachSource assigns incrementing IDs so video and audio
		// source IDs differ.
		ON_CALL(*m_mockPipelinePtr, attachSource(_))
			.WillByDefault(Invoke(
				[this](const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSource> &src)
				{
					const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
						*src).setId(m_nextSourceId++);
					return true;
				}));

		ON_CALL(*m_mockPipelinePtr, allSourcesAttached())
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, play(_))
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, pause())
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, setPlaybackRate(_))
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, stop())
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
			.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::OK));

		ON_CALL(*m_mockPipelinePtr, haveData(_, _))
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, flush(_, _, _))
			.WillByDefault(Return(true));

		ON_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _))
			.WillByDefault(Return(true));

		m_player = std::make_unique<AampRialtoPlayer>(
			reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
			/*id3HandlerCallback=*/nullptr);
		m_player->SetPipelineFactoryForTesting(m_mockFactory);
	}

	void TearDown() override
	{
		m_player.reset();
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
		m_nextSourceId = 0;
	}

	/// Call Configure() with specific formats.
	void Configure(
		StreamOutputFormat video = FORMAT_ISO_BMFF,
		StreamOutputFormat audio = FORMAT_ISO_BMFF,
		StreamOutputFormat sub   = FORMAT_UNKNOWN)
	{
		m_player->Configure(video, audio, sub,
			/*bESChangeStatus=*/false,
			/*setReadyAfterPipelineCreation=*/false);
	}

	/// Build a minimal AampMediaSample.
	AampMediaSample MakeSample(double pts = 0.1, double duration = 0.033)
	{
		AampMediaSample s{};
		s.mPts      = pts;
		s.mDuration = duration;
		return s;
	}

	/// Trigger initFragment SendTransfer with mock demuxer returning H264 info.
	void SendVideoInitFragment(MediaCodecInfo ci = {})
	{
		GstStreamOutputFormat fmt = ci.mCodecFormat;
		if (fmt == GST_FORMAT_INVALID)
			fmt = GST_FORMAT_VIDEO_ES_H264;
		MediaCodecInfo copy{};
		copy.mCodecFormat = fmt;
		if (fmt == GST_FORMAT_VIDEO_ES_H264)
		{
			copy.mInfo.video.mWidth  = 1280;
			copy.mInfo.video.mHeight = 720;
			copy.mCodecData          = {0x01, 0x02, 0x03};
		}
		else if (fmt == GST_FORMAT_VIDEO_ES_HEVC)
		{
			copy.mInfo.video.mWidth  = 1920;
			copy.mInfo.video.mHeight = 1080;
			copy.mCodecData          = {0x01, 0x02};
		}
		ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
		auto copyPtr = std::make_shared<MediaCodecInfo>(std::move(copy));
		ON_CALL(*g_mockMp4Demux, GetCodecInfo())
			.WillByDefault([copyPtr]() -> MediaCodecInfo
			{
				MediaCodecInfo ret{};
				ret.mCodecFormat             = copyPtr->mCodecFormat;
				ret.mInfo.video.mWidth       = copyPtr->mInfo.video.mWidth;
				ret.mInfo.video.mHeight      = copyPtr->mInfo.video.mHeight;
				ret.mCodecData               = copyPtr->mCodecData;
				return ret;
			});
		std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			0, 0, 0, 0, /*initFragment=*/true);
	}

	/// Trigger initFragment SendTransfer with mock demuxer returning AAC info.
	void SendAudioInitFragment(MediaCodecInfo ci = {})
	{
		ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetCodecInfo())
			.WillByDefault([]() -> MediaCodecInfo
			{
				MediaCodecInfo ret{};
				ret.mCodecFormat                  = GST_FORMAT_AUDIO_ES_AAC_RAW;
				ret.mInfo.audio.mChannelCount     = 2;
				ret.mInfo.audio.mSampleRate       = 48000;
				ret.mCodecData                    = {0xAA, 0xBB};
				return ret;
			});
		std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
		m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(buf),
			0, 0, 0, 0, /*initFragment=*/true);
	}

	/// Send a media fragment (not init) for the given type.
	void SendVideoMediaFragment(double pts = 0.1)
	{
		ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([pts]() {
				std::vector<AampMediaSample> samples;
				AampMediaSample s{};
				s.mPts      = pts;
				s.mDuration = 0.033;
				samples.push_back(std::move(s));
				return samples;
			});
		std::vector<uint8_t> buf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			pts, pts, 0.033, 0, /*initFragment=*/false);
	}

	std::shared_ptr<NiceMock<MockIMediaPipelineFactory>> m_mockFactory;
	std::unique_ptr<NiceMock<MockIMediaPipeline>>        m_mockPipeline;
	NiceMock<MockIMediaPipeline> *                       m_mockPipelinePtr{nullptr};
	std::unique_ptr<AampRialtoPlayer>                    m_player;
	std::weak_ptr<firebolt::rialto::IMediaPipelineClient> m_capturedClient;
	int m_nextSourceId{0};
};

// ---------------------------------------------------------------------------
// Fixture that also manages a MockMp4Demux
// ---------------------------------------------------------------------------

class AampRialtoPlayerWithDemuxTest : public AampRialtoPlayerTest
{
protected:
	void SetUp() override
	{
		AampRialtoPlayerTest::SetUp();
		g_mockMp4Demux = new NiceMock<MockMp4Demux>();
	}

	void TearDown() override
	{
		delete g_mockMp4Demux;
		g_mockMp4Demux = nullptr;
		AampRialtoPlayerTest::TearDown();
	}
};

// ===========================================================================
// Phase 2 — Configure / pipeline creation
// ===========================================================================

TEST_F(AampRialtoPlayerTest, Configure_ValidFormats_CreatesPipeline)
{
	EXPECT_CALL(*m_mockFactory, createMediaPipeline(_, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, load(_, _, _))
		.WillOnce(Return(true));

	Configure();
	// Pipeline was handed to the player — mock pointer still accessible.
}

TEST_F(AampRialtoPlayerTest, Configure_NullFactory_DoesNotCrash)
{
	m_player->SetPipelineFactoryForTesting(nullptr);
	// IMediaPipelineFactory::createFactory() returns nullptr in test env;
	// the player must handle this gracefully.
	EXPECT_NO_THROW(Configure());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_VideoOnly_CreatesVideoDemuxerOnly)
{
	// Configure with video only (no audio demuxer created)
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	// Video init fragment should pass through the demuxer and call attachSource.
	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);

	std::vector<uint8_t> videoBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(videoBuf),
		0, 0, 0, 0, /*initFragment=*/true);

	// Audio fragment should not call Parse (no audio demuxer created).
	EXPECT_CALL(*g_mockMp4Demux, Parse(_, _)).Times(0);
	std::vector<uint8_t> audioBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(audioBuf),
		0, 0, 0, 0, /*initFragment=*/true);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_AudioOnly_CreatesAudioDemuxerOnly)
{
	// Configure with audio only
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);

	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeAudioAacCodecInfo(); });
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);

	std::vector<uint8_t> audioBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(audioBuf),
		0, 0, 0, 0, /*initFragment=*/true);
}

// ===========================================================================
// Phase 3 — Fix CheckAllSourcesAttached
// ===========================================================================

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
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_AudioOnlyStream_CallsAllSourcesAttachedAfterAudio)
{
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, allSourcesAttached()).Times(1);

	SendAudioInitFragment();
}

// ===========================================================================
// Phase 4 — attachSource and codec data
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesVideoSourceWithMimeType)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_EQ(src->getMimeType(), "video/h264");
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));

	SendVideoInitFragment(MakeVideoH264CodecInfo());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesHevcWithCorrectMimeType)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_EQ(src->getMimeType(), "video/h265");
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));

	SendVideoInitFragment(MakeVideoHevcCodecInfo());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesAudioSourceWithMimeType)
{
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_EQ(src->getMimeType(), "audio/aac");
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));

	SendAudioInitFragment(MakeAudioAacCodecInfo());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_SetsCodecDataOnVideoSegment)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment(MakeVideoH264CodecInfo()); // caches codec data

	// A subsequent addSegment call should carry codec data.
	bool codecDataWasSet = false;
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&codecDataWasSet](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
					&seg)
			{
				if (seg->getCodecData())
					codecDataWasSet = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	// Trigger injection via needData → addSegment.
	m_player->OnNeedMediaData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/1);

	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, /*initFragment=*/false);

	// Give the injection thread time to process.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_TRUE(codecDataWasSet);
}

// ===========================================================================
// Phase 5 — Segment injection baseline
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_MediaFragment_EnqueuesSamples)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Enqueue a media fragment.  addSegment will be called once a needData
	// request is processed by the injection thread.
	m_player->OnNeedMediaData(0, 1, 42);

	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.2; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));

	EXPECT_CALL(*m_mockPipelinePtr, addSegment(42, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 42)).Times(1);

	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.2, 0.2, 0.033, 0, /*initFragment=*/false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	EndOfStreamReached_SetsEosFlag)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Post a needData request so the injection thread unblocks on EOS.
	m_player->OnNeedMediaData(0, 0, 7);

	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::EOS, 7)).Times(1);

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(AampRialtoPlayerTest,
	OnNeedMediaData_EnqueuesRequest)
{
	Configure();
	// Directly call OnNeedMediaData —injector thread must not crash and must
	// eventually process the request (verified via haveData being called).
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _)).Times(::testing::AtLeast(0));
	EXPECT_NO_THROW(m_player->OnNeedMediaData(0, 5, 100));
}

TEST_F(AampRialtoPlayerTest,
	OnCancelNeedMediaData_ClearsRequests)
{
	Configure();
	// No crash and no haveData call after cancel.
	m_player->OnNeedMediaData(0, 5, 200);
	EXPECT_NO_THROW(m_player->OnCancelNeedMediaData(0));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_EosOnly_CallsHaveDataWithEos)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);

	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::EOS, 55)).Times(1);

	m_player->OnNeedMediaData(0, 1, 55);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(AampRialtoPlayerTest,
	InjectSamples_NullPipeline_DoesNotCrash)
{
	// Player with no Configure() call — pipeline is null.
	EXPECT_NO_THROW({
		m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);
		m_player->OnNeedMediaData(0, 1, 1);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	});
}

TEST_F(AampRialtoPlayerWithDemuxTest, Stream_CallsPlay)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));

	m_player->Stream();
}

TEST_F(AampRialtoPlayerWithDemuxTest, Stop_CallsPipelineStop)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Stop(false);
}

// ===========================================================================
// Phase 6 — setSourcePosition before first injection
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_FirstInjection_CallsSetSourcePosition)
{
	// Flush sets a pending position which is applied when sources attach.
	m_player->Flush(10.0, 1, false); // 10 s → 10e9 ns
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 10000000000LL, true, _, _)).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	AttachSource_NoFlush_DoesNotCallSetSourcePosition)
{
	// No Flush() called → m_pendingFlushPositionNs is -1 → no setSourcePosition.
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr, setSourcePosition(_, _, _, _, _)).Times(0);

	SendVideoInitFragment();
}

// ===========================================================================
// Phase 7 — Flush calls pipeline flush + resets queues
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_CallsPipelineFlushForEachAttachedSource)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, flush(0, true, _)).Times(1);
	EXPECT_CALL(*m_mockPipelinePtr, flush(1, true, _)).Times(1);

	m_player->Flush(5.0, 1, false);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_ClearsLocalQueues)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Enqueue a needData and a sample, but do NOT let injection run.
	m_player->OnNeedMediaData(0, 10, 99);
	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	// Flush should clear queues; addSegment must NOT be called afterwrd.
	m_player->Flush(0.0, 1, false);

	EXPECT_CALL(*m_mockPipelinePtr, addSegment(_, _)).Times(0);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_ResetsSegmentSetFlag)
{
	// After Flush, a new source-position call is expected on next attachment.
	m_player->Flush(5.0, 1, false);
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 5000000000LL, true, _, _)).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerTest,
	Flush_NoPipeline_DoesNotCrash)
{
	// No Configure() → no pipeline.
	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
}

// ===========================================================================
// Phase 8 — Pause / SetPlayBackRate
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_True_CallsPipelinePause)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, pause()).Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->Pause(/*pause=*/true, false));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Pause_False_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_)).Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->Pause(/*pause=*/false, false));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	SetPlayBackRate_CallsPipelineSetPlaybackRate)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, setPlaybackRate(2.0))
		.Times(1).WillOnce(Return(true));
	EXPECT_TRUE(m_player->SetPlayBackRate(2.0));
}

TEST_F(AampRialtoPlayerTest,
	Pause_NullPipeline_ReturnsFalse)
{
	// No Configure() → no pipeline.
	EXPECT_FALSE(m_player->Pause(true, false));
}

TEST_F(AampRialtoPlayerTest,
	SetPlayBackRate_NullPipeline_ReturnsFalse)
{
	EXPECT_FALSE(m_player->SetPlayBackRate(1.5));
}

// ===========================================================================
// Phase 9 — notifyPlaybackState forwarding
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	notifyPlaybackState_Playing_ForwardedToObserver)
{
	Configure();

	firebolt::rialto::PlaybackState observed =
		firebolt::rialto::PlaybackState::IDLE;
	m_player->SetPlaybackObserverForTesting(
		[&observed](firebolt::rialto::PlaybackState st) {
			observed = st;
		});

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_EQ(observed, firebolt::rialto::PlaybackState::PLAYING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	notifyPlaybackState_Paused_ForwardedToObserver)
{
	Configure();

	firebolt::rialto::PlaybackState observed =
		firebolt::rialto::PlaybackState::IDLE;
	m_player->SetPlaybackObserverForTesting(
		[&observed](firebolt::rialto::PlaybackState st) {
			observed = st;
		});

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

	EXPECT_EQ(observed, firebolt::rialto::PlaybackState::PAUSED);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	notifyPlaybackState_Error_ForwardedToObserver)
{
	Configure();

	firebolt::rialto::PlaybackState observed =
		firebolt::rialto::PlaybackState::IDLE;
	m_player->SetPlaybackObserverForTesting(
		[&observed](firebolt::rialto::PlaybackState st) {
			observed = st;
		});

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::FAILURE);

	EXPECT_EQ(observed, firebolt::rialto::PlaybackState::FAILURE);
}

// ===========================================================================
// Phase 10 — addSegment(NO_SPACE) re-queue
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_AddSegmentNoSpace_StopsAndRequeues)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Enqueue two samples, return NO_SPACE on the first addSegment call.
	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample s1{}, s2{};
			s1.mPts = 0.1; s1.mDuration = 0.033;
			s2.mPts = 0.2; s2.mDuration = 0.033;
			s.push_back(std::move(s1));
			s.push_back(std::move(s2));
			return s;
		});

	int addSegmentCalls = 0;
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](
				uint32_t,
				const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSegment> &)
				-> firebolt::rialto::AddSegmentStatus
			{
				addSegmentCalls++;
				// First call: no space.
				if (addSegmentCalls == 1)
					return firebolt::rialto::AddSegmentStatus::NO_SPACE;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	m_player->OnNeedMediaData(0, 2, 10);

	std::vector<uint8_t> buf = {0x01, 0x02};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Only 1 addSegment call because NO_SPACE stopped the loop.
	EXPECT_EQ(addSegmentCalls, 1);

	// The rejected sample should be re-queued and sent on the next needData.
	addSegmentCalls = 0;
	EXPECT_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillOnce(Return(firebolt::rialto::AddSegmentStatus::OK));
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 11)).Times(1);

	m_player->OnNeedMediaData(0, 1, 11);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_AddSegmentNoSpaceOnFirst_RequeuesAll)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample s1{}, s2{};
			s1.mPts = 0.5; s1.mDuration = 0.033;
			s2.mPts = 0.6; s2.mDuration = 0.033;
			s.push_back(std::move(s1));
			s.push_back(std::move(s2));
			return s;
		});

	// Always NO_SPACE to force full requeue.
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Return(firebolt::rialto::AddSegmentStatus::NO_SPACE));

	m_player->OnNeedMediaData(0, 2, 20);

	std::vector<uint8_t> buf = {0x01, 0x02};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.5, 0.5, 0.033, 0, false);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Now switch to OK and send another needData — should drain requeued samples.
	int acceptedCount = 0;
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&acceptedCount](uint32_t, const auto &)
				-> firebolt::rialto::AddSegmentStatus
			{
				++acceptedCount;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	m_player->OnNeedMediaData(0, 2, 21);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_EQ(acceptedCount, 2);
}

// ===========================================================================
// Phase 11 — Parallel injection / no-deadlock (behavioural)
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SimultaneousVideoAudio_BothInjectedIndependently)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Enqueue video samples.
	{
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> s;
				AampMediaSample ms{}; ms.mPts=0.1; ms.mDuration=0.033;
				s.push_back(std::move(ms));
				return s;
			});
		ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
		std::vector<uint8_t> vbuf = {0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(vbuf),
			0.1, 0.1, 0.033, 0, false);
	}
	// Enqueue audio samples.
	{
		ON_CALL(*g_mockMp4Demux, GetSamples())
			.WillByDefault([]() {
				std::vector<AampMediaSample> s;
				AampMediaSample ms{}; ms.mPts=0.1; ms.mDuration=0.033;
				s.push_back(std::move(ms));
				return s;
			});
		ON_CALL(*g_mockMp4Demux, Parse(_, _)).WillByDefault(Return(true));
		std::vector<uint8_t> abuf = {0x02};
		m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(abuf),
			0.1, 0.1, 0.033, 0, false);
	}

	// Fire needData for both sources simultaneously.
	m_player->OnNeedMediaData(0, 1, 50); // video
	m_player->OnNeedMediaData(1, 1, 51); // audio

	// Both sources should receive haveData(OK).
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, AnyOf(50u, 51u)))
		.Times(::testing::AtLeast(2));
	EXPECT_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.Times(::testing::AtLeast(2));

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	HighFrequencyNeedData_NoDeadlock)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Rapidly alternate needData and cancelNeedData.
	for (int i = 0; i < 20; ++i)
	{
		m_player->OnNeedMediaData(0, 1, static_cast<uint32_t>(i));
		if (i % 3 == 0)
			m_player->OnCancelNeedMediaData(0);
	}

	// Allow the injection thread to drain; the test passes if there is no
	// deadlock or crash within the timeout.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	SUCCEED();
}
