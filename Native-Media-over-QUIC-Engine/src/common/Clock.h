#pragma once

#include <chrono>
#include <cstdint>

namespace nmqe {

/** Monotonic nanoseconds; the only clock used for intervals. */
inline int64_t monoNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

/** Wallclock milliseconds since the Unix epoch; used only for cross-host correlation. */
inline int64_t wallMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

/** Wallclock microseconds since the Unix epoch. */
inline int64_t wallUs()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

constexpr int64_t kNsPerMs = 1000000;
constexpr int64_t kNsPerSec = 1000000000;

} // namespace nmqe
