#pragma once

#include "Telemetry.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace nmqe {

/** Keeps samples for an end-of-run distribution summary (count, min, p50, p95, max, mean). */
class Distribution
{
public:
	void add(double value) { mValues.push_back(value); }
	size_t count() const { return mValues.size(); }

	double percentile(double p) const
	{
		if (mValues.empty()) return 0;
		std::vector<double> sorted = mValues;
		std::sort(sorted.begin(), sorted.end());
		const double rank = p / 100.0 * static_cast<double>(sorted.size() - 1);
		const size_t lower = static_cast<size_t>(rank);
		const size_t upper = std::min(lower + 1, sorted.size() - 1);
		return sorted[lower] + (sorted[upper] - sorted[lower]) * (rank - static_cast<double>(lower));
	}

	std::string json() const
	{
		Json out;
		out.add("n", static_cast<uint64_t>(mValues.size()));
		if (!mValues.empty())
		{
			double sum = 0;
			for (double v : mValues) sum += v;
			out.add("min", *std::min_element(mValues.begin(), mValues.end()))
				.add("p50", percentile(50))
				.add("p95", percentile(95))
				.add("max", *std::max_element(mValues.begin(), mValues.end()))
				.add("mean", sum / static_cast<double>(mValues.size()));
		}
		return out.object();
	}

private:
	std::vector<double> mValues;
};

} // namespace nmqe
