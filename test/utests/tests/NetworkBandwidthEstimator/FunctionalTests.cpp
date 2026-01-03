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

#include <gtest/gtest.h>
#include "NetworkBandwidthEstimator.h"
#include <curl/curl.h>
#include <cstdlib>
#include <array>

class FunctionalTests : public ::testing::Test {
protected:
	FunctionalTests()
	{
	}
};


/**
 * @brief libcurl progress callback (XFERINFOFUNCTION)
 * This is called at frequent intervals allowing application to monitor progress and abort stalled transfers
 * Note: this may be called even after all data has been downloaded while final logic is executed
 *
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 * @param ultotal total bytes to upload
 * @param ulnow uploaded bytes so far
 */
static int xferinfo(void *clientp,
					curl_off_t dltotal, curl_off_t dlnow,
					curl_off_t , curl_off_t )
{
	DownloadContext *context = static_cast<DownloadContext*>(clientp);
	const double now = GetCurrentTimeMonotonicSeconds();
	return context->xferinfo( now, dltotal, dlnow );
}

static size_t write_callback(char *, size_t size, size_t nmemb, void *)
{ // stub to avoid spewing download contents to console log
	return size*nmemb;
}

TEST_F(FunctionalTests, MedianTest)
{
	std::vector<double> values;
	EXPECT_EQ( GetMedian(values), 0.0 );
	values.push_back(10.0);
	EXPECT_EQ( GetMedian(values), 10.0 );
	values.push_back(30.0);
	EXPECT_EQ( GetMedian(values), 20.0 );
	values.push_back(30.0);
	EXPECT_EQ( GetMedian(values), 30.0 );
	values.push_back(20.0);
	EXPECT_EQ( GetMedian(values), 25.0 );
}

TEST_F(FunctionalTests, ThroughputPredictionTest)
{
	const double epsilon = 1e-6;
	const size_t segment_size_bytes = 112463;
	const struct
	{
		double timeToFirstByteSeconds;
		double throughputBytesPerSecond;
		double predictedDownloadTimeSeconds;
		
		size_t download_bytes;
		double total_time_seconds;
		double time_to_first_byte_seconds;
	} test_data[] =
	{
		{ 0.000000,0.000000,0.000000,112463,0.398257,0.249254 },
		{ 0.249254,754770.038187,0.398257,112463,0.130057,0.048034 },
		{ 0.148644,935373.273008,0.268877,112463,0.120855,0.056830 },
		{ 0.056830,1107592.676445,0.158368,112463,0.119009,0.057246 },
		{ 0.057038,1239315.381966,0.147784,112463,0.116028,0.048744 },
		{ 0.056830,1315556.262523,0.142317,112463,0.117878,0.052516 },
		{ 0.054673,1380828.841638,0.136119,112463,0.116275,0.051314 },
		{ 0.052516,1433386.044995,0.130976,112463,0.116544,0.051999 }
	};
	
	NetworkBandwidthEstimator networkBandwidthEstimator;
	for( int i=0; i<sizeof(test_data)/sizeof(test_data[0]); i++ )
	{
		EXPECT_NEAR( test_data[i].timeToFirstByteSeconds, networkBandwidthEstimator.GetTimeToFirstByteSeconds(), epsilon );
		EXPECT_NEAR( test_data[i].throughputBytesPerSecond, networkBandwidthEstimator.GetThroughputBytesPerSecond(), epsilon );
		EXPECT_NEAR( test_data[i].predictedDownloadTimeSeconds, networkBandwidthEstimator.GetPredictedDownloadTimeSeconds(segment_size_bytes), epsilon );
		
		CurlInfo curlInfo;
		curlInfo.m_size_download_bytes = test_data[i].download_bytes;
		curlInfo.m_total_time_seconds = test_data[i].total_time_seconds;
		curlInfo.m_time_to_first_byte_seconds = test_data[i].time_to_first_byte_seconds;
		networkBandwidthEstimator.UpdateDownloadMetrics(curlInfo);
	}
}

TEST_F(FunctionalTests, MidDownloadMonitoringTest)
{
	const double epsilon = 1e-6;
	struct TestData
	{
		double now;
		double pct;
		size_t dlnow;
		size_t dltotal;
		double Bps;
		double estimated_remaining;
	};
	const std::array<TestData, 8> test_data{{
		{271850.024298,0.000000},
		{271850.227916,14,15908,112463,78126.688199,1.235877},
		{271850.259620,28,32274,112463,253360.998893,0.316501},
		{271850.259914,43,48640,112463,22418685.647001,0.002847},
		{271850.294995,56,63960,112463,13625892.839653,0.003560},
		{271850.295399,71,80326,112463,24379496.450812,0.001318},
		{271850.317906,85,96692,112463,14918558.491420,0.001057},
		{271850.326387,100,112463,112463,9694962.476504,0.000000}
	}};
	DownloadContext downloadContext;
	downloadContext.Reset(test_data[0].now );
	for( size_t i=1; i<test_data.size(); i++ )
	{
		downloadContext.xferinfo( test_data[i].now, test_data[i].dltotal, test_data[i].dlnow);
		EXPECT_NEAR( test_data[i].estimated_remaining, downloadContext.GetEstimatedRemainingTime(), epsilon );
		EXPECT_NEAR( test_data[i].Bps, downloadContext.GetEstimatedThroughputBytesPerSecond(), epsilon );
	}
}
