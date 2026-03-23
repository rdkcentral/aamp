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
 * @file Mp4DemuxMetricsTests.cpp
 * @brief L1 unit tests for Mp4Demux::RecordDemuxMetrics
 */

#include <gtest/gtest.h>
#include "MP4Demux.h"

/**
 * @class Mp4DemuxMetricsTests
 * @brief L1 unit test fixture for Mp4Demux metrics and logging.
 *
 * Provides a test environment for validating metrics tracking, logging,
 * and interval-based reporting in the Mp4Demux class.
 */
class Mp4DemuxMetricsTests : public ::testing::Test {
protected:
	/**
	 * @brief Mp4Demux instance under test.
	 */
	Mp4Demux demuxer;
};

/**
 * @test ShouldLogMetrics_OnlyAfterInterval
 * @brief Validates that ShouldLogMetrics returns false before 10 minutes have elapsed.
 * Simulates metric updates and checks logging interval logic via public API.
 */
TEST_F(Mp4DemuxMetricsTests, ShouldLogMetrics_OnlyAfterInterval)
{
	EXPECT_FALSE(demuxer.ShouldLogMetrics()) << "ShouldLogMetrics should be false right after creation";
	for (int i = 0; i < 5; ++i) {
		demuxer.RecordDemuxMetrics(5.0, 10.0);
		EXPECT_FALSE(demuxer.ShouldLogMetrics()) << "ShouldLogMetrics should still be false before 10 minutes";
	}
	SUCCEED();
}

/**
 * @test RecordDemuxMetrics_ZeroDemuxTime_ZeroFps_NoCrash
 * @brief Validates that RecordDemuxMetrics handles zero demux time and zero fps without crashing.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_ZeroDemuxTime_ZeroFps_NoCrash)
{
	EXPECT_NO_THROW(demuxer.RecordDemuxMetrics(0.0, 0.0));
	EXPECT_FALSE(demuxer.ShouldLogMetrics());
}

/**
 * @test RecordDemuxMetrics_NonZeroDemuxTime_ZeroFps_NoCrash
 * @brief Validates that RecordDemuxMetrics accepts non-zero demux time with zero fps without crashing.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_NonZeroDemuxTime_ZeroFps_NoCrash)
{
	EXPECT_NO_THROW(demuxer.RecordDemuxMetrics(10.0, 0.0));
	EXPECT_FALSE(demuxer.ShouldLogMetrics());
}

/**
 * @test RecordDemuxMetrics_ZeroDemuxTime_NonZeroFps_NoCrash
 * @brief Validates that RecordDemuxMetrics accepts zero demux time with non-zero fps without crashing.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_ZeroDemuxTime_NonZeroFps_NoCrash)
{
	EXPECT_NO_THROW(demuxer.RecordDemuxMetrics(0.0, 10.0));
	EXPECT_FALSE(demuxer.ShouldLogMetrics());
}

/**
 * @test RecordDemuxMetrics_LargeValues_NoCrash
 * @brief Stress test with large demuxTimeMs and fps values to ensure no overflow or crash.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_LargeValues_NoCrash)
{
	EXPECT_NO_THROW(demuxer.RecordDemuxMetrics(99999.9, 27.7777777778));
	EXPECT_FALSE(demuxer.ShouldLogMetrics());
}

/**
 * @test RecordDemuxMetrics_ThenLogMetrics_NoCrash
 * @brief Validates that recording metrics immediately followed by LogMetrics does not crash.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_ThenLogMetrics_NoCrash)
{
	demuxer.RecordDemuxMetrics(8.0, 24.0);
	EXPECT_NO_THROW(demuxer.LogMetrics());
}

/**
 * @test LogMetrics_CalledRepeatedly_NoCrash
 * @brief Validates that calling LogMetrics multiple times in succession does not crash.
 */
TEST_F(Mp4DemuxMetricsTests, LogMetrics_CalledRepeatedly_NoCrash)
{
	demuxer.RecordDemuxMetrics(5.0, 10.0);
	for (int i = 0; i < 5; ++i) {
		EXPECT_NO_THROW(demuxer.LogMetrics());
	}
}

/**
 * @test RecordDemuxMetrics_MultipleVariedFrameCounts_ShouldLogMetrics_False
 * @brief Validates that varied demux time and fps values across multiple calls
 * do not prematurely trigger the logging interval.
 */
TEST_F(Mp4DemuxMetricsTests, RecordDemuxMetrics_MultipleVariedFrameCounts_ShouldLogMetrics_False)
{
	demuxer.RecordDemuxMetrics(1.0,  2.0);
	demuxer.RecordDemuxMetrics(15.0, 30.0);
	demuxer.RecordDemuxMetrics(30.0, 30.0);
	demuxer.RecordDemuxMetrics(0.0,  0.0);
	demuxer.RecordDemuxMetrics(50.0, 30.0);
	EXPECT_FALSE(demuxer.ShouldLogMetrics());
}




