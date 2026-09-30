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

#pragma once

/**
 * @file AampTimingHistogram.h
 * @brief General-purpose streaming histogram for approximate median / quantile
 *        extraction over large, potentially unbounded, timing sample sets.
 *
 * Motivation
 * ----------
 * When a metric is collected for every network request or every burst event
 * during a long playback session (1+ hours), keeping every raw sample in a
 * std::vector and sorting at session-end is expensive in both memory and CPU.
 * AampTimingHistogram provides:
 *
 *   - O(1) sample ingestion (a single bucket increment)
 *   - O(bucketCount) approximate median/percentile query
 *   - Bounded, fixed memory proportional to the range and resolution,
 *     independent of the number of samples
 *
 * The absolute error on any returned percentile is at most ±(bucketWidthMs/2).
 *
 * Usage
 * -----
 * @code
 *   // Track TTFB over a session with 5 ms resolution up to 2000 ms.
 *   AampTimingHistogram ttfbHist(5.0, 2000.0);
 *   ttfbHist.Add(r.ttfbS * 1000.0);              // O(1) per request
 *   ...
 *   double medTtfb = ttfbHist.ApproximateMedianMs(); // O(bucketCount) once
 * @endcode
 *
 * Bucket layout
 * -------------
 *   bucketCount = static_cast<size_t>(maxTimingMs / bucketWidthMs) + 1
 *   Normal bucket i covers the half-open interval [i*w, (i+1)*w)
 *   where w = bucketWidthMs.
 *   The last bucket (index bucketCount-1) is the overflow bucket: it collects
 *   all values >= maxTimingMs.
 *
 * Choosing parameters
 * -------------------
 *   Choose bucketWidthMs based on the tolerated error, not on the expected
 *   number of samples. A 5 ms bucket width gives ±2.5 ms accuracy regardless
 *   of whether 100 or 10,000,000 samples are ingested. Keeping bucketCount
 *   below ~10,000 is recommended for cache efficiency.
 *
 * Thread safety
 * -------------
 *   Not thread-safe. If multiple threads feed samples concurrently, guard
 *   with an external mutex or use per-thread instances and merge.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>

/**
 * @class AampTimingHistogram
 * @brief Fixed-bucket streaming histogram for approximate quantile calculation.
 *
 * Memory is fixed at construction time regardless of the number of samples
 * added. Intended for timing measurements (milliseconds) but usable for any
 * non-negative numeric domain.
 */
class AampTimingHistogram
{
public:
	/// Maximum supported bucket count. Constructor parameters that would
	/// produce more buckets than this are clamped; a diagnostic is emitted
	/// to stderr.
	static constexpr std::size_t kMaxBucketCount = 100'000;

	/**
	 * @brief Construct a timing histogram.
	 *
	 * Parameters are validated before any conversion or allocation takes
	 * place. Non-finite, zero, or negative values for either parameter are
	 * replaced with 1.0 and a diagnostic is emitted to stderr. A
	 * bucketWidthMs/maxTimingMs ratio that would exceed kMaxBucketCount
	 * buckets is clamped to kMaxBucketCount with a diagnostic.
	 *
	 * @param bucketWidthMs  Width of each normal bucket in milliseconds.
	 *                       Must be finite and > 0. The absolute error on any
	 *                       returned percentile is bounded by
	 *                       ±(bucketWidthMs / 2).
	 * @param maxTimingMs    Upper edge of the highest normal bucket. Values
	 *                       at or above this threshold are placed in the
	 *                       overflow bucket (the last bucket). Must be
	 *                       finite and > 0.
	 *
	 * The total number of internal buckets (including the overflow bucket) is:
	 * @code
	 *   bucketCount = static_cast<size_t>(maxTimingMs / bucketWidthMs) + 1
	 * @endcode
	 * clamped to kMaxBucketCount.
	 */
	AampTimingHistogram(double bucketWidthMs, double maxTimingMs)
		: mBucketWidthMs(CheckedBucketWidthMs(bucketWidthMs))
		, mMaxTimingMs(CheckedMaxTimingMs(maxTimingMs))
		, mBucketCount(ComputeBucketCount(mBucketWidthMs, mMaxTimingMs))
		, mCounts(mBucketCount, 0)
		, mSampleCount(0)
	{
	}

	/**
	 * @brief Add a timing sample (O(1)).
	 *
	 * Negative values are clamped to bucket 0.
	 * Values >= maxTimingMs are placed in the overflow bucket (last bucket).
	 *
	 * @param valueMs  Timing value in milliseconds.
	 */
	void Add(double valueMs)
	{
		const double clamped = std::max(0.0, valueMs);
		const std::size_t index = std::min(
			static_cast<std::size_t>(clamped / mBucketWidthMs),
			mBucketCount - 1);
		++mCounts[index];
		++mSampleCount;
	}

	/**
	 * @brief Compute the approximate median of all accumulated samples (O(bucket_count)).
	 *
	 * Equivalent to ApproximatePercentileMs(50.0).
	 *
	 * @return Midpoint of the bucket containing the lower-median sample,
	 *         or 0.0 if no samples have been added.
	 */
	double ApproximateMedianMs() const
	{
		return ApproximatePercentileMs(50.0);
	}

	/**
	 * @brief Compute an approximate percentile from accumulated samples (O(bucket_count)).
	 *
	 * The returned value is the midpoint of the bucket that contains the
	 * sample at the requested percentile position. The maximum absolute error
	 * relative to the true percentile is ±(bucketWidthMs / 2).
	 *
	 * When the percentile falls in the overflow bucket the midpoint returned
	 * is (mBucketCount - 1) * bucketWidthMs + 0.5 * bucketWidthMs, which is
	 * one half-bucket above maxTimingMs.
	 *
	 * @param p  Desired percentile in [0, 100]. p=50 gives the approximate median.
	 * @return   Midpoint of the bucket containing the p-th percentile sample,
	 *           or 0.0 if no samples have been added.
	 */
	double ApproximatePercentileMs(double p) const
	{
		if (mSampleCount == 0)
		{
			return 0.0;
		}

		// Clamp percentile to valid range
		const double pClamped = std::max(0.0, std::min(100.0, p));

		// 0-indexed position of the p-th-percentile element (lower in case of ties)
		const uint64_t target =
			static_cast<uint64_t>(std::floor(pClamped / 100.0 * static_cast<double>(mSampleCount - 1)));

		uint64_t cumulative = 0;
		for (std::size_t i = 0; i < mBucketCount; ++i)
		{
			cumulative += mCounts[i];
			if (cumulative > target)
			{
				return static_cast<double>(i) * mBucketWidthMs + mBucketWidthMs * 0.5;
			}
		}
		// Unreachable when mSampleCount > 0; return overflow bucket midpoint as safety.
		return static_cast<double>(mBucketCount - 1) * mBucketWidthMs + mBucketWidthMs * 0.5;
	}

	/**
	 * @brief Reset all internal state.
	 *
	 * After Reset(), SampleCount() == 0 and ApproximateMedianMs() returns 0.0.
	 */
	void Reset()
	{
		std::fill(mCounts.begin(), mCounts.end(), uint64_t{0});
		mSampleCount = 0;
	}

	/**
	 * @brief Total number of samples added since construction or the last Reset().
	 * @return Sample count.
	 */
	uint64_t SampleCount() const
	{
		return mSampleCount;
	}

	/**
	 * @brief Number of samples placed in the overflow bucket (values >= maxTimingMs).
	 * @return Overflow count.
	 */
	uint64_t OverflowCount() const
	{
		return mCounts.back();
	}

private:
	// ── Construction helpers ──────────────────────────────────────────────
	// These run inside the member-initializer list, before any storage is
	// allocated, so they prevent UB from a bad floating-point-to-integer
	// conversion regardless of whether NDEBUG is defined.

	/// Validate bucketWidthMs; return a safe positive value.
	/// Emits a diagnostic to stderr and substitutes 1.0 on invalid input.
	static double CheckedBucketWidthMs(double v)
	{
		if (!std::isfinite(v) || v <= 0.0)
		{
			std::fprintf(stderr,
				"AampTimingHistogram: invalid bucketWidthMs=%.6g; substituting 1.0 ms\n", v);
			return 1.0;
		}
		return v;
	}

	/// Validate maxTimingMs; return a safe positive value.
	/// Emits a diagnostic to stderr and substitutes 1.0 on invalid input.
	static double CheckedMaxTimingMs(double v)
	{
		if (!std::isfinite(v) || v <= 0.0)
		{
			std::fprintf(stderr,
				"AampTimingHistogram: invalid maxTimingMs=%.6g; substituting 1.0 ms\n", v);
			return 1.0;
		}
		return v;
	}

	/// Compute bucket count from already-validated parameters, clamped to
	/// kMaxBucketCount.  Emits a diagnostic to stderr when clamping occurs.
	static std::size_t ComputeBucketCount(double bucketWidthMs, double maxTimingMs)
	{
		// Both arguments are finite and positive at this point.
		const double raw = std::floor(maxTimingMs / bucketWidthMs) + 1.0;
		if (raw > static_cast<double>(kMaxBucketCount))
		{
			std::fprintf(stderr,
				"AampTimingHistogram: bucket count %.0f exceeds limit %zu; clamping\n",
				raw, kMaxBucketCount);
			return kMaxBucketCount;
		}
		return static_cast<std::size_t>(raw);
	}

	// ── Data members ──────────────────────────────────────────────────────
	double                mBucketWidthMs; ///< Width of each normal bucket (ms)
	double                mMaxTimingMs;   ///< Upper edge of highest normal bucket (ms)
	std::size_t           mBucketCount;   ///< Total buckets including overflow
	std::vector<uint64_t> mCounts;        ///< Per-bucket sample counts; last = overflow
	uint64_t              mSampleCount;   ///< Total samples added
};
