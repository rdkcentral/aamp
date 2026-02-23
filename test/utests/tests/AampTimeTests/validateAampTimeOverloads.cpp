/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include "AampTime.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <limits>

using ::testing::_;
using ::testing::Return;

// Suite of microtests to validate operation of AampTime

const double oneNano = std::pow(10.0, -9);
constexpr double MICROSECOND_TOLERANCE = 1e-6;

class validateAampTimeOverloads : public ::testing::Test
{
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

TEST_F(validateAampTimeOverloads, testConstructor)
{
	AampTime a;
	AampTime b(100);
	AampTime c(1094.1);
	AampTicks dTicks(1094100, AampTime::TimeScale::milli);
	AampTime d(dTicks);

	// Verify that stored time is accurate to the expected time to within 1ns
	ASSERT_TRUE((fabs(a.inSeconds()) < oneNano));
	ASSERT_TRUE((fabs(b.inSeconds() - 100.0) < oneNano));
	ASSERT_TRUE((fabs(c.inSeconds() - 1094.1) < oneNano));
	ASSERT_TRUE((fabs(d.inSeconds() - 1094.1) < oneNano));
}

// The thud & blunder approach is because Google test does not support overloading operators cleanly.
// Attempte to pass them to the matchers end badly; could use delegation, but using ASSERT is less cumbersome

TEST_F(validateAampTimeOverloads, testAssignment)
{
	AampTime a(0.0);
	AampTime b(100.0);
	AampTime c(200.0);

	// Assign from object
	a = b;
	ASSERT_TRUE((a == b));

	// Assign from double
	a = 200.0;
	ASSERT_TRUE((a == c));

	ASSERT_TRUE((fabs(c.inSeconds() - 200.0) < oneNano));
}

TEST_F(validateAampTimeOverloads, testEquality)
{
	AampTime a;
	AampTime b(100);
	AampTime c(0.0);
	const AampTime d(0.0);
	const AampTime e(1.0);

	// Test equality between objects

	// Compare with self
	ASSERT_TRUE((a == a));
	// Compare with another object
	ASSERT_TRUE((a == c));
	// Compare with a const object
	ASSERT_TRUE((c == d));
	// Compare const object with another object
	ASSERT_TRUE((d == c));
	// Compare with another unequal object
	ASSERT_FALSE((a == b));
	// Verify != (inversion of ==)
	ASSERT_TRUE((a != b));
	// Compare const object with const object
	ASSERT_TRUE((d != e));

	// Test equality between object and double
	ASSERT_TRUE((a == 0.0));
	ASSERT_FALSE((a == 1.0));
	// Verify inverse operation
	ASSERT_TRUE((a != 1.0));
	ASSERT_FALSE((a != 0.0));
	// Test equality between const object and double
	ASSERT_TRUE((e == 1.0));

	// Test equality between double and object
	ASSERT_TRUE((0.0 == a));
	ASSERT_FALSE((1.0 == a));
	// Verify inverse operation
	ASSERT_TRUE((1.0 != a));
	ASSERT_FALSE((0.0 != a));
	// Test equality between double & const object
	ASSERT_TRUE((1.0 == e));
}

TEST_F(validateAampTimeOverloads, testNegation)
{
	AampTime a(1.0);
	AampTime b(-1.0);

	ASSERT_TRUE((-a == -1.0));
	ASSERT_TRUE((a == -b));

	a = 0.0;
	ASSERT_TRUE((-a == 0.0));
}

TEST_F(validateAampTimeOverloads, testComparisons)
{
	AampTime a(100.0);
	AampTime b(100.0);
	AampTime c(200.0);
	const AampTime d(300.0);
	AampTime e(0.0);

	// Verify self is neither > nor < self
	ASSERT_FALSE((a > a));
	ASSERT_FALSE((a < a));

	// Verify object > other object
	ASSERT_TRUE((c > a));
	// Verify inverse operation
	ASSERT_TRUE((a < c));
	// Verify const object > object
	ASSERT_TRUE((d > c));
	// Verify inverse operation (object < const object)
	ASSERT_TRUE((c < d));

	// Test object against double
	ASSERT_FALSE((a > 100.0));
	ASSERT_FALSE((a < 100.0));
	ASSERT_TRUE((a > 0.0));
	ASSERT_TRUE((a < 150.0));
	// Test const object
	ASSERT_TRUE((d > 0.0));
	// ASSERT_TRUE((0.0 < d));  // Not implemented

	// Compare objects with other objects
	ASSERT_TRUE((a <= b));
	ASSERT_TRUE((a >= b));
	ASSERT_TRUE((a <= c));
	ASSERT_TRUE((c >= a));
	ASSERT_FALSE((c <= a));
	ASSERT_FALSE((a >= c));
	// Test using const object as both lvalue and rvalue
	ASSERT_TRUE((a <= d));
	ASSERT_TRUE((d >= a));

	// Comparisons with 0
	ASSERT_FALSE((e > 0.0));
	ASSERT_FALSE((e < 0.0));
	ASSERT_TRUE((e >= 0.0));
	ASSERT_TRUE((e <= 0.0));
}


TEST_F(validateAampTimeOverloads, testAddition)
{
	AampTime a(0);
	AampTime b(10.0);
	const AampTime c(20.0);
	const double d(5.0);

	ASSERT_TRUE((a != b));

	// Object lvalue, double rvalue
	a = b + 10;
	ASSERT_TRUE((a == 20.0));

	a = 0.0;
	// Double rvalue
	a += 10.0;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, object rvalue
	a = a + b;
	ASSERT_TRUE((a == 20.0));

	// Object rvalue
	a += b;
	ASSERT_TRUE((a == 30.0));

	// Addition with const object as both lvalue and rvalue in overload
	a = c + 10.0;
	ASSERT_TRUE((a == 30.0));
	a = 10.0 + c;
	ASSERT_TRUE((a == 30.0));
	a += c;
	ASSERT_TRUE((a == 50.0));

	// Object lvalue, const double rvalue
	a = b + d;
	ASSERT_TRUE((a == 15.0));

	// Const object lvalue, const double rvalue
	a = c + d;
	ASSERT_TRUE((a == 25.0));
}

TEST_F(validateAampTimeOverloads, testSubtraction)
{
	AampTime a(10.0);
	AampTime b(30.0);
	const AampTime c(10);
	const double d(5.0);

	// Object lvalue, double rvalue
	a = b - 10.0;
	ASSERT_TRUE((a == 20.0));

	a = b;
	// Double rvalue
	a -= 10.0;
	ASSERT_TRUE((a == 20.0));

	// Const object lvalue, object rvalue
	a = c - b;
	ASSERT_TRUE((a == -20.0));

	// Object lvalue, const object rvalue
	a = b - c;
	ASSERT_TRUE((a == 20.0));

	// Double lvalue, const object rvalue
	a = 20.0 - c;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, const double rvalue
	a = b - d;
	ASSERT_TRUE((a == 25.0));

	// Const double lvalue, object rvalue
	a = d - b;
	ASSERT_TRUE((a == -25.0));

	// Const object lvalue, const double rvalue
	a = c - d;
	ASSERT_TRUE((a == 5.0));

	// Const double lvalue, const object rvalue
	a = d - c;
	ASSERT_TRUE((a == -5.0));

	// Const double rvalue
	a-=d;
	ASSERT_TRUE((a == -10.0));
}

TEST_F(validateAampTimeOverloads, testDivision)
{
	AampTime a(10.0);
	const AampTime b(20.0);
	const double c(2.0);

	// Object lvalue, double rvalue
	a = a / 2.0;
	ASSERT_TRUE((a == 5.0));

	// Const object lvalue, double rvalue
	a = b / 2.0;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, const double rvalue
	a = a / c;
	ASSERT_TRUE((a == 5.0));

	// Const object lvalue, const double rvalue
	a = b / c;
	ASSERT_TRUE((a == 10.0));

	// double lvalue, object rvalue not implemented
}

TEST_F(validateAampTimeOverloads, testMultiplication)
{
	AampTime a(10.0);
	const AampTime b(20.0);
	const double c(2.0);

	// Object lvalue, double rvalue
	a = a * 2.0;
	ASSERT_TRUE((a == 20.0));

	// Demonstrate type promotion works
	a = a * 3;
	ASSERT_TRUE((a == 60));

	// Const object lvalue, double rvalue
	a = b * 2.0;
	ASSERT_TRUE((a == 40.0));

	// Const object lvalue, const double rvalue
	a = b * c;
	ASSERT_TRUE((a == 40.0));

	// double lvalue, object rvalue not implemented
}

TEST_F(validateAampTimeOverloads, testIntegerHelpers)
{
	AampTime a(2.4);
	AampTime b(1.9999);
	AampTime c(0.1);
	AampTime d{0.0001};

	ASSERT_EQ(a.seconds(), 2);
	ASSERT_EQ(a.milliseconds(), 2400);
	ASSERT_EQ(b.seconds(), 1);
	ASSERT_EQ(b.milliseconds(), 1999);
	ASSERT_EQ(c.seconds(), 0);
	ASSERT_EQ(c.milliseconds(), 100);
	ASSERT_EQ(d.seconds(), 0);
	ASSERT_EQ(d.milliseconds(), 0);
	ASSERT_EQ(a.nearestSecond(), 2);
	ASSERT_EQ(b.nearestSecond(), 2);
	ASSERT_EQ(c.nearestSecond(), 0);
}


TEST_F(validateAampTimeOverloads, testCasting)
{
	AampTime a{2.4};

	ASSERT_DOUBLE_EQ((double)a, 2.4);
	ASSERT_EQ((int64_t)a, 2);
}

/**
 * @brief Test case for AampTicks::inMilli
 */
TEST_F(validateAampTimeOverloads, AampTicksInMilli)
{
	AampTicks ticks(5000, 1000); // 5000 ticks with a timescale of 1000
	EXPECT_EQ(ticks.inMilli(), 5000); // 5000 milliseconds
}

// New tests to validate overflow handling when converting AampTicks -> AampTime
TEST_F(validateAampTimeOverloads, AampTicksConversion_MaxValueNoOverflow)
{
	// Use timescale 1 and INT64_MAX ticks to test overflow protection (expects clamping to INT64_MAX)
	AampTicks hugeTicks(std::numeric_limits<int64_t>::max(), 1u);
	AampTime t(hugeTicks);

	// The implementation should handle this value without clamping; inSeconds() should equal INT64_MAX / baseTimescale
	const double expected = static_cast<double>(std::numeric_limits<int64_t>::max()) / 1e9;
	EXPECT_NEAR(t.inSeconds(), expected, MICROSECOND_TOLERANCE);
}

TEST_F(validateAampTimeOverloads, AampTicksConversion_NegativeOverflowClamps)
{
	AampTicks hugeNegTicks(std::numeric_limits<int64_t>::min(), uint32_t{1});
	AampTime t(hugeNegTicks);

	const double expected = static_cast<double>(std::numeric_limits<int64_t>::min()) / 1e9;
	EXPECT_NEAR(t.inSeconds(), expected, MICROSECOND_TOLERANCE);
}

// Validate AampTicks -> AampTime conversion when values do not overflow
TEST_F(validateAampTimeOverloads, AampTicksConversion_NoOverflow)
{
	// Use 33-bit max PTS example and a 90kHz timebase (common PTS timebase)
	const int64_t ticks = static_cast<int64_t>((1ULL << 33) - 1ULL); // max 33-bit PTS
	const uint32_t timebase = 90000u; // 90 kHz

	AampTicks smallTicks(ticks, timebase);
	AampTime t(smallTicks);

	// Expected seconds: ticks / timebase
	// Note: Due to the internal conversion (ticks * 1e9 / timebase) then / 1e9,
	// there may be minor floating-point precision differences, so use NEAR comparison
	const double expected = static_cast<double>(ticks) / static_cast<double>(timebase);
	EXPECT_NEAR(t.inSeconds(), expected, MICROSECOND_TOLERANCE);
}

// Test divide-by-zero protection in AampTicks conversion
TEST_F(validateAampTimeOverloads, AampTicksConversion_ZeroTimescaleProtection)
{
	// Test 1: Positive ticks with zero timescale
	AampTicks positiveTicks(1000, 0u);
	AampTime t1(positiveTicks);
	EXPECT_DOUBLE_EQ(t1.inSeconds(), 0.0);

	// Test 2: Negative ticks with zero timescale
	AampTicks negativeTicks(-1000, 0u);
	AampTime t2(negativeTicks);
	EXPECT_DOUBLE_EQ(t2.inSeconds(), 0.0);

	// Test 3: Zero ticks with zero timescale (edge case)
	AampTicks zeroTicks(0, 0u);
	AampTime t3(zeroTicks);
	EXPECT_DOUBLE_EQ(t3.inSeconds(), 0.0);

	// Test 4: inMilli() with zero timescale
	AampTicks ticksForMilli(5000, 0u);
	EXPECT_EQ(ticksForMilli.inMilli(), 0);
}

// Test overflow clamping when ticks * 1e9 exceeds INT64_MAX
TEST_F(validateAampTimeOverloads, AampTicksConversion_PositiveOverflowClamps)
{
	// Internally, AampTime stores time in nanoseconds (baseTime is int64_t).
	// convertTicksWithOverflowProtection() computes: (ticks * baseTimescale) / timescale
	// where baseTimescale = 1e9 (nanoseconds per second).
	// 
	// With ticks = INT64_MAX/100 and timescale = 1:
	// (INT64_MAX/100 * 1e9) / 1 = ~9.2e19, which exceeds INT64_MAX (~9.2e18)
	// So the result clamps to INT64_MAX nanoseconds.
	// 
	// inSeconds() then converts back: INT64_MAX / 1e9 ≈ 9.2e9 seconds
	const int64_t ticks = std::numeric_limits<int64_t>::max() / 100;
	const uint32_t timescale = 1u;
	
	AampTicks overflowTicks(ticks, timescale);
	AampTime t(overflowTicks);
	
	// After clamping to INT64_MAX nanoseconds, inSeconds() should return INT64_MAX / 1e9
	const double expectedMax = static_cast<double>(std::numeric_limits<int64_t>::max()) / 1e9;
	EXPECT_DOUBLE_EQ(t.inSeconds(), expectedMax);
	
	// Sanity checks that value is in expected range
	EXPECT_GT(t.inSeconds(), 9e9);  // Greater than 9 billion seconds
	EXPECT_LT(t.inSeconds(), 1e10); // Less than 10 billion seconds
}

// Test overflow clamping when ticks * 1e9 goes below INT64_MIN
TEST_F(validateAampTimeOverloads, AampTicksConversion_NegativeOverflowClampsToMin)
{
	// Internally, AampTime stores time in nanoseconds (baseTime is int64_t).
	// convertTicksWithOverflowProtection() computes: (ticks * baseTimescale) / timescale
	// where baseTimescale = 1e9 (nanoseconds per second).
	// 
	// With ticks = INT64_MIN/100 and timescale = 1:
	// (INT64_MIN/100 * 1e9) / 1 = ~-9.2e19, which is less than INT64_MIN (~-9.2e18)
	// So the result clamps to INT64_MIN nanoseconds.
	// 
	// inSeconds() then converts back: INT64_MIN / 1e9 ≈ -9.2e9 seconds
	const int64_t ticks = std::numeric_limits<int64_t>::min() / 100;
	const uint32_t timescale = 1u;
	
	AampTicks overflowTicks(ticks, timescale);
	AampTime t(overflowTicks);
	
	// After clamping to INT64_MIN nanoseconds, inSeconds() should return INT64_MIN / 1e9
	const double expectedMin = static_cast<double>(std::numeric_limits<int64_t>::min()) / 1e9;
	EXPECT_DOUBLE_EQ(t.inSeconds(), expectedMin);
	
	// Sanity checks that value is in expected range
	EXPECT_LT(t.inSeconds(), -9e9);  // Less than -9 billion seconds
	EXPECT_GT(t.inSeconds(), -1e10); // Greater than -10 billion seconds
}

// Test that overflow protection prevents wraparound (this test FAILS without protection)
TEST_F(validateAampTimeOverloads, AampTicksConversion_OverflowProtectionPreventsWraparound)
{
	// This test ensures overflow protection is actually working
	// Without protection, (INT64_MAX / 100) * 1e9 wraps to a small negative value
	// With protection, it clamps to INT64_MAX (positive, ~9.2e9 seconds)
	const int64_t ticks = std::numeric_limits<int64_t>::max() / 100;
	const uint32_t timescale = 1u;
	
	AampTicks overflowTicks(ticks, timescale);
	AampTime t(overflowTicks);
	
	// Value MUST be positive (if it wrapped, it would be negative)
	EXPECT_GT(t.inSeconds(), 0.0);
	
	// Value MUST be very large (> 1 billion seconds)
	EXPECT_GT(t.inSeconds(), 1e9);
	
	// Specifically, it should be clamped to INT64_MAX / 1e9
	EXPECT_DOUBLE_EQ(t.inSeconds(), static_cast<double>(std::numeric_limits<int64_t>::max()) / 1e9);
}

// Test that negative overflow protection prevents wraparound with different divisor
TEST_F(validateAampTimeOverloads, AampTicksConversion_NegativeOverflowProtectionPreventsWraparound)
{
	// Test negative overflow with a very small timescale (different from _NegativeOverflowClampsToMin)
	// With ticks = INT64_MIN / 10 and timescale = 100:
	// (INT64_MIN/10 * 1e9) / 100 still overflows and clamps to INT64_MIN
	// This tests that overflow protection works across different timescale values
	const int64_t ticks = std::numeric_limits<int64_t>::min() / 10;
	const uint32_t timescale = 100u;
	
	AampTicks overflowTicks(ticks, timescale);
	AampTime t(overflowTicks);
	
	// Value MUST be negative (verifies no wraparound occurred)
	EXPECT_LT(t.inSeconds(), 0.0);
	
	// Value MUST be within expected INT64_MIN clamping range
	// Result is clamped to INT64_MIN nanoseconds, then converted: INT64_MIN / 1e9
	const double expectedMin = static_cast<double>(std::numeric_limits<int64_t>::min()) / 1e9;
	EXPECT_DOUBLE_EQ(t.inSeconds(), expectedMin);
}

// Test AampTicks::inMilli() positive overflow clamping
TEST_F(validateAampTimeOverloads, AampTicksInMilli_PositiveOverflowClamps)
{
	// Create scenario where (ticks * 1000) / timescale > INT64_MAX
	// Use ticks = INT64_MAX / 10, timescale = 1
	const int64_t ticks = std::numeric_limits<int64_t>::max() / 10;
	const uint32_t timescale = 1u;
	
	AampTicks overflowTicks(ticks, timescale);
	
	// Should clamp to INT64_MAX
	EXPECT_EQ(overflowTicks.inMilli(), std::numeric_limits<int64_t>::max());
}

// Test AampTicks::inMilli() negative overflow clamping
TEST_F(validateAampTimeOverloads, AampTicksInMilli_NegativeOverflowClamps)
{
	// Create scenario where (ticks * 1000) / timescale < INT64_MIN
	const int64_t ticks = std::numeric_limits<int64_t>::min() / 10;
	const uint32_t timescale = 1u;
	
	AampTicks overflowTicks(ticks, timescale);
	
	// Should clamp to INT64_MIN
	EXPECT_EQ(overflowTicks.inMilli(), std::numeric_limits<int64_t>::min());
}

// Test AampTicks::inMilli() with normal values (no overflow)
TEST_F(validateAampTimeOverloads, AampTicksInMilli_NormalValues)
{
	// Test various normal scenarios
	// Formula: (ticks * 1000) / timescale
	AampTicks ticks1(90000, 90); // (90000 * 1000) / 90 = 1000000 milliseconds
	EXPECT_EQ(ticks1.inMilli(), 1000000);
	
	AampTicks ticks2(45000, 90); // (45000 * 1000) / 90 = 500000 milliseconds
	EXPECT_EQ(ticks2.inMilli(), 500000);
	
	AampTicks ticks3(-90000, 90); // (-90000 * 1000) / 90 = -1000000 milliseconds
	EXPECT_EQ(ticks3.inMilli(), -1000000);
	
	// Test with 90kHz timescale (common for PTS) with large but safe value
	// Value: 90000000 ticks at 90kHz
	// Calculation: (90000000 * 1000) / 90000 = 90000000000 / 90000 = 1000000 ms
	// This verifies large value handling without triggering overflow
	AampTicks ticks4(90000000LL, 90000);
	EXPECT_EQ(ticks4.inMilli(), 1000000);
}

// Test boundary conditions for timescale conversion
TEST_F(validateAampTimeOverloads, AampTicksConversion_BoundaryTimescales)
{
	// Test with timescale = 1 (ticks == nanoseconds)
	AampTicks ticks1(1000000000LL, 1u);
	AampTime t1(ticks1);
	EXPECT_NEAR(t1.inSeconds(), 1000000000.0, MICROSECOND_TOLERANCE);
	
	// Test with timescale = 1000 (ticks in milliseconds)
	AampTicks ticks2(5000, 1000u);
	AampTime t2(ticks2);
	EXPECT_NEAR(t2.inSeconds(), 5.0, MICROSECOND_TOLERANCE);
	
	// Test with timescale = 90000 (common PTS timescale)
	AampTicks ticks3(90000, 90000u);
	AampTime t3(ticks3);
	EXPECT_NEAR(t3.inSeconds(), 1.0, MICROSECOND_TOLERANCE);
	
	// Test with very large timescale
	AampTicks ticks4(1000000000LL, 1000000000u);
	AampTime t4(ticks4);
	EXPECT_NEAR(t4.inSeconds(), 1.0, MICROSECOND_TOLERANCE);
}

// Test that small timescale values don't cause precision loss
TEST_F(validateAampTimeOverloads, AampTicksConversion_SmallTimescalePrecision)
{
	// With small timescale, each tick represents a large time interval
	AampTicks ticks1(100, 10u); // 10 seconds
	AampTime t1(ticks1);
	EXPECT_NEAR(t1.inSeconds(), 10.0, MICROSECOND_TOLERANCE);
	
	AampTicks ticks2(1, 1u); // 1 second
	AampTime t2(ticks2);
	EXPECT_NEAR(t2.inSeconds(), 1.0, MICROSECOND_TOLERANCE);
}

// Test conversion with maximum safe values (no overflow expected)
TEST_F(validateAampTimeOverloads, AampTicksConversion_MaximumSafeValues)
{
	// Find a safe maximum: we need ticks such that ticks * 1e9 <= INT64_MAX
	// INT64_MAX / 1e9 ≈ 9.22e9
	const int64_t safeTicks = 9000000000LL; // 9 billion ticks
	const uint32_t timescale = 1000u; // milliseconds
	
	AampTicks ticks(safeTicks, timescale);
	AampTime t(ticks);
	
	const double expected = static_cast<double>(safeTicks) / static_cast<double>(timescale);
	EXPECT_NEAR(t.inSeconds(), expected, MICROSECOND_TOLERANCE);
}

// Test that negative values work correctly without overflow
TEST_F(validateAampTimeOverloads, AampTicksConversion_NegativeValuesNoOverflow)
{
	// Test negative ticks with various timescales
	AampTicks ticks1(-90000, 90000u); // -1 second
	AampTime t1(ticks1);
	EXPECT_NEAR(t1.inSeconds(), -1.0, MICROSECOND_TOLERANCE);
	
	AampTicks ticks2(-5000, 1000u); // -5 seconds
	AampTime t2(ticks2);
	EXPECT_NEAR(t2.inSeconds(), -5.0, MICROSECOND_TOLERANCE);
}

// Test realistic PTS overflow scenario (33-bit PTS wrapping)
TEST_F(validateAampTimeOverloads, AampTicksConversion_PTSWrapScenario)
{
	// 33-bit PTS can wrap around; test values near the boundary
	const int64_t maxPTS33 = (1LL << 33) - 1; // 8589934591
	const uint32_t ptsTimescale = 90000u; // Standard MPEG-2 TS timescale
	
	AampTicks nearMaxPTS(maxPTS33, ptsTimescale);
	AampTime t(nearMaxPTS);
	
	const double expected = static_cast<double>(maxPTS33) / static_cast<double>(ptsTimescale);
	EXPECT_NEAR(t.inSeconds(), expected, MICROSECOND_TOLERANCE);
	
	// Verify this is approximately 26.5 hours (millisecond precision tolerance)
	// This value should be ~95443.717588... seconds (~26.51 hours)
	EXPECT_NEAR(t.inSeconds(), 95443.717588889, 1e-3);
}

// Test arithmetic operations don't cause internal overflow in baseTime
TEST_F(validateAampTimeOverloads, ArithmeticOperations_LargeValues)
{
	// Create large AampTime values from safe tick conversions
	const double largeTime = 1000000.0; // ~11.5 days in seconds
	AampTime t1(largeTime);
	AampTime t2(largeTime);
	
	// Test addition of large values
	AampTime sum = t1 + t2;
	EXPECT_NEAR(sum.inSeconds(), 2000000.0, MICROSECOND_TOLERANCE);
	
	// Test subtraction
	AampTime diff = t2 - t1;
	EXPECT_NEAR(diff.inSeconds(), 0.0, MICROSECOND_TOLERANCE);
	
	// Test multiplication
	AampTime product = t1 * 2.0;
	EXPECT_NEAR(product.inSeconds(), 2000000.0, MICROSECOND_TOLERANCE);
	
	// Test division
	AampTime quotient = t1 / 2.0;
	EXPECT_NEAR(quotient.inSeconds(), 500000.0, MICROSECOND_TOLERANCE);
}

// Test that multiplication doesn't overflow with large values
TEST_F(validateAampTimeOverloads, MultiplicationOperator_ExtremeValues)
{
	// Use a large but safe value for multiplication testing
	const double largeTime = 1000000.0; // ~11.5 days
	AampTime t(largeTime);
	
	// Multiplying by small values should work
	AampTime result1 = t * 0.5;
	EXPECT_NEAR(result1.inSeconds(), largeTime * 0.5, MICROSECOND_TOLERANCE);
	
	// Multiplying by 1 should preserve value
	AampTime result2 = t * 1.0;
	EXPECT_NEAR(result2.inSeconds(), largeTime, MICROSECOND_TOLERANCE);
	
	// Multiplying by larger values
	AampTime result3 = t * 2.0;
	EXPECT_NEAR(result3.inSeconds(), largeTime * 2.0, MICROSECOND_TOLERANCE);
}

// Test that division by very small numbers doesn't overflow
TEST_F(validateAampTimeOverloads, DivisionOperator_SmallDivisor)
{
	AampTime t(100.0);
	
	// Division by small positive number
	AampTime result1 = t / 0.5;
	EXPECT_NEAR(result1.inSeconds(), 200.0, MICROSECOND_TOLERANCE);
	
	// Division by very small number
	AampTime result2 = t / 0.01;
	EXPECT_NEAR(result2.inSeconds(), 10000.0, MICROSECOND_TOLERANCE);
	
	// Division by zero should return zero (as per implementation)
	AampTime result3 = t / 0.0;
	EXPECT_DOUBLE_EQ(result3.inSeconds(), 0.0);
}

// Test addition with mixed positive and negative values
TEST_F(validateAampTimeOverloads, Addition_MixedSigns)
{
	AampTime positive(1000.0);
	AampTime negative(-500.0);
	
	AampTime result1 = positive + negative;
	EXPECT_NEAR(result1.inSeconds(), 500.0, MICROSECOND_TOLERANCE);
	
	AampTime result2 = negative + positive;
	EXPECT_NEAR(result2.inSeconds(), 500.0, MICROSECOND_TOLERANCE);
	
	AampTime result3 = negative + negative;
	EXPECT_NEAR(result3.inSeconds(), -1000.0, MICROSECOND_TOLERANCE);
}

// Test subtraction with mixed positive and negative values
TEST_F(validateAampTimeOverloads, Subtraction_MixedSigns)
{
	AampTime positive(1000.0);
	AampTime negative(-500.0);
	
	AampTime result1 = positive - negative;
	EXPECT_NEAR(result1.inSeconds(), 1500.0, MICROSECOND_TOLERANCE);
	
	AampTime result2 = negative - positive;
	EXPECT_NEAR(result2.inSeconds(), -1500.0, MICROSECOND_TOLERANCE);
	
	AampTime result3 = negative - negative;
	EXPECT_NEAR(result3.inSeconds(), 0.0, MICROSECOND_TOLERANCE);
}

// Test that milliseconds() doesn't overflow for large values
TEST_F(validateAampTimeOverloads, Milliseconds_LargeValues)
{
	// Test with large positive time value
	const double largeSeconds = 1000000.0; // ~11.5 days
	AampTime t(largeSeconds);
	
	int64_t expectedMillis = static_cast<int64_t>(largeSeconds * 1000);
	EXPECT_EQ(t.milliseconds(), expectedMillis);
	
	// Test with moderate positive values
	AampTime t2(5000.0);  // 5000 seconds
	EXPECT_EQ(t2.milliseconds(), 5000000);
	
	// Test with negative values (bug fix validation)
	AampTime t3(-100.0);  // -100 seconds
	EXPECT_EQ(t3.milliseconds(), -100000);
	
	AampTime t4(-5000.0);  // -5000 seconds
	EXPECT_EQ(t4.milliseconds(), -5000000);
}

// Test edge case: zero values in various operations
TEST_F(validateAampTimeOverloads, ZeroValue_Operations)
{
	AampTime zero(0.0);
	AampTime nonZero(100.0);
	
	// Addition with zero
	EXPECT_DOUBLE_EQ((zero + nonZero).inSeconds(), 100.0);
	EXPECT_DOUBLE_EQ((nonZero + zero).inSeconds(), 100.0);
	
	// Subtraction with zero
	EXPECT_DOUBLE_EQ((zero - nonZero).inSeconds(), -100.0);
	EXPECT_DOUBLE_EQ((nonZero - zero).inSeconds(), 100.0);
	
	// Multiplication with zero
	EXPECT_DOUBLE_EQ((zero * 100.0).inSeconds(), 0.0);
	EXPECT_DOUBLE_EQ((nonZero * 0.0).inSeconds(), 0.0);
	
	// Division of zero
	EXPECT_DOUBLE_EQ((zero / 100.0).inSeconds(), 0.0);
	
	// Negation of zero
	EXPECT_DOUBLE_EQ((-zero).inSeconds(), 0.0);
}

// Test copy constructor with extreme values
TEST_F(validateAampTimeOverloads, CopyConstructor_ExtremeValues)
{
	// Test with maximum safe value
	const double maxSafeTime = static_cast<double>(std::numeric_limits<int64_t>::max()) / 1e9;
	AampTime original(maxSafeTime);
	AampTime copy(original);
	
	EXPECT_DOUBLE_EQ(original.inSeconds(), copy.inSeconds());
	EXPECT_TRUE(original == copy);
	
	// Test with minimum safe value
	const double minSafeTime = static_cast<double>(std::numeric_limits<int64_t>::min()) / 1e9;
	AampTime originalMin(minSafeTime);
	AampTime copyMin(originalMin);
	
	EXPECT_DOUBLE_EQ(originalMin.inSeconds(), copyMin.inSeconds());
	EXPECT_TRUE(originalMin == copyMin);
}

// Test assignment operator with large values
TEST_F(validateAampTimeOverloads, AssignmentOperator_ExtremeValues)
{
	const double largeTime = 1000000.0; // ~11.5 days - safe value
	AampTime t1(largeTime);
	AampTime t2;
	
	// Assignment from object
	t2 = t1;
	EXPECT_DOUBLE_EQ(t1.inSeconds(), t2.inSeconds());
	
	// Assignment from double
	AampTime t3;
	t3 = largeTime;
	EXPECT_NEAR(t3.inSeconds(), largeTime, MICROSECOND_TOLERANCE);
}

// Test nearestSecond() with various fractional parts
TEST_F(validateAampTimeOverloads, NearestSecond_FractionalParts)
{
	AampTime t1(10.4);  // Should round to 10
	EXPECT_EQ(t1.nearestSecond(), 10);
	
	AampTime t2(10.5);  // Should round to 11
	EXPECT_EQ(t2.nearestSecond(), 11);
	
	AampTime t3(10.6);  // Should round to 11
	EXPECT_EQ(t3.nearestSecond(), 11);
	
	// Test with zero
	AampTime t4(0.4);
	EXPECT_EQ(t4.nearestSecond(), 0);
	
	AampTime t5(0.5);
	EXPECT_EQ(t5.nearestSecond(), 1);
}

// Test conversion with timescale equal to baseTimescale (1e9)
TEST_F(validateAampTimeOverloads, AampTicksConversion_BaseTimescale)
{
	// When timescale == baseTimescale (1e9), ticks should directly map to baseTime
	const int64_t ticks = 5000000000LL; // 5 seconds in nanoseconds
	AampTicks ticksNano(ticks, 1000000000u);
	AampTime t(ticksNano);
	
	EXPECT_NEAR(t.inSeconds(), 5.0, MICROSECOND_TOLERANCE);
}

// Test that very small time values are preserved accurately
TEST_F(validateAampTimeOverloads, SmallTimeValues_Precision)
{
	// Test microsecond precision
	AampTime t1(0.000001); // 1 microsecond
	EXPECT_NEAR(t1.inSeconds(), 0.000001, 1e-9);
	
	// Test nanosecond level (at the limit of representation)
	AampTime t2(0.000000001); // 1 nanosecond
	EXPECT_NEAR(t2.inSeconds(), 0.000000001, 1e-10);
	
	// Test that values smaller than 1 nanosecond are lost due to double precision limits
	// NOTE: This test assumes that the AampTime constructor multiplies the input
	// double (seconds) by 1e9 and truncates to int64_t nanoseconds, so sub-nanosecond
	// values (e.g., 0.1 ns) become 0. This is an implementation detail and should be
	// kept in sync with the AampTime constructor's documentation and behavior.
	AampTime t3(0.0000000001); // 0.1 nanoseconds (sub-nanosecond)
	EXPECT_DOUBLE_EQ(t3.inSeconds(), 0.0);
}

TEST_F(validateAampTimeOverloads, MulOverflow_Detect)
{
	int64_t out;
	
	EXPECT_EQ(mul_overflow(0, 123, &out), false);
	EXPECT_EQ(out, 0);

	EXPECT_EQ(mul_overflow(INT64_MAX, 0, &out), false);
	EXPECT_EQ(out, 0);

	EXPECT_EQ(mul_overflow(INT64_MIN, 1, &out), false);
	EXPECT_EQ(out, INT64_MIN);
	
	EXPECT_EQ(mul_overflow(1, 42, &out), false);
	EXPECT_EQ(out, 42);

	EXPECT_EQ(mul_overflow(-42, 1, &out), false);
	EXPECT_EQ(out, -42);

	EXPECT_EQ(mul_overflow(10, 20, &out), false);
	EXPECT_EQ(out, 200);

	EXPECT_EQ(mul_overflow(-10, -20, &out), false);
	EXPECT_EQ(out, 200);

	EXPECT_EQ(mul_overflow(INT64_MIN, -1, &out), true);
	EXPECT_EQ(mul_overflow(-1,INT64_MIN, &out), true);
	
	EXPECT_EQ(mul_overflow(INT64_MAX, 2, &out), true);
	EXPECT_EQ(mul_overflow(INT64_MIN, 2, &out), true);
}
