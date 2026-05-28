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
 * Tests verify the player's orchestration of its dependencies (pipeline,
 * sources, DRM bridge, notifiable) using mock objects.  Per-source behaviour
 * (codec mapping, segment creation, injection) is covered by the dedicated
 * AampRialtoVideoSourceTests, AampRialtoAudioSourceTests, and
 * AampRialtoSubtitleSourceTests suites.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "AampRialtoMediaSource.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineFactory.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"
#include "MockDrmBridge.h"
#include "MockIStreamSinkNotifiable.h"
#include "MockIRialtoControlBackend.h"
#include "MockAampRialtoMediaSource.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;
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
 * @brief Base fixture that wires MockIMediaPipelineFactory and a
 *        SourceCreator lambda into AampRialtoPlayer.
 *
 * The mock source factory returns NiceMock<MockAampRialtoMediaSource>
 * instances whose raw pointers are captured so tests can set expectations.
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

		g_mockPipelineFactory = m_mockFactory;

		// Create a NiceMock control backend.
		auto controlBackend =
				std::make_unique<NiceMock<MockIRialtoControlBackend>>();
		m_mockControlBackend = controlBackend.get();
		ON_CALL(*m_mockControlBackend, waitForRunning(_))
				.WillByDefault(Return(true));

		// Build a SourceCreator lambda that returns mock sources and
		// captures raw pointers so tests can set per-test expectations.
		SourceCreator sourceCreator =
			[this](AampMediaType type)
				-> std::unique_ptr<AampRialtoMediaSource>
			{
				++m_createSourceCallCount;

				auto src = std::make_unique<
					NiceMock<MockAampRialtoMediaSource>>();
				auto *rawPtr = src.get();

					ON_CALL(*rawPtr, mediaType())
						.WillByDefault(Return(type));

					// mapCodecToMime returns true with reasonable defaults
					ON_CALL(*rawPtr, mapCodecToMime(_, _, _))
						.WillByDefault(Invoke(
							[type](GstStreamOutputFormat,
								std::string &mimeType,
								firebolt::rialto::StreamFormat &fmt)
							{
								if (type == eMEDIATYPE_VIDEO)
								{
									mimeType = "video/h264";
									fmt = firebolt::rialto::StreamFormat::AVC;
								}
								else if (type == eMEDIATYPE_AUDIO)
								{
									mimeType = "audio/aac";
									fmt = firebolt::rialto::StreamFormat::RAW;
								}
								else if (type == eMEDIATYPE_SUBTITLE)
								{
									mimeType = "text/vtt";
									fmt = firebolt::rialto::StreamFormat::RAW;
								}
								return true;
							}));

					// createRialtoSource returns a MediaSourceVideo/Audio
					ON_CALL(*rawPtr, createRialtoSource(_, _, _, _, _))
						.WillByDefault(Invoke(
							[type](const std::string &mime, bool hasDrm,
								const MediaCodecInfo &,
								firebolt::rialto::StreamFormat fmt,
								std::shared_ptr<firebolt::rialto::CodecData>)
							{
								if (type == eMEDIATYPE_VIDEO)
								{
									return std::unique_ptr<
										firebolt::rialto::IMediaPipeline::MediaSource>(
										std::make_unique<
											firebolt::rialto::IMediaPipeline::MediaSourceVideo>(
												mime, hasDrm, 0, 0,
												firebolt::rialto::SegmentAlignment::AU,
												fmt, nullptr));
								}
								if (type == eMEDIATYPE_SUBTITLE)
								{
									return std::unique_ptr<
										firebolt::rialto::IMediaPipeline::MediaSource>(
										std::make_unique<
											firebolt::rialto::IMediaPipeline::MediaSourceSubtitle>(
												mime, ""));
								}
								return std::unique_ptr<
									firebolt::rialto::IMediaPipeline::MediaSource>(
									std::make_unique<
										firebolt::rialto::IMediaPipeline::MediaSourceAudio>(
											mime, hasDrm,
											firebolt::rialto::AudioConfig{},
											firebolt::rialto::SegmentAlignment::UNDEFINED,
											fmt, nullptr));
							}));

					ON_CALL(*rawPtr, updateCachedMetadata(_))
						.WillByDefault(Return());

					// injectSingleSampleProxy default — subtitle SendSample tests
					// verify routing without requiring a NeedData handshake.
					ON_CALL(*rawPtr, injectSingleSampleProxy(_))
						.WillByDefault(Return(true));

					// processDataFragmentProxy default — subtitle SendTransfer tests
					// verify routing without demuxer setup.
					ON_CALL(*rawPtr, processDataFragmentProxy(_, _, _, _, _))
						.WillByDefault(Return(true));

					ON_CALL(*rawPtr, createSegment(_))
						.WillByDefault(Invoke(
							[](const AampMediaSample &sample)
							{
								const int64_t ptsNs      = static_cast<int64_t>(sample.mPts      * kNsPerSecond);
								const int64_t durationNs = static_cast<int64_t>(sample.mDuration * kNsPerSecond);
								return std::make_unique<
									firebolt::rialto::IMediaPipeline::MediaSegmentVideo>(
										0, ptsNs, durationNs, 0, 0);
							}));

					auto idx = static_cast<size_t>(type);
					if (idx < m_mockSources.size())
					{
						m_mockSources[idx] = rawPtr;
					}
					return src;
			};

		m_player = std::make_unique<AampRialtoPlayer>(
				reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
				&m_mockNotifiable,
				std::move(controlBackend),
				/*id3HandlerCallback=*/nullptr,
				/*exportFrames=*/nullptr,
				std::move(sourceCreator));
	}

	void TearDown() override
	{
		m_player.reset();
		g_mockPipelineFactory = nullptr;
		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
		m_nextSourceId = 0;
		m_createSourceCallCount = 0;
		m_mockSources = {};
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

	/// Post a Rialto buffer-underflow notification via the captured client.
	void PostBufferUnderflow(int32_t sourceId)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr)
			<< "Configure() must be called before PostBufferUnderflow";
		client->notifyBufferUnderflow(sourceId);
	}

	/// Call Configure() with specific formats.
	/// Recreate m_mockPipeline with fresh default behaviours.
	/// Must be called before every player Configure() so the factory lambda
	/// has a valid pipeline to move into the player.
	void ResetMockPipeline()
	{
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_mockPipelinePtr = m_mockPipeline.get();

		ON_CALL(*m_mockPipelinePtr, load(_, _, _))
			.WillByDefault(Return(true));
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
	}

	void Configure(
		StreamOutputFormat video = FORMAT_ISO_BMFF,
		StreamOutputFormat audio = FORMAT_ISO_BMFF,
		StreamOutputFormat sub   = FORMAT_INVALID)
	{
		// The factory lambda moves m_mockPipeline into the player on each call.
		// If it was already consumed (null), recreate it with default behaviours
		// so the player gets a valid pipeline and EXPECT_CALLs on
		// m_mockPipelinePtr target the new pipeline correctly.
		if (!m_mockPipeline)
		{
			ResetMockPipeline();
		}
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

	/// Trigger initFragment SendTransfer — relies on the mock source's
	/// attachOrUpdate (inherited from base class).
	void SendVideoInitFragment()
	{
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetCodecInfo())
			.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
		std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
		m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
			0, 0, 0, 0, /*initFragment=*/true);
	}

	/// Trigger initFragment SendTransfer for audio.
	void SendAudioInitFragment()
	{
		ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
		ON_CALL(*g_mockMp4Demux, GetCodecInfo())
			.WillByDefault([]() { return MakeAudioAacCodecInfo(); });
		std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
		m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(buf),
			0, 0, 0, 0, /*initFragment=*/true);
	}

	std::shared_ptr<NiceMock<MockIMediaPipelineFactory>> m_mockFactory;
	std::unique_ptr<NiceMock<MockIMediaPipeline>>        m_mockPipeline;
	NiceMock<MockIMediaPipeline> *                       m_mockPipelinePtr{nullptr};
	std::unique_ptr<AampRialtoPlayer>                    m_player;
	std::weak_ptr<firebolt::rialto::IMediaPipelineClient> m_capturedClient;
	NiceMock<MockIStreamSinkNotifiable>                  m_mockNotifiable;
	MockIRialtoControlBackend *                          m_mockControlBackend{nullptr};
	std::array<NiceMock<MockAampRialtoMediaSource> *, 3> m_mockSources{};
	int                                                  m_createSourceCallCount{0};
	int32_t                                              m_nextSourceId{0};
};

/**
 * @class AampRialtoPlayerWithDemuxTest
 * @brief Fixture that additionally sets up g_mockMp4Demux for tests
 *        exercising SendTransfer (which uses the demuxer owned by sources).
 */
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
}

TEST_F(AampRialtoPlayerTest, Configure_NullSourceCreator_DoesNotCrash)
{
	g_mockPipelineFactory = nullptr;
	m_player.reset();

	SourceCreator nullCreator = [](AampMediaType) { return nullptr; };

	m_player = std::make_unique<AampRialtoPlayer>(
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP),
		&m_mockNotifiable,
		std::unique_ptr<IRialtoControlBackend>(nullptr),
		/*id3HandlerCallback=*/nullptr,
		/*exportFrames=*/nullptr,
		std::move(nullCreator));
	EXPECT_NO_THROW(
		m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID, false, false));
	g_mockPipelineFactory = m_mockFactory;
}

TEST_F(AampRialtoPlayerTest, Configure_CallsSourceCreatorForEachFormat)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	EXPECT_EQ(m_createSourceCallCount, 2);
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
}

TEST_F(AampRialtoPlayerTest, Configure_VideoOnly_CreatesVideoSourceOnly)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	EXPECT_EQ(m_createSourceCallCount, 1);
	EXPECT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	EXPECT_EQ(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
}

TEST_F(AampRialtoPlayerTest, Configure_FormatUnknown_CreatesSourceWithoutDemuxer)
{
	m_createSourceCallCount = 0;
	Configure(FORMAT_UNKNOWN, FORMAT_UNKNOWN);
	EXPECT_EQ(m_createSourceCallCount, 2);
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
// Phase 4 — attachSource via SendTransfer
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_InitFragment_AttachesSourceToPipeline)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_)).Times(1);

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

	// Video init arrives — should trigger both attachSource calls
	// (video first, then the deferred audio).
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
// Phase 5 — Segment injection baseline
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SendTransfer_MediaFragment_EnqueuesSamples)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

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
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

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
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _)).Times(::testing::AtLeast(0));
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	EXPECT_NO_THROW(client->notifyNeedMediaData(0, 5, 100, nullptr));
}

TEST_F(AampRialtoPlayerTest,
	OnCancelNeedMediaData_ClearsRequests)
{
	Configure();
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyNeedMediaData(0, 5, 200, nullptr);
	EXPECT_NO_THROW(client->notifyCancelNeedMediaData(0));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_EosOnly_CallsHaveDataWithEos)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
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

TEST_F(AampRialtoPlayerTest, Stop_NullPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->Stop(false));
}

TEST_F(AampRialtoPlayerWithDemuxTest, Stop_Idempotent_DoesNotCrash)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(2);

	m_player->Stop(false);
	m_player->Stop(false);
}

TEST_F(AampRialtoPlayerWithDemuxTest, Stop_KeepLastFrame_StillCallsPipelineStop)
{
	Configure();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Stop(/*keepLastFrame=*/true);
}

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

TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_NeedDataAfterStop_DoesNotReactivateInjection)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	m_player->Stop(false);

	EXPECT_CALL(*m_mockPipelinePtr, addSegment(_, _)).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, haveData(_, _)).Times(0);

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyNeedMediaData(/*sourceId=*/0, /*frameCount=*/3,
		/*requestId=*/99, /*shmInfo=*/nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Stop_ResetsEosFlags)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	m_player->EndOfStreamReached(eMEDIATYPE_VIDEO);
	m_player->EndOfStreamReached(eMEDIATYPE_AUDIO);

	m_player->Stop(false);

	EXPECT_CALL(*m_mockPipelinePtr,
		haveData(firebolt::rialto::MediaSourceStatus::EOS, _)).Times(0);

	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr);
	client->notifyNeedMediaData(/*sourceId=*/0, /*frameCount=*/3,
		/*requestId=*/100, /*shmInfo=*/nullptr);

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}


// ===========================================================================
// Phase 5b — NotifyInjectorToPause / wake blocked injection threads
// ===========================================================================

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

TEST_F(AampRialtoPlayerTest,
	NotifyInjectorToPause_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->NotifyInjectorToPause());
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	NotifyInjectorToPause_DoesNotCallPipelineStop)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);

	m_player->NotifyInjectorToPause();
}

// ===========================================================================
// Phase 6 — setSourcePosition before first injection
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	InjectSamples_FirstInjection_CallsSetSourcePosition)
{
	m_player->Flush(10.0, 1, false);
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 10000000000LL, true, _, _)).Times(1);

	SendVideoInitFragment();
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	AttachSource_NoFlush_DoesNotCallSetSourcePosition)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

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

TEST_F(AampRialtoPlayerTest,
	Flush_NoPipeline_DoesNotCrash)
{
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
	EXPECT_FALSE(m_player->Pause(true, false));
}

TEST_F(AampRialtoPlayerTest,
	SetPlayBackRate_NullPipeline_ReturnsFalse)
{
	EXPECT_FALSE(m_player->SetPlayBackRate(1.5));
}

// ===========================================================================
// Phase DRM — QueueProtectionEvent / ClearProtectionEvent
// ===========================================================================

class AampRialtoPlayerDrmTest : public AampRialtoPlayerWithDemuxTest
{
protected:
	void SetUp() override
	{
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

	std::unique_ptr<NiceMock<MockDrmBridge>> m_mockDrmBridge;
};

TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_Video_StoresParamsWithoutCallingBridge)
{
	const char *systemId = "com.widevine.alpha";
	const uint8_t initData[] = {0x01, 0x02, 0x03};

	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);

	m_player->QueueProtectionEvent(
		systemId, initData, sizeof(initData), eMEDIATYPE_VIDEO);
}

TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_NullInitData_DoesNotCallBridge)
{
	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", nullptr, 0, eMEDIATYPE_VIDEO);
}

TEST_F(AampRialtoPlayerDrmTest,
	ClearProtectionEvent_CallsClearSessions)
{
	EXPECT_CALL(*m_mockDrmBridge, clearSessions()).Times(1);
	m_player->ClearProtectionEvent();
}

TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithValidMksId_AttachesWithHasDrmTrue)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

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

TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithoutMksId_AttachesWithHasDrmFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
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

// ===========================================================================
// IStreamSinkNotifiable — OnPlaybackState notifications
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnPlaybackState_Playing_FirstTime_CallsAllFirstFrameNotifications)
{
	Configure();

	ON_CALL(m_mockNotifiable, GetState())
		.WillByDefault(Return(eSTATE_IDLE));

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
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(0UL)).Times(1);
	EXPECT_CALL(m_mockNotifiable, LogFirstFrame()).Times(0);
	EXPECT_CALL(m_mockNotifiable, LogTuneComplete()).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

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
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(_)).Times(0);

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
}

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

TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Paused_DoesNotSetInjectionGatedOnSources)
{
	// Rialto sends PAUSED for buffering-pause (e.g. AampUnderflowMonitor).
	// Injection must be allowed to continue so the buffer refills —
	// aborting it here would discard samples and delay recovery.
	// Only invalidateGeneration() (flush/stop paths) should set injectionGated.
	Configure();

	ASSERT_NE(m_mockSources[eMEDIATYPE_VIDEO], nullptr);
	ASSERT_NE(m_mockSources[eMEDIATYPE_AUDIO], nullptr);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().injectionGated);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->state().injectionGated);

	PostPlaybackState(firebolt::rialto::PlaybackState::PAUSED);

	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().injectionGated);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->state().injectionGated);
}

TEST_F(AampRialtoPlayerTest,
	OnPlaybackState_Playing_ClearsInjectionGatedOnAllSources)
{
	Configure();

	// Force sources into injectionGated state first.
	m_mockSources[eMEDIATYPE_VIDEO]->state().injectionGated = true;
	m_mockSources[eMEDIATYPE_AUDIO]->state().injectionGated = true;

	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);

	EXPECT_FALSE(m_mockSources[eMEDIATYPE_VIDEO]->state().injectionGated);
	EXPECT_FALSE(m_mockSources[eMEDIATYPE_AUDIO]->state().injectionGated);
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

// Segment start = 0: elapsed time equals raw PTS (unchanged behaviour for
// content whose first PTS is zero).
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_ReturnsElapsedTimeSinceSegmentStart)
{
	Configure();

	constexpr int64_t  kStartNs    = 0LL;
	constexpr int64_t  kCurrentNs  = 5'000'000'000LL;
	constexpr long long kExpectedMs = 5'000LL;

	PostPosition(kStartNs);  // establishes segment start = 0
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kCurrentNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpectedMs);
}

// When PTSReStamp causes the pipeline to start at a non-zero PTS offset
// (e.g. 13440 ms of accumulated prior content), GetPositionMilliseconds()
// must return elapsed time from that first PTS, not the raw PTS value.
// Without the fix this returns 14921 ms instead of 1481 ms.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_WithNonZeroSegmentStart_ReturnsElapsedTime)
{
	Configure();

	// Simulate 7 × 1920 ms of prior restamped content already buffered
	constexpr int64_t  kSegmentStartNs = 13'440'000'000LL;  // 13440 ms
	constexpr int64_t  kCurrentPosNs   = 14'921'000'000LL;  // 14921 ms
	constexpr long long kExpectedMs    = 1'481LL;            // elapsed

	PostPosition(kSegmentStartNs);  // first OnPosition → records segment start
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kCurrentPosNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpectedMs);
}

// After Configure() the segment-start offset must be cleared so that the
// next OnPosition call establishes a fresh baseline.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_AfterReconfigure_ResetsSegmentStart)
{
	// First session: inject content starting at 5000 ms.
	Configure();
	constexpr int64_t kFirstSessionStartNs = 5'000'000'000LL;
	PostPosition(kFirstSessionStartNs);  // segment start = 5000 ms
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(7'000'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 2'000LL);

	// Reconfigure — simulates a re-tune; segment start resets to -1.
	Configure();

	// Before the first new OnPosition the position must be zero.
	// (Pipeline query may return anything; startMs=-1 forces result=0.)
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);

	// Second session: different segment start.
	constexpr int64_t kSecondSessionStartNs = 2'000'000'000LL;
	PostPosition(kSecondSessionStartNs);  // segment start = 2000 ms
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
// GetPositionMilliseconds() must return 0 — not a stale OnPosition value.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_WhenPipelineQueryFails_ReturnsZero)
{
	Configure();
	constexpr int64_t kSegmentStartNs = 13'440'000'000LL;
	PostPosition(kSegmentStartNs);  // segment start = 13440 ms

	ON_CALL(*m_mockPipelinePtr, getPosition(_)).WillByDefault(Return(false));

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
// SetStreamCaps — source attach via external demuxer path
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_VideoH264_AttachesVideoSource)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

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
		.WillOnce(Invoke(
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
		ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], mapCodecToMime(_, _, _))
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

	m_player->Flush(10.0);

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(0, testing::Ge(10'000'000'000LL), _, _, _))
		.WillOnce(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

// ===========================================================================
// SendSample — per-sample injection via external demuxer path
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	SendSample_Video_InjectsSample)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

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

TEST_F(AampRialtoPlayerTest,
	SendSample_NoSourceAttached_ReturnsFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	bool result = m_player->SendSample(eMEDIATYPE_VIDEO, MakeSample());
	EXPECT_FALSE(result);
}

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

	m_player->Flush(5.0);

	WaitFor([&sampleReturned]{ return sampleReturned.load(); });
	EXPECT_TRUE(sampleReturned.load());

	sender.join();
}

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
// OnNeedMediaData dispatches to the correct source
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	OnNeedMediaData_DoesNotBlockCallerThread)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	SendVideoInitFragment();
	SendAudioInitFragment();

	for (int i = 0; i < 30; ++i)
	{
		PostNeedData(0, 1, static_cast<uint32_t>(100 + i));
		PostNeedData(1, 1, static_cast<uint32_t>(200 + i));
	}
	SUCCEED();
}

// ===========================================================================
// Back-pressure (synchronous pacing)
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	BackPressure_SendTransfer_BlocksUntilNeedData)
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

	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
	WaitFor([&sendDone]{ return sendDone.load(); });
	EXPECT_TRUE(sendDone.load());
	sender.join();
}

// ===========================================================================
// StopBuffering
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	StopBuffering_ForceStop_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.Times(1).WillOnce(Return(true));
	m_player->StopBuffering(/*forceStop=*/true);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	StopBuffering_NotForced_CallsPipelinePlay)
{
	Configure();
	EXPECT_CALL(*m_mockPipelinePtr, play(_))
		.Times(1).WillOnce(Return(true));
	m_player->StopBuffering(/*forceStop=*/false);
}

TEST_F(AampRialtoPlayerTest,
	StopBuffering_NullPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->StopBuffering(true));
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
	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
	WaitFor([&sendDone]{ return sendDone.load(); });
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

// ===========================================================================
// notifyBufferUnderflow — OnBufferUnderflow dispatch
// ===========================================================================

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
	// Audio source was assigned sourceId=1 (second m_nextSourceId++).

	EXPECT_CALL(m_mockNotifiable,
		NotifyBufferUnderflow(eMEDIATYPE_AUDIO))
		.Times(1);

	PostBufferUnderflow(/*sourceId=*/1);
}

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
// Phase N — Subtitle source injection
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	SendTransfer_SubtitleRawFragment_RoutesViaProcessDataFragment)
{
	/**
	 * @brief Verifies that a non-init subtitle fragment is routed through
	 *        processDataFragment() (the subtitle source override), with the
	 *        correct fpts, fdts, fDuration and fragmentPTSoffset parameters.
	 *
	 *        The mock's processDataFragmentProxy intercepts the call so the
	 *        test does not need a demuxer or a Rialto NeedData handshake.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);
	ASSERT_NE(m_mockSources[eMEDIATYPE_SUBTITLE], nullptr);

	// Expect processDataFragment proxy called with the exact parameters
	// passed to SendTransfer (fragmentPTSoffset == 0.0).
	EXPECT_CALL(*m_mockSources[eMEDIATYPE_SUBTITLE],
		processDataFragmentProxy(
			Ref(*m_mockPipelinePtr),
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
		processDataFragmentProxy(
			Ref(*m_mockPipelinePtr),
			/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/1.0,
			/*fragmentPTSoffset=*/kOffsetSec))
		.WillOnce(Return(true));

	std::vector<uint8_t> buf = {0x3C, 0x74, 0x74, 0x3E};
	m_player->SendTransfer(
		eMEDIATYPE_SUBTITLE, std::move(buf),
		/*fpts=*/2.0, /*fdts=*/2.0, /*fDuration=*/1.0,
		/*fragmentPTSoffset=*/kOffsetSec, /*initFragment=*/false);
}
