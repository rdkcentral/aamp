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
#include "IDirectRialtoCC.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineFactory.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"
#include "MockDrmBridge.h"
#include "MockIStreamSinkNotifiable.h"
#include "MockIRialtoControlBackend.h"
#include "MockAampRialtoMediaSource.h"
#include "MockAampConfig.h"
#include "MockGLib.h"
#include "MockIMediaPipelineCapabilities.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
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

static MediaCodecInfo MakeSubtitleTtmlCodecInfo()
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_SUBTITLE_TTML;
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
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockGLib = std::make_shared<NiceMock<MockGLib>>();

		ON_CALL(*g_mockAampConfig,
			GetConfigValue(eAAMPConfig_ReportProgressInterval))
			.WillByDefault(Return(1.0));

		ON_CALL(*g_mockGLib, g_timeout_add(_, _, _))
			.WillByDefault(Invoke(
				[this](guint /*interval*/, GSourceFunc function, gpointer data)
				{
					m_progressTimerCallback = function;
					m_progressTimerUserData = data;
					return m_nextTimerId++;
				}));

		ON_CALL(*g_mockGLib, g_source_remove(_))
			.WillByDefault(Return(TRUE));

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

		ON_CALL(*m_mockPipelinePtr, load(_, _, _, _))
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

		// Set up the capabilities factory mock.  Default returns nullptr so
		// computeAppliedRate() falls back to 1.0 unless a test overrides it.
		m_mockCapabilitiesFactory =
			std::make_shared<NiceMock<MockIMediaPipelineCapabilitiesFactory>>();
		ON_CALL(*m_mockCapabilitiesFactory, createMediaPipelineCapabilities())
			.WillByDefault(Invoke([]{
				return std::unique_ptr<
					firebolt::rialto::IMediaPipelineCapabilities>{nullptr};
			}));
		g_mockCapabilitiesFactory = m_mockCapabilitiesFactory;

		// Create a NiceMock control backend.
		auto controlBackend =
				std::make_unique<NiceMock<MockIRialtoControlBackend>>();
		m_mockControlBackend = controlBackend.get();
		ON_CALL(*m_mockControlBackend, waitForRunning(_))
				.WillByDefault(Return(true));

		// Build a SourceCreator lambda that returns mock sources and
		// captures raw pointers so tests can set per-test expectations.
		ON_CALL(m_mockNotifiable, GetProgressReportIntervalSeconds())
			.WillByDefault(Return(1.0));

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

					// Delegate isInbandCC() to the real format check so
					// that setFormat(FORMAT_INVALID) makes it return true.
					ON_CALL(*rawPtr, isInbandCC())
						.WillByDefault([rawPtr]() {
							return rawPtr->format() == FORMAT_INVALID;
						});
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

					// For subtitle sources, default to Return(true) since
					// there is no NeedData handshake or demuxer to drive.
					// For video/audio, delegate to the base class so the
					// real Rialto state machine runs.
					if (type == eMEDIATYPE_SUBTITLE)
					{
						ON_CALL(*rawPtr, injectSingleSample(_, _, _))
							.WillByDefault(Return(true));
						ON_CALL(*rawPtr, processDataFragment(_, _, _, _, _, _))
							.WillByDefault(Return(true));
					}
					else
					{
						ON_CALL(*rawPtr, injectSingleSample(_, _, _))
							.WillByDefault(
								[rawPtr](firebolt::rialto::IMediaPipeline &pipeline,
									AampMediaSample &&sample, bool morePending)
								{
									return rawPtr->AampRialtoMediaSource::injectSingleSample(
										pipeline, std::move(sample), morePending);
								});
						ON_CALL(*rawPtr, processDataFragment(_, _, _, _, _, _))
							.WillByDefault(
								[rawPtr](firebolt::rialto::IMediaPipeline &pipeline,
									std::shared_ptr<std::vector<uint8_t>> buffer,
									double fpts, double fdts,
									double fDuration, double offset)
								{
									return rawPtr->AampRialtoMediaSource::processDataFragment(
										pipeline, std::move(buffer),
										fpts, fdts, fDuration, offset);
								});
					}

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

				// Default: no sample injected yet — GetPositionMilliseconds()
				// returns 0.  Individual tests override to simulate a known
				// first PTS (segment start).
				ON_CALL(*rawPtr, firstPtsMs())
					.WillByDefault(Return(
						AampRialtoMediaSource::kFirstPtsNotSet));

				auto idx = static_cast<size_t>(type);
					if (idx < m_mockSources.size())
					{
						m_mockSources[idx] = rawPtr;
					}
					return src;
			};

		m_player = std::make_unique<AampRialtoPlayer>(
				reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get()),
				&m_mockNotifiable,
				std::move(controlBackend),
				/*id3HandlerCallback=*/nullptr,
				/*exportFrames=*/nullptr,
				std::move(sourceCreator));
	}

	void TearDown() override
	{
		m_player.reset();
		g_mockGLib.reset();
		g_mockAampConfig.reset();
		g_mockPipelineFactory = nullptr;
		g_mockCapabilitiesFactory = nullptr;
		g_mockPrivateInstanceAAMP.reset();
		m_nextSourceId = 0;
		m_createSourceCallCount = 0;
		m_mockSources = {};
		m_progressTimerCallback = nullptr;
		m_progressTimerUserData = nullptr;
		m_nextTimerId = 1;
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

	/// Post a Rialto source-flushed notification via the captured client.
	void PostSourceFlushed(int32_t sourceId)
	{
		auto client = m_capturedClient.lock();
		ASSERT_NE(client, nullptr)
			<< "Configure() must be called before PostSourceFlushed";
		client->notifySourceFlushed(sourceId);
	}

	/// Call Configure() with specific formats.
	/// Recreate m_mockPipeline with fresh default behaviours.
	/// Must be called before every player Configure() so the factory lambda
	/// has a valid pipeline to move into the player.
	void ResetMockPipeline()
	{
		m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
		m_mockPipelinePtr = m_mockPipeline.get();

		ON_CALL(*m_mockPipelinePtr, load(_, _, _, _))
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

	void TriggerProgressTimerTick()
	{
		// With the new ProgressTimer implementation, we directly invoke
		// OnProgressTimerTick() to simulate a timer tick.
		m_player->OnProgressTimerTick();
	}

	std::shared_ptr<NiceMock<MockIMediaPipelineFactory>>        m_mockFactory;
	std::unique_ptr<NiceMock<MockIMediaPipeline>>               m_mockPipeline;
	NiceMock<MockIMediaPipeline> *                              m_mockPipelinePtr{nullptr};
	std::shared_ptr<NiceMock<MockIMediaPipelineCapabilitiesFactory>>
	                                                            m_mockCapabilitiesFactory;
	std::unique_ptr<AampRialtoPlayer>                           m_player;
	std::weak_ptr<firebolt::rialto::IMediaPipelineClient>       m_capturedClient;
	NiceMock<MockIStreamSinkNotifiable>                         m_mockNotifiable;
	MockIRialtoControlBackend *                                 m_mockControlBackend{nullptr};
	std::array<NiceMock<MockAampRialtoMediaSource> *, 3>        m_mockSources{};
	int                                                         m_createSourceCallCount{0};
	int32_t                                                     m_nextSourceId{0};
	GSourceFunc                                                 m_progressTimerCallback{nullptr};
	gpointer                                                    m_progressTimerUserData{nullptr};
	guint                                                       m_nextTimerId{1};
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
		g_mockMp4Demux = std::make_shared<NiceMock<MockMp4Demux>>();
	}

	void TearDown() override
	{
		g_mockMp4Demux.reset();
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

	// setSourcePosition fires for both video (id=0) and the inband CC
	// subtitle source (id=1) created alongside video in Configure().
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, 10000000000LL, true, _, _)).Times(2);

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

	// Source IDs: video=0, inband CC subtitle=1 (deferred, drained with
	// video in SendVideoInitFragment), audio=2.
	EXPECT_CALL(*m_mockPipelinePtr, flush(0, true, _)).Times(1);  // video
	EXPECT_CALL(*m_mockPipelinePtr, flush(1, true, _)).Times(1);  // CC subtitle
	EXPECT_CALL(*m_mockPipelinePtr, flush(2, true, _)).Times(1);  // audio

	m_player->Flush(5.0, 1, false);
}

TEST_F(AampRialtoPlayerTest,
	Flush_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->Flush(0.0, 1, false));
}

// ===========================================================================
// SeekStreamSink — Uses pipeline->setPosition() to seek
// ===========================================================================

TEST_F(AampRialtoPlayerWithDemuxTest,
	SeekStreamSink_CallsSetPositionWithCorrectValue)
{
	Configure();
	SendVideoInitFragment();
	SendAudioInitFragment();

	// SeekStreamSink should call pipeline->setPosition() with position in nanoseconds.
	// position = 10.5 seconds = 10,500,000,000 nanoseconds
	const int64_t expectedPosNs = 10'500'000'000LL;
	EXPECT_CALL(*m_mockPipelinePtr, setPosition(expectedPosNs))
		.WillOnce(Return(true));

	m_player->SeekStreamSink(10.5, 1.0);
}

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

TEST_F(AampRialtoPlayerTest,
	SeekStreamSink_NoPipeline_DoesNotCrash)
{
	EXPECT_NO_THROW(m_player->SeekStreamSink(0.0, 1.0));
}

TEST_F(AampRialtoPlayerTest,
	SeekStreamSink_NoSourcesAttached_DoesNotCrash)
{
	Configure();
	EXPECT_NO_THROW(m_player->SeekStreamSink(5.0, 1.0));
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
	ClearProtectionEvent_ClearsPendingProtection)
{
	const uint8_t initData[] = {0x01};
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// ClearProtectionEvent must NOT call clearSessions — it only clears the
	// buffered init data so that a subsequent Configure/attachOrUpdate does
	// not re-use stale protection params.
	EXPECT_CALL(*m_mockDrmBridge, clearSessions()).Times(0);
	m_player->ClearProtectionEvent();
}

// ---------------------------------------------------------------------------
// SetEncryptedAamp — lazy DRM bridge initialisation for pre-roll ad scenario
// ---------------------------------------------------------------------------

TEST_F(AampRialtoPlayerDrmTest,
	SetEncryptedAamp_CreatesBridge_BridgeDelegatesToMock)
{
	// Before any call the bridge is absent; SetEncryptedAamp must create it.
	// The fake AampDrmBridge delegates to g_mockDrmBridge, so subsequent
	// calls on the player's bridge reach m_mockDrmBridge.
 	PrivateInstanceAAMP encryptedAamp{};
 	m_player->SetEncryptedAamp(&encryptedAamp);

	// ClearProtectionEvent only clears pending params — it does not call
	// clearSessions.  Verify that the bridge was created by queuing protection
	// and checking createSession is reachable through the mock.
	EXPECT_CALL(*m_mockDrmBridge, clearSessions()).Times(0);
	m_player->ClearProtectionEvent();
}

TEST_F(AampRialtoPlayerDrmTest,
	AttachSource_WithNoBridgeSet_LazilyCreatesBridge)
{
	// Bridge is not pre-initialised via SetEncryptedAamp.
	// It must be created lazily when AttachSource is first called, and the
	// fake must delegate calls to g_mockDrmBridge (set by the fixture).
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	// ClearProtectionEvent only clears pending params, not DRM sessions.
	EXPECT_CALL(*m_mockDrmBridge, clearSessions()).Times(0);
	m_player->ClearProtectionEvent();
}

TEST_F(AampRialtoPlayerDrmTest,
	AttachSource_AfterSetEncryptedAamp_DoesNotReplaceBridge)
{
	// SetEncryptedAamp sets the bridge.  Subsequent AttachSource calls must
	// NOT replace it — the lazy guard `if (!m_drmBridge)` ensures this.
	// Verify by checking createSession is called through the same mock.
	const uint8_t initData[] = {0x01};
 	PrivateInstanceAAMP encryptedAamp{};
 	m_player->SetEncryptedAamp(&encryptedAamp);
	
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// createSession must be called on the bridge installed by SetEncryptedAamp
	// (which delegates to m_mockDrmBridge via the fake).
	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(7));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
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
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
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
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
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
	// The direct-rialto CC path passes a non-zero IDirectRialtoCC* handle.
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(Ne(0UL)))
		.Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstVideoFrameDisplayed()).Times(1);

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
	// CC source already created on first PLAYING; handle is still non-zero.
	EXPECT_CALL(m_mockNotifiable, NotifyFirstFrameReceived(Ne(0UL))).Times(1);
	EXPECT_CALL(m_mockNotifiable, NotifyFirstVideoFrameDisplayed()).Times(1);
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

// Segment start = 0: elapsed time equals raw PTS (unchanged behaviour for
// content whose first PTS is zero).
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_ReturnsElapsedTimeSinceSegmentStart)
{
	Configure();

	constexpr int64_t  kCurrentNs  = 5'000'000'000LL;
	constexpr long long kExpectedMs = 5'000LL;

	// Simulate video source reporting first PTS = 0 ms (PTS-restamped start).
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0LL));
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

	// Simulate 7 × 1920 ms of prior restamped content already buffered.
	// The first video sample had PTS = 13440 ms; this is reported via
	// firstPtsMs() (set lazily in injectOneSample()).
	constexpr int64_t  kSegmentStartMs = 13'440LL;
	constexpr int64_t  kCurrentPosNs   = 14'921'000'000LL;  // 14921 ms
	constexpr long long kExpectedMs    = 1'481LL;            // elapsed

	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(kSegmentStartMs));
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(kCurrentPosNs), Return(true)));

	EXPECT_EQ(m_player->GetPositionMilliseconds(), kExpectedMs);
}

// After Configure() the segment-start offset must be cleared so that the
// next injection establishes a fresh baseline for the new session.
TEST_F(AampRialtoPlayerWithDemuxTest,
	GetPositionMilliseconds_AfterReconfigure_ResetsSegmentStart)
{
	// First session: first video sample had PTS = 5000 ms.
	Configure();
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(5'000LL));
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(7'000'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 2'000LL);

	// Reconfigure — simulates a re-tune (Stop then Configure).  Stop() marks
	// the pipeline as stopped so Configure() performs a full recreation.
	// New sources are created; their default firstPtsMs() = kFirstPtsNotSet.
	m_player->Stop(false);
	Configure();

	// Before the first sample from the new session, position must be zero.
	// Default firstPtsMs() = kFirstPtsNotSet (-1) forces result = 0.
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 0LL);

	// Second session: first video sample had PTS = 2000 ms.
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(2'000LL));
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

	// Trickplay rewind: seek to 12 s at rate=-2.
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

	// Enter trickplay rewind.
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
	// injectionGated, not mbTrackDownloadsBlocked for subtitle backpressure).
	// SetStreamCaps must therefore call ResumeTrackDownloads(SUBTITLE) after
	// attaching the subtitle source so the inject loop is unblocked.
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_SUBTITLE_TTML);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		ResumeTrackDownloads(eMEDIATYPE_SUBTITLE))
		.Times(1);

	m_player->SetStreamCaps(eMEDIATYPE_SUBTITLE, MakeSubtitleTtmlCodecInfo());
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
	// Source IDs: video=0, inband CC subtitle=1 (deferred, drained with
	// video), audio=2 (third m_nextSourceId++).

	EXPECT_CALL(m_mockNotifiable,
		NotifyBufferUnderflow(eMEDIATYPE_AUDIO))
		.Times(1);

	PostBufferUnderflow(/*sourceId=*/2);
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
	 * The sequence Flush(shouldTearDown=false) → Configure(audio=INVALID)
	 * previously left the state machine stuck in FLUSHING, unable to
	 * respond to Rialto's playback state callbacks.
	 *
	 * Fix: FlushingState now handles onPlaybackStarted() and
	 * onPlaybackPaused(), so it can transition to PLAYING or PAUSED
	 * when Rialto sends those notifications.
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
	m_player->Flush(/*position=*/0.0, /*rate=*/-2, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Precondition: Flush(shouldTearDown=false) must move state to FLUSHING";

	// Simulate Rialto confirming the flush for all attached sources.
	// OnSourceFlushed commits m_pendingFlushRate → m_rate and unblocks
	// WaitForFlushToComplete() so the subsequent Configure() can proceed.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, _, /*resetTime=*/true, _, _))
		.Times(3)
		.WillRepeatedly(Return(true));
	PostSourceFlushed(/*sourceId=*/0);  // video
	PostSourceFlushed(/*sourceId=*/1);  // inband CC subtitle
	PostSourceFlushed(/*sourceId=*/2);  // audio

	// Trickplay Configure: audio → FORMAT_INVALID, no pipeline recreation.
	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_INVALID, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);

	// State machine stays in FLUSHING (no pipeline recreation, no state change).
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "State machine stays in FLUSHING when Configure() doesn't recreate pipeline";

	// Verify the FIX: FlushingState responds to Rialto PLAYING callback.
	PostPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::PLAYING)
		<< "FlushingState must transition to PLAYING when Rialto sends PLAYING";
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
// Flush — shouldTearDown parameter tests
// ===========================================================================

TEST_F(AampRialtoPlayerTest,
	Flush_NullPipeline_ShouldTearDownTrue_CallsStop)
{
	/**
	 * @brief When the player is in IDLE state (no pipeline) and shouldTearDown=true,
	 *        Flush() must call Stop(true) to tear down gracefully.
	 *
	 * This mirrors GStreamer's behavior: if the pipeline is in an invalid
	 * state (GST_STATE_NULL), it calls stopCallback(true) to tear down.
	 */
	// Setup: DON'T call Configure() so player remains in IDLE state.
	// Player is constructed in SetUp() but no pipeline is created yet.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::IDLE)
		<< "Precondition: player must be in IDLE state";
	
	// Flush() with shouldTearDown=true should call Stop() even in IDLE state.
	m_player->Flush(/*position=*/10.0, /*rate=*/1, /*shouldTearDown=*/true);

	// Verify player transitions to STOPPED state.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED)
		<< "Player must transition to STOPPED after Flush(shouldTearDown=true)";
}

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

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PipelineStopped_ShouldTearDownTrue_CallsStop)
{
	/**
	 * @brief When the player is in STOPPED state and shouldTearDown=true,
	 *        Flush() must call Stop(true).
	 */
	Configure();
	m_player->Stop(false);

	// Verify player is in STOPPED state.
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED)
		<< "Precondition: player must be in STOPPED state after Stop()";

	// Expect Stop() to be called again when shouldTearDown=true.
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(1);

	m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/true);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_PipelineStopped_ShouldTearDownFalse_DoesNotCallStop)
{
	/**
	 * @brief When the player is in STOPPED state and shouldTearDown=false,
	 *        Flush() must NOT call Stop(), only return early.
	 */
	Configure();
	m_player->Stop(false);

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::STOPPED);

	// Expect NO additional stop() call.
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);

	m_player->Flush(/*position=*/0.0, /*rate=*/1, /*shouldTearDown=*/false);
}

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

	// Player is in PLAYING state; expect normal flush() calls, NOT stop().
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _))
		.Times(::testing::AtLeast(1));

	m_player->Flush(/*position=*/20.0, /*rate=*/1, /*shouldTearDown=*/true);

	// Verify player transitions to FLUSHING state.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Player must transition to FLUSHING after successful Flush()";
}

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

	m_player->Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF, FORMAT_INVALID,
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

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

	// Player is in PLAYING state; expect normal flush() calls, NOT stop().
	EXPECT_CALL(*m_mockPipelinePtr, stop()).Times(0);
	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _))
		.Times(::testing::AtLeast(1));

	m_player->Flush(/*position=*/15.0, /*rate=*/1, /*shouldTearDown=*/false);

	// Verify player transitions to FLUSHING state.
	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);
}

static void SetupCapabilities(
	std::shared_ptr<NiceMock<MockIMediaPipelineCapabilitiesFactory>> &factory,
	bool querySucceeds,
	bool videoMaster);

TEST_F(AampRialtoPlayerTest,
	Flush_AlreadyFlushing_SkipsSecondPipelineFlushButUpdatesRateAndPosition)
{
	/**
	 * @brief A second Flush() while already in FLUSHING should not re-issue
	 *        pipeline flush IPC, but must still update staged position/rate.
	 */
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	// First flush transitions SOURCES_ATTACHED -> FLUSHING.
	// Configure(FORMAT_ISO_BMFF, FORMAT_INVALID) attaches two sources:
	// video (sourceId=0) and inband CC subtitle (sourceId=1).  Each
	// attached source issues one pipeline flush IPC on the first Flush().
	EXPECT_CALL(*m_mockPipelinePtr, flush(_, _, _)).Times(2);
	m_player->Flush(/*position=*/10.0, /*rate=*/2, /*shouldTearDown=*/false);

	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Precondition: first Flush() must put player into FLUSHING";

	// Re-entrant flush while FLUSHING: no second pipeline flush command.
	m_player->Flush(/*position=*/33.0, /*rate=*/-4, /*shouldTearDown=*/false);

	EXPECT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING)
		<< "Re-entrant Flush() must keep player in FLUSHING";

	// Both sources (video=0, subtitle=1) report flushed; setSourcePosition
	// is called for each with the position from the second (pending) flush.
	// The rate is committed only after all sources have reported flushed.
	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(33'000'000'000LL),
			/*resetTime=*/true, _, _))
		.Times(2)
		.WillRepeatedly(Return(true));
	PostSourceFlushed(/*sourceId=*/0);
	PostSourceFlushed(/*sourceId=*/1);  // inband CC subtitle source

	// GetPositionMilliseconds() must use latest rate from second flush.
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0LL));
	EXPECT_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillOnce(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));
	EXPECT_EQ(m_player->GetPositionMilliseconds(), -2000LL);
}

TEST_F(AampRialtoPlayerWithDemuxTest,
	Flush_MultiSource_CommitsRateAfterAllSourcesFlushed)
{
	/**
	 * @brief Flush() stages pending rate immediately, but active playback rate
	 *        must change only after every attached source reports flushed.
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

	m_player->Flush(/*position=*/12.0, /*rate=*/-4, /*shouldTearDown=*/false);

	ON_CALL(*m_mockPipelinePtr, getPosition(_))
		.WillByDefault(DoAll(SetArgReferee<0>(500'000'000LL), Return(true)));
	ON_CALL(*m_mockSources[eMEDIATYPE_VIDEO], firstPtsMs())
		.WillByDefault(Return(0LL));

	// Configure() attaches three sources: video (id=0), inband CC subtitle
	// (id=1), and audio (id=2).  Active rate must not commit until every
	// source has reported flushed.

	// Video flushed -> rate still uncommitted (subtitle + audio pending).
	PostSourceFlushed(/*sourceId=*/0);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 500LL);

	// Inband CC subtitle flushed -> rate still uncommitted (audio pending).
	PostSourceFlushed(/*sourceId=*/1);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), 500LL);

	// Audio flushed (last source) -> pending rate commits to active rate.
	PostSourceFlushed(/*sourceId=*/2);
	EXPECT_EQ(m_player->GetPositionMilliseconds(), -2000LL);
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

TEST_F(AampRialtoPlayerTest,
	SetStreamCaps_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition)
{
	/**
	 * @brief When the platform reports isVideoMaster==false, setSourcePosition
	 * must be called with appliedRate equal to the flushed rate (2.0 here).
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
		.Times(2)
		.WillRepeatedly(Return(true));

	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());
}

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

TEST_F(AampRialtoPlayerTest,
	OnSourceFlushed_FlushAtRate2_NotVideoMaster_UsesStoredRateInSetSourcePosition)
{
	/**
	 * @brief OnSourceFlushed must forward appliedRate == stored rate when the
	 * platform reports isVideoMaster==false.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/false);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// Attach source first so Flush() can mark it flushing.
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 2.0, _))
		.WillOnce(Return(true));

	PostSourceFlushed(/*sourceId=*/0);
}

TEST_F(AampRialtoPlayerTest,
	OnSourceFlushed_FlushAtRate2_VideoMaster_UsesRate1InSetSourcePosition)
{
	/**
	 * @brief OnSourceFlushed must forward appliedRate == 1.0 when the platform
	 * reports isVideoMaster==true.
	 */
	SetupCapabilities(m_mockCapabilitiesFactory, /*querySucceeds=*/true,
		/*videoMaster=*/true);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);

	// Attach source first so Flush() can mark it flushing.
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	m_player->Flush(10.0, 2, /*shouldTearDown=*/false);

	EXPECT_CALL(*m_mockPipelinePtr,
		setSourcePosition(_, testing::Ge(10'000'000'000LL),
			/*resetTime=*/true, 1.0, _))
		.WillOnce(Return(true));

	PostSourceFlushed(/*sourceId=*/0);
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

// ===========================================================================
// WaitForFlushToComplete — Configure blocks until flush cycle finishes
// ===========================================================================

/**
 * @test When Configure() is called while the player is FLUSHING, it must block
 *       until all sources have finished flushing so that m_rate reflects the
 *       pending flush rate before ShouldRecreatePipeline checks it.
 *
 * Scenario: Flush at rate=4 → state=FLUSHING → Configure() on another thread
 * → OnSourceFlushed() completes the flush cycle → Configure() unblocks with
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
	m_player->Flush(10.0, 4, /*shouldTearDown=*/false);
	ASSERT_EQ(m_player->GetCurrentPlayerState(), PlayerStateId::FLUSHING);

	// At this point m_rate has NOT been committed yet (sources are flushing).
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

	// Give the thread time to block — Configure should NOT have finished yet.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(configureFinished.load(std::memory_order_acquire))
		<< "Configure() must block while sources are still flushing";

	// Complete the flush cycle by posting SourceFlushed for all sources.
	// Video source has sourceId=0, CC subtitle=1, audio=2.
	PostSourceFlushed(0);
	PostSourceFlushed(1);
	PostSourceFlushed(2);

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
