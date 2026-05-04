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
