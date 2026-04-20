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
#include <cjson/cJSON.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cmath>
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
		// Clean up any generated files
		std::string path = GetOutputPath(kBasePath);
		std::remove(path.c_str());
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

	// Read and parse the output
	std::string outputPath = GetOutputPath(kBasePath);
	std::string jsonStr = ReadFile(outputPath);
	ASSERT_FALSE(jsonStr.empty()) << "Persona JSON file is empty or missing: " << outputPath;

	cJSON* root = cJSON_Parse(jsonStr.c_str());
	ASSERT_NE(root, nullptr) << "Failed to parse persona JSON";

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
		cJSON* item = cJSON_GetObjectItem(root, field);
		EXPECT_NE(item, nullptr) << "Missing field: " << field;
		if (item)
		{
			EXPECT_TRUE(cJSON_IsNumber(item)) << "Field not a number: " << field;
		}
	}

	// Verify RTT estimates are in sane range
	double baseRtt = cJSON_GetObjectItem(root, "base_rtt_ms")->valuedouble;
	EXPECT_GT(baseRtt, 20.0) << "base_rtt_ms too low";
	EXPECT_LT(baseRtt, 40.0) << "base_rtt_ms too high (should reflect reused conns ~25-33ms)";

	// Verify connection reuse probability
	double pReuse = cJSON_GetObjectItem(root, "p_conn_reuse")->valuedouble;
	EXPECT_NEAR(pReuse, 20.0 / 30.0, 0.05);

	// Verify new connection penalty is positive (fresh > reused)
	double penalty = cJSON_GetObjectItem(root, "new_conn_penalty_ms")->valuedouble;
	EXPECT_GT(penalty, 0.0);

	// Verify bursts per segment is reasonable (we fed 3-4 per request)
	double bps = cJSON_GetObjectItem(root, "bursts_per_segment")->valuedouble;
	EXPECT_GE(bps, 3.0);
	EXPECT_LE(bps, 5.0);

	// Verify cadence is in ~200ms range (our simulated gap)
	double cadence = cJSON_GetObjectItem(root, "cadence_ms")->valuedouble;
	EXPECT_GT(cadence, 150.0);
	EXPECT_LT(cadence, 250.0);

	// Verify throughput is positive and finite
	double thr = cJSON_GetObjectItem(root, "mean_thr_mbps")->valuedouble;
	EXPECT_GT(thr, 0.0);
	EXPECT_TRUE(std::isfinite(thr));

	// Static defaults
	EXPECT_DOUBLE_EQ(cJSON_GetObjectItem(root, "capacity_drop_p")->valuedouble, 0.0);
	EXPECT_DOUBLE_EQ(cJSON_GetObjectItem(root, "capacity_drop_factor")->valuedouble, 0.6);
	EXPECT_DOUBLE_EQ(cJSON_GetObjectItem(root, "rtt_inflation_ms")->valuedouble, 0.0);
	EXPECT_DOUBLE_EQ(cJSON_GetObjectItem(root, "flush_jitter_ms")->valuedouble, 6.0);

	cJSON_Delete(root);
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
