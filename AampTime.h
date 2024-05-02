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

#ifndef AAMPTIME_H
#define AAMPTIME_H

/// @brief time class to work around the use of doubles within Aamp
//  While operators are overloaded for comparisons, the underlying data type is integer
//  But the code is tolerant of being treated as a double

class AampTime
{
	private:
		static const uint64_t timescale = 1000000000;
		int64_t nanoseconds;

	public:
		/// @brief Constructor
		/// @param seconds time in seconds, as a double
		AampTime(double seconds = 0.0) : nanoseconds(int64_t(seconds * timescale)){}
		AampTime(const AampTime& rhs)  : nanoseconds(rhs.nanoseconds){}

		/// @brief Get the stored time
		/// @return Time in seconds (double)
		inline const double inSeconds() const {return (nanoseconds / double(timescale));}

		inline const int64_t seconds() const { return (nanoseconds / timescale); }
		inline const int64_t milliseconds() const { return (nanoseconds / (timescale / 1000)); }

		// Equivalent to round() but in integer domain
		inline const int64_t nearestSecond() const
		{
			int64_t retval = this->seconds();

			// Fractional part
			int64_t tempval = nanoseconds - retval * timescale;

			if (tempval >= ((5 * timescale)/10))
			{
				retval += 1;
			}

			return retval;
		}

		// Overloads for comparison operators to check AampTime : AampTime and AampTime : double
		// Converting (and truncating) the double to the timescale should avoid the issues around epsilon for floating point
		inline const bool operator==(const AampTime &rhs) const
		{
			if (this == &rhs)
				return true;
			else
				return (nanoseconds == rhs.nanoseconds);
		}

		inline const bool operator==(double &rhs) const { return (nanoseconds == int64_t(rhs * timescale)); }

		inline AampTime& operator=(const AampTime &rhs)
		{
			if (this == &rhs)
				return *this;

			nanoseconds = rhs.nanoseconds;
			return *this;
		}

		inline AampTime& operator=(const double &rhs)
		{
			nanoseconds = int64_t(rhs * timescale);
			return *this;
		}

		inline AampTime operator-() const
		{
			AampTime temp(*this);
			temp.nanoseconds = -nanoseconds;
			return temp;
		}

		inline const bool operator!=(const AampTime &rhs) const { return !(*this == rhs); }
		inline const bool operator!=(double &rhs) const { return !(*this == rhs); }

		inline const bool operator>(const AampTime &rhs) const { return (nanoseconds > rhs.nanoseconds); }
		inline const bool operator>(const double &rhs) const { return (nanoseconds > int64_t(rhs * timescale)); }

		inline const bool operator<(const AampTime &rhs) const { return ((*this != rhs) && (!(*this > rhs))); }
		inline const bool operator<(const double &rhs) const { return ((*this != rhs) && (!(*this > rhs))); }

		inline const bool operator>=(const AampTime &rhs) const { return ((*this > rhs) || (*this == rhs)); }
		inline const bool operator>=(double rhs) const { return ((*this > rhs) || (*this == rhs)); }

		inline const bool operator<=(const AampTime &rhs) const { return ((*this < rhs) || (*this == rhs)); }
		inline const bool operator<=(double rhs) const { return ((*this < rhs) || (*this == rhs)); }

		inline const AampTime operator+(const AampTime &t) const
		{
			AampTime temp(*this);

			temp.nanoseconds = nanoseconds + t.nanoseconds;
			return temp;
		}
		inline const AampTime operator+(const double &t) const
		{
			AampTime temp(*this);

			temp.nanoseconds = nanoseconds + int64_t(t * timescale);
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

		inline const AampTime operator-(const AampTime &t) const
		{
			AampTime temp(*this);

			temp.nanoseconds = nanoseconds - t.nanoseconds;
			return std::move(temp);
		}

		inline const AampTime operator-(const double &t) const
		{
			AampTime temp(*this);
			temp.nanoseconds = nanoseconds - int64_t(t * timescale);
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

		inline const AampTime operator/(const double &t) const
		{
			AampTime temp(*this);

			temp.nanoseconds = (int64_t)((double)nanoseconds/t);
			return std::move(temp);
		}

		inline const AampTime operator*(const double &t) const
		{
			AampTime temp(*this);

			temp.nanoseconds = (int64_t)((double)nanoseconds * t);
			return std::move(temp);
		}

		explicit operator double() const { return this->inSeconds(); }
		explicit operator int64_t() const { return this->seconds(); }
};

//  For those who like if (0.0 == b)
inline const bool operator==(const double& lhs, const AampTime& rhs) { return (rhs.operator==(lhs)); };
inline const bool operator!=(const double& lhs, const AampTime& rhs) { return !(rhs == lhs); };

inline const AampTime operator+(const double &lhs, const AampTime &rhs) { return rhs + lhs; };
inline const AampTime operator-(const double &lhs, const AampTime &rhs) { return -rhs + lhs; };

inline const AampTime operator*(const int64_t &lhs, const AampTime &rhs) { return rhs * lhs; };

// Adding double & AampTime and expecting a double will need to use AampTime::inSeconds() instead
// Where a double is to be passed by reference, if the prototype cannot be rewritten or overloaded then
// a temporary double will be needed

inline const double operator+=(double &lhs, const AampTime &rhs)
{
	lhs = lhs + rhs.inSeconds();
	return lhs;
}

inline const bool operator>(const double &lhs, const AampTime &rhs) { return (rhs.operator<(lhs)); };
inline const bool operator<(const double &lhs, const AampTime &rhs) { return (rhs.operator>(lhs)); };
inline const bool operator<=(const double &lhs, const AampTime &rhs) { return (rhs >= lhs); };
inline const bool operator>=(const double &lhs, const AampTime &rhs) { return (rhs <= lhs); };

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
