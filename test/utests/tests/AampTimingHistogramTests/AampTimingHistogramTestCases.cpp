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
 * @file AampTimingHistogramTestCases.cpp
 * @brief Unit tests for AampTimingHistogram.
 *
 * Histogram configuration used throughout (unless otherwise stated):
 *   bucketWidthMs = 10.0, maxTimingMs = 100.0
 *   => bucketCount = size_t(100.0/10.0) + 1 = 11
 *      Buckets 0–9 cover [0,10), [10,20), ..., [90,100) ms
 *      Bucket 10 is the overflow bucket (values >= 100 ms)
 *   => Bucket i midpoint = i * 10 + 5 ms
 *   => Overflow bucket midpoint = 10 * 10 + 5 = 105 ms
 */

#include <gtest/gtest.h>

#include "AampTimingHistogram.h"

/// Shared configuration for most tests.
static constexpr double kBucketWidthMs      = 10.0;
static constexpr double kMaxTimingMs        = 100.0;
/// Midpoint of the overflow bucket: maxTimingMs + bucketWidthMs/2 = 105 ms.
static constexpr double kOverflowMidpointMs = kMaxTimingMs + kBucketWidthMs * 0.5;

class AampTimingHistogramTests : public ::testing::Test
{
};

// ─────────────────────────────────────────────────────────────────────────────
// Empty histogram
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief An empty histogram returns 0.0 for all query methods and zero counts.
 */
TEST_F(AampTimingHistogramTests, Add_Empty_ReturnsZero)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);

	EXPECT_EQ(hist.SampleCount(),                          uint64_t{0});
	EXPECT_EQ(hist.OverflowCount(),                        uint64_t{0});
	EXPECT_EQ(hist.ApproximateMedianMs(),                  0.0);
	EXPECT_EQ(hist.ApproximatePercentileMs(0.0),           0.0);
	EXPECT_EQ(hist.ApproximatePercentileMs(50.0),          0.0);
	EXPECT_EQ(hist.ApproximatePercentileMs(100.0),         0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Single sample
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A single sample at 50 ms lands in bucket 5 ([50,60)); median = 55 ms.
 *
 * Algorithm trace:
 *   sampleCount=1, target = floor(0.5 * 0) = 0
 *   Walk: cumul(5) = 1 > 0 → return 5*10 + 5 = 55
 */
TEST_F(AampTimingHistogramTests, Add_SingleSample_ReturnsBucketMidpoint)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(50.0);

	EXPECT_EQ(hist.SampleCount(),         uint64_t{1});
	EXPECT_EQ(hist.ApproximateMedianMs(), 55.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Odd count — middle element
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Three samples in distinct buckets; median is the middle bucket.
 *
 * Samples: 15 ms (bucket 1), 55 ms (bucket 5), 95 ms (bucket 9).
 * Algorithm trace:
 *   sampleCount=3, target = floor(0.5 * 2) = 1
 *   cumul(1)=1 — not > 1; cumul(5)=2 > 1 → bucket 5 → 55 ms
 */
TEST_F(AampTimingHistogramTests, Add_OddCount_ReturnsMiddleBucketMidpoint)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(15.0);
	hist.Add(55.0);
	hist.Add(95.0);

	EXPECT_EQ(hist.SampleCount(),         uint64_t{3});
	EXPECT_EQ(hist.ApproximateMedianMs(), 55.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Even count — lower median
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Four samples; histogram returns the lower-median bucket.
 *
 * Samples: 15 (bucket 1), 55 (bucket 5), 95 (bucket 9), 135 (overflow bucket 10).
 * Algorithm trace:
 *   sampleCount=4, target = floor(0.5 * 3) = 1
 *   cumul(1)=1 — not > 1; cumul(5)=2 > 1 → bucket 5 → 55 ms
 */
TEST_F(AampTimingHistogramTests, Add_EvenCount_ReturnsLowerMedianBucket)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(15.0);
	hist.Add(55.0);
	hist.Add(95.0);
	hist.Add(135.0); // overflow

	EXPECT_EQ(hist.SampleCount(),         uint64_t{4});
	EXPECT_EQ(hist.OverflowCount(),       uint64_t{1});
	EXPECT_EQ(hist.ApproximateMedianMs(), 55.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// All samples in the same bucket
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Three samples all in bucket 2 ([20,30)); midpoint = 25 ms.
 *
 *   floor(21/10)=2, floor(22/10)=2, floor(27/10)=2 → all bucket 2
 */
TEST_F(AampTimingHistogramTests, Add_AllInSameBucket_ReturnsThatBucketMidpoint)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(21.0);
	hist.Add(22.0);
	hist.Add(27.0);

	EXPECT_EQ(hist.ApproximateMedianMs(), 25.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Overflow — values above maxTimingMs
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Values strictly above maxTimingMs are counted in the overflow bucket.
 */
TEST_F(AampTimingHistogramTests, Add_AboveMax_CountedInOverflow)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(150.0);
	hist.Add(200.0);

	EXPECT_EQ(hist.SampleCount(),   uint64_t{2});
	EXPECT_EQ(hist.OverflowCount(), uint64_t{2});
}

/**
 * @brief A value exactly at maxTimingMs goes to the overflow bucket.
 *
 *   bucketCount = 11; index = min(size_t(100.0/10.0), 10) = min(10, 10) = 10 (overflow).
 */
TEST_F(AampTimingHistogramTests, Add_ExactlyAtMax_GoesToOverflowBucket)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(100.0);

	EXPECT_EQ(hist.OverflowCount(), uint64_t{1});
}

/**
 * @brief A value just below maxTimingMs does NOT go to the overflow bucket.
 *
 *   index = min(size_t(99.9/10.0), 10) = min(9, 10) = 9 — bucket 9, not overflow.
 */
TEST_F(AampTimingHistogramTests, Add_JustBelowMax_NotOverflow)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(99.9);

	EXPECT_EQ(hist.OverflowCount(), uint64_t{0});
	EXPECT_EQ(hist.SampleCount(),   uint64_t{1});
}

// ─────────────────────────────────────────────────────────────────────────────
// Negative values
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Negative values are clamped to 0 and land in bucket 0; midpoint = 5 ms.
 */
TEST_F(AampTimingHistogramTests, Add_NegativeValue_ClampedToBucketZero)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(-50.0);
	hist.Add(-100.0);

	EXPECT_EQ(hist.SampleCount(),         uint64_t{2});
	EXPECT_EQ(hist.OverflowCount(),       uint64_t{0});
	EXPECT_EQ(hist.ApproximateMedianMs(), 5.0); // bucket 0 midpoint
}

// ─────────────────────────────────────────────────────────────────────────────
// Reset
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Reset() clears all state; subsequent queries return empty-histogram values.
 */
TEST_F(AampTimingHistogramTests, Reset_ClearsAllState)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(15.0);
	hist.Add(55.0);
	hist.Add(95.0);
	hist.Add(150.0); // overflow

	EXPECT_EQ(hist.SampleCount(), uint64_t{4});

	hist.Reset();

	EXPECT_EQ(hist.SampleCount(),         uint64_t{0});
	EXPECT_EQ(hist.OverflowCount(),       uint64_t{0});
	EXPECT_EQ(hist.ApproximateMedianMs(), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleCount
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief SampleCount() reflects the exact number of Add() calls.
 */
TEST_F(AampTimingHistogramTests, SampleCount_TracksAddCalls)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	for (int i = 0; i < 7; ++i)
	{
		hist.Add(static_cast<double>(i * 5));
	}
	EXPECT_EQ(hist.SampleCount(), uint64_t{7});
}

// ─────────────────────────────────────────────────────────────────────────────
// ApproximatePercentileMs — p=0 and p=100
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief p=0 returns the midpoint of the first occupied bucket.
 *
 * Samples: 50 (bucket 5, ×2), 90 (bucket 9, ×1); sampleCount=3.
 * target = floor(0/100 * 2) = 0; cumul(5)=2 > 0 → bucket 5 → 55 ms.
 */
TEST_F(AampTimingHistogramTests, ApproximatePercentileMs_P0_ReturnsFirstOccupiedBucket)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(50.0);
	hist.Add(50.0);
	hist.Add(90.0);

	EXPECT_EQ(hist.ApproximatePercentileMs(0.0), 55.0);
}

/**
 * @brief p=100 returns the midpoint of the last occupied bucket.
 *
 * Same samples: bucket 5 (×2), bucket 9 (×1); sampleCount=3.
 * target = floor(100/100 * 2) = 2;
 * cumul(5)=2 — not > 2; cumul(9)=3 > 2 → bucket 9 → 95 ms.
 */
TEST_F(AampTimingHistogramTests, ApproximatePercentileMs_P100_ReturnsLastOccupiedBucket)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(50.0);
	hist.Add(50.0);
	hist.Add(90.0);

	EXPECT_EQ(hist.ApproximatePercentileMs(100.0), 95.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// ApproximatePercentileMs — IQR use-case (p25, p75)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief p25 < p75, confirming that an IQR-style computation is supported.
 *
 * Samples: 15 (bucket 1, ×2), 95 (bucket 9, ×2); sampleCount=4.
 * p25: target = floor(0.25 * 3) = 0; cumul(1)=2 > 0 → bucket 1 → 15 ms.
 * p75: target = floor(0.75 * 3) = 2; cumul(1)=2 — not > 2;
 *      cumul(9)=4 > 2 → bucket 9 → 95 ms.
 * q75 (95) > q25 (15) ✓
 */
TEST_F(AampTimingHistogramTests, ApproximatePercentileMs_P25_P75_IQR_PositiveWidth)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(15.0);
	hist.Add(15.0);
	hist.Add(95.0);
	hist.Add(95.0);

	const double q25 = hist.ApproximatePercentileMs(25.0);
	const double q75 = hist.ApproximatePercentileMs(75.0);

	EXPECT_EQ(q25, 15.0);
	EXPECT_EQ(q75, 95.0);
	EXPECT_GT(q75, q25);
}

// ─────────────────────────────────────────────────────────────────────────────
// Large sample count — accuracy
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief With 10,000 uniformly distributed samples in [0,100) ms the approximate
 *        median falls within ±bucketWidthMs of the true median (50 ms).
 *
 * Each of buckets 0–9 receives exactly 1,000 samples.
 * sampleCount=10000, target = floor(0.5 * 9999) = 4999.
 * Cumulative after bucket 4 = 5000 > 4999 → bucket 4 → 45 ms.
 * 45 ms is within [40, 60] = [50 ± bucketWidthMs].
 */
TEST_F(AampTimingHistogramTests, ApproximateMedianMs_LargeNSamples_StaysApproximate)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);

	constexpr int kN = 10000;
	for (int i = 0; i < kN; ++i)
	{
		// Values uniformly spread over [0, kMaxTimingMs): 0.0, 0.01, 0.02, ..., 99.99
		hist.Add((static_cast<double>(i) / static_cast<double>(kN)) * kMaxTimingMs);
	}

	const double median = hist.ApproximateMedianMs();
	EXPECT_GE(median, 50.0 - kBucketWidthMs);
	EXPECT_LE(median, 50.0 + kBucketWidthMs);
}

// ─────────────────────────────────────────────────────────────────────────────
// Overflow majority — median in overflow bucket
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief When all samples are in the overflow bucket the median is the overflow
 *        bucket midpoint: maxTimingMs + bucketWidthMs/2 = 105 ms.
 *
 * All three values (150, 200, 999) exceed maxTimingMs=100 → bucket 10.
 * sampleCount=3, target = floor(0.5 * 2) = 1.
 * Walk: buckets 0–9 are empty; cumul(10)=3 > 1 → bucket 10 → 10*10+5 = 105.
 */
TEST_F(AampTimingHistogramTests, ApproximateMedianMs_OverflowMajority_ReturnsAboveMaxMidpoint)
{
	AampTimingHistogram hist(kBucketWidthMs, kMaxTimingMs);
	hist.Add(150.0);
	hist.Add(200.0);
	hist.Add(999.0);

	EXPECT_EQ(hist.ApproximateMedianMs(), kOverflowMidpointMs); // 105.0
}
