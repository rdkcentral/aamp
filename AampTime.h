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

#if defined(__has_builtin)
#if __has_builtin(__builtin_mul_overflow)
#define HAVE_BUILTIN_MUL_OVERFLOW
#endif
#endif
#if defined(__GNUC__) || defined(__clang__)
#define HAVE_BUILTIN_MUL_OVERFLOW
#endif

inline bool mul_overflow(int64_t a, int64_t b, int64_t* out) noexcept
{
#ifdef HAVE_BUILTIN_MUL_OVERFLOW
	return __builtin_mul_overflow(a, b, out);
#else
	if( a == 0 || b == 0 ){ // if either argument zero, result is zero and no overflow
		*out = 0;
		return false;
	}
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	// handle edge cases - we can't negate INT64_MIN (won't fit)
	if( a == -1 ){
		if( b == min ) return true;
		*out = -b;
		return false;
	}
	if (b == -1) {
		if( a == min ) return true;
		*out = -a;
		return false;
	}
	if( a > 0 ){ // positive a
		if( b > 0 ){ // positive b
			if( a > max/b ) return true;
		} else { // negative b
			if( b < min/a) return true; // positive a,
		}
	}
	else { // negative a
		if( b > 0 ){ // positive b
			if( a < min/b ) return true;
		}
		else
		{ // negative b
			if( b < max/a) return true;
		}
	}
	*out = a * b;
	return false;
#endif
}


/**
 * @brief Helper function for overflow-protected multiplication and division
 * @param multiplicand The value to multiply
 * @param multiplier The multiplier
 * @param divisor The divisor
 * @return Result of (multiplicand * multiplier) / divisor, clamped to INT64_MIN/MAX if overflow detected
 */
inline int64_t multiplyDivideWithOverflowProtection( int64_t multiplicand, int64_t multiplier, uint32_t divisor) noexcept
{
	if (divisor == 0)
	{ // policy: return zero on avoid divide-by-zero
		return 0;
	}
	int64_t product;
	if (mul_overflow(multiplicand, multiplier, &product))
	{
		const bool positive = (multiplicand > 0) == (multiplier > 0);
		return positive ?
		std::numeric_limits<int64_t>::max() :
		std::numeric_limits<int64_t>::min();
	}
	return product / static_cast<int64_t>(divisor);
}

/** @brief struct to hold time in ticks and timescale */
struct AampTicks
{
	int64_t ticks;
	uint32_t timescale;

	/** @brief Constructor
	  * @param ticks
	  * @param timescale
	  */
	AampTicks(int64_t ticks, uint32_t timescale) : ticks(ticks), timescale(timescale) {}

	/**
	 * @brief Get time in milliseconds with overflow protection.
	 *
	 * Prevents overflow when (ticks * 1000) would exceed INT64_MAX or INT64_MIN.
	 * If overflow would occur, clamps the result to INT64_MAX or INT64_MIN.
	 *
	 * @return Time in milliseconds, clamped to INT64_MIN or INT64_MAX if overflow occurs.
	 */
	int64_t inMilli() const
	{
		// Fast path: same timescale as milliseconds
		if (timescale == 1000u)
		{
			return ticks;
		}
		return multiplyDivideWithOverflowProtection(ticks, 1000, timescale);
	}
};

/** @brief time class to work around the use of doubles within Aamp
  * While operators are overloaded for comparisons, the underlying data type is integer
  * But the code is tolerant of being treated as a double
  */
class AampTime
{
	public:
		typedef enum { milli = 1000, micro = 1000000, nano = 1000000000 } TimeScale;

	private:
		static const uint64_t baseTimescale = nano;
		int64_t baseTime;

		/**
		 * @brief Convert ticks to base time with overflow protection
		 * @param ticks The tick count (signed 64-bit)
		 * @param timescale The timescale (unsigned 32-bit); if zero, returns 0
		 * @return The converted time in nanoseconds, clamped to INT64_MIN/MAX if overflow detected, or 0 if timescale is zero
		 */
		static inline int64_t convertTicksWithOverflowProtection(int64_t ticks, uint32_t timescale) noexcept
		{
			return multiplyDivideWithOverflowProtection(ticks, baseTimescale, timescale);
		}

		/** @brief Helper to convert seconds (double) into base units */
		static inline int64_t toBase(double seconds) noexcept
		{
			return static_cast<int64_t>(seconds * static_cast<double>(baseTimescale));
		}

	public:
		/**
		  * @brief Constructor
		  * @param seconds time in seconds, as a double
		  */
		constexpr AampTime(double seconds = 0.0) : baseTime(static_cast<int64_t>(seconds * baseTimescale)) {}

		/**
		  * @brief Copy constructor
		  * @param rhs AampTime object to copy
		  */
		constexpr AampTime(const AampTime& rhs) : baseTime(rhs.baseTime) {}

		/**
		  * @brief Constructor
		  * @param time struct containing time in ticks and timescale
		  * @note This is used to convert from AampTicks to AampTime; it is lossy and cannot be converted back
		  * @note avoids overflow; clamps result to INT64_MIN/MAX if needed
		  */
		explicit AampTime(const AampTicks& time)
			: baseTime(convertTicksWithOverflowProtection(time.ticks, time.timescale)) {}

		/**
		  * @brief Get the stored time
		  * @return Time in seconds (double)
		  */
		inline double inSeconds() const { return baseTime / static_cast<double>(baseTimescale); }

		/**
		  * @brief Get the stored time in seconds
		  * @return Time in seconds (integer)
		  */
		inline int64_t seconds() const { return baseTime / static_cast<int64_t>(baseTimescale); }
		
		/**
		  * @brief Get the stored time in milliseconds
		  * @return Time in milliseconds (integer)
		  */
		inline int64_t milliseconds() const
		{
			return baseTime / static_cast<int64_t>(baseTimescale / milli);
		}
		
		/**
		  * @brief Return the nearest second to the stored time
		  * @return Nearest second (integer)
		  * @note Equivalent to round() but in integer domain
		  */
		inline int64_t nearestSecond() const noexcept
		{
			const int64_t scale = static_cast<int64_t>(baseTimescale);
			int64_t adj = (baseTime>=0)?(scale/2):-(scale/2);
			return (baseTime + adj) / scale;
		}
	
		// Overloads for comparison operators to check AampTime : AampTime and AampTime : double
		inline bool operator==(const AampTime &rhs) const
		{
			if (this == &rhs)
			{
				return true;
			}
			return (baseTime == rhs.baseTime);
		}

		inline bool operator==(const double &rhs) const
		{
			return baseTime == toBase(rhs);
		}

		inline AampTime& operator=(const AampTime &rhs)
		{
			if (this != &rhs)
			{
				baseTime = rhs.baseTime;
			}
			return *this;
		}

		inline AampTime& operator=(const double &rhs)
		{
			baseTime = toBase(rhs);
			return *this;
		}

		inline AampTime operator-() const
		{
			AampTime temp(*this);
			temp.baseTime = -baseTime;
			return temp;
		}

		inline bool operator!=(const AampTime &rhs) const { return !(*this == rhs); }

		inline bool operator!=(const double &rhs) const { return !(*this == rhs); }

		inline bool operator>(const AampTime &rhs) const { return baseTime > rhs.baseTime; }

		inline bool operator>(const double &rhs) const { return baseTime > toBase(rhs); }

		inline bool operator<(const AampTime &rhs) const { return baseTime < rhs.baseTime; }

		inline bool operator<(const double &rhs) const { return baseTime < toBase(rhs); }

		inline bool operator>=(const AampTime &rhs) const { return baseTime >= rhs.baseTime; }

		inline bool operator>=(double rhs) const { return baseTime >= toBase(rhs); }

		inline bool operator<=(const AampTime &rhs) const { return baseTime <= rhs.baseTime; }

		inline bool operator<=(double rhs) const { return baseTime <= toBase(rhs); }

		inline AampTime operator+(const AampTime &t) const
		{
			AampTime temp(*this);
			temp.baseTime = baseTime + t.baseTime;
			return temp;
		}

		inline AampTime operator+(const double &t) const
		{
			AampTime temp(*this);
			temp.baseTime = baseTime + toBase(t);
			return temp;
		}

		inline const AampTime &operator+=(const AampTime &t)
		{
			baseTime += t.baseTime;
			return *this;
		}

		inline const AampTime &operator+=(const double &t)
		{
			baseTime += toBase(t);
			return *this;
		}

		inline AampTime operator-(const AampTime &t) const
		{
			AampTime temp(*this);
			temp.baseTime = baseTime - t.baseTime;
			return temp;
		}

		inline AampTime operator-(const double &t) const
		{
			AampTime temp(*this);
			temp.baseTime = baseTime - toBase(t);
			return temp;
		}

		inline const AampTime &operator-=(const AampTime &t)
		{
			baseTime -= t.baseTime;
			return *this;
		}

		inline const AampTime &operator-=(const double &t)
		{
			baseTime -= toBase(t);
			return *this;
		}

		inline AampTime operator/(const double &t) const
		{
			AampTime temp;
			if (t != 0.0)
			{
				temp.baseTime = static_cast<int64_t>(
					static_cast<double>(baseTime) / t);
			}
			// Otherwise leave as zero
			return temp;
		}

		inline AampTime operator*(const double &t) const
		{
			AampTime temp(*this);
			temp.baseTime = static_cast<int64_t>(
				static_cast<double>(baseTime) * t);
			return temp;
		}

		explicit operator double() const { return this->inSeconds(); }
		explicit operator int64_t() const { return this->seconds(); }
};

//  For those who like if (0.0 == b)
inline bool operator==(const double& lhs, const AampTime& rhs) { return rhs == lhs; }
inline bool operator!=(const double& lhs, const AampTime& rhs) { return !(rhs == lhs); }

inline AampTime operator+(const double &lhs, const AampTime &rhs) { return rhs + lhs; }
inline AampTime operator-(const double &lhs, const AampTime &rhs) { return -rhs + lhs; }

inline AampTime operator*(const int64_t &lhs, const AampTime &rhs) { return rhs * lhs; }

// Adding double & AampTime and expecting a double will need to use AampTime::inSeconds() instead
// Where a double is to be passed by reference, if the prototype cannot be rewritten or overloaded then
// a temporary double will be needed

inline double operator+=(double &lhs, const AampTime &rhs)
{
	lhs = lhs + rhs.inSeconds();
	return lhs;
}

inline bool operator>(const double &lhs, const AampTime &rhs) { return rhs < lhs; }
inline bool operator<(const double &lhs, const AampTime &rhs) { return rhs > lhs; }
inline bool operator<=(const double &lhs, const AampTime &rhs) { return rhs >= lhs; }
inline bool operator>=(const double &lhs, const AampTime &rhs) { return rhs <= lhs; }

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

#endif // AAMPTIME_H
