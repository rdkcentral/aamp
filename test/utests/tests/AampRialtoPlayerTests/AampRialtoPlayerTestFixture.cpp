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
 * @file AampRialtoPlayerTestFixture.cpp
 * @brief Out-of-line members of the AampRialtoPlayer test fixtures, compiled
 *        once instead of in every test file.
 */

#include "AampRialtoPlayerTestFixture.h"

void AampRialtoPlayerTest::SetUp()
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

			auto destroyedFlag = std::make_shared<std::atomic<bool>>(false);
			if (static_cast<size_t>(type) < m_sourceDestroyed.size())
			{
				m_sourceDestroyed[static_cast<size_t>(type)] = destroyedFlag;
			}
			auto src = std::make_unique<TrackedMockSource>(destroyedFlag);
			auto *rawPtr = src.get();

				ON_CALL(*rawPtr, mediaType())
					.WillByDefault(Return(type));

				// Delegate isInbandCC() to the real format check so
				// that setFormat(FORMAT_INVALID) makes it return true.
				ON_CALL(*rawPtr, isInbandCC())
					.WillByDefault([rawPtr]() {
						return rawPtr->format() == FORMAT_INVALID;
					});
				ON_CALL(*rawPtr, mapCodecToMime(_, _, _, _))
					.WillByDefault(Invoke(
						[type](GstStreamOutputFormat,
							bool,
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
						const int64_t ptsNs      = SecondsToNs(sample.mPts);
						const int64_t durationNs = SecondsToNs(sample.mDuration);
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
			std::move(sourceCreator));
}

void AampRialtoPlayerTest::TearDown()
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

void AampRialtoPlayerTest::PostPlaybackState(firebolt::rialto::PlaybackState state)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr)
		<< "Configure() must be called before PostPlaybackState";
	client->notifyPlaybackState(state);
}

void AampRialtoPlayerTest::PostPosition(int64_t positionNs)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr)
		<< "Configure() must be called before PostPosition";
	client->notifyPosition(positionNs);
}

void AampRialtoPlayerTest::PostDuration(int64_t durationNs)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr)
		<< "Configure() must be called before PostDuration";
	client->notifyDuration(durationNs);
}

void AampRialtoPlayerTest::PostNeedData(int32_t sourceId, size_t frameCount, uint32_t requestId)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr) << "Configure() must be called before PostNeedData";
	client->notifyNeedMediaData(sourceId, frameCount, requestId, nullptr);
}

void AampRialtoPlayerTest::CancelNeedData(int32_t sourceId)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr) << "Configure() must be called before CancelNeedData";
	client->notifyCancelNeedMediaData(sourceId);
}

void AampRialtoPlayerTest::PostBufferUnderflow(int32_t sourceId)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr)
		<< "Configure() must be called before PostBufferUnderflow";
	client->notifyBufferUnderflow(sourceId);
}

void AampRialtoPlayerTest::PostPlaybackError(int32_t sourceId, firebolt::rialto::PlaybackError error)
{
	auto client = m_capturedClient.lock();
	ASSERT_NE(client, nullptr)
		<< "Configure() must be called before PostPlaybackError";
	client->notifyPlaybackError(sourceId, error);
}

void AampRialtoPlayerTest::ResetMockPipeline()
{
	m_mockPipeline = std::make_unique<NiceMock<MockIMediaPipeline>>();
	m_mockPipelinePtr = m_mockPipeline.get();
	ResetMockPipelineDefaults();
}

void AampRialtoPlayerTest::ResetMockPipelineDefaults()
{
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

void AampRialtoPlayerTest::Configure(
	StreamOutputFormat video,
	StreamOutputFormat audio,
	StreamOutputFormat sub)
{
	// The factory lambda moves m_mockPipeline into the player on each call.
	// If it was already consumed (null), recreate it with default behaviours
	// so the player gets a valid pipeline and EXPECT_CALLs on
	// m_mockPipelinePtr target the new pipeline correctly.
	if (!m_mockPipeline)
	{
		ResetMockPipeline();
	}
	m_player->Configure(MakeCodecInfo(video, audio, sub),
		/*bESChangeStatus=*/false,
		/*setReadyAfterPipelineCreation=*/false);
}

AampMediaSample AampRialtoPlayerTest::MakeSample(double pts, double duration)
{
	AampMediaSample s{};
	s.mPts      = pts;
	s.mDuration = duration;
	return s;
}

void AampRialtoPlayerTest::WaitFor(
	const std::function<bool()> &condition,
	std::chrono::milliseconds timeout)
{
	const auto deadline =
		std::chrono::steady_clock::now() + timeout;
	while (!condition() && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void AampRialtoPlayerTest::RepeatUntil(
	const std::function<bool()> &done,
	const std::function<void()> &action)
{
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (!done() && std::chrono::steady_clock::now() < deadline)
	{
		action();
		WaitFor(done, std::chrono::milliseconds(20));
	}
}

void AampRialtoPlayerTest::SendVideoInitFragment()
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() { return MakeVideoH264CodecInfo(); });
	std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0, 0, 0, 0, /*initFragment=*/true);
}

void AampRialtoPlayerTest::SendAudioInitFragment(bool encrypted)
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([encrypted]() {
			MediaCodecInfo ci = MakeAudioAacCodecInfo();
			ci.mIsEncrypted = encrypted;
			return ci;
		});
	std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
	m_player->SendTransfer(eMEDIATYPE_AUDIO, std::move(buf),
		0, 0, 0, 0, /*initFragment=*/true);
}

void AampRialtoPlayerTest::SendEncryptedAudioInitFragmentOnVideoSlot()
{
	ON_CALL(*g_mockMp4Demux, Parse(_)).WillByDefault(Return(true));
	ON_CALL(*g_mockMp4Demux, GetCodecInfo())
		.WillByDefault([]() {
			MediaCodecInfo ci = MakeAudioAacCodecInfo();
			ci.mIsEncrypted = true;
			return ci;
		});
	std::vector<uint8_t> buf = {0x00, 0x00, 0x00, 0x01};
	m_player->SendTransfer(eMEDIATYPE_VIDEO, std::move(buf),
		0, 0, 0, 0, /*initFragment=*/true);
}

void AampRialtoPlayerTest::TriggerProgressTimerTick()
{
	// With the new ProgressTimer implementation, we directly invoke
	// OnProgressTimerTick() to simulate a timer tick.
	m_player->OnProgressTimerTick();
}

void AampRialtoPlayerWithDemuxTest::SetUp()
{
	AampRialtoPlayerTest::SetUp();
	g_mockMp4Demux = std::make_shared<NiceMock<MockMp4Demux>>();
}

void AampRialtoPlayerWithDemuxTest::TearDown()
{
	g_mockMp4Demux.reset();
	g_mockMp4Demux = nullptr;
	AampRialtoPlayerTest::TearDown();
}
