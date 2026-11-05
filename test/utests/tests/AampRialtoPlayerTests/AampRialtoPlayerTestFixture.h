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
 * @file AampRialtoPlayerTestFixture.h
 * @brief Fixtures and helpers shared by the AampRialtoPlayer L1 test files.
 *
 * Tests verify the player's orchestration of its dependencies (pipeline,
 * sources, DRM bridge, notifiable) using mock objects.  Per-source behaviour
 * (codec mapping, segment creation, injection) is covered by the dedicated
 * AampRialtoVideoSourceTests, AampRialtoAudioSourceTests, and
 * AampRialtoSubtitleSourceTests suites.
 */

#ifndef AAMP_RIALTO_PLAYER_TEST_FIXTURE_H
#define AAMP_RIALTO_PLAYER_TEST_FIXTURE_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <thread>

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaPipelineClient.h"
#include "AampRialtoMediaSource.h"
#include "AampSegmentInfo.hpp"
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
#include "MockPlayerCCManager.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::DoubleEq;
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
inline MediaCodecInfo MakeVideoH264CodecInfo(
	uint32_t width = 1280, uint32_t height = 720)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat               = GST_FORMAT_VIDEO_ES_H264;
	ci.mInfo.video.mWidth         = width;
	ci.mInfo.video.mHeight        = height;
	ci.mCodecData                 = {0x01, 0x02, 0x03}; // fake SPS/PPS
	return ci;
}

inline MediaCodecInfo MakeVideoHevcCodecInfo(
	uint32_t width = 1920, uint32_t height = 1080)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat               = GST_FORMAT_VIDEO_ES_HEVC;
	ci.mInfo.video.mWidth         = width;
	ci.mInfo.video.mHeight        = height;
	ci.mCodecData                 = {0x01, 0x02};
	return ci;
}

inline MediaCodecInfo MakeAudioAacCodecInfo(
	uint32_t channels = 2, uint32_t sampleRate = 48000)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat                  = GST_FORMAT_AUDIO_ES_AAC_RAW;
	ci.mInfo.audio.mChannelCount     = channels;
	ci.mInfo.audio.mSampleRate       = sampleRate;
	ci.mCodecData                    = {0xAA, 0xBB};
	return ci;
}

inline MediaCodecInfo MakeSubtitleTtmlCodecInfo()
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_SUBTITLE_TTML;
	return ci;
}

/// Mock source that records its destruction through a flag the test keeps.
class TrackedMockSource : public NiceMock<MockAampRialtoMediaSource>
{
public:
	explicit TrackedMockSource(std::shared_ptr<std::atomic<bool>> destroyed)
		: m_destroyed(std::move(destroyed))
	{
	}

	~TrackedMockSource() override { m_destroyed->store(true); }

private:
	std::shared_ptr<std::atomic<bool>> m_destroyed;
};

/// Mock pipeline that records its destruction through a flag the test keeps.
class TrackedMockPipeline : public NiceMock<MockIMediaPipeline>
{
public:
	explicit TrackedMockPipeline(std::shared_ptr<std::atomic<bool>> destroyed)
		: m_destroyed(std::move(destroyed))
	{
	}

	~TrackedMockPipeline() override { m_destroyed->store(true); }

private:
	std::shared_ptr<std::atomic<bool>> m_destroyed;
};

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
	void SetUp() override;

	void TearDown() override;

	/// Post a playback-state notification via the captured client.
	void PostPlaybackState(firebolt::rialto::PlaybackState state);

	/// Post a position notification (nanoseconds) via the captured client.
	void PostPosition(int64_t positionNs);

	/// Post a duration notification (nanoseconds) via the captured client.
	void PostDuration(int64_t durationNs);

	/// Post a needData event via the real IPC callback path.
	void PostNeedData(int32_t sourceId, size_t frameCount, uint32_t requestId);

	/// Cancel pending needData requests via the real IPC callback path.
	void CancelNeedData(int32_t sourceId);

	/// Post a Rialto buffer-underflow notification via the captured client.
	void PostBufferUnderflow(int32_t sourceId);

	/// Post a Rialto playback-error notification via the captured client.
	void PostPlaybackError(int32_t sourceId, firebolt::rialto::PlaybackError error);

	/// Call Configure() with specific formats.
	/// Recreate m_mockPipeline with fresh default behaviours.
	/// Must be called before every player Configure() so the factory lambda
	/// has a valid pipeline to move into the player.
	void ResetMockPipeline();

	/// Apply the default behaviours to m_mockPipelinePtr.  Use directly after
	/// assigning a custom MockIMediaPipeline subclass to m_mockPipeline.
	void ResetMockPipelineDefaults();

	/// Build the StreamCodecInfo that StreamSink::Configure() receives from
	/// AAMP. GstStreamOutputFormat mirrors StreamOutputFormat enumerator for
	/// enumerator (see PrivateInstanceAAMP::GetMediaCodecInfo()).
	static StreamCodecInfo MakeCodecInfo(
		StreamOutputFormat video,
		StreamOutputFormat audio,
		StreamOutputFormat sub)
	{
		StreamCodecInfo codecInfo;
		codecInfo.video.mCodecFormat = static_cast<GstStreamOutputFormat>(video);
		codecInfo.audio.mCodecFormat = static_cast<GstStreamOutputFormat>(audio);
		codecInfo.subtitle.mCodecFormat = static_cast<GstStreamOutputFormat>(sub);
		return codecInfo;
	}

	void Configure(
		StreamOutputFormat video = FORMAT_ISO_BMFF,
		StreamOutputFormat audio = FORMAT_ISO_BMFF,
		StreamOutputFormat sub   = FORMAT_INVALID);

	/// Build a minimal AampMediaSample.
	AampMediaSample MakeSample(double pts = 0.1, double duration = 0.033);

	/// Spin-poll until condition() returns true or the timeout elapses.
	static void WaitFor(
		const std::function<bool()> &condition,
		std::chrono::milliseconds timeout = std::chrono::seconds(5));

	/// Run action() until done() is true.  A sender that only reaches its
	/// blocking wait after the first action() missed that wake-up, so retry.
	static void RepeatUntil(
		const std::function<bool()> &done,
		const std::function<void()> &action);

	/// Trigger initFragment SendTransfer — relies on the mock source's
	/// attachOrUpdate (inherited from base class).
	void SendVideoInitFragment();

	/// Trigger initFragment SendTransfer for audio.
	void SendAudioInitFragment(bool encrypted = false);

	void TriggerProgressTimerTick();

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
	/// Destruction flag of the most recently created source of each type.
	std::array<std::shared_ptr<std::atomic<bool>>, 3>           m_sourceDestroyed{};
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
	void SetUp() override;

	void TearDown() override;
};

#endif /* AAMP_RIALTO_PLAYER_TEST_FIXTURE_H */
