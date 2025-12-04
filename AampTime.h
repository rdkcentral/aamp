/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include <cstdint>
#include <ostream>
#include <cmath>
#include <limits>

#ifndef AAMPTIME_H
#define AAMPTIME_H

/// @brief struct to hold time in ticks and timescale
struct AampTicks
{
	int64_t ticks;
	uint32_t timescale;

	/// @brief Constructor
	/// @param ticks
	/// @param timescale
	AampTicks(int64_t ticks, uint32_t timescale) : ticks(ticks), timescale(timescale) {}

	/// @brief Get time in milliseconds
	int64_t inMilli() { return (ticks * 1000) / (int64_t)timescale; }
};

/// @brief time class to work around the use of doubles within Aamp
//  While operators are overloaded for comparisons, the underlying data type is integer
//  But the code is tolerant of being treated as a double

class AampTime
{
	public:
		typedef enum { milli = 1000, micro = 1000000, nano = 1000000000 } TimeScale;

	private:
		static const uint64_t baseTimescale = nano;
		int64_t baseTime;

		/// @brief Convert ticks to base time with overflow protection using 128-bit arithmetic
		/// @param ticks The tick count (signed 64-bit)
		/// @param timescale The timescale (unsigned 32-bit)
		/// @return The converted time in nanoseconds, clamped to INT64_MIN/MAX if overflow detected
		static inline int64_t convertTicksWithOverflowProtection(int64_t ticks, uint32_t timescale)
		{
			// Use 128-bit intermediate to avoid overflow during multiplication
			// __int128 is supported by GCC and Clang on 64-bit platforms
			#if defined(__SIZEOF_INT128__)
				__int128 intermediate = static_cast<__int128>(ticks) * static_cast<__int128>(baseTimescale);
				__int128 result = intermediate / static_cast<__int128>(timescale);
				
				// Check if result fits in int64_t range
				if (result > std::numeric_limits<int64_t>::max())
				{
					return std::numeric_limits<int64_t>::max();
				}
				else if (result < std::numeric_limits<int64_t>::min())
				{
					return std::numeric_limits<int64_t>::min();
				}
				else
				{
					return static_cast<int64_t>(result);
				}
			#else
				// Fallback for platforms without __int128 support
				// Use double precision (loses some precision but avoids overflow)
				double intermediate = static_cast<double>(ticks) * static_cast<double>(baseTimescale);
				double result = intermediate / static_cast<double>(timescale);
				
				if (result > static_cast<double>(std::numeric_limits<int64_t>::max()))
				{
					return std::numeric_limits<int64_t>::max();
				}
				else if (result < static_cast<double>(std::numeric_limits<int64_t>::min()))
				{
					return std::numeric_limits<int64_t>::min();
				}
				else
				{
					return static_cast<int64_t>(result);
				}
			#endif
		}

	public:
		/// @brief Constructor
		/// @param seconds time in seconds, as a double
		constexpr AampTime(double seconds = 0.0) : baseTime(int64_t(seconds * baseTimescale)){}

		/// @brief Copy constructor
		/// @param rhs AampTime object to copy
		constexpr AampTime(const AampTime& rhs) : baseTime(rhs.baseTime){}

		/// @brief Constructor
		/// @param time struct containing time in ticks and timescale
		/// @note This is used to convert from AampTicks to AampTime; it is lossy and cannot be converted back
		/// @note Uses 128-bit intermediate to prevent overflow; clamps result to INT64_MIN/MAX if needed
		inline AampTime(AampTicks &time) : baseTime(convertTicksWithOverflowProtection(time.ticks, time.timescale)) {}

		/// @brief Get the stored time
		/// @return Time in seconds (double)
		inline double inSeconds() const { return (baseTime / double(baseTimescale)); }

		/// @brief Get the stored time in seconds
		/// @return Time in seconds (integer)
		inline int64_t seconds() const { return (baseTime / baseTimescale); }

		/// @brief Get the stored time in milliseconds
		/// @return Time in milliseconds (integer)
		inline int64_t milliseconds() const { return (baseTime / (baseTimescale / milli)); }

		// Equivalent to round() but in integer domain
		inline int64_t nearestSecond() const
		{
			int64_t retval = this->seconds();

			// Fractional part
			int64_t tempval = baseTime - retval * baseTimescale;

			if (tempval >= ((5 * baseTimescale)/10))
			{
				retval += 1;
			}

			return retval;
		}

		// Overloads for comparison operators to check AampTime : AampTime and AampTime : double
		// Converting (and truncating) the double to the timescale should avoid the issues around epsilon for floating point
		inline bool operator==(const AampTime &rhs) const
		{
			if (this == &rhs)
				return true;
			else
				return (baseTime == rhs.baseTime);
		}

		inline bool operator==(const double &rhs) const { return (baseTime == int64_t(rhs * baseTimescale)); }

		inline AampTime& operator=(const AampTime &rhs)
		{
			if (this == &rhs)
				return *this;

			baseTime = rhs.baseTime;
			return *this;
		}

		inline AampTime& operator=(const double &rhs)
		{
			baseTime = int64_t(rhs * baseTimescale);
			return *this;
		}

		inline AampTime operator-() const
		{
			AampTime temp(*this);
			temp.baseTime = -baseTime;
			return temp;
		}

		inline bool operator!=(const AampTime &rhs) const { return !(*this == rhs); }
		inline bool operator!=(double &rhs) const { return !(*this == rhs); }

		inline bool operator>(const AampTime &rhs) const { return (baseTime > rhs.baseTime); }
		inline bool operator>(const double &rhs) const { return (baseTime > int64_t(rhs * baseTimescale)); }

		inline bool operator<(const AampTime &rhs) const { return ((*this != rhs) && (!(*this > rhs))); }
		inline bool operator<(const double &rhs) const { return ((*this != rhs) && (!(*this > rhs))); }

		inline bool operator>=(const AampTime &rhs) const { return ((*this > rhs) || (*this == rhs)); }
		inline bool operator>=(double rhs) const { return ((*this > rhs) || (*this == rhs)); }

		inline bool operator<=(const AampTime &rhs) const { return ((*this < rhs) || (*this == rhs)); }
		inline bool operator<=(double rhs) const { return ((*this < rhs) || (*this == rhs)); }

		inline AampTime operator+(const AampTime &t) const
		{
			AampTime temp(*this);

			temp.baseTime = baseTime + t.baseTime;
			return temp;
		}
		inline AampTime operator+(const double &t) const
		{
			AampTime temp(*this);

			temp.baseTime = baseTime + int64_t(t * baseTimescale);
			return std::move(temp);
		}

		inline const AampTime &operator+=(const AampTime &t)
		{
			*this = *this + t;
			return *this;
		}
		inline const AampTime &operator+=(const double &t)
		{
			*this = *this + t;
			return *this;
		}

		inline AampTime operator-(const AampTime &t) const
		{
			AampTime temp(*this);

			temp.baseTime = baseTime - t.baseTime;
			return std::move(temp);
		}

		inline AampTime operator-(const double &t) const
		{
			AampTime temp(*this);
			temp.baseTime = baseTime - int64_t(t * baseTimescale);
			return std::move(temp);
		}

		inline const AampTime &operator-=(const AampTime &t)
		{
			*this = *this - t;
			return *this;
		}
		inline const AampTime &operator-=(const double &t)
		{
			*this = *this - t;
			return *this;
		}

		inline AampTime operator/(const double &t) const
		{
			AampTime temp(*this);

			temp.baseTime = (int64_t)((double)baseTime/t);
			return std::move(temp);
		}

		inline AampTime operator*(const double &t) const
		{
			AampTime temp(*this);

			temp.baseTime = (int64_t)((double)baseTime * t);
			return std::move(temp);
		}

		explicit operator double() const { return this->inSeconds(); }
		explicit operator int64_t() const { return this->seconds(); }
};

//  For those who like if (0.0 == b)
inline bool operator==(const double& lhs, const AampTime& rhs) { return (rhs.operator==(lhs)); };
inline bool operator!=(const double& lhs, const AampTime& rhs) { return !(rhs == lhs); };

inline AampTime operator+(const double &lhs, const AampTime &rhs) { return rhs + lhs; };
inline AampTime operator-(const double &lhs, const AampTime &rhs) { return -rhs + lhs; };

inline AampTime operator*(const int64_t &lhs, const AampTime &rhs) { return rhs * lhs; };

// Adding double & AampTime and expecting a double will need to use AampTime::inSeconds() instead
// Where a double is to be passed by reference, if the prototype cannot be rewritten or overloaded then
// a temporary double will be needed

inline double operator+=(double &lhs, const AampTime &rhs)
{
	lhs = lhs + rhs.inSeconds();
	return lhs;
}

inline bool operator>(const double &lhs, const AampTime &rhs) { return (rhs.operator<(lhs)); };
inline bool operator<(const double &lhs, const AampTime &rhs) { return (rhs.operator>(lhs)); };
inline bool operator<=(const double &lhs, const AampTime &rhs) { return (rhs >= lhs); };
inline bool operator>=(const double &lhs, const AampTime &rhs) { return (rhs <= lhs); };

// Is stream operator used?
inline std::ostream &operator<<(std::ostream &out, const AampTime& t)
{
	return out << t.inSeconds();
}

inline double abs(AampTime t)
{
	return std::abs(t.inSeconds());
}

inline double fabs(AampTime t)
{
	return std::fabs(t.inSeconds());
}

inline double round(AampTime t)
{
	return std::round(t.inSeconds());
}

inline double floor(AampTime t)
{
	return std::floor(t.inSeconds());
}

#endif
