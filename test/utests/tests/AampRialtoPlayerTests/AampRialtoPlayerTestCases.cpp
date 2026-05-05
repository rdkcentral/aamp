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
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "mp4demux/MP4Demux.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineFactory.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"
#include "MockDrmBridge.h"
#include "MockIStreamSinkNotifiable.h"
#include "MockIRialtoControlBackend.h"

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

		// Wire the factory global before constructing the player so that
		// IMediaPipelineFactory::createFactory() returns the mock.
		g_mockPipelineFactory = m_mockFactory;

		// Create a NiceMock control backend and keep a raw pointer for
		// assertions in individual tests.
		auto controlBackend =
				std::make_unique<NiceMock<MockIRialtoControlBackend>>();
		m_mockControlBackend = controlBackend.get();
		ON_CALL(*m_mockControlBackend, waitForRunning(_))
				.WillByDefault(Return(true));

		m_player = std::make_unique<AampRialtoPlayer>(
				reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
				&m_mockNotifiable,
				std::unique_ptr<IRialtoControlBackend>(std::move(controlBackend)),
				/*id3HandlerCallback=*/nullptr);
	}

	void TearDown() override
	{
		m_player.reset();
		g_mockPipelineFactory = nullptr;
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
		m_nextSourceId = 0;
	}

	/// Post a playback-state notification via the captured client.
	void PostPlaybackState(firebolt::rialto::PlaybackState state)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr)
			<< "Configure() must be called before PostPlaybackState";
		client->notifyPlaybackState(state);
	}

	/// Post a position notification (nanoseconds) via the captured client.
	void PostPosition(int64_t positionNs)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr)
			<< "Configure() must be called before PostPosition";
		client->notifyPosition(positionNs);
	}

	/// Post a duration notification (nanoseconds) via the captured client.
	void PostDuration(int64_t durationNs)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr)
			<< "Configure() must be called before PostDuration";
		client->notifyDuration(durationNs);
	}

	/// Post a needData event via the real IPC callback path.
	void PostNeedData(int32_t sourceId, size_t frameCount, uint32_t requestId)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr) << "Configure() must be called before PostNeedData";
		client->notifyNeedMediaData(sourceId, frameCount, requestId, nullptr);
	}

	/// Cancel pending needData requests via the real IPC callback path.
	void CancelNeedData(int32_t sourceId)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr) << "Configure() must be called before CancelNeedData";
		client->notifyCancelNeedMediaData(sourceId);
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

	/// Spin-poll until condition() returns true or the timeout elapses.
	/// Pass []{ return false; } with an explicit timeout for tests that have
	/// no positive completion signal (e.g. negative/no-crash tests).
	static void WaitFor(
		const std::function<bool()> &condition,
		std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
	{
		const auto deadline =
			std::chrono::steady_clock::now() + timeout;
		while (!condition() && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
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
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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
	NiceMock<MockIStreamSinkNotifiable>                  m_mockNotifiable;
	MockIRialtoControlBackend *                          m_mockControlBackend{nullptr};
	int32_t                                              m_nextSourceId{0};
};

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
	// Temporarily null the global so createFactory() returns nullptr.
	// The player must handle a null factory gracefully.
	g_mockPipelineFactory = nullptr;
	m_player.reset();
	m_player = std::make_unique<AampRialtoPlayer>(
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
		&m_mockNotifiable,
		std::unique_ptr<IRialtoControlBackend>(nullptr),
		/*id3HandlerCallback=*/nullptr);
	EXPECT_NO_THROW(
		m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_UNKNOWN, false, false));
	// Restore so subsequent tests in this fixture are not affected.
	g_mockPipelineFactory = m_mockFactory;
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_VideoOnly_CreatesVideoDemuxerOnly)
{
	// Configure with video only (no audio demuxer created)
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	// Video init fragment should pass through the demuxer and call attachSource.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);

	std::vector<uint8_t> videoBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(videoBuf),
		0, 0, 0, 0, /*initFragment=*/true);

	// Audio fragment should not call Parse (no audio demuxer created).
	EXPECT_CALL(*g_mockMp4Demux, Parse(_)).Times(0);
	std::vector<uint8_t> audioBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(audioBuf),
		0, 0, 0, 0, /*initFragment=*/true);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Configure_AudioOnly_CreatesAudioDemuxerOnly)
{
	// Configure with audio only
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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
	std::atomic<bool> codecDataWasSet{false};
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
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/1);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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

	WaitFor([&codecDataWasSet]{ return codecDataWasSet.load(); });

	EXPECT_TRUE(codecDataWasSet.load());
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

TEST_F(AampRialtoPlayerWithDemuxTest,
	EndOfStreamReached_SetsEosFlag)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Post a needData request so the injection thread unblocks on EOS.
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

TEST_F(AampRialtoPlayerTest,
	OnNeedMediaData_EnqueuesRequest)
{
	Configure();
	// Trigger via the real IPC callback path — injector thread must not crash.
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _)).Times(::testing::AtLeast(0));
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	EXPECT_NO_THROW(client->notifyNeedMediaData(0, 5, 100, nullptr));
}

TEST_F(AampRialtoPlayerTest,
	OnCancelNeedMediaData_ClearsRequests)
{
	Configure();
	// No crash and no haveData call after cancel.
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyNeedMediaData(0, 5, 200, nullptr);
	EXPECT_NO_THROW(client->notifyCancelNeedMediaData(0));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_EosOnly_CallsHaveDataWithEos)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

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

TEST_F(AampRialtoPlayerTest,
	InjectSamples_NullPipeline_DoesNotCrash)
{
	// Temporarily make createMediaPipeline return nullptr so the player has
	// no pipeline.  InjectSamples must not crash with m_pipeline == nullptr.
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
	PostNeedData(0, 10, 99);
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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
	WaitFor([]{ return false; }, std::chrono::milliseconds(30));
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
	notifyPlaybackState_Playing_TransitionsStateMachine)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	// State machine should now be in SOURCES_ATTACHED after allSourcesAttached().

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	notifyPlaybackState_Paused_TransitionsStateMachine)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	notifyPlaybackState_Error_TransitionsStateMachine)
{
	Configure();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::FAILURE);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::ERROR);
}

// ===========================================================================
// Phase 10 — addSegment(NO_SPACE) re-queue
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_AddSegmentNoSpace_StopsAndRequeues)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Two samples per fragment; the first addSegment call returns NO_SPACE,
	// closing out the current needData.  SendTransfer then blocks waiting
	// for the next needData and retries the same sample on the second pass.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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

	std::atomic<int> addSegmentCalls{0};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](
				uint32_t,
				const std::unique_ptr<
					firebolt::rialto::IMediaPipeline::MediaSegment> &)
				-> firebolt::rialto::AddSegmentStatus
			{
				const int call = ++addSegmentCalls;
				// First call: no space.  All subsequent calls: OK.
				return (call == 1)
					? firebolt::rialto::AddSegmentStatus::NO_SPACE
					: firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 2, 10);

	// SendTransfer blocks once it hits NO_SPACE; run it in a worker thread.
	std::vector<uint8_t> buf = {0x01, 0x02};
	std::thread sender([this, b = std::move(buf)]() mutable {
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
	});

	WaitFor([&addSegmentCalls]{ return addSegmentCalls.load() >= 1; });
	EXPECT_EQ(addSegmentCalls.load(), 1);

	// A new needData arrives; SendTransfer retries the rejected sample
	// and then injects the second sample.
	PostNeedData(0, 2, 11);
	WaitFor([&addSegmentCalls]{ return addSegmentCalls.load() >= 3; });
	EXPECT_GE(addSegmentCalls.load(), 3);

	sender.join();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_AddSegmentNoSpaceOnFirst_RequeuesAll)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
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

	// First addSegment call returns NO_SPACE (atomic toggle); all subsequent
	// calls return OK so the second needData drains the rejected samples.
	std::atomic<int> addSegmentCalls{0};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](uint32_t, const auto &)
				-> firebolt::rialto::AddSegmentStatus
			{
				const int call = ++addSegmentCalls;
				return (call == 1)
					? firebolt::rialto::AddSegmentStatus::NO_SPACE
					: firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 2, 20);

	std::vector<uint8_t> buf = {0x01, 0x02};
	std::thread sender([this, b = std::move(buf)]() mutable {
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(b),
			0.5, 0.5, 0.033, 0, false);
	});

	// Wait for the NO_SPACE attempt.
	WaitFor([&addSegmentCalls]{ return addSegmentCalls.load() >= 1; });

	// Second needData drains the rejected sample plus the remaining one.
	PostNeedData(0, 2, 21);
	WaitFor([&addSegmentCalls]{ return addSegmentCalls.load() >= 3; });
	EXPECT_GE(addSegmentCalls.load(), 3);

	sender.join();
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

	std::atomic<int> haveDataCount{0};
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, AnyOf(50u, 51u)))
		.Times(::testing::AtLeast(2))
		.WillRepeatedly(DoAll(
			Invoke([&haveDataCount](auto, auto){ ++haveDataCount; }),
			Return(true)));
	EXPECT_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.Times(::testing::AtLeast(2));

	// Pre-arm needData for both sources so SendTransfer does not block.
	PostNeedData(0, 1, 50); // video
	PostNeedData(1, 1, 51); // audio

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{}; ms.mPts=0.1; ms.mDuration=0.033;
			s.push_back(std::move(ms));
			return s;
		});

	std::vector<uint8_t> vbuf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(vbuf),
		0.1, 0.1, 0.033, 0, false);
	std::vector<uint8_t> abuf = {0x02};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(abuf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&haveDataCount]{ return haveDataCount.load() >= 2; });
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	HighFrequencyNeedData_NoDeadlock)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();

	// Rapidly alternate needData and cancelNeedData.
	for (int i = 0; i < 20; ++i)
	{
		PostNeedData(0, 1, static_cast<uint32_t>(i));
		if (i % 3 == 0)
			CancelNeedData(0);
	}

	// Allow the injection thread to drain; the test passes if there is no
	// deadlock or crash within the timeout.
	WaitFor([]{ return false; }, std::chrono::milliseconds(100));
	SUCCEED();
}

// ===========================================================================
// Phase 12 — Per-segment codec data (mid-stream codec change)
// ===========================================================================

// Verifies that codec data cached during AttachVideoSource is forwarded on
// every subsequent MediaSegmentVideo via setCodecData().
TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_VideoSegment_CarriesCodecDataFromInitFragment)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	// Init fragment supplies codec data {0x01, 0x02, 0x03}.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	std::vector<uint8_t> initBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(initBuf),
		0, 0, 0, 0, /*initFragment=*/true);

	// Collect codec data seen on each addSegment call.
	std::vector<std::shared_ptr<const firebolt::rialto::CodecData>> capturedCodecData;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedCodecData, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedCodecData.push_back(seg->getCodecData());
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/1);

	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> mediaBuf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(mediaBuf),
		0.1, 0.1, 0.033, 0, /*initFragment=*/false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	ASSERT_EQ(capturedCodecData.size(), 1u);
	ASSERT_NE(capturedCodecData[0], nullptr);
	EXPECT_EQ(capturedCodecData[0]->data,
		(std::vector<uint8_t>{0x01, 0x02, 0x03}));
}

// Verifies that codec data cached during AttachAudioSource is forwarded on
// every subsequent MediaSegmentAudio via setCodecData().
TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_AudioSegment_CarriesCodecDataFromInitFragment)
{
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeAudioAacCodecInfo(); });
	std::vector<uint8_t> initBuf = {0x00};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(initBuf),
		0, 0, 0, 0, /*initFragment=*/true);

	std::vector<std::shared_ptr<const firebolt::rialto::CodecData>> capturedCodecData;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedCodecData, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedCodecData.push_back(seg->getCodecData());
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/2);

	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> mediaBuf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(mediaBuf),
		0.1, 0.1, 0.033, 0, /*initFragment=*/false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	ASSERT_EQ(capturedCodecData.size(), 1u);
	ASSERT_NE(capturedCodecData[0], nullptr);
	EXPECT_EQ(capturedCodecData[0]->data,
		(std::vector<uint8_t>{0xAA, 0xBB}));
}

// Verifies that when a second init fragment arrives for a source that is
// already attached (mid-stream codec change), the new codec data replaces the
// cached value and is forwarded on all subsequent segments.  attachSource()
// must NOT be called a second time.
TEST_F(AampRialtoPlayerWithDemuxTest,
	InitFragment_SecondVideoInit_UpdatesCodecDataWithoutReattaching)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);

	// First init fragment — H264 with codec data {0x01, 0x02, 0x03}.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);
	std::vector<uint8_t> init1 = {0x00};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(init1),
		0, 0, 0, 0, /*initFragment=*/true);

	// Second init fragment — HEVC with different codec data {0x01, 0x02}.
	// attachSource() must NOT be called again.
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoHevcCodecInfo(); });
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(0);
	std::vector<uint8_t> init2 = {0x00};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(init2),
		0, 0, 0, 0, /*initFragment=*/true);

	// The next injected segment must carry the new HEVC codec data.
	std::vector<std::shared_ptr<const firebolt::rialto::CodecData>> capturedCodecData;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedCodecData, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedCodecData.push_back(seg->getCodecData());
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/3);

	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.2; ms.mDuration = 0.033;
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> mediaBuf = {0x02};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(mediaBuf),
		0.2, 0.2, 0.033, 0, /*initFragment=*/false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	ASSERT_EQ(capturedCodecData.size(), 1u);
	ASSERT_NE(capturedCodecData[0], nullptr);
	// Must be the HEVC codec data from the second init fragment.
	EXPECT_EQ(capturedCodecData[0]->data,
		(std::vector<uint8_t>{0x01, 0x02}));
}

// ===========================================================================
// Phase DRM — QueueProtectionEvent / ClearProtectionEvent / segment encryption
// ===========================================================================

/**
 * @brief Fixture that adds a MockDrmBridge on top of the demux fixture.
 */
class AampRialtoPlayerDrmTest : public AampRialtoPlayerWithDemuxTest
{
protected:
	void SetUp() override
	{
		// Set the global before calling base SetUp so the player is constructed
		// while g_mockDrmBridge is already active.  The fake AampDrmBridge will
		// delegate all calls to the mock from the first createSession() onwards.
		m_mockDrmBridge = std::make_unique<NiceMock<MockDrmBridge>>();
		g_mockDrmBridge = m_mockDrmBridge.get();
		AampRialtoPlayerWithDemuxTest::SetUp();
	}

	void TearDown() override
	{
		AampRialtoPlayerWithDemuxTest::TearDown();
		g_mockDrmBridge = nullptr;
		m_mockDrmBridge.reset();
	}

	/// Build an encrypted AampMediaSample with CENC cipher.
	AampMediaSample MakeEncryptedSample(
		double pts = 0.1,
		double duration = 0.033,
		const std::vector<uint8_t> &keyId = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
		                                     0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10},
		const std::vector<uint8_t> &iv = {0xAA,0xBB,0xCC,0xDD,
		                                  0xEE,0xFF,0x00,0x11,
		                                  0x22,0x33,0x44,0x55,
		                                  0x66,0x77,0x88,0x99})
	{
		AampMediaSample s{};
		s.mPts      = pts;
		s.mDuration = duration;
		s.mDrmMetadata.mIsEncrypted    = true;
		s.mDrmMetadata.mKeyId          = keyId;
		s.mDrmMetadata.mIV             = iv;
		s.mDrmMetadata.mCipher         = CIPHER_TYPE_CENC;
		s.mDrmMetadata.mNumSubSamples  = 0;
		return s;
	}

	std::unique_ptr<NiceMock<MockDrmBridge>> m_mockDrmBridge;
};

// ---------------------------------------------------------------------------
// QueueProtectionEvent
// ---------------------------------------------------------------------------

/**
 * @test QueueProtectionEvent for video stores the protection parameters
 *       without immediately calling IDrmBridge::createSession.
 */
TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_Video_StoresParamsWithoutCallingBridge)
{
	const char *systemId = "com.widevine.alpha";
	const uint8_t initData[] = {0x01, 0x02, 0x03};
	const size_t  initLen    = sizeof(initData);

	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);

	m_player->QueueProtectionEvent(
		systemId, initData, initLen, eMEDIATYPE_VIDEO);
}

/**
 * @test QueueProtectionEvent for audio stores the protection parameters
 *       without immediately calling IDrmBridge::createSession.
 */
TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_Audio_StoresParamsWithoutCallingBridge)
{
	const char *systemId = "com.widevine.alpha";
	const uint8_t initData[] = {0xAA, 0xBB};
	const size_t  initLen    = sizeof(initData);

	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);

	m_player->QueueProtectionEvent(
		systemId, initData, initLen, eMEDIATYPE_AUDIO);
}

/**
 * @test QueueProtectionEvent with null initData must not call createSession.
 */
TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_NullInitData_DoesNotCallBridge)
{
	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", nullptr, 0, eMEDIATYPE_VIDEO);
}

// ---------------------------------------------------------------------------
// ClearProtectionEvent
// ---------------------------------------------------------------------------

/**
 * @test ClearProtectionEvent calls IDrmBridge::clearSessions().
 */
TEST_F(AampRialtoPlayerDrmTest,
	ClearProtectionEvent_CallsClearSessions)
{
	EXPECT_CALL(*m_mockDrmBridge, clearSessions()).Times(1);
	m_player->ClearProtectionEvent();
}

/**
 * @test After ClearProtectionEvent, hasDrm=false is passed to the next
 *       attachSource call (mks_id reset to -1).
 */
TEST_F(AampRialtoPlayerDrmTest,
	ClearProtectionEvent_ResetsHasDrmOnNextAttach)
{
	const uint8_t initData[] = {0x01};
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(5));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// Clear resets mks_id.
	ON_CALL(*m_mockDrmBridge, clearSessions()).WillByDefault(Return());
	m_player->ClearProtectionEvent();

	// Now configure and attach — hasDrm must be false.
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_FALSE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

// ---------------------------------------------------------------------------
// attachSource hasDrm
// ---------------------------------------------------------------------------

/**
 * @test When QueueProtectionEvent returns a valid mks_id, attachSource is
 *       called with hasDrm=true for the video source.
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithValidMksId_AttachesWithHasDrmTrue)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// createSession is deferred: it must be called when the init fragment
	// arrives (i.e. inside AttachVideoSource), not during QueueProtectionEvent.
	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(10));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

/**
 * @test Without QueueProtectionEvent, attachSource is called with hasDrm=false.
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithoutMksId_AttachesWithHasDrmFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_FALSE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

/**
 * @test QueueProtectionEvent for audio makes attachSource set hasDrm=true
 *       on the audio source.
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachAudioSource_WithValidMksId_AttachesWithHasDrmTrue)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_AUDIO);

	// createSession is deferred: it must be called when the init fragment
	// arrives (i.e. inside AttachAudioSource), not during QueueProtectionEvent.
	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_AUDIO))
		.WillOnce(Return(3));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendAudioInitFragment();
}

// ---------------------------------------------------------------------------
// InjectSamples — encryption metadata on addSegment
// ---------------------------------------------------------------------------

/**
 * @test An encrypted video sample causes the MediaSegment to have
 *       setEncrypted(true) and the correct mks_id set.
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_EncryptedVideoSample_SetsEncryptedAndMksId)
{
	const int32_t kMksId = 99;
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(kMksId));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	// Capture the segment injected by the injection thread.
	std::atomic<bool> encryptedSet{false};
	std::atomic<int32_t> capturedMksId{-1};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&encryptedSet, &capturedMksId](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				encryptedSet  = seg->isEncrypted();
				capturedMksId = seg->getMediaKeySessionId();
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	// Inject a needData request then an encrypted sample.
	PostNeedData(/*sourceId=*/0, /*frameCount=*/1, /*requestId=*/5);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([this]() {
			std::vector<AampMediaSample> s;
			s.push_back(MakeEncryptedSample());
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, /*initFragment=*/false);

	WaitFor([&capturedMksId]{ return capturedMksId.load() != -1; });

	EXPECT_TRUE(encryptedSet.load());
	EXPECT_EQ(capturedMksId.load(), kMksId);
}

/**
 * @test An encrypted sample carries the correct Key ID and IV on the segment.
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_EncryptedSample_SetsKeyIdAndIv)
{
	const int32_t kMksId = 3;
	const std::vector<uint8_t> kKeyId = {0x11,0x22,0x33,0x44,
	                                      0x55,0x66,0x77,0x88,
	                                      0x99,0xaa,0xbb,0xcc,
	                                      0xdd,0xee,0xff,0x00};
	const std::vector<uint8_t> kIv    = {0xA1,0xB2,0xC3,0xD4,
	                                      0xE5,0xF6,0x07,0x18,
	                                      0x29,0x3a,0x4b,0x5c,
	                                      0x6d,0x7e,0x8f,0x90};

	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(kMksId));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	std::vector<uint8_t> capturedKeyId;
	std::vector<uint8_t> capturedIv;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedKeyId, &capturedIv, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedKeyId = seg->getKeyId();
				capturedIv    = seg->getInitVector();
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 1, 6);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([this, kKeyId, kIv]() {
			std::vector<AampMediaSample> s;
			s.push_back(MakeEncryptedSample(0.1, 0.033, kKeyId, kIv));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, /*initFragment=*/false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	EXPECT_EQ(capturedKeyId, kKeyId);
	EXPECT_EQ(capturedIv, kIv);
}

/**
 * @test A clear (non-encrypted) sample does NOT set encrypted on the segment,
 *       even when a DRM session is active.
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_ClearSample_DoesNotSetEncrypted)
{
	const int32_t kMksId = 5;
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(kMksId));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	std::atomic<bool> segmentInjected{false};
	bool encryptedSet = false;
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&encryptedSet, &segmentInjected](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				encryptedSet     = seg->isEncrypted();
				segmentInjected  = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 1, 8);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			// mIsEncrypted defaults to false
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&segmentInjected]{ return segmentInjected.load(); });

	EXPECT_FALSE(encryptedSet);
}

/**
 * @test An encrypted CBCS sample causes setEncryptionPattern to be called
 *       with the crypt/skip byte block values from MediaDrmMetadata.
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_EncryptedCbcsSample_SetsEncryptionPattern)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(1));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	uint32_t capturedCrypt = 0;
	uint32_t capturedSkip  = 0;
	firebolt::rialto::CipherMode capturedCipherMode = firebolt::rialto::CipherMode::UNKNOWN;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedCrypt, &capturedSkip, &capturedCipherMode, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedCipherMode = seg->getCipherMode();
				seg->getEncryptionPattern(capturedCrypt, capturedSkip);
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 1, 9);

	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			ms.mDrmMetadata.mIsEncrypted   = true;
			ms.mDrmMetadata.mCipher        = CIPHER_TYPE_CBCS;
			ms.mDrmMetadata.mCryptByteBlock = 5;
			ms.mDrmMetadata.mSkipByteBlock  = 9;
			ms.mDrmMetadata.mKeyId = {0x01,0x02,0x03,0x04,
			                           0x05,0x06,0x07,0x08,
			                           0x09,0x0a,0x0b,0x0c,
			                           0x0d,0x0e,0x0f,0x10};
			ms.mDrmMetadata.mIV = {0xAA,0xBB,0xCC,0xDD,
			                        0xEE,0xFF,0x00,0x11,
			                        0x22,0x33,0x44,0x55,
			                        0x66,0x77,0x88,0x99};
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	EXPECT_EQ(capturedCipherMode, firebolt::rialto::CipherMode::CBCS);
	EXPECT_EQ(capturedCrypt, 5u);
	EXPECT_EQ(capturedSkip,  9u);
}

/**
 * @test An encrypted sample with subsamples causes addSubSample to be called
 *       the correct number of times with the correct byte counts.
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_EncryptedWithSubSamples_AddsCorrectSubSamples)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(2));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	std::vector<firebolt::rialto::SubSamplePair> capturedSubSamples;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedSubSamples, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedSubSamples = seg->getSubSamples();
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 1, 10);

	// Build two subsamples: {clear=100, enc=200} and {clear=50, enc=300}.
	// Packed as big-endian uint16+uint32 pairs.
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			ms.mDrmMetadata.mIsEncrypted  = true;
			ms.mDrmMetadata.mCipher       = CIPHER_TYPE_CENC;
			ms.mDrmMetadata.mNumSubSamples = 2;
			ms.mDrmMetadata.mKeyId = {0x01,0x02,0x03,0x04,
			                           0x05,0x06,0x07,0x08,
			                           0x09,0x0a,0x0b,0x0c,
			                           0x0d,0x0e,0x0f,0x10};
			ms.mDrmMetadata.mIV = {0xAA,0xBB,0xCC,0xDD,
			                        0xEE,0xFF,0x00,0x11,
			                        0x22,0x33,0x44,0x55,
			                        0x66,0x77,0x88,0x99};
			// Two subsamples packed big-endian: [{100, 200}, {50, 300}]
			// Each is uint16_t clear + uint32_t enc = 6 bytes per entry.
			ms.mDrmMetadata.mSubSamples = {
				0x00, 0x64,              // clear = 100
				0x00, 0x00, 0x00, 0xC8, // enc   = 200
				0x00, 0x32,              // clear = 50
				0x00, 0x00, 0x01, 0x2C  // enc   = 300
			};
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	ASSERT_EQ(capturedSubSamples.size(), 2u);
	EXPECT_EQ(capturedSubSamples[0].numClearBytes,     100u);
	EXPECT_EQ(capturedSubSamples[0].numEncryptedBytes, 200u);
	EXPECT_EQ(capturedSubSamples[1].numClearBytes,     50u);
	EXPECT_EQ(capturedSubSamples[1].numEncryptedBytes, 300u);
}

/**
 * @test An encrypted sample with no subsamples causes a single subsample
 *       covering the whole sample to be added (clear=0, enc=sampleSize).
 */
TEST_F(AampRialtoPlayerDrmTest,
	InjectSamples_EncryptedWithNoSubSamples_AddsWholeSampleAsEncrypted)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	ON_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).WillByDefault(Return(2));
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);
	SendVideoInitFragment();

	std::vector<firebolt::rialto::SubSamplePair> capturedSubSamples;
	std::atomic<bool> injectionDone{false};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&capturedSubSamples, &injectionDone](
				uint32_t,
				const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &seg)
			{
				capturedSubSamples = seg->getSubSamples();
				injectionDone = true;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	PostNeedData(0, 1, 11);

	const size_t kSampleSize = 64;
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetSamples())
		.WillByDefault([kSampleSize, this]() {
			std::vector<AampMediaSample> s;
			AampMediaSample ms{};
			ms.mPts = 0.1; ms.mDuration = 0.033;
			ms.mDrmMetadata.mIsEncrypted   = true;
			ms.mDrmMetadata.mCipher        = CIPHER_TYPE_CENC;
			ms.mDrmMetadata.mNumSubSamples = 0; // no subsamples
			ms.mDrmMetadata.mKeyId = {0x01,0x02,0x03,0x04,
			                           0x05,0x06,0x07,0x08,
			                           0x09,0x0a,0x0b,0x0c,
			                           0x0d,0x0e,0x0f,0x10};
			ms.mDrmMetadata.mIV = {0xAA,0xBB,0xCC,0xDD,
			                        0xEE,0xFF,0x00,0x11,
			                        0x22,0x33,0x44,0x55,
			                        0x66,0x77,0x88,0x99};
			// Fake sample data of exactly kSampleSize bytes so that the
			// fallback subsample (0, sampleSize) can be checked.
			auto buf = std::make_shared<std::vector<uint8_t>>(kSampleSize, 0xBE);
			ms.mData = std::shared_ptr<const uint8_t>(buf, buf->data());
			ms.mDataSize = buf->size();
			s.push_back(std::move(ms));
			return s;
		});
	std::vector<uint8_t> buf = {0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&injectionDone]{ return injectionDone.load(); });

	ASSERT_EQ(capturedSubSamples.size(), 1u);
	EXPECT_EQ(capturedSubSamples[0].numClearBytes,     0u);
	EXPECT_EQ(capturedSubSamples[0].numEncryptedBytes, kSampleSize);
}

// ===========================================================================
// Phase 13 — PlayerState state machine (GoF State pattern)
// ===========================================================================

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerTest, StateMachine_InitialState_IsIdle)
{
	// Before any Configure() call the state machine must be in IDLE.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

// ---------------------------------------------------------------------------
// IDLE → PIPELINE_CREATED
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerTest, StateMachine_AfterSuccessfulConfigure_IsPipelineCreated)
{
	// load() succeeds (mocked to return true in the base fixture).
	Configure();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PIPELINE_CREATED);
}

TEST_F(AampRialtoPlayerTest, StateMachine_AfterFailedLoad_RemainsIdle)
{
	// When load() fails the state machine must not advance.
	EXPECT_CALL(*m_mockPipelinePtr, load(_, _, _)).WillOnce(Return(false));
	Configure();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

// ---------------------------------------------------------------------------
// PIPELINE_CREATED → SOURCES_ATTACHING
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFirstVideoInit_IsSourcesAttaching_ForDualTrack)
{
	// For a dual-track stream, only the video init fragment has arrived —
	// the player waits for audio before calling allSourcesAttached().
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	// Audio source not yet attached → still SOURCES_ATTACHING.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHING);
}

// ---------------------------------------------------------------------------
// SOURCES_ATTACHING → SOURCES_ATTACHED
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterBothInitFragments_IsSourcesAttached)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_VideoOnlyStream_IsSourcesAttachedAfterVideoInit)
{
	// For a video-only stream allSourcesAttached() fires immediately after
	// the single video init fragment.
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
	SendVideoInitFragment();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AudioOnlyStream_IsSourcesAttachedAfterAudioInit)
{
	Configure(FORMAT_UNKNOWN, FORMAT_ISO_BMFF);
	SendAudioInitFragment();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::SOURCES_ATTACHED);
}

// ---------------------------------------------------------------------------
// SOURCES_ATTACHED → PLAYING (via notifyPlaybackState)
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterPlaybackStartedNotification_IsPlaying)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

// ---------------------------------------------------------------------------
// PLAYING → PAUSED
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterPausedNotification_IsPaused)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);

	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);
}

// ---------------------------------------------------------------------------
// PAUSED → PLAYING
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterResumeFromPaused_IsPlaying)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PAUSED);

	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING);
}

// ---------------------------------------------------------------------------
// → FLUSHING
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFlushFromSourcesAttached_IsFlushing)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	m_player->Flush(0.0, 1, false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFlushFromPlaying_IsFlushing)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	m_player->Flush(0.0, 1, false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFlushFromPaused_IsFlushing)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

	m_player->Flush(0.0, 1, false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

// ---------------------------------------------------------------------------
// FLUSHING → SOURCES_ATTACHING (new init fragment after flush)
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFlushThenReconfigure_ResetsToIdle)
{
	// Verify the FLUSHING → IDLE transition driven by onReconfigure().
	// After Flush() the state is FLUSHING; a subsequent Configure() call
	// fires onReconfigure() which resets to IDLE.  In the test environment
	// the pipeline factory has been exhausted so load() is not called and
	// the state stays at IDLE rather than advancing to PIPELINE_CREATED.
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();
	m_player->Flush(5.0, 1, false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// Re-configure fires onReconfigure(): FLUSHING → IDLE.
	Configure();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

// ---------------------------------------------------------------------------
// → STOPPED
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterStop_IsStopped)
{
	Configure();
	m_player->Stop(false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_StopFromPlaying_IsStopped)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	m_player->Stop(false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED);
}

// ---------------------------------------------------------------------------
// → ERROR
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_AfterFailureNotification_IsError)
{
	Configure();

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::FAILURE);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::ERROR);
}

// ---------------------------------------------------------------------------
// Reconfigure (re-tune) resets to IDLE then PIPELINE_CREATED
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_Reconfigure_ResetsFromStoppedToIdle)
{
	// Verify the STOPPED → IDLE transition driven by onReconfigure().
	// In the test environment the mock pipeline factory is exhausted after
	// the first Configure(), so the second call stops at IDLE rather than
	// advancing to PIPELINE_CREATED.  The important assertion is that the
	// state is no longer STOPPED after the re-configure.
	Configure();
	m_player->Stop(false);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED);

	// Re-configure fires onReconfigure(): STOPPED → IDLE.
	Configure();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StateMachine_Reconfigure_ResetsFromErrorToIdle)
{
	// Verify the ERROR → IDLE transition driven by onReconfigure().
	Configure();
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyPlaybackState(firebolt::rialto::PlaybackState::FAILURE);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::ERROR);

	// Re-configure fires onReconfigure(): ERROR → IDLE.
	Configure();
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE);
}

// ---------------------------------------------------------------------------
// IPC thread is non-blocking: OnNeedMediaData must return promptly even
// while a SendTransfer call is blocked waiting for needData.
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnNeedMediaData_DoesNotBlockCallerThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	// Rapid bursts of needData/cancel must complete without blocking.
	for (int i = 0; i < 30; ++i)
	{
		PostNeedData(0, 1, static_cast<uint32_t>(100 + i));
		PostNeedData(1, 1, static_cast<uint32_t>(200 + i));
	}
	SUCCEED();
}

// ===========================================================================
// IStreamSinkNotifiable — OnPlaybackState notifications
// ===========================================================================

// ---------------------------------------------------------------------------
// Initial tune: first PLAYING notification
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_CallsAllFirstFrameNotifications)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

	// All four first-frame / tune-complete methods must fire exactly once.
	EXPECT_CALL(m_mockNotifiable, LogFirstFrame()).Times(1);
	EXPECT_CALL(m_mockNotifiable, LogTuneComplete()).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(/*ccHandle=*/0UL))
		.Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_DoesNotCallSpeedChanged)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

	EXPECT_CALL(m_mockNotifiable, NotifySpeedChanged(_, _)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

// ---------------------------------------------------------------------------
// Second PLAYING after seek (post-seek recovery)
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_PostSeek_CallsNotifyFirstBufferAndFrame)
{
	Configure();

	// First PLAYING fires the initial-tune path.
	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// Now report SEEKING state so the second PLAYING takes the post-seek path.
	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_SEEKING));

	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(0UL)).Times(1);
	// Log calls must NOT fire on the second PLAYING.
	EXPECT_CALL(m_mockNotifiable, LogFirstFrame()).Times(0);
	EXPECT_CALL(m_mockNotifiable, LogTuneComplete()).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

// ---------------------------------------------------------------------------
// Second PLAYING after pause (resume)
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_ResumeFromPause_CallsNotifySpeedChanged)
{
	Configure();

	// First PLAYING (initial tune).
	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	// Now paused – GetState() returns eSTATE_PAUSED, not eSTATE_SEEKING.
	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_PAUSED));

	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(1);
	EXPECT_CALL(m_mockNotifiable,
		NotifySpeedChanged(AAMP_NORMAL_PLAY_RATE, /*changeState=*/true))
		.Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

// ---------------------------------------------------------------------------
// End-of-stream
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_EndOfStream_CallsNotifyEOSReached)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyEOSReached()).Times(1);

	PostPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_EndOfStream_DoesNotCallFirstFrameNotifications)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(0);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstBufferProcessed(_)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::END_OF_STREAM);
}

// ---------------------------------------------------------------------------
// PAUSED state (no notification expected on IStreamSinkNotifiable)
// ---------------------------------------------------------------------------

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

// ===========================================================================
// IStreamSinkNotifiable — position and duration callbacks
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyPosition_CallsMonitorProgress)
{
	Configure();

	EXPECT_CALL(m_mockNotifiable, MonitorProgress(/*sync=*/false, /*bos=*/false))
		.Times(1);

	constexpr int64_t kTwoSecondsNs = 2'000'000'000LL;
	PostPosition(kTwoSecondsNs);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_ReturnsLatestNotifiedPosition)
{
	Configure();

	ON_CALL(m_mockNotifiable, MonitorProgress(_, _))
		.WillByDefault(Return());

	constexpr int64_t kFiveSecondsNs  = 5'000'000'000LL;
	constexpr long long kExpectedMs   = 5'000LL;

	PostPosition(kFiveSecondsNs);

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpectedMs);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_BeforeConfigure_ReturnsZero)
{
	// Player constructed but Configure() not yet called → no pipeline.
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

// ===========================================================================
// SetVideoRectangle / GetVideoRectangle
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	GetVideoRectangle_ReturnsStringSetBySetVideoRectangle)
{
	Configure();

	ON_CALL(*m_mockPipelinePtr, setVideoWindow(_, _, _, _))
		.WillByDefault(Return(true));

	m_player->SetVideoRectangle(10, 20, 640, 480);

	EXPECT_EQ(m_player->GetVideoRectangle(), "10,20,640,480");
}

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
// Phase 13 — Back-pressure (synchronous pacing replaces the in-class queue)
// ===========================================================================

/**
 * @test SendTransfer with no pending needData blocks until a needData
 *       request arrives, providing natural back-pressure to AAMP's per-track
 *       injector thread without an in-class sample queue.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	BackPressure_SendTransfer_BlocksUntilNeedData)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
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
	std::atomic<int>  addSegmentCalls{0};
	ON_CALL(*m_mockPipelinePtr, addSegment(_, _))
		.WillByDefault(Invoke(
			[&addSegmentCalls](uint32_t, const auto &)
				-> firebolt::rialto::AddSegmentStatus
			{
				++addSegmentCalls;
				return firebolt::rialto::AddSegmentStatus::OK;
			}));

	// Run SendTransfer on a worker thread — it must block until needData.
	std::vector<uint8_t> buf = {0x01};
	std::thread sender([this, b = std::move(buf), &sendDone]() mutable {
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(b),
			0.1, 0.1, 0.033, 0, false);
		sendDone = true;
	});

	// Confirm SendTransfer is still blocked while there is no needData.
	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 0);

	// Posting needData unblocks the sender.
	PostNeedData(0, 1, 1);
	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	EXPECT_EQ(addSegmentCalls.load(), 1);

	sender.join();
}

/**
 * @test Calling Flush() while a SendTransfer call is blocked waiting for
 *       needData unblocks the sender (it returns without injecting), and a
 *       subsequent fragment can be injected normally after a fresh needData.
 */
TEST_F(AampRialtoPlayerWithDemuxTest,
	BackPressure_FlushWhileBlocked_UnblocksSendTransfer)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_UNKNOWN);
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

	// Sender is blocked.
	WaitFor([&sendDone]{ return sendDone.load(); },
		std::chrono::milliseconds(30));
	EXPECT_FALSE(sendDone.load());

	// Flush should wake the sender so it returns immediately.
	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	sender.join();

	// After flush, the next fragment must inject normally on the next needData.
	std::atomic<bool> haveDataCalled{false};
	EXPECT_CALL(*m_mockPipelinePtr, haveData(
		firebolt::rialto::MediaSourceStatus::OK, 77))
		.WillOnce(DoAll(
			Invoke([&haveDataCalled](auto, auto){ haveDataCalled = true; }),
			Return(true)));

	PostNeedData(0, 1, 77);
	std::vector<uint8_t> freshBuf = {0x02};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(freshBuf),
		0.1, 0.1, 0.033, 0, false);

	WaitFor([&haveDataCalled]{ return haveDataCalled.load(); });
	EXPECT_TRUE(haveDataCalled.load());
}
// ---------------------------------------------------------------------------
// IRialtoControlBackend integration
// ---------------------------------------------------------------------------

/**
 * @brief Configure() must call waitForRunning() on the control backend before
 *        creating the IMediaPipeline.
 *
 * This is the regression test for the Rialto race condition where
 * createMediaPipeline() can be called before the server reports RUNNING,
 * causing NeedMediaData events to be silently dropped.
 */
TEST_F(AampRialtoPlayerTest, Configure_CallsWaitForRunningBeforeCreatingPipeline)
{
	// Strict ordering: waitForRunning must precede createMediaPipeline.
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

/**
 * @brief When waitForRunning() returns false, Configure() should still
 *        attempt to create the pipeline (best-effort, same as the previous
 *        EnsureRialtoRunning behaviour).
 */
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

	// Should not crash or skip pipeline creation.
	Configure();
}