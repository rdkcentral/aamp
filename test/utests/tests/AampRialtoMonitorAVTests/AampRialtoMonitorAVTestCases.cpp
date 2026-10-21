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
 * @file AampRialtoMonitorAVTestCases.cpp
 * @brief L1 unit tests for AampRialtoMonitorAV.
 *
 * Component under test: direct-rialto/AampRialtoMonitorAV.cpp
 *
 * Behavioral contracts tested:
 *   - Timer lifecycle: start() registers two GLib timers; stop() removes them.
 *   - start() is idempotent when already running.
 *   - Destructor calls stop() automatically.
 *   - Sample tick is skipped when isPlayingGetter() returns false.
 *   - AV health classification: "ok", "stall", "eos", "trickplay".
 *   - Report tick skips before any sample has been taken.
 *   - Report tick calls SendMonitorAvEvent with the classification,
 *     positions, time-in-state, and dropped frame count from getStats().
 *   - timeInState is clamped to the report interval.
 *   - Dropped frames are 0 when videoSourceId is -1 or getStats() fails.
 *
 * Oracle derivation:
 *   - Classification logic mirrors GStreamer MonitorAV() in
 *     middleware/InterfacePlayerRDK.cpp.
 *   - Report logic mirrors MonitorAvTimerCallback() in aampgstplayer.cpp.
 *   - In the Rialto path, both video and audio always share the same
 *     pipeline position, so "video freeze" and "audio drop" in isolation
 *     cannot occur — both tracks advance or stall together.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampRialtoMonitorAV.h"
#include "MockIMediaPipeline.h"
#include "MockIStreamSinkNotifiable.h"
#include "MockGLib.h"
#include "MockAampUtils.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgReferee;
using ::testing::StrictMock;

// Required by fake AAMP infrastructure linked via fakes library.
AampConfig *gpGlobalConfig{nullptr};

// ---------------------------------------------------------------------------
// Constants shared across fixture and tests
// ---------------------------------------------------------------------------

static constexpr guint kSampleIntervalMs = 1000u;
static constexpr guint kReportIntervalMs = 2000u;
static constexpr int   kSyncPosThreshMs  = 100;
static constexpr int   kSyncNegThreshMs  = -100;
static constexpr int   kJumpThreshMs     = 500;

static constexpr guint   kSampleTimerId  = 101u;
static constexpr guint   kReportTimerId  = 102u;

static constexpr int64_t kNsPerMs        = 1'000'000LL;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class AampRialtoMonitorAVTest : public ::testing::Test
{
protected:
	// Mocks
	std::shared_ptr<NiceMock<MockGLib>>                    m_mockGLib;
	std::shared_ptr<MockAampUtils>                         m_mockUtils;
	std::shared_ptr<NiceMock<MockIMediaPipeline>>          m_mockPipeline;
	NiceMock<MockIStreamSinkNotifiable>                   *m_mockNotifiable{nullptr};

	// Captured GLib timer callbacks – set by the g_timeout_add interceptor.
	GSourceFunc m_sampleCb{nullptr};
	gpointer    m_sampleData{nullptr};
	GSourceFunc m_reportCb{nullptr};
	gpointer    m_reportData{nullptr};

	// Controlled time value returned by aamp_GetCurrentTimeMS().
	long long m_currentTimeMs{1000LL};

	// The component under test.
	std::unique_ptr<AampRialtoMonitorAV> m_monitor;

	// Lambdas passed to the monitor.
	int32_t m_videoSourceId{1};
	int     m_rate{AAMP_NORMAL_PLAY_RATE};
	bool    m_playing{true};

	void SetUp() override
	{
		m_mockGLib       = std::make_shared<NiceMock<MockGLib>>();
		m_mockUtils      = std::make_shared<MockAampUtils>();
		m_mockPipeline   = std::make_shared<NiceMock<MockIMediaPipeline>>();
		m_mockNotifiable = new NiceMock<MockIStreamSinkNotifiable>();

		g_mockGLib      = m_mockGLib;
		g_mockAampUtils = m_mockUtils;

		// Default: time advances by kSampleIntervalMs each call.
		ON_CALL(*m_mockUtils, aamp_GetCurrentTimeMS())
			.WillByDefault(Invoke([this]() { return m_currentTimeMs; }));

		// Capture sample-timer registration (matched by interval).
		ON_CALL(*m_mockGLib, g_timeout_add(kSampleIntervalMs, _, _))
			.WillByDefault(Invoke(
				[this](guint, GSourceFunc fn, gpointer data) -> guint
				{
					m_sampleCb   = fn;
					m_sampleData = data;
					return kSampleTimerId;
				}));

		// Capture report-timer registration.
		ON_CALL(*m_mockGLib, g_timeout_add(kReportIntervalMs, _, _))
			.WillByDefault(Invoke(
				[this](guint, GSourceFunc fn, gpointer data) -> guint
				{
					m_reportCb   = fn;
					m_reportData = data;
					return kReportTimerId;
				}));

		ON_CALL(*m_mockGLib, g_source_remove(_))
			.WillByDefault(Return(TRUE));

		// Default: getPosition advances position each call.
		ON_CALL(*m_mockPipeline, getPosition(_))
			.WillByDefault(DoAll(
				SetArgReferee<0>(500LL * kNsPerMs),
				Return(true)));

		// Default: getStats reports no dropped frames.
		ON_CALL(*m_mockPipeline, getStats(_, _, _))
			.WillByDefault(DoAll(
				SetArgReferee<1>(uint64_t{100}),
				SetArgReferee<2>(uint64_t{0}),
				Return(true)));

		m_monitor = makeSUT();
	}

	void TearDown() override
	{
		m_monitor.reset();
		delete m_mockNotifiable;
		m_mockNotifiable = nullptr;
		g_mockGLib.reset();
		g_mockAampUtils.reset();
	}

	std::unique_ptr<AampRialtoMonitorAV> makeSUT()
	{
		AampRialtoMonitorAV::Config cfg{
			static_cast<int>(kSampleIntervalMs),
			static_cast<int>(kReportIntervalMs),
			kSyncPosThreshMs,
			kSyncNegThreshMs,
			kJumpThreshMs};

		return std::make_unique<AampRialtoMonitorAV>(
			m_mockPipeline,
			m_mockNotifiable,
			[this]() -> int32_t { return m_videoSourceId; },
			[this]()             { return m_rate; },
			[this]()             { return m_playing; },
			cfg);
	}

	/// Directly invoke the sample timer callback, advancing time by delta.
	void triggerSampleTick(long long timeDeltaMs = 1000LL)
	{
		m_currentTimeMs += timeDeltaMs;
		ASSERT_NE(m_sampleCb, nullptr)
			<< "Sample callback not captured; was start() called?";
		m_sampleCb(m_sampleData);
	}

	/// Directly invoke the report timer callback (time does not advance).
	void triggerReportTick()
	{
		ASSERT_NE(m_reportCb, nullptr)
			<< "Report callback not captured; was start() called?";
		m_reportCb(m_reportData);
	}
};

// ---------------------------------------------------------------------------
// Timer lifecycle
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMonitorAVTest, start_RegistersSampleAndReportTimers)
{
	EXPECT_CALL(*m_mockGLib,
		g_timeout_add(kSampleIntervalMs, _, _)).Times(1);
	EXPECT_CALL(*m_mockGLib,
		g_timeout_add(kReportIntervalMs, _, _)).Times(1);

	m_monitor->start();
}

TEST_F(AampRialtoMonitorAVTest, start_WhenAlreadyRunning_IsIdempotent)
{
	EXPECT_CALL(*m_mockGLib, g_timeout_add(_, _, _)).Times(2);

	m_monitor->start();
	m_monitor->start(); // second call must not register more timers
}

TEST_F(AampRialtoMonitorAVTest, stop_RemovesBothTimerIds)
{
	m_monitor->start();

	EXPECT_CALL(*m_mockGLib, g_source_remove(kSampleTimerId)).Times(1);
	EXPECT_CALL(*m_mockGLib, g_source_remove(kReportTimerId)).Times(1);

	m_monitor->stop();
}

TEST_F(AampRialtoMonitorAVTest, stop_WhenNotRunning_IsIdempotent)
{
	// stop() before start() must not call g_source_remove.
	EXPECT_CALL(*m_mockGLib, g_source_remove(_)).Times(0);
	m_monitor->stop();
}

TEST_F(AampRialtoMonitorAVTest, destructor_StopsTimersWhenRunning)
{
	m_monitor->start();

	EXPECT_CALL(*m_mockGLib, g_source_remove(kSampleTimerId)).Times(1);
	EXPECT_CALL(*m_mockGLib, g_source_remove(kReportTimerId)).Times(1);

	m_monitor.reset(); // triggers destructor
}

// ---------------------------------------------------------------------------
// Sample tick — guard condition
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMonitorAVTest, sampleTick_WhenNotPlaying_SkipsGetPosition)
{
	m_playing = false;
	m_monitor->start();

	EXPECT_CALL(*m_mockPipeline, getPosition(_)).Times(0);

	triggerSampleTick();
}

// ---------------------------------------------------------------------------
// Sample tick — classification
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMonitorAVTest, sampleTick_PositionAdvances_ClassifiesOk)
{
	// After start(), trigger a sample tick.  getPosition returns 500 ms
	// (> initial 0), so both tracks are counted; no freeze/stall/avsync.
	// The component should transition from nullptr to "ok".
	// The report tick should then fire SendMonitorAvEvent with "ok".
	m_monitor->start();
	triggerSampleTick();

	// Verify via the report tick that the description is "ok".
	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		std::string{"ok"}, _, _, _, _)).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest, sampleTick_PositionUnchanged_ClassifiesStall)
{
	// getPosition always returns 0 ns (= 0 ms).  On the first tick both
	// m_avPositionMs[VIDEO] and [AUDIO] are already 0, so the position
	// matches → "stall".
	ON_CALL(*m_mockPipeline, getPosition(_))
		.WillByDefault(DoAll(
			SetArgReferee<0>(int64_t{0}),
			Return(true)));

	m_monitor->start();
	triggerSampleTick();

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		std::string{"stall"}, _, _, _, _)).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest, sampleTick_GetPositionFails_ClassifiesEos)
{
	// When getPosition() returns false the track is skipped → numTracks == 0
	// → description = "eos".
	ON_CALL(*m_mockPipeline, getPosition(_))
		.WillByDefault(Return(false));

	m_monitor->start();
	triggerSampleTick();

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		std::string{"eos"}, _, _, _, _)).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest, sampleTick_TrickplayRate_ClassifiesTrickplay)
{
	// When rate != AAMP_NORMAL_PLAY_RATE only the video track is sampled
	// (maxTracks == 1), so numTracks == 1 → description = "trickplay".
	m_rate = 2; // fast-forward

	m_monitor->start();
	triggerSampleTick();

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		std::string{"trickplay"}, _, _, _, _)).Times(1);

	triggerReportTick();
}

// ---------------------------------------------------------------------------
// Report tick — event firing
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMonitorAVTest, reportTick_BeforeFirstSample_SkipsSendEvent)
{
	// The report timer fires before any sample tick has been taken.
	// m_tLastSampled == 0 and m_description == nullptr → skip.
	m_monitor->start();

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(_, _, _, _, _)).Times(0);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest, reportTick_AfterSample_PassesDroppedFrames)
{
	constexpr uint64_t kDropped = 7u;

	ON_CALL(*m_mockPipeline, getStats(m_videoSourceId, _, _))
		.WillByDefault(DoAll(
			SetArgReferee<1>(uint64_t{200}),
			SetArgReferee<2>(kDropped),
			Return(true)));

	m_monitor->start();
	triggerSampleTick(); // position=500ms → "ok"

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		_, _, _, _, kDropped)).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest,
	reportTick_WhenVideoSourceIdNegative_ReportsZeroDropped)
{
	m_videoSourceId = -1;

	m_monitor->start();
	triggerSampleTick(); // position=500ms → "ok"

	// getStats must NOT be called when sourceId < 0.
	EXPECT_CALL(*m_mockPipeline, getStats(_, _, _)).Times(0);
	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		_, _, _, _, uint64_t{0})).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest,
	reportTick_WhenGetStatsFails_ReportsZeroDropped)
{
	ON_CALL(*m_mockPipeline, getStats(_, _, _))
		.WillByDefault(Return(false));

	m_monitor->start();
	triggerSampleTick(); // position=500ms → "ok"

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		_, _, _, _, uint64_t{0})).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest,
	reportTick_TimeInState_ClampedToReportInterval)
{
	// Set up a sequence of time values:
	//   Tick 1 (t=1000): position=500ms → "ok"; m_tLastReported=1000.
	//   Tick 2 (t=2000): position=500ms (unchanged) → "stall";
	//                     m_tLastReported=2000.
	//   Tick 3 (t=10000): position=500ms (unchanged) → still "stall",
	//                      no state change; m_tLastSampled=10000.
	//   Report:  timeInState = 10000-2000 = 8000 ms
	//            → capped to kReportIntervalMs (2000 ms).

	// Time sequence: we advance m_currentTimeMs manually per tick.
	m_currentTimeMs = 0LL; // start before first tick

	// Position: first call returns 500ms; subsequent calls return 500ms too
	// (position doesn't advance → stall on tick 2+).
	ON_CALL(*m_mockPipeline, getPosition(_))
		.WillByDefault(DoAll(
			SetArgReferee<0>(500LL * kNsPerMs),
			Return(true)));

	m_monitor->start();

	// Tick 1: t=1000, position=500ms → "ok" (500 != initial 0).
	triggerSampleTick(1000LL);

	// Tick 2: t=2000, position=500ms again → frozen → "stall".
	triggerSampleTick(1000LL);

	// Tick 3: t=10000, still stall; m_tLastReported stays at 2000.
	triggerSampleTick(8000LL);

	// timeInState = 10000-2000 = 8000, capped to kReportIntervalMs = 2000.
	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		_, _, _, uint64_t{kReportIntervalMs}, _)).Times(1);

	triggerReportTick();
}

TEST_F(AampRialtoMonitorAVTest,
	reportTick_PassesCorrectVideoAndAudioPositions)
{
	// Positions are set to 750ms; both video and audio should be 750.
	constexpr int64_t kPosMs = 750LL;

	ON_CALL(*m_mockPipeline, getPosition(_))
		.WillByDefault(DoAll(
			SetArgReferee<0>(kPosMs * kNsPerMs),
			Return(true)));

	m_monitor->start();
	triggerSampleTick();

	EXPECT_CALL(*m_mockNotifiable, SendMonitorAvEvent(
		_, kPosMs, kPosMs, _, _)).Times(1);

	triggerReportTick();
}
