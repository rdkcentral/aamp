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
 * @file NetPersonaFitterTestCases.cpp
 * @brief L1 tests for aamptrace::NetPersonaFitter
 *
 * Validates the statistical fitting logic ported from persona_fit.py.
 * Feeds known request/burst data and verifies the generated persona JSON
 * contains all expected fields with values within tolerance.
 */

#include "net_persona_fitter.h"
#include <gtest/gtest.h>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <cstdio>
#include <unistd.h>

namespace {

/// Read a file into a string
std::string ReadFile(const std::string& path)
{
	std::ifstream ifs{path};
	if (!ifs.is_open()) return {};
	std::ostringstream ss;
	ss << ifs.rdbuf();
	return ss.str();
}

/// Build the PID-suffixed output path matching GeneratePersonaJson behavior
std::string GetOutputPath(const std::string& basePath)
{
	return basePath + "." + std::to_string(getpid());
}

/// Extract a numeric value from a flat JSON string for the given key.
/// Returns NaN if the key is not found or the value is not parseable.
double ExtractJsonDouble(const std::string& json, const std::string& key)
{
	std::string needle = "\"" + key + "\": ";
	auto pos = json.find(needle);
	if (pos == std::string::npos) return std::numeric_limits<double>::quiet_NaN();
	pos += needle.size();
	try { return std::stod(json.substr(pos)); }
	catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
}

} // anonymous namespace

/**
 * @brief Test fixture for NetPersonaFitter
 *
 * Note: NetPersonaFitter is a Meyer's singleton so data accumulates across tests.
 * Tests are designed to be additive — each test adds data on top of previous ones.
 * The fixture manages cleanup of output files.
 */
class NetPersonaFitterTest : public ::testing::Test
{
protected:
	static constexpr const char* kBasePath = "/tmp/aamp_net_persona_test.json";

	void SetUp() override
	{
		// Isolate the process-wide singleton between cases so results are
		// order-independent whether ctest runs each test in its own process
		// (gtest_discover_tests) or the whole suite in one (RDK-E mode).
		aamptrace::NetPersonaFitter::GetInstance().Reset();
	}

	void TearDown() override
	{
		// Clean up the test-specific output file
		std::remove(GetOutputPath(kBasePath).c_str());
		// Clean up the atexit-registered default output file so CI runs
		// do not leave artifacts under /tmp
		std::remove(GetOutputPath(aamptrace::NetPersonaFitter::kDefaultBasePath).c_str());
	}
};

/**
 * @brief Verify GeneratePersonaJson returns false with no data
 *
 * The singleton starts empty on first use. With no AddRequest/AddBurst calls,
 * generation should fail gracefully.
 */
TEST_F(NetPersonaFitterTest, EmptyDataReturnsFalse)
{
	// Fresh singleton has no data from prior tests (first test to run)
	// But since it's a singleton, we can only test this if it's truly empty.
	// We use a separate fitter instance approach here — but since GetInstance()
	// is singleton, we just verify the counts.
	// If data was already added by another test, skip this check.
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	if (fitter.GetRequestCount() == 0 && fitter.GetBurstCount() == 0)
	{
		EXPECT_FALSE(fitter.GeneratePersonaJson(kBasePath));
	}
}

/**
 * @brief Feed realistic request/burst data and verify persona JSON output
 *
 * Simulates 30 HTTP requests with a mix of reused/fresh connections and
 * multiple bursts per request. Verifies all 19 persona fields are present
 * in the output JSON with sane values.
 */
TEST_F(NetPersonaFitterTest, RealisticDataProducesValidJson)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();

	// Simulate 30 requests: 20 reused connections, 10 fresh
	// Reused TTFB: ~25ms with jitter; Fresh TTFB: ~60ms with jitter
	uint64_t reqId = 1;
	for (int i = 0; i < 20; ++i)
	{
		double ttfb = 0.025 + (i % 5) * 0.002; // 25-33ms range
		fitter.AddRequest(ttfb, /*connReused=*/1);

		// 4 bursts per request with ~200ms cadence gaps
		for (int b = 0; b < 4; ++b)
		{
			double gap = (b == 0) ? 0.0 : 0.200 + (b % 3) * 0.010;
			double dur = 0.010 + (b % 4) * 0.005; // 10-25ms
			std::size_t bytes = 50000 + b * 10000;
			fitter.AddBurst(reqId, b, dur, bytes, gap);
		}
		++reqId;
	}

	for (int i = 0; i < 10; ++i)
	{
		double ttfb = 0.060 + (i % 3) * 0.005; // 60-70ms range
		fitter.AddRequest(ttfb, /*connReused=*/0);

		for (int b = 0; b < 3; ++b)
		{
			double gap = (b == 0) ? 0.0 : 0.200 + (b % 2) * 0.015;
			double dur = 0.012 + (b % 3) * 0.003;
			std::size_t bytes = 40000 + b * 15000;
			fitter.AddBurst(reqId, b, dur, bytes, gap);
		}
		++reqId;
	}

	EXPECT_GE(fitter.GetRequestCount(), 30u);
	EXPECT_GE(fitter.GetBurstCount(), 100u);

	// Generate persona JSON
	ASSERT_TRUE(fitter.GeneratePersonaJson(kBasePath));

	// Read the output file
	std::string outputPath = GetOutputPath(kBasePath);
	std::string json = ReadFile(outputPath);
	ASSERT_FALSE(json.empty()) << "Persona JSON file is empty or missing: " << outputPath;

	// Verify all 19 fields are present
	const char* expectedFields[] = {
		"base_rtt_ms", "rtt_jitter_ms", "ttfb_spike_p", "ttfb_spike_ms",
		"mean_thr_mbps", "thr_sigma_ln", "thr_rho",
		"bursts_per_segment", "burst_bytes_cv",
		"cadence_ms", "cadence_jitter_ms", "flush_jitter_ms",
		"late_chunk_p", "late_chunk_extra_ms",
		"p_conn_reuse", "new_conn_penalty_ms",
		"capacity_drop_p", "capacity_drop_factor", "rtt_inflation_ms"
	};
	for (const auto* field : expectedFields)
	{
		EXPECT_NE(json.find(field), std::string::npos) << "Missing field: " << field;
	}

	// Verify RTT estimates are in sane range
	double baseRtt = ExtractJsonDouble(json, "base_rtt_ms");
	EXPECT_GT(baseRtt, 20.0) << "base_rtt_ms too low";
	EXPECT_LT(baseRtt, 40.0) << "base_rtt_ms too high (should reflect reused conns ~25-33ms)";

	// Verify connection reuse probability
	double pReuse = ExtractJsonDouble(json, "p_conn_reuse");
	EXPECT_NEAR(pReuse, 20.0 / 30.0, 0.05);

	// Verify new connection penalty is positive (fresh > reused)
	double penalty = ExtractJsonDouble(json, "new_conn_penalty_ms");
	EXPECT_GT(penalty, 0.0);

	// Verify bursts per segment is reasonable (we fed 3-4 per request)
	double bps = ExtractJsonDouble(json, "bursts_per_segment");
	EXPECT_GE(bps, 3.0);
	EXPECT_LE(bps, 5.0);

	// Verify cadence is in ~200ms range (our simulated gap)
	double cadence = ExtractJsonDouble(json, "cadence_ms");
	EXPECT_GT(cadence, 150.0);
	EXPECT_LT(cadence, 250.0);

	// Verify throughput is positive and finite
	double thr = ExtractJsonDouble(json, "mean_thr_mbps");
	EXPECT_GT(thr, 0.0);
	EXPECT_TRUE(std::isfinite(thr));

	// Static defaults
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "capacity_drop_p"),      0.0);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "capacity_drop_factor"),  0.6);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "rtt_inflation_ms"),      0.0);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "flush_jitter_ms"),       6.0);
}

/**
 * @brief Verify that AddRequest and AddBurst accumulate counts correctly
 */
TEST_F(NetPersonaFitterTest, CountsAccumulate)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	auto prevReq = fitter.GetRequestCount();
	auto prevBur = fitter.GetBurstCount();

	fitter.AddRequest(0.030, 1);
	fitter.AddBurst(9999, 0, 0.010, 50000, 0.0);
	fitter.AddBurst(9999, 1, 0.015, 60000, 0.200);

	EXPECT_EQ(fitter.GetRequestCount(), prevReq + 1);
	EXPECT_EQ(fitter.GetBurstCount(), prevBur + 2);
}

/**
 * @brief Verify output file path includes PID suffix
 */
TEST_F(NetPersonaFitterTest, OutputPathIncludesPid)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();

	// Ensure there's some data
	fitter.AddRequest(0.025, 1);
	fitter.AddBurst(10000, 0, 0.010, 50000, 0.0);

	ASSERT_TRUE(fitter.GeneratePersonaJson(kBasePath));

	std::string expectedPath = GetOutputPath(kBasePath);
	std::ifstream ifs{expectedPath};
	EXPECT_TRUE(ifs.good()) << "Expected file at: " << expectedPath;
}

// ======================== Inline streaming persona (O(1)) ========================

/**
 * @brief With no streaming data, BuildMinimalPersonaJson returns an empty string
 */
TEST_F(NetPersonaFitterTest, StreamingEmptyReturnsEmptyJson)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();
	EXPECT_TRUE(fitter.BuildMinimalPersonaJson().empty());
}

/**
 * @brief Streaming accumulators produce the expected minimal persona values
 *
 * Feeds a deterministic set: 4 requests (3 reused, 1 fresh) and 2 bursts with
 * a fixed rate (1e7 B/s -> 80 Mbps) and guard-band gaps {0.20, 0.30} s.
 */
TEST_F(NetPersonaFitterTest, StreamingComputesMinimalFields)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	fitter.AddRequest(0.030, 1);
	fitter.AddRequest(0.031, 1);
	fitter.AddRequest(0.032, 1);
	fitter.AddRequest(0.061, 0);

	// Identical rate -> zero throughput spread; gaps 0.20 & 0.30 -> cadence 250ms
	fitter.AddBurst(1, 0, 0.010, 100000, 0.20);
	fitter.AddBurst(1, 1, 0.010, 100000, 0.30);
	// Summarize the one request's 2 equal-sized bursts, as NetTrace::FlushCsv does.
	fitter.AddRequestBurstSummary(/*burstCount=*/2, /*bytesSum=*/200000.0, /*bytesSumSq=*/2.0e10);

	std::string json = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(json.empty());

	EXPECT_NEAR(ExtractJsonDouble(json, "mean_thr_mbps"),     80.0, 1e-6);
	EXPECT_NEAR(ExtractJsonDouble(json, "thr_sigma_ln"),       0.0, 1e-9);
	EXPECT_NEAR(ExtractJsonDouble(json, "cadence_ms"),       250.0, 1e-6);
	EXPECT_NEAR(ExtractJsonDouble(json, "cadence_jitter_ms"), 70.71067811865476, 1e-6);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "flush_jitter_ms"), 6.0);
	EXPECT_NEAR(ExtractJsonDouble(json, "p_conn_reuse"),      0.75, 1e-9);

	// Group A median/percentile RTT fields are now produced from fixed-memory
	// TTFB histograms. Oracles (1 ms buckets, midpoint convention):
	//   TTFB samples ms = {30, 31, 32 (reused), 61 (fresh)}; reused < 5 -> use all.
	//   median -> bucket 31 midpoint = 31.5
	//   P25 -> bucket 30 midpoint = 30.5, P75 -> bucket 32 midpoint = 32.5
	//   rtt_jitter = IQR/1.349 = (32.5 - 30.5)/1.349
	//   reused < 5 -> new_conn_penalty = max(0, base_rtt * 0.95)
	EXPECT_NEAR(ExtractJsonDouble(json, "base_rtt_ms"),         31.5,         1e-6);
	EXPECT_NEAR(ExtractJsonDouble(json, "rtt_jitter_ms"),       2.0 / 1.349,  1e-6);
	EXPECT_NEAR(ExtractJsonDouble(json, "new_conn_penalty_ms"), 31.5 * 0.95,  1e-6);

	// Group B burst-shape fields from fixed-memory histograms. Oracles:
	//   bursts_per_segment: count 2 -> bucket 2 midpoint 2.5 -> floor 2
	//   burst_bytes_cv: two equal-sized bursts -> CV 0; bucket-0 midpoint = 0.005
	//     (0.01 CV bucket width, +/- half-bucket error)
	EXPECT_NEAR(ExtractJsonDouble(json, "bursts_per_segment"), 2.0,   1e-9);
	EXPECT_NEAR(ExtractJsonDouble(json, "burst_bytes_cv"),     0.005, 1e-9);

	// Group C tail fields are now present; this dataset triggers none of them:
	//   reused count 3 < 20 -> no TTFB spike stats
	//   gaps {200, 300} ms are both below cadence + 2*jitter (391.4 ms) -> no late gaps
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "ttfb_spike_p"),        0.0);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "ttfb_spike_ms"),       0.0);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "late_chunk_p"),        0.0);
	EXPECT_DOUBLE_EQ(ExtractJsonDouble(json, "late_chunk_extra_ms"), 0.0);
}

/**
 * @brief ttfb_spike_p / ttfb_spike_ms are the fraction and mean excess of reused
 *        TTFB above P90 (requires >= 20 reused samples).
 *
 * Feeds 20 reused requests with TTFB = 20..39 ms (1 ms apart). Oracles use the
 * 1 ms TTFB histogram and its midpoint convention:
 *   median (base_rtt) -> bucket 29 midpoint 29.5
 *   P90 -> bucket 37 midpoint 37.5; tail buckets {37,38,39} -> 3 samples
 *   ttfb_spike_p = 3/20 = 0.15
 *   tail mean = (37.5+38.5+39.5)/3 = 38.5; ttfb_spike_ms = 38.5 - 29.5 = 9.0
 */
TEST_F(NetPersonaFitterTest, StreamingTtfbSpikeTail)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	for (int i = 0; i < 20; ++i)
	{
		fitter.AddRequest((20.0 + i) / 1000.0, /*connReused=*/1);
	}

	std::string json = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(json.empty());

	EXPECT_NEAR(ExtractJsonDouble(json, "base_rtt_ms"),   29.5, 1e-9);
	EXPECT_NEAR(ExtractJsonDouble(json, "ttfb_spike_p"),  0.15, 1e-9);
	EXPECT_NEAR(ExtractJsonDouble(json, "ttfb_spike_ms"), 9.0,  1e-9);
}

/**
 * @brief late_chunk_p / late_chunk_extra_ms are the fraction and mean excess of
 *        inter-burst gaps above the dynamic threshold cadence + 2*jitter.
 *
 * Gaps (s): {0.00, 0.20, 0.30, 0.80}. Guard-band gaps {0.20, 0.30} give
 * cadence 250 ms and jitter 70.7107 ms -> threshold 391.42 ms. Only the 800 ms
 * gap exceeds it (5 ms gap buckets):
 *   late_chunk_p = 1/4 = 0.25
 *   tail mean = bucket 160 midpoint 802.5; late_chunk_extra_ms = 802.5 - 250 = 552.5
 */
TEST_F(NetPersonaFitterTest, StreamingLateChunkTail)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	fitter.AddRequest(0.030, 1);
	fitter.AddBurst(1, 0, 0.010, 100000, 0.00);
	fitter.AddBurst(1, 1, 0.010, 100000, 0.20);
	fitter.AddBurst(1, 2, 0.010, 100000, 0.30);
	fitter.AddBurst(1, 3, 0.010, 100000, 0.80);

	std::string json = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(json.empty());

	EXPECT_NEAR(ExtractJsonDouble(json, "cadence_ms"),          250.0, 1e-6);
	EXPECT_NEAR(ExtractJsonDouble(json, "late_chunk_p"),        0.25,  1e-9);
	EXPECT_NEAR(ExtractJsonDouble(json, "late_chunk_extra_ms"), 552.5, 1e-9);
}

/**
 * @brief bursts_per_segment is the (approximate) median of per-request burst counts
 *
 * Feeds three request summaries with burst counts {2, 4, 6}. Bursts within each
 * request are equal-sized, so every per-request CV is 0.
 */
TEST_F(NetPersonaFitterTest, StreamingBurstCountMedian)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	// At least one request is needed so the minimal persona is emitted at all.
	fitter.AddRequest(0.030, 1);

	// Equal-sized bursts (100000 B each): sum = 100000*n, sumSq = n*1e10 -> CV 0.
	fitter.AddRequestBurstSummary(2, 200000.0, 2.0e10);
	fitter.AddRequestBurstSummary(4, 400000.0, 4.0e10);
	fitter.AddRequestBurstSummary(6, 600000.0, 6.0e10);

	std::string json = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(json.empty());

	// median of counts {2,4,6} -> bucket 4 midpoint 4.5 -> floor 4
	EXPECT_NEAR(ExtractJsonDouble(json, "bursts_per_segment"), 4.0,   1e-9);
	// all three CVs are 0 -> bucket-0 midpoint 0.005
	EXPECT_NEAR(ExtractJsonDouble(json, "burst_bytes_cv"),     0.005, 1e-9);
}

/**
 * @brief burst_bytes_cv is the (approximate) median of per-request byte-size CVs
 *
 * Three 2-burst requests with CVs {0, sqrt(2)/2, 0.8*sqrt(2)}; the middle value
 * is selected as the median. CV(a,b) = sqrt(2)*|a-b|/(a+b) for a 2-burst request.
 */
TEST_F(NetPersonaFitterTest, StreamingBurstCvMedian)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	fitter.AddRequest(0.030, 1);

	// {100000,100000} -> CV 0
	fitter.AddRequestBurstSummary(2, 200000.0, 2.0e10);
	// {50000,150000}  -> CV sqrt(2)/2 = 0.70710678 -> bucket 70 midpoint 0.705
	fitter.AddRequestBurstSummary(2, 200000.0, 2.5e10);
	// {20000,180000}  -> CV 0.8*sqrt(2) = 1.1313708 -> bucket 113 midpoint 1.135
	fitter.AddRequestBurstSummary(2, 200000.0, 3.28e10);

	std::string json = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(json.empty());

	// median of CVs {0, 0.707, 1.131} selects the middle sample -> bucket 70 midpoint
	EXPECT_NEAR(ExtractJsonDouble(json, "burst_bytes_cv"),     0.705, 1e-9);
	// all counts are 2 -> bucket 2 midpoint 2.5 -> floor 2
	EXPECT_NEAR(ExtractJsonDouble(json, "bursts_per_segment"), 2.0,   1e-9);
}

/**
 * @brief keepRecord=false updates streaming only, leaving the O(N) vectors empty
 */
TEST_F(NetPersonaFitterTest, StreamingKeepRecordFalseDoesNotGrowVectors)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	auto prevReq = fitter.GetRequestCount();
	auto prevBur = fitter.GetBurstCount();

	fitter.AddRequest(0.030, 1, /*keepRecord=*/false);
	fitter.AddBurst(1, 0, 0.010, 100000, 0.20, /*keepRecord=*/false);
	fitter.AddBurst(1, 1, 0.010, 100000, 0.30, /*keepRecord=*/false);

	EXPECT_EQ(fitter.GetRequestCount(), prevReq);
	EXPECT_EQ(fitter.GetBurstCount(), prevBur);
	EXPECT_FALSE(fitter.BuildMinimalPersonaJson().empty());
}

/**
 * @brief ResetStreaming clears the accumulators
 */
TEST_F(NetPersonaFitterTest, ResetStreamingClears)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();
	fitter.AddRequest(0.030, 1, /*keepRecord=*/false);
	fitter.AddBurst(1, 0, 0.010, 100000, 0.20, /*keepRecord=*/false);
	ASSERT_FALSE(fitter.BuildMinimalPersonaJson().empty());

	fitter.ResetStreaming();
	EXPECT_TRUE(fitter.BuildMinimalPersonaJson().empty());
}

/**
 * @brief GeneratePersonaJson (vector swap) leaves streaming aggregates intact
 */
TEST_F(NetPersonaFitterTest, StreamingSurvivesFilePersonaSwap)
{
	auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
	fitter.ResetStreaming();

	fitter.AddRequest(0.030, 1, /*keepRecord=*/true);
	fitter.AddBurst(1, 0, 0.010, 100000, 0.20, /*keepRecord=*/true);
	fitter.AddBurst(1, 1, 0.010, 100000, 0.30, /*keepRecord=*/true);

	std::string before = fitter.BuildMinimalPersonaJson();
	ASSERT_FALSE(before.empty());

	// Consuming (swapping out) the O(N) vectors must not disturb streaming scalars.
	ASSERT_TRUE(fitter.GeneratePersonaJson(kBasePath));

	EXPECT_EQ(fitter.BuildMinimalPersonaJson(), before);
}
